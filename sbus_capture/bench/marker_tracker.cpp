#include "marker_tracker.h"

#include "config.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace bench {

namespace {

std::string resolveModelPath(const char* rel_path)
{
    namespace fs = std::filesystem;
    std::error_code ec;

    const fs::path rel(rel_path);
    if (fs::exists(rel, ec)) {
        return rel.string();
    }

    const fs::path from_exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        const fs::path from_build = from_exe.parent_path() / ".." / rel;
        const fs::path canon      = fs::weakly_canonical(from_build, ec);
        if (!ec && fs::exists(canon, ec)) {
            return canon.string();
        }
    }

    return std::string(rel_path);
}

}  // namespace

MarkerTracker::MarkerTracker()  = default;
MarkerTracker::~MarkerTracker() = default;

cv::Rect MarkerTracker::clampRect(const cv::Rect& r, int cols, int rows)
{
    const int x = std::clamp(r.x, 0, std::max(0, cols - 1));
    const int y = std::clamp(r.y, 0, std::max(0, rows - 1));
    const int w = std::clamp(r.width, 1, cols - x);
    const int h = std::clamp(r.height, 1, rows - y);
    return {x, y, w, h};
}

bool MarkerTracker::bboxVisibleEnough(const cv::Rect& r)
{
    if (r.width <= 0 || r.height <= 0) {
        return false;
    }
    const cv::Rect full(0, 0, kMainWidth, kMainHeight);
    const cv::Rect inter = r & full;
    return static_cast<float>(inter.area()) >=
           static_cast<float>(r.area()) * kTrackMinVisibleFraction;
}

float MarkerTracker::nanoSearchSidePx(const cv::Rect& bbox_main)
{
    const float w = static_cast<float>(bbox_main.width);
    const float h = static_cast<float>(bbox_main.height);
    if (w <= 0.0f || h <= 0.0f) {
        return 0.0f;
    }
    const float p = (w + h) * 0.5f;
    return 2.0f * std::sqrt((w + p) * (h + p));
}

bool MarkerTracker::bboxSizeSane(const cv::Rect& r) const
{
    if (r.width <= 0 || r.height <= 0) {
        return false;
    }
    // Абсолютный предел: метка не может занимать пол-экрана.
    if (r.width > kMainWidth * kBboxMaxFrameSideRatio ||
        r.height > kMainHeight * kBboxMaxFrameSideRatio) {
        return false;
    }
    // Предел относительно исходного ROI (масштаб по площади).
    if (init_size_.width > 0 && init_size_.height > 0) {
        // Масштаб по площади: инвариантен к изменению соотношения сторон
        // рамки при повороте цели (NanoTrack регрессирует w и h независимо).
        const float area_ratio =
            (static_cast<float>(r.width) * r.height) /
            (static_cast<float>(init_size_.width) * init_size_.height);
        const float s = std::sqrt(area_ratio);
        if (s < kScaleMinRatio || s > kScaleMaxRatio) {
            return false;
        }
    }
    return true;
}

bool MarkerTracker::maybeSwitchPyramid(const cv::Rect& proven_bbox_main)
{
    if (!kPyramidEnable) {
        return false;
    }
    if (pyr_switch_cooldown_ > 0) {
        --pyr_switch_cooldown_;
        return false;
    }

    const float side = nanoSearchSidePx(proven_bbox_main);
    int desired      = pyr_level_;
    if (pyr_level_ == 0 && side >= kPyramidUpSearchPx) {
        desired = 1;
    } else if (pyr_level_ == 1 && side < kPyramidDownSearchPx) {
        desired = 0;
    }
    if (desired == pyr_level_) {
        return false;
    }

    const float scale = (desired == 1) ? 0.5f : 1.0f;
    cv::Mat frame_for_level;
    if (desired == 1) {
        cv::pyrDown(bgr_work_, bgr_half_);
        frame_for_level = bgr_half_;
    } else {
        frame_for_level = bgr_work_;
    }

    cv::Rect roi_level(
        static_cast<int>(std::lround(proven_bbox_main.x * scale)),
        static_cast<int>(std::lround(proven_bbox_main.y * scale)),
        std::max(1, static_cast<int>(std::lround(proven_bbox_main.width * scale))),
        std::max(1, static_cast<int>(std::lround(proven_bbox_main.height * scale))));
    roi_level = clampRect(roi_level, frame_for_level.cols, frame_for_level.rows);
    if (roi_level.width < 8 || roi_level.height < 8) {
        return false;
    }

    try {
        cv::TrackerNano::Params params;
        params.backbone = resolveModelPath(kNanoBackbonePath);
        params.neckhead = resolveModelPath(kNanoNeckheadPath);
        cv::Ptr<cv::TrackerNano> t = cv::TrackerNano::create(params);
        if (!t) {
            return false;
        }
        t->init(frame_for_level, roi_level);
        tracker_ = t;
    } catch (const cv::Exception& e) {
        std::cerr << "bench: pyramid reinit failed: " << e.what() << '\n';
        return false;
    }

    pyr_level_           = desired;
    pyr_switch_cooldown_ = kPyramidSwitchCooldownFrames;
    std::cout << "bench: pyramid -> level " << pyr_level_
              << " (search side " << side << " px, box "
              << proven_bbox_main.width << 'x' << proven_bbox_main.height << ")\n";
    return true;
}

void MarkerTracker::toBgr(const cv::Mat& main_rgb, cv::Mat& bgr_out) const
{
    if (main_rgb.empty()) {
        bgr_out.release();
        return;
    }
    if (main_rgb.channels() == 1) {
        cv::cvtColor(main_rgb, bgr_out, cv::COLOR_GRAY2BGR);
    } else if (kCameraRgb888BytesAreRgb) {
        cv::cvtColor(main_rgb, bgr_out, cv::COLOR_RGB2BGR);
    } else {
        main_rgb.copyTo(bgr_out);  // байты уже BGR (Шаг 5 камеры)
    }
}

void MarkerTracker::resetUnlocked()
{
    tracker_.release();
    bgr_work_.release();
    bgr_half_.release();
    pyr_level_           = 0;
    pyr_switch_cooldown_ = 0;
    verifier_.reset();
    object_size_    = {};
    init_size_        = {};
    size_reject_prev_ = false;
    center_         = {};
    prev_center_    = {};
    velocity_       = {};
    smoothed_center_      = {};
    has_smoothed_center_  = false;
    last_bbox_      = {};
    active_         = false;
    search_gave_up_ = false;
    miss_frames_    = 0;
    verify_log_counter_ = 0;
    miss_time_sec_  = 0.0;
    marker_lost_    = false;
}

void MarkerTracker::reset()
{
    std::lock_guard<std::mutex> lock(mtx_);
    resetUnlocked();
}

bool MarkerTracker::init(const cv::Mat& main_rgb, const cv::Rect& roi_main)
{
    std::lock_guard<std::mutex> lock(mtx_);
    resetUnlocked();

    if (main_rgb.empty() || roi_main.width <= 0 || roi_main.height <= 0) {
        return false;
    }
    if (main_rgb.cols != kMainWidth || main_rgb.rows != kMainHeight) {
        return false;
    }

    toBgr(main_rgb, bgr_work_);
    if (bgr_work_.empty()) {
        return false;
    }

    const cv::Rect roi = clampRect(roi_main, kMainWidth, kMainHeight);
    if (roi.width < 8 || roi.height < 8) {
        return false;
    }

    const std::string backbone = resolveModelPath(kNanoBackbonePath);
    const std::string neckhead = resolveModelPath(kNanoNeckheadPath);

    try {
        cv::TrackerNano::Params params;
        params.backbone = backbone;
        params.neckhead = neckhead;
        tracker_        = cv::TrackerNano::create(params);
        if (!tracker_) {
            std::cerr << "bench: TrackerNano::create failed\n";
            return false;
        }
        tracker_->init(bgr_work_, roi);
    } catch (const cv::Exception& e) {
        std::cerr << "bench: NanoTrack init failed (модели ONNX?): " << e.what() << '\n';
        tracker_.release();
        return false;
    }

    object_size_ = roi.size();
    init_size_   = roi.size();
    verifier_.init(bgr_work_, roi);
    center_      = {roi.x + roi.width * 0.5f, roi.y + roi.height * 0.5f};
    prev_center_ = center_;
    velocity_    = {};
    last_bbox_   = roi;
    active_      = true;

    std::cout << "bench: NanoTrack init " << roi.width << 'x' << roi.height
              << " @ " << roi.x << ',' << roi.y
              << " models=" << backbone << '\n';
    return true;
}

MarkerDetection MarkerTracker::makeOutput(const cv::Rect& bbox, MarkerTrackMode mode,
                                            bool capturing, float confidence)
{
    MarkerDetection out;
    const cv::Point2f frame_center(kMainWidth * 0.5f, kMainHeight * 0.5f);
    out.valid      = true;
    out.live       = (mode == MarkerTrackMode::Live || mode == MarkerTrackMode::Predicted);
    out.capturing  = capturing;
    out.track      = mode;
    out.confidence = confidence;

    if (bbox.width > 0 && bbox.height > 0) {
        const cv::Point2f raw_center(bbox.x + bbox.width * 0.5f,
                                     bbox.y + bbox.height * 0.5f);
        if (!has_smoothed_center_) {
            smoothed_center_     = raw_center;
            has_smoothed_center_ = true;
        } else {
            const float a = kMarkerCenterEmaAlpha;
            smoothed_center_ = smoothed_center_ * (1.0f - a) + raw_center * a;
        }
        out.ex = smoothed_center_.x - frame_center.x;
        out.ey = smoothed_center_.y - frame_center.y;
        out.bbox = cv::Rect(
            static_cast<int>(std::lround(smoothed_center_.x - bbox.width * 0.5f)),
            static_cast<int>(std::lround(smoothed_center_.y - bbox.height * 0.5f)),
            bbox.width, bbox.height);
    } else {
        out.ex = 0.0;
        out.ey = 0.0;
        has_smoothed_center_ = false;
        out.bbox = bbox;
    }
    return out;
}

MarkerDetection MarkerTracker::handleMiss(double dt_sec)
{
    ++miss_frames_;
    miss_time_sec_ += dt_sec;

    // Длительная потеря (≥ 3 с): сдаёмся, запрашиваем новый ROI.
    if (miss_time_sec_ >= kTrackSearchGiveUpSec) {
        if (!search_gave_up_) {
            search_gave_up_ = true;
            std::cout << "bench: search gave up after " << miss_time_sec_ << " s\n";
        }
        return makeOutput({}, MarkerTrackMode::Lost, false, 0.0f);
    }

    // Кратковременная потеря / выход за кадр (≤ 2 с): ведём рамку по последней скорости.
    // Модель не разрушаем — при возврате объекта score снова превысит порог.
    if (miss_time_sec_ < kTrackReacquireTimeoutSec) {
        center_ = center_ + velocity_ * static_cast<float>(dt_sec);
        cv::Rect pred(
            static_cast<int>(std::lround(center_.x - object_size_.width * 0.5f)),
            static_cast<int>(std::lround(center_.y - object_size_.height * 0.5f)),
            object_size_.width, object_size_.height);
        pred = clampRect(pred, kMainWidth, kMainHeight);
        last_bbox_ = pred;
        return makeOutput(pred, MarkerTrackMode::Predicted, /*capturing=*/false, 0.2f);
    }

    // Потеря 2–3 с: Lost (в main.cpp баннер «MARK LOST»).
    if (!marker_lost_) {
        marker_lost_ = true;
        std::cout << "bench: marker lost (" << miss_time_sec_ << " s)\n";
    }
    return makeOutput({}, MarkerTrackMode::Lost, false, 0.0f);
}

MarkerDetection MarkerTracker::update(const cv::Mat& main_rgb, double dt_sec)
{
    std::lock_guard<std::mutex> lock(mtx_);

    MarkerDetection none;
    if (!active_ || main_rgb.empty()) {
        return none;
    }
    if (search_gave_up_) {
        return makeOutput({}, MarkerTrackMode::Lost, false, 0.0f);
    }
    if (dt_sec <= 0.0) {
        dt_sec = 1.0 / kMainFps;
    }
    if (main_rgb.cols != kMainWidth || main_rgb.rows != kMainHeight) {
        return none;
    }

    toBgr(main_rgb, bgr_work_);
    if (bgr_work_.empty()) {
        return none;
    }

    const float lvl_scale = (pyr_level_ == 1) ? 0.5f : 1.0f;
    cv::Mat* frame_lvl    = &bgr_work_;
    if (pyr_level_ == 1) {
        cv::pyrDown(bgr_work_, bgr_half_);
        frame_lvl = &bgr_half_;
    }

    cv::Rect bbox_lvl(
        static_cast<int>(std::lround(last_bbox_.x * lvl_scale)),
        static_cast<int>(std::lround(last_bbox_.y * lvl_scale)),
        std::max(1, static_cast<int>(std::lround(last_bbox_.width * lvl_scale))),
        std::max(1, static_cast<int>(std::lround(last_bbox_.height * lvl_scale))));
    bool  ok    = false;
    float score = 0.0f;
    try {
        ok = tracker_ && tracker_->update(*frame_lvl, bbox_lvl);
        if (ok) {
            score = tracker_->getTrackingScore();
        }
    } catch (const cv::Exception& e) {
        std::cerr << "bench: NanoTrack update error: " << e.what() << '\n';
        ok = false;
    }

    cv::Rect bbox = last_bbox_;
    if (ok) {
        const float inv = 1.0f / lvl_scale;
        bbox = cv::Rect(
            static_cast<int>(std::lround(bbox_lvl.x * inv)),
            static_cast<int>(std::lround(bbox_lvl.y * inv)),
            static_cast<int>(std::lround(bbox_lvl.width * inv)),
            static_cast<int>(std::lround(bbox_lvl.height * inv)));
    }

    // Потеря определяется по уверенности NanoTrack + факту, что рамка в кадре.
    const bool size_sane = bboxSizeSane(bbox);
    const bool nano_good =
        ok && score >= kNanoScoreThreshold && bbox.width > 0 && bbox.height > 0 &&
        bboxVisibleEnough(bbox) && size_sane;

    // Лог по фронту (без спама на каждом кадре).
    if (ok && !size_sane && !size_reject_prev_) {
        std::cout << "bench: bbox size reject " << bbox.width << 'x' << bbox.height
                  << " (init " << init_size_.width << 'x' << init_size_.height
                  << ")\n";
    }
    size_reject_prev_ = ok && !size_sane;

    bool good = nano_good;
    if (nano_good && kVerifyMode > 0) {
        const cv::Rect verify_box = clampRect(bbox, kMainWidth, kMainHeight);
        const VerifyResult vr     = verifier_.verify(bgr_work_, verify_box);
        if (kVerifyMode >= 2) {
            good = vr.ok;
        }

        ++verify_log_counter_;
        if (verify_log_counter_ >= kMainFps) {
            verify_log_counter_ = 0;
            std::cout << "bench: verify ncc=" << vr.ncc_score
                      << " base=" << vr.baseline << " score=" << score
                      << " box=" << bbox.width << 'x' << bbox.height << '\n';
        }

        // Лог только значимых событий: начало серии провалов и сама потеря.
        if (!vr.ok) {
            std::cout << "bench: verify "
                      << (kVerifyMode >= 2 ? "reject" : "would-reject")
                      << " (ncc=" << vr.ncc_score << " base=" << vr.baseline
                      << ")\n";
        }
    }

    if (good) {
        miss_frames_   = 0;
        miss_time_sec_ = 0.0;
        marker_lost_   = false;

        const cv::Rect cb = clampRect(bbox, kMainWidth, kMainHeight);
        last_bbox_       = cb;
        object_size_    = cb.size();  // сиамская голова адаптирует масштаб

        const cv::Point2f c{cb.x + cb.width * 0.5f, cb.y + cb.height * 0.5f};
        const cv::Point2f inst = (c - prev_center_) / static_cast<float>(dt_sec);
        velocity_ = velocity_ * (1.0f - kVelocityEmaAlpha) + inst * kVelocityEmaAlpha;
        prev_center_          = c;
        center_               = c;
        maybeSwitchPyramid(cb);

        return makeOutput(cb, MarkerTrackMode::Live, /*capturing=*/true, score);
    }

    return handleMiss(dt_sec);
}

}  // namespace bench

