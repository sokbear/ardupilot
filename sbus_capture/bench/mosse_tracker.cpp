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

cv::Rect MosseTracker::bboxFromCenterAndScale(const cv::Point2f& center) const
{
    const int out_w =
        std::max(kScaleMinSidePx, static_cast<int>(init_bbox_size_.width * scale_display_));
    const int out_h =
        std::max(kScaleMinSidePx, static_cast<int>(init_bbox_size_.height * scale_display_));
    return clampRect({static_cast<int>(center.x - out_w * 0.5f),
                      static_cast<int>(center.y - out_h * 0.5f), out_w, out_h},
                     kMainWidth, kMainHeight);
}

void MosseTracker::reset()
{
    tracker_.release();
    scaler_.reset();
    active_           = false;
    lock_center_      = {};
    lock_velocity_    = {};
    lock_bbox_        = {};
    init_bbox_size_   = {};
    scale_display_    = 1.0f;
    last_scale_ratio_ = 1.0f;
    miss_frames_      = 0;
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
    init_bbox_size_ = lock_bbox_.size();
    lock_center_ = {lock_bbox_.x + lock_bbox_.width * 0.5f,
                    lock_bbox_.y + lock_bbox_.height * 0.5f};
    scale_display_    = 1.0f;
    last_scale_ratio_ = 1.0f;

    scaler_.init(main_bgr_, lock_bbox_);

    tracker_ = cv::legacy::TrackerMOSSE::create();
    if (!tracker_ || !tracker_->init(main_bgr_, lock_bbox_)) {
        reset();
        return false;
    }

    active_        = true;
    miss_frames_   = 0;
    return true;
}

MarkerDetection MosseTracker::update(const cv::Mat& main_rgb, double dt_sec,
                                     bool run_scale_refine)
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
            out.bbox       = bboxFromCenterAndScale(predicted);
            out.confidence = 0.2f;
        }
        return out;
    }

    cv::Point2f center{static_cast<float>(bbox.x + bbox.width * 0.5),
                       static_cast<float>(bbox.y + bbox.height * 0.5)};

    if (run_scale_refine) {
        float measured = scale_display_;
        if (scaler_.measureScale(main_bgr_, center, init_bbox_size_, measured)) {
            scale_display_ = measured;
        }
    } else {
        const float target = scaler_.scaleTarget();
        scale_display_ += (target - scale_display_) * kScaleDisplayAlpha;
        scale_display_ = std::clamp(scale_display_, kScaleMinRatio, kScaleMaxRatio);
    }

    cv::Rect bbox_main = bboxFromCenterAndScale(center);
    center = {bbox_main.x + bbox_main.width * 0.5f, bbox_main.y + bbox_main.height * 0.5f};

    const cv::Point2f instant_v = (center - lock_center_) / static_cast<float>(dt_sec);
    lock_velocity_ = lock_velocity_ * (1.0f - kVelocityEmaAlpha) +
                     instant_v * kVelocityEmaAlpha;
    lock_center_ = center;
    lock_bbox_   = bbox_main;
    miss_frames_ = 0;

    if (run_scale_refine &&
        std::abs(scale_display_ - last_scale_ratio_) > kScaleReinitThreshold) {
        if (tracker_) {
            tracker_->init(main_bgr_, lock_bbox_);
        }
        last_scale_ratio_ = scale_display_;
    }

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
