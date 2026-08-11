#include "camera_capture.h"
#include "config.h"
#include "composite_out.h"
#include "console_stats.h"
#include "coord_map.h"
#include "display_hub.h"
#include "gimbal_tracker.h"
#include "marker_tracker.h"
#include "mouse_input.h"
#include "osd_renderer.h"
#include "roi_selector.h"
#include "servo_gimbal.h"
#include "track_result.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <csignal>
#include <iostream>
#include <thread>

namespace {

std::atomic<bool> g_running{true};

void onSignal(int)
{
    g_running = false;
}

}  // namespace

int main()
{
    // При запуске через systemd stdout — pipe: без linebuf логи трекинга
    // (std::cout) не попадают в journal до выхода процесса.
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    bench::CameraCapture camera;
    bench::CompositeOutput composite;
    bench::MarkerTracker  marker;
    bench::OsdRenderer osd;
    bench::ServoGimbal gimbal;
    bench::GimbalTracker tracker(gimbal);
    bench::MouseInput mouse;
    bench::RoiSelector roi_sel;
    bench::DisplayHub display;
    bench::ConsoleStats console_stats(bench::kConsoleStatsPath);

    if (!camera.open()) {
        return 1;
    }
    if (!composite.open()) {
        std::cerr << "bench: composite output failed — проверьте J7 и config.txt (см. bench/README.md)\n";
        return 1;
    }
    if (!gimbal.open()) {
        return 1;
    }

    mouse.setScreenSize(bench::kLoresWidth, bench::kLoresHeight);
    roi_sel.setScreenSize(bench::kLoresWidth, bench::kLoresHeight);
    if (!mouse.open()) {
        std::cerr << "bench: mouse optional — ROI selection disabled\n";
    }

    bench::OsdTelemetry telem;
    telem.status    = "STEND";
    telem.mode      = "WAIT ROI";
    telem.gimbal_hw = gimbal.hardwareActive();

    cv::Rect prev_roi;
    bool     prev_was_lost = false;

    using clock = std::chrono::steady_clock;
    clock::time_point lost_banner_until{};
    auto prev_loop     = clock::now();
    uint64_t prev_frame_seq = 0;

    bench::MarkerDetection det_main;
    std::atomic<unsigned> track_frames{0};
    std::atomic<unsigned> pal_frames{0};

    std::thread pal_thread([&]() {
        const auto pal_period =
            std::chrono::milliseconds(1000 / bench::kLoresFps);
        auto next_pal_tick = clock::now();

        while (g_running.load(std::memory_order_relaxed)) {
            next_pal_tick += pal_period;

            bench::DisplaySnapshot snap;
            if (display.consume(snap) && !snap.main_rgb.empty()) {
                bench::CameraFrame frame;
                frame.main_rgb = snap.main_rgb;
                cv::Mat pal_bgr = bench::makeLoresFromFrame(frame);

                const bench::PalMarkerOverlay pal_overlay = bench::mapDetectionToPal(
                    snap.det, bench::kMainWidth, bench::kMainHeight, bench::kLoresWidth,
                    bench::kLoresHeight);

                osd.render(pal_bgr, snap.det, pal_overlay, snap.telem, snap.roi_ui);
                composite.present(pal_bgr);

                ++pal_frames;
            }

            std::this_thread::sleep_until(next_pal_tick);
        }
    });

    // --- инструментирование стадий трек-цикла ---
    auto stat_t0 = clock::now();
    int    stat_frames = 0;
    double sum_grab = 0.0, sum_upd = 0.0, sum_pub = 0.0;
    double max_grab = 0.0, max_upd = 0.0, max_pub = 0.0;
    auto to_ms = [](clock::duration d) {
        return std::chrono::duration<double, std::milli>(d).count();
    };
    while (g_running) {
        bench::CameraFrame frame;
        const auto t_g0 = clock::now();
        const bool got_frame = camera.grabLatest(frame, 1);
        const double t_grab = to_ms(clock::now() - t_g0);
        if (!got_frame) {
            continue;
        }
        if (frame.seq == prev_frame_seq) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            continue;
        }

        const auto loop_now = clock::now();
        double dt_sec = std::chrono::duration<double>(loop_now - prev_loop).count();
        if (frame.seq != prev_frame_seq && prev_frame_seq != 0) {
            dt_sec = std::max(1.0 / (bench::kMainFps * 2.0), dt_sec);
        }
        prev_loop = loop_now;
        prev_frame_seq = frame.seq;
        ++track_frames;

        mouse.poll();
        roi_sel.update(mouse, bench::kMainWidth, bench::kMainHeight);
        mouse.clearEdges();

        const cv::Rect roi_main = roi_sel.trackRoiMain();
        if (roi_main.width > 0 &&
            (roi_main.x != prev_roi.x || roi_main.y != prev_roi.y ||
             roi_main.width != prev_roi.width || roi_main.height != prev_roi.height)) {
            if (frame.main_rgb.empty()) {
                std::cerr << "bench: tracker init failed — need FullHD RGB888 stream\n";
            } else if (marker.init(frame.main_rgb, roi_main)) {
                prev_roi = roi_main;
                prev_was_lost = false;
                lost_banner_until = {};
                telem.mode = "TRACK";
                std::cout << "bench: tracker init " << roi_main.x << ',' << roi_main.y << ' '
                          << roi_main.width << 'x' << roi_main.height << '\n';
            } else {
                std::cerr << "bench: tracker init failed\n";
            }
        } else if (!roi_sel.hasTrackRoi() && prev_roi.width > 0) {
            marker.reset();
            prev_roi = {};
            prev_was_lost = false;
            telem.mode = "WAIT ROI";
        }

        double t_upd = 0.0;
        if (marker.active() && !frame.main_rgb.empty()) {
            const auto t_u0 = clock::now();
            det_main = marker.update(frame.main_rgb, dt_sec);
            t_upd = to_ms(clock::now() - t_u0);

            if (marker.needsOperatorRoi()) {
                roi_sel.clearRoi();
                marker.reset();
                prev_roi          = {};
                prev_was_lost     = false;
                lost_banner_until = {};
                telem.mode        = "WAIT ROI";
                det_main          = {};
            } else if (det_main.track == bench::MarkerTrackMode::Lost) {
                telem.mode = "LOST";
                if (!prev_was_lost) {
                    lost_banner_until = loop_now + std::chrono::duration_cast<clock::duration>(
                                            std::chrono::duration<double>(
                                                bench::kOsdLostMessageSec));
                    prev_was_lost     = true;
                }
            } else if (det_main.capturing) {
                telem.mode        = "TRACK";
                prev_was_lost     = false;
                lost_banner_until = {};
            } else if (det_main.track == bench::MarkerTrackMode::Predicted) {
                telem.mode = "SEARCH";
            }
        } else {
            det_main = {};
        }

        telem.lost_banner = (loop_now < lost_banner_until);

        if (det_main.track == bench::MarkerTrackMode::Lost && !telem.lost_banner &&
            lost_banner_until == clock::time_point{}) {
            lost_banner_until = loop_now + std::chrono::duration_cast<clock::duration>(
                                    std::chrono::duration<double>(bench::kOsdLostMessageSec));
            telem.lost_banner = true;
        }

        const bench::GimbalState gstate = tracker.update(det_main, dt_sec);
        telem.pan_deg  = gstate.pan_deg;
        telem.tilt_deg = gstate.tilt_deg;

        cv::Point2f track_center{};
        if (det_main.valid && det_main.bbox.width > 0) {
            track_center = {det_main.bbox.x + det_main.bbox.width * 0.5f,
                            det_main.bbox.y + det_main.bbox.height * 0.5f};
        }

        // --- агрегируем метрики трек-цикла (до publish → OSD) ---
        ++stat_frames;
        sum_grab += t_grab;
        sum_upd += t_upd;
        max_grab = std::max(max_grab, t_grab);
        max_upd  = std::max(max_upd, t_upd);
        const double stat_elapsed =
            std::chrono::duration<double>(clock::now() - stat_t0).count();
        if (stat_elapsed >= 1.0 && stat_frames > 0) {
            bench::ConsoleStatsSample cs;
            cs.loop_fps     = stat_frames / std::max(stat_elapsed, 1e-6);
            cs.grab_avg     = sum_grab / stat_frames;
            cs.grab_max     = max_grab;
            cs.upd_avg      = sum_upd / stat_frames;
            cs.upd_max      = max_upd;
            cs.pub_avg      = sum_pub / stat_frames;
            cs.pub_max      = max_pub;
            cs.box_w        = det_main.bbox.width;
            cs.box_h        = det_main.bbox.height;
            cs.score        = (det_main.valid ? det_main.confidence : 0.0f);
            cs.track_frames = track_frames.load(std::memory_order_relaxed);
            cs.pal_frames   = pal_frames.load(std::memory_order_relaxed);
            console_stats.write(cs);
            // Временный код для замеров (perf → journal).
            std::cout << "bench: perf loop=" << cs.loop_fps << " box=" << cs.box_w
                      << "x" << cs.box_h << "\n";

            stat_t0     = clock::now();
            stat_frames = 0;
            sum_grab = sum_upd = sum_pub = 0.0;
            max_grab = max_upd = max_pub = 0.0;
        }

        bench::DisplaySnapshot pub;
        pub.det             = det_main;
        pub.telem           = telem;
        pub.roi_ui          = roi_sel.ui();
        pub.tracking_active = marker.active() && det_main.capturing;
        pub.track_center    = track_center;
        pub.init_bbox_size  = marker.initBboxSize();

        const auto t_p0 = clock::now();
        display.publish(frame.main_rgb, frame.seq, loop_now, pub);
        const double t_pub = to_ms(clock::now() - t_p0);
        sum_pub += t_pub;
        max_pub  = std::max(max_pub, t_pub);
    }

    if (pal_thread.joinable()) {
        pal_thread.join();
    }

    gimbal.setAnglesDeg(0.0f, 0.0f);
    gimbal.close();
    composite.close();
    camera.close();
    mouse.close();
    std::cout << "bench: exit\n";
    return 0;
}
