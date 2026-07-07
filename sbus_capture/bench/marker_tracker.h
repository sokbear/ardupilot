#pragma once

#include "track_result.h"

#include <mutex>
#include <opencv2/core.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/tracking/tracking_legacy.hpp>

namespace bench {

// Гибрид: сегментация (контраст) или MOSSE+NCC-scale (низкий контраст).
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
    MarkerDetection handleMiss(double dt_sec, const cv::Point2f& frame_center);
    void beginMissEpisode();
    void resetUnlocked();
    bool tryReacquire(const cv::Point2f& frame_center, MarkerDetection& out);
    MarkerDetection makeLostOutput() const;
    MarkerDetection makeLiveHoldOutput(const cv::Point2f& frame_center) const;

    static cv::Rect clampRect(const cv::Rect& r, int cols, int rows);
    void prepareGray(const cv::Mat& main_rgb, cv::Mat& gray_out);
    void prepareBgr(const cv::Mat& main_rgb);
    cv::Rect mosseBboxAt(cv::Point2f center, float scale) const;
    bool initMosse(const cv::Mat& main_rgb, const cv::Rect& roi);
    void maybeReinitMosse(float prev_scale);
    bool updateMossePosition(cv::Point2f& out_center);
    cv::Rect bboxFromCenter() const;
    cv::Rect frozenMissBbox() const;
    cv::Rect templateRoiAtCenter(const cv::Mat& gray, cv::Point2f c, cv::Size size) const;
    cv::Rect trackZoneAround(cv::Point2f hint, float zone_factor) const;

    void setTemplateFromBbox(const cv::Mat& gray, const cv::Rect& bbox, bool update_bbox_size = true);
    bool measureTrackBlob(const cv::Mat& gray, cv::Point2f hint, float zone_factor,
                          cv::Point2f& out_center, cv::Size& out_size) const;
    void updateScaleFromMeasure(const cv::Size& measured);
    void probeScaleNcc(const cv::Mat& gray, const cv::Point2f& center);
    void probeScaleMosse(const cv::Point2f& center);
    void setMosseScaleTemplate(const cv::Mat& gray, const cv::Rect& roi);
    cv::Rect bboxForOutput() const;
    static float measureRoiContrast(const cv::Mat& gray, const cv::Rect& roi);
    bool locateWeighted(const cv::Mat& gray, const cv::Point2f& hint, cv::Point2f& out_center,
                        double& response_out, float window_factor, float max_drift_factor) const;
    bool resolvePosition(const cv::Mat& gray, const cv::Point2f& pred, double dt_sec,
                         cv::Point2f& out_center, double& response_out);
    bool responseWeightedAt(const cv::Mat& gray, const cv::Point2f& center,
                            double& response_out) const;

    cv::Ptr<cv::legacy::Tracker> mosse_tracker_;
    cv::Mat       template_gray_;
    cv::Mat       weight_;
    cv::Mat       weight_sqrt_;
    cv::Mat       gray_work_;
    cv::Mat       main_bgr_;
    cv::Mat       mosse_scale_gray_;
    cv::Size      ref_size_;
    cv::Size      object_size_;
    cv::Size      operator_roi_size_;
    cv::Size      init_bbox_size_;
    cv::Point2f   anchor_center_{};
    cv::Point2f   center_{};
    cv::Point2f   prev_center_{};
    cv::Point2f   velocity_{};
    float         scale_          = 1.0f;
    bool          use_seg_track_  = false;
    bool          use_mosse_track_ = false;
    cv::Rect      mosse_bbox_{};
    float         mosse_scale_    = 1.0f;
    int           mosse_soft_miss_ = 0;
    float         contrast_score_ = 0.0f;
    bool          active_         = false;
    int           miss_frames_    = 0;
    double        miss_time_sec_  = 0.0;
    bool          marker_lost_    = false;
    bool          search_gave_up_ = false;
    cv::Point2f   miss_origin_{};
    cv::Size      miss_bbox_size_{};
    float         miss_scale_     = 1.0f;
    int           frame_tick_     = 0;
    int           fail_streak_    = 0;
    std::mutex    mtx_;
};

}  // namespace bench
