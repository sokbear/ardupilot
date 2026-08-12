#pragma once

#include "track_result.h"
#include "target_verifier.h"

#include <opencv2/core.hpp>
#include <opencv2/video.hpp>      // cv::TrackerNano (модуль video)

#include <mutex>

namespace bench {

// Ядро сопровождения на базе cv::TrackerNano (лёгкий сиамский dnn-трекер).
// Публичный интерфейс сохранён — main.cpp и прочие модули не меняются.
class MarkerTracker {
public:
    MarkerTracker();
    ~MarkerTracker();

    bool init(const cv::Mat& main_rgb, const cv::Rect& roi_main);
    MarkerDetection update(const cv::Mat& main_rgb, double dt_sec);
    void reset();

    bool active() const { return active_; }
    bool needsOperatorRoi() const { return search_gave_up_; }
    cv::Size initBboxSize() const { return object_size_; }

private:
    void resetUnlocked();
    void toBgr(const cv::Mat& main_rgb, cv::Mat& bgr_out) const;
    static cv::Rect clampRect(const cv::Rect& r, int cols, int rows);
    static bool bboxVisibleEnough(const cv::Rect& r);
    static float nanoSearchSidePx(const cv::Rect& bbox_main);
    bool bboxSizeSane(const cv::Rect& r) const;
    bool maybeSwitchPyramid(const cv::Rect& proven_bbox_main);
    MarkerDetection makeOutput(const cv::Rect& bbox, MarkerTrackMode mode,
                               bool capturing, float confidence);
    MarkerDetection handleMiss(double dt_sec);

    cv::Ptr<cv::TrackerNano> tracker_;
    cv::Mat        bgr_work_;
    cv::Mat        bgr_half_;
    int            pyr_level_           = 0;
    int            pyr_switch_cooldown_ = 0;
    TargetVerifier verifier_;
    cv::Size       object_size_;
    cv::Size       init_size_{};
    bool           size_reject_prev_ = false;
    cv::Point2f center_{};
    cv::Point2f prev_center_{};
    cv::Point2f velocity_{};
    cv::Point2f smoothed_center_{};
    bool        has_smoothed_center_ = false;
    cv::Rect    last_bbox_{};

    bool   active_         = false;
    bool   search_gave_up_ = false;
    int    miss_frames_    = 0;
    int    verify_log_counter_ = 0;
    double miss_time_sec_  = 0.0;
    bool   marker_lost_    = false;

    std::mutex mtx_;
};

}  // namespace bench
