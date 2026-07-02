#include "mosse_tracker.h"

#include "config.h"

#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include <opencv2/tracking/tracking_legacy.hpp>

namespace bench {

MosseTracker::MosseTracker() = default;
MosseTracker::~MosseTracker() = default;

cv::Rect MosseTracker::clampRect(const cv::Rect& r, int cols, int rows)
{
    const int x = std::clamp(r.x, 0, cols - 1);
    const int y = std::clamp(r.y, 0, rows - 1);
    const int w = std::clamp(r.width, 1, cols - x);
    const int h = std::clamp(r.height, 1, rows - y);
    return {x, y, w, h};
}

void MosseTracker::prepareBgr(const cv::Mat& main_rgb)
{
    if (main_rgb.empty()) {
        return;
    }
    if (kCameraRgb888BytesAreRgb) {
        cv::cvtColor(main_rgb, main_bgr_, cv::COLOR_RGB2BGR);
    } else if (main_bgr_.data != main_rgb.data) {
        main_rgb.copyTo(main_bgr_);
    }
}

void MosseTracker::reset()
{
    tracker_.release();
    active_        = false;
    lock_center_   = {};
    lock_velocity_ = {};
    lock_bbox_     = {};
    miss_frames_   = 0;
}

bool MosseTracker::init(const cv::Mat& main_rgb, const cv::Rect& roi_main)
{
    reset();
    if (main_rgb.empty() || roi_main.width <= 0 || roi_main.height <= 0) {
        return false;
    }
    if (main_rgb.cols != kMainWidth || main_rgb.rows != kMainHeight) {
        return false;
    }

    prepareBgr(main_rgb);
    if (main_bgr_.empty()) {
        return false;
    }

    lock_bbox_ = clampRect(roi_main, kMainWidth, kMainHeight);
    lock_center_ = {lock_bbox_.x + lock_bbox_.width * 0.5f,
                    lock_bbox_.y + lock_bbox_.height * 0.5f};

    tracker_ = cv::legacy::TrackerMOSSE::create();
    if (!tracker_ || !tracker_->init(main_bgr_, lock_bbox_)) {
        reset();
        return false;
    }

    active_      = true;
    miss_frames_ = 0;
    return true;
}

MarkerDetection MosseTracker::update(const cv::Mat& main_rgb, double dt_sec)
{
    MarkerDetection out;
    if (!active_ || main_rgb.empty()) {
        return out;
    }

    if (dt_sec <= 0.0) {
        dt_sec = 1.0 / kMainFps;
    }

    prepareBgr(main_rgb);
    if (main_bgr_.empty()) {
        return out;
    }

    cv::Rect2d bbox = lock_bbox_;
    const bool ok   = tracker_ && tracker_->update(main_bgr_, bbox);

    const cv::Point2f frame_center(kMainWidth * 0.5f, kMainHeight * 0.5f);

    if (!ok) {
        if (miss_frames_ < kMosseMaxMissFrames) {
            ++miss_frames_;
            const cv::Point2f predicted =
                lock_center_ + lock_velocity_ * static_cast<float>(dt_sec);
            lock_center_ = predicted;

            out.valid      = true;
            out.live       = false;
            out.capturing  = false;
            out.track      = MarkerTrackMode::Predicted;
            out.ex         = predicted.x - frame_center.x;
            out.ey         = predicted.y - frame_center.y;
            out.bbox       = lock_bbox_;
            out.bbox.x     = static_cast<int>(predicted.x - lock_bbox_.width * 0.5f);
            out.bbox.y     = static_cast<int>(predicted.y - lock_bbox_.height * 0.5f);
            out.confidence = 0.2f;
        }
        return out;
    }

    const cv::Rect bbox_main =
        clampRect({static_cast<int>(bbox.x), static_cast<int>(bbox.y),
                   static_cast<int>(bbox.width), static_cast<int>(bbox.height)},
                  kMainWidth, kMainHeight);
    const cv::Point2f center{bbox_main.x + bbox_main.width * 0.5f,
                             bbox_main.y + bbox_main.height * 0.5f};

    const cv::Point2f instant_v = (center - lock_center_) / static_cast<float>(dt_sec);
    lock_velocity_ = lock_velocity_ * (1.0f - kVelocityEmaAlpha) +
                     instant_v * kVelocityEmaAlpha;
    lock_center_ = center;
    lock_bbox_   = bbox_main;
    miss_frames_ = 0;

    out.valid      = true;
    out.live       = true;
    out.capturing  = true;
    out.track      = MarkerTrackMode::Live;
    out.ex         = center.x - frame_center.x;
    out.ey         = center.y - frame_center.y;
    out.bbox       = bbox_main;
    out.confidence = 0.95f;
    return out;
}

}  // namespace bench
