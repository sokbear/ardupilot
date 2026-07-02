#include "camera_capture.h"

#include "config.h"

#include "composite_out.h"

#include "coord_map.h"

#include "gimbal_tracker.h"

#include "mosse_tracker.h"

#include "mouse_input.h"

#include "osd_renderer.h"

#include "roi_selector.h"

#include "servo_gimbal.h"

#include "track_result.h"

#include <atomic>

#include <chrono>

#include <csignal>

#include <iostream>

namespace {

std::atomic<bool> g_running{true};

void onSignal(int)
{
    g_running = false;
}

}  // namespace

int main()
{
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    bench::CameraCapture camera;
    bench::CompositeOutput composite;
    bench::MosseTracker   mosse;
    bench::OsdRenderer osd;
    bench::ServoGimbal gimbal;
    bench::GimbalTracker tracker(gimbal);
    bench::MouseInput mouse;
    bench::RoiSelector roi_sel;

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
    int      track_loss_frames = 0;

    using clock = std::chrono::steady_clock;
    const auto pal_period =
        std::chrono::milliseconds(1000 / bench::kLoresFps);
    auto next_pal_tick = clock::now();
    auto prev_loop     = clock::now();
    uint64_t prev_frame_seq = 0;

    bench::MarkerDetection det_main;
    unsigned pal_frames  = 0;
    unsigned track_frames = 0;

    while (g_running) {
        // --- 1. FullHD @kMainFps: приём кадра с камеры ---
        bench::CameraFrame frame;
        if (!camera.grabLatest(frame, 1)) {
            continue;
        }
        if (frame.seq == prev_frame_seq) {
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
                std::cerr << "bench: MOSSE init failed — need FullHD RGB888 stream\n";
            } else if (mosse.init(frame.main_rgb, roi_main)) {
                prev_roi = roi_main;
                telem.mode = "ROI+MOSSE";
                std::cout << "bench: MOSSE init " << roi_main.x << ',' << roi_main.y << ' '
                          << roi_main.width << 'x' << roi_main.height << '\n';
            } else {
                std::cerr << "bench: MOSSE init failed\n";
            }
        } else if (!roi_sel.hasTrackRoi() && prev_roi.width > 0) {
            mosse.reset();
            prev_roi = {};
            telem.mode = "WAIT ROI";
        }

        // --- 2. FullHD: MOSSE и координаты метки (ex/ey, bbox в main) ---
        if (mosse.active() && !frame.main_rgb.empty()) {
            det_main = mosse.update(frame.main_rgb, dt_sec);
            if (det_main.capturing) {
                track_loss_frames = 0;
            } else if (roi_sel.hasTrackRoi()) {
                ++track_loss_frames;
                if (track_loss_frames >= bench::kTrackLossClearFrames) {
                    roi_sel.clearRoi();
                    mosse.reset();
                    prev_roi = {};
                    track_loss_frames = 0;
                    telem.mode = "WAIT ROI";
                    det_main = {};
                    std::cout << "bench: track lost — ROI cleared\n";
                }
            }
        } else {
            det_main = {};
        }

        const bench::GimbalState gstate = tracker.update(det_main, dt_sec);
        telem.pan_deg  = gstate.pan_deg;
        telem.tilt_deg = gstate.tilt_deg;

        const auto now = clock::now();
        if (now >= next_pal_tick) {
            // --- 3. FullHD → PAL (720×576) ---
            cv::Mat pal_bgr = bench::makeLoresFromFrame(frame);

            // --- 4. Координаты метки FullHD → PAL (рамка OSD) ---
            const bench::PalMarkerOverlay pal_overlay = bench::mapDetectionToPal(
                det_main, bench::kMainWidth, bench::kMainHeight, bench::kLoresWidth,
                bench::kLoresHeight);

            // --- 5. Рамка + OSD на PAL ---
            osd.render(pal_bgr, det_main, pal_overlay, telem, roi_sel.ui());
            composite.present(pal_bgr);

            next_pal_tick = now + pal_period;
            ++pal_frames;

            if ((pal_frames % bench::kLoresFps) == 0) {
                std::cout << "bench: pal=" << pal_frames << " track=" << track_frames
                          << " cap=" << (det_main.capturing ? "yes" : "no")
                          << " roi=" << (roi_sel.hasTrackRoi() ? "yes" : "no")
                          << " pan=" << gstate.pan_deg << " tilt=" << gstate.tilt_deg;
                if (det_main.capturing) {
                    std::cout << " ex=" << det_main.ex << " ey=" << det_main.ey;
                }
                std::cout << std::endl;
            }
        }
    }

    gimbal.setAnglesDeg(0.0f, 0.0f);
    gimbal.close();
    composite.close();
    camera.close();
    mouse.close();
    std::cout << "bench: exit\n";
    return 0;
}
