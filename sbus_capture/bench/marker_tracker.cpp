#include "marker_tracker.h"

#include "config.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace bench {

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
        main_rgb.copyTo(bgr_out);   // байты уже BGR (см. Шаг 5) — конверсии нет
    }
}

void MarkerTracker::resetUnlocked()
{
    tracker_.release();
    bgr_work_.release();
    object_size_    = {};
    center_         = {};
    prev_center_    = {};
    velocity_       = {};
    last_bbox_      = {};
    active_         = false;
    search_gave_up_ = false;
    miss_frames_    = 0;
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

    // --- Профиль сцены (переключаемая ветка, см. Шаг 3 / config.h) ---
    cv::TrackerCSRT::Params params;   // по умолчанию: HOG + Color Names + сегментация
    if (kTrackerSceneProfile == 1) {
        // "Sky": силуэт на однородном фоне — только яркость/HOG (дешевле, без цветового шума).
        params.use_color_names  = false;
        params.use_segmentation = false;
        params.use_hog          = true;
        params.use_gray         = true;
    }
    // (Для крупной цели можно ограничить стоимость: params.template_size = 200.0f;)

    tracker_ = cv::TrackerCSRT::create(params);
    if (!tracker_) {
        return false;
    }
    tracker_->init(bgr_work_, roi);   // в основном API init() возвращает void

    object_size_ = roi.size();
    center_      = {roi.x + roi.width * 0.5f, roi.y + roi.height * 0.5f};
    prev_center_ = center_;
    velocity_    = {};
    last_bbox_   = roi;
    active_      = true;

    std::cout << "bench: CSRT init " << roi.width << 'x' << roi.height
              << " @ " << roi.x << ',' << roi.y
              << " profile=" << (kTrackerSceneProfile == 1 ? "sky" : "cluttered") << '\n';
    return true;
}

MarkerDetection MarkerTracker::makeOutput(const cv::Rect& bbox, MarkerTrackMode mode,
                                          bool capturing, float confidence) const
{
    MarkerDetection out;
    const cv::Point2f frame_center(kMainWidth * 0.5f, kMainHeight * 0.5f);
    out.valid      = true;
    out.live       = (mode == MarkerTrackMode::Live || mode == MarkerTrackMode::Predicted);
    out.capturing  = capturing;
    out.track      = mode;
    out.bbox       = bbox;
    out.ex = (bbox.width  > 0) ? (bbox.x + bbox.width  * 0.5f - frame_center.x) : 0.0;
    out.ey = (bbox.height > 0) ? (bbox.y + bbox.height * 0.5f - frame_center.y) : 0.0;
    out.confidence = confidence;
    return out;
}

MarkerDetection MarkerTracker::handleMiss(double dt_sec)
{
    ++miss_frames_;
    miss_time_sec_ += dt_sec;

    // Длительная потеря (≥ 3 с): сдаёмся, запрашиваем новый ROI (как раньше).
    if (miss_time_sec_ >= kTrackSearchGiveUpSec) {
        if (!search_gave_up_) {
            search_gave_up_ = true;
            std::cout << "bench: search gave up after " << miss_time_sec_ << " s\n";
        }
        return makeOutput({}, MarkerTrackMode::Lost, false, 0.0f);
    }

    // Кратковременная потеря / выход за границы кадра (< 2 с): ведём рамку по
    // последней скорости, модель CSRT НЕ разрушаем — при возврате объекта
    // следующий update() снова его найдёт и вернёт в Live.
    if (miss_time_sec_ < kTrackReacquireTimeoutSec) {
        center_ = center_ + velocity_ * static_cast<float>(dt_sec);
        cv::Rect pred(
            static_cast<int>(std::lround(center_.x - object_size_.width  * 0.5f)),
            static_cast<int>(std::lround(center_.y - object_size_.height * 0.5f)),
            object_size_.width, object_size_.height);
        pred = clampRect(pred, kMainWidth, kMainHeight);
        last_bbox_ = pred;
        return makeOutput(pred, MarkerTrackMode::Predicted, /*capturing=*/false, 0.2f);
    }

    // Потеря 2–3 с: Lost (в main.cpp показывается баннер «MARK LOST»).
    if (!marker_lost_) {
        marker_lost_ = true;
        std::cout << "bench: marker lost (" << miss_time_sec_ << " s)\n";
    }
    return makeOutput({}, MarkerTrackMode::Lost, false, 0.0f);
}

MarkerDetection MarkerTracker::update(const cv::Mat& main_rgb, double dt_sec)
{
    std::lock_guard<std::mutex> lock(mtx_);

    MarkerDetection none;  // valid = false по умолчанию
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

    cv::Rect bbox = last_bbox_;
    const bool ok       = tracker_ && tracker_->update(bgr_work_, bbox);
    const bool in_frame = ok && bbox.width > 0 && bbox.height > 0 && bboxVisibleEnough(bbox);

    if (ok && in_frame) {
        // Успех: цель удержана и (в основном) в кадре.
        miss_frames_   = 0;
        miss_time_sec_ = 0.0;
        marker_lost_   = false;

        const cv::Rect cb = clampRect(bbox, kMainWidth, kMainHeight);
        last_bbox_ = cb;

        const cv::Point2f c{cb.x + cb.width * 0.5f, cb.y + cb.height * 0.5f};
        const cv::Point2f inst = (c - prev_center_) / static_cast<float>(dt_sec);
        velocity_    = velocity_ * 0.65f + inst * 0.35f;  // EMA скорости
        prev_center_ = c;
        center_      = c;

        return makeOutput(cb, MarkerTrackMode::Live, /*capturing=*/true, 1.0f);
    }

    // CSRT потерял цель ИЛИ объект (в основном) вышел за границы кадра.
    return handleMiss(dt_sec);
}

}  // namespace bench
