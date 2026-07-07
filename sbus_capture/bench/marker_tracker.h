#pragma once

#include "track_result.h"

#include <opencv2/core.hpp>
#include <opencv2/tracking.hpp>   // cv::TrackerCSRT (основной API)
#include <opencv2/video.hpp>      // на случай, если TrackerCSRT в модуле video

#include <mutex>

namespace bench {

// Ядро сопровождения на базе cv::TrackerCSRT.
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
    static bool bboxVisibleEnough(const cv::Rect& r);   // объект (в основном) в кадре
    MarkerDetection makeOutput(const cv::Rect& bbox, MarkerTrackMode mode,
                               bool capturing, float confidence) const;
    MarkerDetection handleMiss(double dt_sec);          // кратковременная/длительная потеря

    cv::Ptr<cv::TrackerCSRT> tracker_;
    cv::Mat     bgr_work_;
    cv::Size    object_size_;      // размер стартовой рамки (для initBboxSize)
    cv::Point2f center_{};
    cv::Point2f prev_center_{};
    cv::Point2f velocity_{};
    cv::Rect    last_bbox_{};

    bool   active_         = false;
    bool   search_gave_up_ = false;
    int    miss_frames_    = 0;
    double miss_time_sec_  = 0.0;
    bool   marker_lost_    = false;

    std::mutex mtx_;
};

}  // namespace bench
