#pragma once

#include "track_result.h"

#include <opencv2/core.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/tracking/tracking_legacy.hpp>

namespace bench {

// MOSSE на FullHD (1920×1080): координаты метки в пространстве main.
class MosseTracker {
public:
    MosseTracker();
    ~MosseTracker();
    bool init(const cv::Mat& main_rgb, const cv::Rect& roi_main);
    MarkerDetection update(const cv::Mat& main_rgb, double dt_sec);
    void reset();
    bool active() const { return active_; }

private:
    static cv::Rect clampRect(const cv::Rect& r, int cols, int rows);
    void prepareBgr(const cv::Mat& main_rgb);

    cv::Ptr<cv::legacy::Tracker> tracker_;
    cv::Mat                       main_bgr_;
    bool                          active_      = false;
    cv::Point2f                   lock_center_ {};
    cv::Point2f                   lock_velocity_{};
    cv::Rect                      lock_bbox_   {};
    int                           miss_frames_ = 0;
};

}  // namespace bench
