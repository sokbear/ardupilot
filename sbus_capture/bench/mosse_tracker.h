#pragma once

#include "bbox_scaler.h"
#include "track_result.h"

#include <opencv2/core.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/tracking/tracking_legacy.hpp>

namespace bench {

// FullHD @60: MOSSE (позиция) + BboxScaler (размер, ROI, реже).
class MosseTracker {
public:
    MosseTracker();
    ~MosseTracker();
    bool init(const cv::Mat& main_rgb, const cv::Rect& roi_main);
    MarkerDetection update(const cv::Mat& main_rgb, double dt_sec, bool run_scale_refine);
    void reset();
    bool active() const { return active_; }

private:
    static cv::Rect clampRect(const cv::Rect& r, int cols, int rows);
    void prepareBgr(const cv::Mat& main_rgb);
    cv::Rect bboxFromCenterAndScale(const cv::Point2f& center) const;

    cv::Ptr<cv::legacy::Tracker> tracker_;
    BboxScaler                   scaler_;
    cv::Mat                       main_bgr_;
    bool                          active_           = false;
    cv::Point2f                   lock_center_      {};
    cv::Point2f                   lock_velocity_    {};
    cv::Rect                      lock_bbox_        {};
    cv::Size                      init_bbox_size_   {};
    float                         scale_display_    = 1.0f;
    float                         last_scale_ratio_ = 1.0f;
    int                           miss_frames_      = 0;
};

}  // namespace bench
