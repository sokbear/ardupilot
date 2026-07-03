#include "marker_tracker.h"

#include "config.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <opencv2/imgproc.hpp>

namespace bench {

namespace {

cv::Rect clampRect(const cv::Rect& r, int cols, int rows)
{
    const int x = std::clamp(r.x, 0, cols - 1);
    const int y = std::clamp(r.y, 0, rows - 1);
    const int w = std::clamp(r.width, 1, cols - x);
    const int h = std::clamp(r.height, 1, rows - y);
    return {x, y, w, h};
}

bool bboxMostlyInFrame(const cv::Rect& r, int cols, int rows)
{
    if (r.width <= 0 || r.height <= 0) {
        return false;
    }
    const cv::Rect full(0, 0, cols, rows);
    const cv::Rect inter = r & full;
    return static_cast<float>(inter.area()) >=
           static_cast<float>(r.area()) * kTrackMinVisibleFraction;
}

void makeGaussianWeights(int w, int h, cv::Mat& out)
{
    out.create(h, w, CV_32F);
    const float cx = (w - 1) * 0.5f;
    const float cy = (h - 1) * 0.5f;
    const float sigma = std::max(2.0f, kWeightSigmaFactor * static_cast<float>(std::min(w, h)));
    const float denom = 2.0f * sigma * sigma;
    for (int y = 0; y < h; ++y) {
        auto* row = out.ptr<float>(y);
        for (int x = 0; x < w; ++x) {
            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cy;
            row[x]        = std::exp(-(dx * dx + dy * dy) / denom);
        }
    }
}

cv::Size scaledSize(const cv::Size& ref, float scale)
{
    const int w = std::max(kScaleMinSidePx, static_cast<int>(ref.width * scale));
    const int h = std::max(kScaleMinSidePx, static_cast<int>(ref.height * scale));
    return {w, h};
}

cv::Size extractPatchSize(const cv::Size& bbox_size, float scale)
{
    cv::Size sz = scaledSize(bbox_size, scale);
    const int max_side = std::max(sz.width, sz.height);
    if (max_side <= kTrackExtractMaxSide) {
        return sz;
    }
    const float down =
        static_cast<float>(kTrackExtractMaxSide) / static_cast<float>(max_side);
    return {std::max(4, static_cast<int>(sz.width * down)),
            std::max(4, static_cast<int>(sz.height * down))};
}

float distSq(cv::Point2f a, cv::Point2f b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

bool maskFillsZone(const cv::Rect& local, int zone_area)
{
    return zone_area > 0 &&
           static_cast<float>(local.area()) > static_cast<float>(zone_area) * kSegMaxZoneFillRatio;
}

bool segBboxMeetsMinVsZone(const cv::Rect& local, const cv::Rect& zone)
{
    if (local.width <= 0 || local.height <= 0 || zone.width <= 0 || zone.height <= 0) {
        return false;
    }
    const int zone_min = std::min(zone.width, zone.height);
    const int seg_min  = std::min(local.width, local.height);
    if (seg_min < std::max(kScaleMinSidePx, static_cast<int>(zone_min * kSegMinRoiSideRatio))) {
        return false;
    }
    if (local.area() < static_cast<int>(zone.area() * kSegMinRoiAreaRatio)) {
        return false;
    }
    return true;
}

bool largestBlobNearPrefer(const cv::Mat& mask, cv::Point2f prefer_local, int min_area,
                           float max_dist, cv::Rect& local_out)
{
    if (mask.empty()) {
        return false;
    }

    cv::Mat labels;
    cv::Mat stats;
    cv::Mat centroids;
    const int n = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8, CV_32S);
    if (n <= 1) {
        return false;
    }

    const float max_dist2 = max_dist * max_dist;
    int         best_area = 0;
    cv::Rect    best;
    for (int i = 1; i < n; ++i) {
        const int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area < min_area) {
            continue;
        }
        const float cx = centroids.at<double>(i, 0);
        const float cy = centroids.at<double>(i, 1);
        if (distSq({cx, cy}, prefer_local) > max_dist2) {
            continue;
        }
        if (area > best_area) {
            best_area = area;
            best      = {stats.at<int>(i, cv::CC_STAT_LEFT), stats.at<int>(i, cv::CC_STAT_TOP),
                         stats.at<int>(i, cv::CC_STAT_WIDTH), stats.at<int>(i, cv::CC_STAT_HEIGHT)};
        }
    }
    if (best_area <= 0) {
        return false;
    }
    local_out = best;
    return true;
}

bool blobFromMaskInZone(const cv::Mat& mask, cv::Point2f prefer_local, int min_area, int zone_area,
                        const cv::Rect& zone, cv::Rect& local_out)
{
    const float max_dist =
        static_cast<float>(std::min(zone.width, zone.height)) * kSegPreferDistFactor;
    if (!largestBlobNearPrefer(mask, prefer_local, min_area, max_dist, local_out)) {
        return false;
    }
    if (!segBboxMeetsMinVsZone(local_out, zone)) {
        return false;
    }
    return !maskFillsZone(local_out, zone_area);
}

cv::Mat darkMaskOtsu(const cv::Mat& patch)
{
    cv::Mat mask;
    cv::threshold(patch, mask, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, {5, 5}));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, {3, 3}));
    return mask;
}

cv::Mat darkMaskAdaptive(const cv::Mat& patch)
{
    cv::Scalar mean;
    cv::Scalar stddev;
    cv::meanStdDev(patch, mean, stddev);
    const double thresh =
        std::clamp(mean[0] - std::max(8.0, stddev[0] * 0.55), 0.0, 255.0);
    cv::Mat mask;
    cv::threshold(patch, mask, thresh, 255, cv::THRESH_BINARY_INV);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, {5, 5}));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, {3, 3}));
    return mask;
}

bool weightedNccPatch(const cv::Mat& gray_u8, const cv::Mat& templ, const cv::Mat& weight_sqrt,
                      double& response_out)
{
    response_out = 0.0;
    if (gray_u8.empty() || templ.empty() || weight_sqrt.empty()) {
        return false;
    }
    if (templ.size() != weight_sqrt.size()) {
        return false;
    }

    cv::Mat sample;
    if (gray_u8.size() != templ.size()) {
        cv::Mat resized;
        cv::resize(gray_u8, resized, templ.size(), 0, 0, cv::INTER_AREA);
        resized.convertTo(sample, CV_32F);
    } else {
        gray_u8.convertTo(sample, CV_32F);
    }

    cv::Mat sw = sample.mul(weight_sqrt);
    cv::Mat tw = templ.mul(weight_sqrt);
    const double norm_s = cv::norm(sw);
    const double norm_t = cv::norm(tw);
    if (norm_s < 1e-6 || norm_t < 1e-6) {
        return false;
    }
    response_out = sw.dot(tw) / (norm_s * norm_t);
    return true;
}

bool weightedNccAtPatchSize(const cv::Mat& gray_u8, const cv::Mat& templ, const cv::Mat& weight_sqrt,
                            const cv::Size& patch_sz, double& response_out)
{
    if (patch_sz.width < 4 || patch_sz.height < 4) {
        return false;
    }
    cv::Mat templ_r;
    cv::Mat weight_r;
    cv::resize(templ, templ_r, patch_sz, 0, 0, cv::INTER_AREA);
    cv::resize(weight_sqrt, weight_r, patch_sz, 0, 0, cv::INTER_AREA);
    return weightedNccPatch(gray_u8, templ_r, weight_r, response_out);
}

bool initSegTrusted(const cv::Mat& gray, const cv::Rect& roi, cv::Point2f roi_center,
                    const cv::Rect& seg)
{
    if (seg.width <= 0 || seg.height <= 0) {
        return false;
    }

    const cv::Point2f seg_c{seg.x + seg.width * 0.5f, seg.y + seg.height * 0.5f};
    const float       max_shift =
        static_cast<float>(std::min(roi.width, roi.height)) * kSegInitMaxCenterShift;
    if (std::sqrt(distSq(seg_c, roi_center)) > max_shift) {
        return false;
    }
    if (seg.area() < static_cast<int>(roi.area() * kSegInitMinAreaRatio)) {
        return false;
    }

    const cv::Rect seg_clamped = clampRect(seg, gray.cols, gray.rows);
    const cv::Point roi_pt{static_cast<int>(std::lround(roi_center.x)),
                           static_cast<int>(std::lround(roi_center.y))};
    if (!seg_clamped.contains(roi_pt)) {
        return false;
    }

    const double seg_mean = cv::mean(gray(seg_clamped))[0];
    const cv::Rect z      = clampRect(roi, gray.cols, gray.rows);
    const double   roi_mean = cv::mean(gray(z))[0];
    if (seg_mean > roi_mean - 8.0) {
        return false;
    }
    return true;
}

void maskCorrOutsideDrift(cv::Mat& corr, const cv::Rect& search, const cv::Size& patch_work,
                          cv::Point2f hint, float max_drift, float inv_ds)
{
    const float max_drift2 = max_drift * max_drift;
    for (int py = 0; py < corr.rows; ++py) {
        auto* row = corr.ptr<float>(py);
        for (int px = 0; px < corr.cols; ++px) {
            const float cx =
                static_cast<float>(search.x) + (static_cast<float>(px) + patch_work.width * 0.5f) * inv_ds;
            const float cy =
                static_cast<float>(search.y) + (static_cast<float>(py) + patch_work.height * 0.5f) * inv_ds;
            const float dx = cx - hint.x;
            const float dy = cy - hint.y;
            if ((dx * dx + dy * dy) > max_drift2) {
                row[px] = -1.0f;
            }
        }
    }
}

bool segmentDarkBlob(const cv::Mat& gray, const cv::Rect& zone, cv::Point2f prefer_full,
                     cv::Rect& out_full)
{
    const cv::Rect z = clampRect(zone, gray.cols, gray.rows);
    if (z.width < 8 || z.height < 8) {
        return false;
    }

    const cv::Mat patch = gray(z);
    const int     min_area =
        std::max(kSegMinAbsAreaPx, static_cast<int>(z.area() * kSegMinZoneAreaFactor));
    const cv::Point2f prefer_local{prefer_full.x - static_cast<float>(z.x),
                                   prefer_full.y - static_cast<float>(z.y)};

    cv::Rect local;
    const cv::Mat masks[] = {darkMaskOtsu(patch), darkMaskAdaptive(patch)};
    bool          found   = false;
    for (const cv::Mat& mask : masks) {
        if (blobFromMaskInZone(mask, prefer_local, min_area, z.area(), z, local)) {
            found = true;
            break;
        }
    }
    if (!found) {
        const float max_dist =
            static_cast<float>(std::min(z.width, z.height)) * kSegPreferDistFactor;
        if (!largestBlobNearPrefer(darkMaskOtsu(patch), prefer_local, min_area, max_dist,
                                   local) ||
            !segBboxMeetsMinVsZone(local, z) || maskFillsZone(local, z.area())) {
            return false;
        }
    }

    const int pad = std::max(3, std::min(local.width, local.height) / 10);
    cv::Rect padded{local.x - pad, local.y - pad, local.width + 2 * pad, local.height + 2 * pad};
    padded &= cv::Rect(0, 0, z.width, z.height);

    out_full = {z.x + padded.x, z.y + padded.y, padded.width, padded.height};
    return true;
}

bool darkCircleFromContours(const cv::Mat& patch, cv::Point2f prefer_local, int min_area,
                            float max_dist, cv::Rect& local_out)
{
    const cv::Mat     mask = darkMaskOtsu(patch);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    const float max_dist2 = max_dist * max_dist;
    double      best_score = -1.0;
    cv::Rect    best;
    for (const auto& c : contours) {
        const double area = cv::contourArea(c);
        if (area < static_cast<double>(min_area)) {
            continue;
        }
        const double perim = cv::arcLength(c, true);
        if (perim < 1.0) {
            continue;
        }
        const float circularity =
            static_cast<float>(4.0 * CV_PI * area / (perim * perim));
        if (circularity < kSegMinCircularity) {
            continue;
        }
        const cv::Rect br = cv::boundingRect(c);
        const cv::Point2f cc{br.x + br.width * 0.5f, br.y + br.height * 0.5f};
        if (distSq(cc, prefer_local) > max_dist2) {
            continue;
        }
        const double score = area * static_cast<double>(circularity);
        if (score > best_score) {
            best_score = score;
            best       = br;
        }
    }
    if (best_score <= 0.0) {
        return false;
    }
    local_out = best;
    return true;
}

bool segmentDarkCircle(const cv::Mat& gray, const cv::Rect& zone, cv::Point2f prefer_full,
                       cv::Rect& out_full)
{
    const cv::Rect z = clampRect(zone, gray.cols, gray.rows);
    if (z.width < 12 || z.height < 12) {
        return false;
    }

    const cv::Mat     patch = gray(z);
    const int         min_area =
        std::max(kSegMinAbsAreaPx, static_cast<int>(z.area() * kSegMinZoneAreaFactor));
    const cv::Point2f prefer_local{prefer_full.x - static_cast<float>(z.x),
                                   prefer_full.y - static_cast<float>(z.y)};
    const float       max_dist =
        static_cast<float>(std::min(z.width, z.height)) * kSegPreferDistFactor;

    cv::Rect local;
    if (darkCircleFromContours(patch, prefer_local, min_area, max_dist, local)) {
        const int pad = std::max(2, std::min(local.width, local.height) / 12);
        cv::Rect  padded{local.x - pad, local.y - pad, local.width + 2 * pad,
                        local.height + 2 * pad};
        padded &= cv::Rect(0, 0, z.width, z.height);
        out_full = {z.x + padded.x, z.y + padded.y, padded.width, padded.height};
        return true;
    }
    return false;
}

bool initSegmentBbox(const cv::Mat& gray, const cv::Rect& roi, cv::Point2f roi_center,
                     cv::Rect& seg_bbox)
{
    seg_bbox = roi;
    if (segmentDarkCircle(gray, roi, roi_center, seg_bbox)) {
        return true;
    }
    if (segmentDarkBlob(gray, roi, roi_center, seg_bbox)) {
        return true;
    }
    return false;
}

}  // namespace

MarkerTracker::MarkerTracker()  = default;
MarkerTracker::~MarkerTracker() = default;

cv::Rect MarkerTracker::clampRect(const cv::Rect& r, int cols, int rows)
{
    return bench::clampRect(r, cols, rows);
}

void MarkerTracker::resetUnlocked()
{
    template_gray_.release();
    weight_.release();
    weight_sqrt_.release();
    gray_work_.release();
    ref_size_          = {};
    object_size_       = {};
    operator_roi_size_ = {};
    init_bbox_size_    = {};
    anchor_center_     = {};
    center_            = {};
    prev_center_       = {};
    velocity_          = {};
    scale_             = 1.0f;
    use_seg_track_     = false;
    contrast_score_    = 0.0f;
    active_            = false;
    miss_frames_       = 0;
    miss_time_sec_     = 0.0;
    marker_lost_       = false;
    search_gave_up_    = false;
    miss_origin_       = {};
    miss_bbox_size_    = {};
    miss_scale_        = 1.0f;
    frame_tick_        = 0;
    fail_streak_       = 0;
}

void MarkerTracker::reset()
{
    std::lock_guard<std::mutex> lock(mtx_);
    resetUnlocked();
}

void MarkerTracker::prepareGray(const cv::Mat& main_rgb, cv::Mat& gray_out)
{
    if (main_rgb.empty()) {
        gray_out.release();
        return;
    }
    if (main_rgb.channels() == 1) {
        main_rgb.convertTo(gray_out, CV_8U);
        return;
    }
    const int code = kCameraRgb888BytesAreRgb ? cv::COLOR_RGB2GRAY : cv::COLOR_BGR2GRAY;
    cv::cvtColor(main_rgb, gray_out, code);
}

void MarkerTracker::setTemplateFromBbox(const cv::Mat& gray, const cv::Rect& bbox, bool update_bbox_size)
{
    const cv::Rect roi = clampRect(bbox, gray.cols, gray.rows);
    if (roi.width < 4 || roi.height < 4) {
        return;
    }

    cv::Mat patch;
    gray(roi).convertTo(patch, CV_32F);

    const int   max_side = std::max(roi.width, roi.height);
    const float down     = (max_side > kTemplateRefMaxSide)
                               ? static_cast<float>(kTemplateRefMaxSide) / static_cast<float>(max_side)
                               : 1.0f;
    if (down < 0.999f) {
        cv::resize(patch, template_gray_, cv::Size(), down, down, cv::INTER_AREA);
        ref_size_ = template_gray_.size();
    } else {
        template_gray_ = patch;
        ref_size_      = roi.size();
    }

    makeGaussianWeights(ref_size_.width, ref_size_.height, weight_);
    cv::sqrt(weight_, weight_sqrt_);
    if (update_bbox_size) {
        object_size_    = roi.size();
        init_bbox_size_ = object_size_;
    }
}

bool MarkerTracker::resolvePosition(const cv::Mat& gray, const cv::Point2f& pred, double /*dt_sec*/,
                                    cv::Point2f& out_center, double& response_out)
{
    const float win =
        use_seg_track_ ? kSearchWindowFactor : kSearchWindowTemplate;
    const float drift =
        use_seg_track_ ? kSearchMaxDriftFactor : kSearchDriftTemplate;

    cv::Point2f peak{};
    double      peak_resp = 0.0;
    if (!locateWeighted(gray, pred, peak, peak_resp, win, drift)) {
        double pred_resp = 0.0;
        if (responseWeightedAt(gray, pred, pred_resp) && pred_resp >= kTrackMinResponse) {
            out_center   = pred;
            response_out = pred_resp;
            return true;
        }
        return false;
    }

    double pred_resp = 0.0;
    responseWeightedAt(gray, pred, pred_resp);

    const float ref_side = static_cast<float>(
        std::max(scaledSize(object_size_, scale_).width, scaledSize(object_size_, scale_).height));
    const float latch_dist     = ref_side * kLatchMaxPredDistFactor;
    const float dist_peak_pred = std::sqrt(distSq(peak, pred));

    if (dist_peak_pred <= latch_dist) {
        out_center   = peak;
        response_out = peak_resp;
        return true;
    }
    if (pred_resp >= kTrackMinResponse) {
        out_center   = pred;
        response_out = pred_resp;
        return true;
    }
    if (peak_resp >= kTrackMinResponse && dist_peak_pred <= latch_dist * 1.5f) {
        out_center   = peak;
        response_out = peak_resp;
        return true;
    }
    return false;
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

    prepareGray(main_rgb, gray_work_);
    if (gray_work_.empty()) {
        return false;
    }

    const cv::Rect roi = clampRect(roi_main, kMainWidth, kMainHeight);
    const cv::Point2f roi_center{roi.x + roi.width * 0.5f, roi.y + roi.height * 0.5f};

    operator_roi_size_ = roi.size();
    anchor_center_     = roi_center;
    center_            = roi_center;
    prev_center_       = roi_center;
    velocity_          = {};

    contrast_score_ = measureRoiContrast(gray_work_, roi);

    cv::Rect seg_bbox = roi;
    const bool seg_found = initSegmentBbox(gray_work_, roi, roi_center, seg_bbox);
    const bool seg_smaller =
        seg_found &&
        (seg_bbox.area() < static_cast<int>(roi.area() * kSegMaxZoneFillRatio) ||
         seg_bbox.width < static_cast<int>(roi.width * 0.92f) ||
         seg_bbox.height < static_cast<int>(roi.height * 0.92f));

    const cv::Point2f seg_center{seg_bbox.x + seg_bbox.width * 0.5f,
                                 seg_bbox.y + seg_bbox.height * 0.5f};
    const float seg_shift = std::sqrt(distSq(seg_center, roi_center));
    const float shift_lim =
        static_cast<float>(std::min(roi.width, roi.height)) * 0.12f;

    const bool use_seg =
        seg_smaller && (initSegTrusted(gray_work_, roi, roi_center, seg_bbox) ||
                        seg_shift <= shift_lim);

    use_seg_track_ = use_seg && contrast_score_ >= kMinContrastForSeg;

    if (use_seg_track_) {
        const cv::Size  target_sz = seg_bbox.size();
        anchor_center_ = seg_center;
        center_        = seg_center;
        prev_center_   = seg_center;
        const cv::Rect templ_roi = templateRoiAtCenter(gray_work_, seg_center, target_sz);
        std::cout << "bench: segment init " << templ_roi.width << 'x' << templ_roi.height
                  << " (roi " << roi.width << 'x' << roi.height << ") contrast="
                  << contrast_score_ << " center="
                  << static_cast<int>(seg_center.x) << ',' << static_cast<int>(seg_center.y)
                  << '\n';
        setTemplateFromBbox(gray_work_, templ_roi);
    } else {
        std::cout << "bench: template init " << roi.width << 'x' << roi.height << " contrast="
                  << contrast_score_ << '\n';
        setTemplateFromBbox(gray_work_, roi);
    }

    if (template_gray_.empty() || ref_size_.width < 4) {
        resetUnlocked();
        return false;
    }

    scale_               = 1.0f;
    object_size_         = init_bbox_size_;
    active_              = true;
    return true;
}

bool MarkerTracker::responseWeightedAt(const cv::Mat& gray, const cv::Point2f& center,
                                       double& response_out) const
{
    response_out = 0.0;
    if (template_gray_.empty() || weight_sqrt_.empty()) {
        return false;
    }

    const cv::Size patch_sz = extractPatchSize(object_size_, scale_);
    const cv::Rect patch{static_cast<int>(std::lround(center.x - patch_sz.width * 0.5f)),
                         static_cast<int>(std::lround(center.y - patch_sz.height * 0.5f)),
                         patch_sz.width, patch_sz.height};
    if (patch.x < 0 || patch.y < 0 || patch.x + patch.width > gray.cols ||
        patch.y + patch.height > gray.rows) {
        return false;
    }

    return weightedNccPatch(gray(patch), template_gray_, weight_sqrt_, response_out);
}

bool MarkerTracker::locateWeighted(const cv::Mat& gray, const cv::Point2f& hint,
                                   cv::Point2f& out_center, double& response_out,
                                   float window_factor, float max_drift_factor) const
{
    response_out = 0.0;
    if (template_gray_.empty() || weight_sqrt_.empty()) {
        return false;
    }

    const cv::Size patch_sz = extractPatchSize(object_size_, scale_);
    const int      ref_side = std::max(patch_sz.width, patch_sz.height);
    const int      win_w =
        std::max(patch_sz.width + 8, static_cast<int>(ref_side * window_factor));
    const int win_h =
        std::max(patch_sz.height + 8, static_cast<int>(ref_side * window_factor));

    cv::Rect search{static_cast<int>(hint.x - win_w * 0.5f), static_cast<int>(hint.y - win_h * 0.5f),
                    win_w, win_h};
    search = clampRect(search, gray.cols, gray.rows);
    if (search.width < patch_sz.width + 2 || search.height < patch_sz.height + 2) {
        return false;
    }

    const float max_drift = static_cast<float>(ref_side) * max_drift_factor;

    cv::Mat templ_match;
    cv::resize(template_gray_, templ_match, patch_sz, 0, 0, cv::INTER_AREA);
    cv::Mat weight_match;
    cv::resize(weight_sqrt_, weight_match, patch_sz, 0, 0, cv::INTER_AREA);
    cv::Mat tw;
    cv::multiply(templ_match, weight_match, tw);

    const cv::Mat search_roi = gray(search);
    const int     max_dim    = std::max(search.width, search.height);
    float         ds         = 1.0f;
    if (max_dim > kNccSearchMaxSide) {
        ds = static_cast<float>(kNccSearchMaxSide) / static_cast<float>(max_dim);
    }

    cv::Mat search_work;
    cv::Size patch_work = patch_sz;
    cv::Mat  tw_work    = tw;
    if (ds < 0.999f) {
        cv::resize(search_roi, search_work, cv::Size(), ds, ds, cv::INTER_AREA);
        patch_work = {std::max(4, static_cast<int>(patch_sz.width * ds)),
                      std::max(4, static_cast<int>(patch_sz.height * ds))};
        cv::resize(tw, tw_work, patch_work, 0, 0, cv::INTER_AREA);
    } else {
        search_work = search_roi;
    }

    if (search_work.cols < patch_work.width + 2 || search_work.rows < patch_work.height + 2) {
        return false;
    }

    cv::Mat search_f;
    search_work.convertTo(search_f, CV_32F);

    cv::Mat corr;
    cv::matchTemplate(search_f, tw_work, corr, cv::TM_CCOEFF_NORMED);
    if (corr.empty()) {
        return false;
    }

    const float inv_ds = 1.0f / ds;
    maskCorrOutsideDrift(corr, search, patch_work, hint, max_drift, inv_ds);

    double    best_resp = 0.0;
    cv::Point best_pt;
    cv::minMaxLoc(corr, nullptr, &best_resp, nullptr, &best_pt);
    if (best_resp < kTrackMinResponse) {
        return false;
    }

    out_center = {static_cast<float>(search.x) + (static_cast<float>(best_pt.x) +
                                                  patch_work.width * 0.5f) * inv_ds,
                  static_cast<float>(search.y) + (static_cast<float>(best_pt.y) +
                                                  patch_work.height * 0.5f) * inv_ds};

    const cv::Rect verify{
        static_cast<int>(std::lround(out_center.x - patch_sz.width * 0.5f)),
        static_cast<int>(std::lround(out_center.y - patch_sz.height * 0.5f)), patch_sz.width,
        patch_sz.height};
    if (verify.x >= 0 && verify.y >= 0 && verify.x + verify.width <= gray.cols &&
        verify.y + verify.height <= gray.rows) {
        double verified = 0.0;
        if (weightedNccPatch(gray(verify), template_gray_, weight_sqrt_, verified)) {
            response_out = verified;
            return true;
        }
    }
    response_out = best_resp;
    return true;
}

cv::Rect MarkerTracker::trackZoneAround(cv::Point2f hint, float zone_factor) const
{
    const cv::Size base =
        (init_bbox_size_.width > 0) ? init_bbox_size_ : object_size_;
    const cv::Size cur = scaledSize(base, scale_);
    const int      pad =
        std::max(16, static_cast<int>(std::max(cur.width, cur.height) * (zone_factor - 1.0f) * 0.5f));
    cv::Rect zone{static_cast<int>(std::lround(hint.x - cur.width * 0.5f - pad)),
                  static_cast<int>(std::lround(hint.y - cur.height * 0.5f - pad)),
                  cur.width + 2 * pad, cur.height + 2 * pad};
    return clampRect(zone, kMainWidth, kMainHeight);
}

bool MarkerTracker::measureTrackBlob(const cv::Mat& gray, cv::Point2f hint, float zone_factor,
                                     cv::Point2f& out_center, cv::Size& out_size) const
{
    const cv::Rect zone = trackZoneAround(hint, zone_factor);
    if (zone.width < 16 || zone.height < 16) {
        return false;
    }

    cv::Rect blob;
    if (!segmentDarkCircle(gray, zone, hint, blob) && !segmentDarkBlob(gray, zone, hint, blob)) {
        return false;
    }

    const int max_side = std::max(init_bbox_size_.width, init_bbox_size_.height);
    const int abs_max  = std::max(kScaleMinSidePx,
                                  static_cast<int>(max_side * kScaleMaxRatio));
    if (std::max(blob.width, blob.height) > abs_max) {
        return false;
    }
    if (blob.area() > static_cast<int>(zone.area() * kSegMaxZoneFillRatio)) {
        return false;
    }

    const int min_side = std::max(kScaleMinSidePx, static_cast<int>(
                                      std::min(init_bbox_size_.width, init_bbox_size_.height) *
                                      kScaleMinRatio * 0.75f));
    if (std::min(blob.width, blob.height) < min_side) {
        return false;
    }

    out_center = {blob.x + blob.width * 0.5f, blob.y + blob.height * 0.5f};
    out_size   = blob.size();
    return true;
}

void MarkerTracker::updateScaleFromMeasure(const cv::Size& measured)
{
    if (init_bbox_size_.width <= 0 || init_bbox_size_.height <= 0) {
        return;
    }

    const cv::Size cur_sz    = scaledSize(init_bbox_size_, scale_);
    const int      cur_side  = std::max(cur_sz.width, cur_sz.height);
    const int      meas_side = std::max(measured.width, measured.height);

    const float sx =
        measured.width / static_cast<float>(std::max(1, init_bbox_size_.width));
    const float sy =
        measured.height / static_cast<float>(std::max(1, init_bbox_size_.height));
    float target = std::clamp((sx + sy) * 0.5f, kScaleMinRatio, kScaleMaxRatio);

    if (cur_side > 0 && meas_side > 0) {
        const float jump = static_cast<float>(meas_side) / static_cast<float>(cur_side);
        if (jump > kScaleMeasureMaxJump || jump < 1.0f / kScaleMeasureMaxJump) {
            target = scale_ * std::clamp(jump, 1.0f / kScaleMeasureMaxJump, kScaleMeasureMaxJump);
            target = std::clamp(target, kScaleMinRatio, kScaleMaxRatio);
        }
    }

    const float delta  = target - scale_;
    const float max_d = std::max(0.02f, scale_ * kScaleMaxStepRatio);
    target           = scale_ + std::clamp(delta, -max_d, max_d);

    const float alpha =
        (target < scale_) ? std::min(0.48f, kScaleSmoothAlpha * 1.35f) : kScaleSmoothAlpha * 0.85f;
    scale_ = scale_ * (1.0f - alpha) + target * alpha;
    scale_ = std::clamp(scale_, kScaleMinRatio, kScaleMaxRatio);
}

float MarkerTracker::measureRoiContrast(const cv::Mat& gray, const cv::Rect& roi)
{
    const cv::Rect z = clampRect(roi, gray.cols, gray.rows);
    if (z.width < 12 || z.height < 12) {
        return 0.0f;
    }

    cv::Scalar mean;
    cv::Scalar stddev;
    cv::meanStdDev(gray(z), mean, stddev);

    const int pad = std::max(2, std::min(z.width, z.height) / 5);
    cv::Rect  inner(z.x + pad, z.y + pad, z.width - 2 * pad, z.height - 2 * pad);
    if (inner.width < 4 || inner.height < 4) {
        return static_cast<float>(stddev[0]);
    }

    const double inner_m      = cv::mean(gray(inner))[0];
    const float  edge_contrast = std::abs(static_cast<float>(inner_m - mean[0]));
    return edge_contrast + static_cast<float>(stddev[0]) * 0.4f;
}

void MarkerTracker::probeScaleNcc(const cv::Mat& gray, const cv::Point2f& center)
{
    if (template_gray_.empty() || weight_sqrt_.empty()) {
        return;
    }

    const float scale_down =
        std::clamp(scale_ * kScaleProbeDownMul, kScaleMinRatio, kScaleMaxRatio);
    const float scale_up = std::clamp(scale_ * kScaleProbeUpMul, kScaleMinRatio, kScaleMaxRatio);

    auto trial_resp = [&](float trial_scale, double& resp) -> bool {
        trial_scale = std::clamp(trial_scale, kScaleMinRatio, kScaleMaxRatio);
        const cv::Size patch_sz = scaledSize(init_bbox_size_, trial_scale);
        cv::Rect       patch{static_cast<int>(std::lround(center.x - patch_sz.width * 0.5f)),
                       static_cast<int>(std::lround(center.y - patch_sz.height * 0.5f)),
                       patch_sz.width, patch_sz.height};
        if (patch.x < 0 || patch.y < 0 || patch.x + patch.width > gray.cols ||
            patch.y + patch.height > gray.rows) {
            return false;
        }
        return weightedNccAtPatchSize(gray(patch), template_gray_, weight_sqrt_, patch_sz, resp);
    };

    double resp_cur = 0.0;
    double resp_down = 0.0;
    double resp_up   = 0.0;
    const bool ok_cur  = trial_resp(scale_, resp_cur);
    const bool ok_down = scale_down < scale_ * 0.995f && trial_resp(scale_down, resp_down);
    const bool ok_up   = scale_up > scale_ * 1.005f && trial_resp(scale_up, resp_up);
    if (!ok_cur || resp_cur < kTrackMinResponseTemplate) {
        return;
    }

    if (ok_up && resp_up > resp_cur + 0.012 && (!ok_down || resp_up >= resp_down)) {
        scale_ = scale_up;
    } else if (ok_down && resp_down > resp_cur + 0.012 && (!ok_up || resp_down > resp_up)) {
        scale_ = scale_down;
    }
}

cv::Rect MarkerTracker::templateRoiAtCenter(const cv::Mat& gray, cv::Point2f c,
                                            cv::Size size) const
{
    cv::Rect roi{static_cast<int>(std::lround(c.x - size.width * 0.5f)),
                 static_cast<int>(std::lround(c.y - size.height * 0.5f)), size.width, size.height};
    return clampRect(roi, gray.cols, gray.rows);
}

cv::Rect MarkerTracker::bboxFromCenter() const
{
    const cv::Size base =
        (init_bbox_size_.width > 0 && init_bbox_size_.height > 0) ? init_bbox_size_ : object_size_;
    cv::Size out = scaledSize(base, scale_);

    const int frame_cap_w =
        std::max(kScaleMinSidePx, static_cast<int>(kMainWidth * kBboxMaxFrameSideRatio));
    const int frame_cap_h =
        std::max(kScaleMinSidePx, static_cast<int>(kMainHeight * kBboxMaxFrameSideRatio));
    const int init_cap_w = std::max(
        kScaleMinSidePx, static_cast<int>(base.width * kScaleMaxRatio));
    const int init_cap_h = std::max(
        kScaleMinSidePx, static_cast<int>(base.height * kScaleMaxRatio));
    out.width  = std::clamp(out.width, kScaleMinSidePx, std::min(frame_cap_w, init_cap_w));
    out.height = std::clamp(out.height, kScaleMinSidePx, std::min(frame_cap_h, init_cap_h));

    return clampRect({static_cast<int>(std::lround(center_.x - out.width * 0.5f)),
                      static_cast<int>(std::lround(center_.y - out.height * 0.5f)), out.width,
                      out.height},
                     kMainWidth, kMainHeight);
}

cv::Rect MarkerTracker::frozenMissBbox() const
{
    const cv::Size ref = (miss_bbox_size_.width > 0) ? miss_bbox_size_ : object_size_;
    const cv::Size out = scaledSize(ref, miss_scale_);
    return clampRect({static_cast<int>(miss_origin_.x - out.width * 0.5f),
                      static_cast<int>(miss_origin_.y - out.height * 0.5f), out.width, out.height},
                     kMainWidth, kMainHeight);
}

void MarkerTracker::beginMissEpisode()
{
    if (miss_frames_ == 0) {
        miss_origin_    = center_;
        miss_bbox_size_ = object_size_;
        miss_scale_     = scale_;
        velocity_       = {};
    }
}

MarkerDetection MarkerTracker::makeLostOutput() const
{
    MarkerDetection out;
    out.valid      = true;
    out.live       = false;
    out.capturing  = false;
    out.track      = MarkerTrackMode::Lost;
    out.confidence = 0.0f;
    return out;
}

MarkerDetection MarkerTracker::makeLiveHoldOutput(const cv::Point2f& frame_center) const
{
    MarkerDetection out;
    const cv::Rect bbox = bboxFromCenter();
    out.valid           = true;
    out.live            = true;
    out.capturing       = true;
    out.track           = MarkerTrackMode::Live;
    out.ex              = center_.x - frame_center.x;
    out.ey              = center_.y - frame_center.y;
    out.bbox            = bbox;
    out.confidence      = 0.5f;
    return out;
}

bool MarkerTracker::tryReacquire(const cv::Point2f& frame_center, MarkerDetection& out)
{
    cv::Point2f found;
    double      resp = 0.0;
    bool        ok   = false;

    if (use_seg_track_) {
        cv::Size sz;
        ok = measureTrackBlob(gray_work_, miss_origin_, kSegReacquireZoneFactor, found, sz) &&
             responseWeightedAt(gray_work_, found, resp) && resp >= kTrackMinResponse;
        if (ok) {
            updateScaleFromMeasure(sz);
        }
    } else {
        ok = locateWeighted(gray_work_, miss_origin_, found, resp, kSearchWindowTemplate,
                            kSearchDriftTemplate) &&
             resp >= kTrackMinResponse;
    }

    if (!ok) {
        return false;
    }

    center_        = found;
    prev_center_   = found;
    scale_         = miss_scale_;
    velocity_      = {};
    fail_streak_   = 0;
    miss_frames_   = 0;
    miss_time_sec_ = 0.0;
    marker_lost_   = false;
    miss_origin_   = {};

    const cv::Rect bbox = bboxFromCenter();
    out.valid           = true;
    out.live            = true;
    out.capturing       = true;
    out.track           = MarkerTrackMode::Live;
    out.ex              = center_.x - frame_center.x;
    out.ey              = center_.y - frame_center.y;
    out.bbox            = bbox;
    out.confidence      = static_cast<float>(resp);
    return true;
}

MarkerDetection MarkerTracker::handleMiss(double dt_sec, const cv::Point2f& frame_center)
{
    beginMissEpisode();
    miss_time_sec_ += dt_sec;
    ++miss_frames_;

    if (miss_time_sec_ >= kTrackSearchGiveUpSec) {
        if (!search_gave_up_) {
            search_gave_up_ = true;
            std::cout << "bench: search gave up after " << miss_time_sec_
                      << " s — select new ROI\n";
        }
        return makeLostOutput();
    }

    const cv::Rect frozen_bbox = frozenMissBbox();
    if (miss_time_sec_ < kTrackReacquireTimeoutSec) {
        if (frame_tick_ % kRelocateEveryNFrames == 0) {
            MarkerDetection reacquired;
            if (tryReacquire(frame_center, reacquired)) {
                return reacquired;
            }
        }

        MarkerDetection out;
        out.valid      = true;
        out.live       = true;
        out.capturing  = false;
        out.track      = MarkerTrackMode::Predicted;
        out.ex         = miss_origin_.x - frame_center.x;
        out.ey         = miss_origin_.y - frame_center.y;
        out.bbox       = frozen_bbox;
        out.confidence = 0.2f;
        return out;
    }

    if (!marker_lost_) {
        marker_lost_ = true;
        std::cout << "bench: marker lost (" << miss_time_sec_ << " s)\n";
    }
    return makeLostOutput();
}

MarkerDetection MarkerTracker::update(const cv::Mat& main_rgb, double dt_sec)
{
    std::lock_guard<std::mutex> lock(mtx_);

    MarkerDetection out;
    if (!active_ || main_rgb.empty()) {
        return out;
    }
    if (search_gave_up_) {
        return makeLostOutput();
    }
    if (dt_sec <= 0.0) {
        dt_sec = 1.0 / kMainFps;
    }

    prepareGray(main_rgb, gray_work_);
    if (gray_work_.empty()) {
        return out;
    }

    ++frame_tick_;
    const cv::Point2f frame_center(kMainWidth * 0.5f, kMainHeight * 0.5f);

    if (miss_frames_ > 0 || marker_lost_) {
        return handleMiss(dt_sec, frame_center);
    }

    const cv::Point2f pred = center_ + velocity_ * static_cast<float>(dt_sec);

    cv::Point2f found;
    double      resp    = 0.0;
    bool        located = false;

    const int lock_frames = use_seg_track_ ? kCenterLockFrames : kCenterLockTemplate;
    const int fail_limit  = use_seg_track_ ? kVerifyFailToSearch : kVerifyFailTemplate;

    if (frame_tick_ <= lock_frames) {
        found   = anchor_center_;
        resp    = 1.0;
        located = true;
    } else if (use_seg_track_) {
        const float zone_factor = scale_ >= kSegLargeScaleThreshold ? kSegTrackZoneFactorLarge
                                                                    : kSegTrackZoneFactor;
        cv::Point2f seg_center;
        cv::Size    seg_size;
        if (measureTrackBlob(gray_work_, pred, zone_factor, seg_center, seg_size)) {
            const cv::Size cur_sz = scaledSize(init_bbox_size_, scale_);
            const int      cur_side  = std::max(cur_sz.width, cur_sz.height);
            const int      meas_side = std::max(seg_size.width, seg_size.height);
            const bool     grow_jump =
                meas_side > static_cast<int>(static_cast<float>(cur_side) * kScaleMeasureMaxJump);
            const bool shrink_jump =
                meas_side < static_cast<int>(static_cast<float>(cur_side) / kScaleMeasureMaxJump);

            double seg_resp = 0.0;
            if (!grow_jump && responseWeightedAt(gray_work_, seg_center, seg_resp) &&
                seg_resp >= kTrackMinResponse) {
                const float ref_side = static_cast<float>(cur_side);
                const float max_step = std::max(4.0f, ref_side * kSegMaxCenterStepFactor);
                if (distSq(seg_center, pred) <= max_step * max_step) {
                    found   = seg_center;
                    if (!shrink_jump || seg_resp >= kTrackMinResponse + 0.04) {
                        updateScaleFromMeasure(seg_size);
                    }
                    resp    = seg_resp;
                    located = true;
                }
            }
        }
        if (!located) {
            located = resolvePosition(gray_work_, pred, dt_sec, found, resp);
        }
    } else {
        double pred_resp = 0.0;
        if (responseWeightedAt(gray_work_, pred, pred_resp) &&
            pred_resp >= kTrackMinResponseTemplate) {
            found   = pred;
            resp    = pred_resp;
            located = true;

            cv::Point2f peak{};
            double      peak_resp = 0.0;
            if (locateWeighted(gray_work_, pred, peak, peak_resp, 1.06f, 0.12f)) {
                const float ref_side = static_cast<float>(std::max(
                    scaledSize(init_bbox_size_, scale_).width,
                    scaledSize(init_bbox_size_, scale_).height));
                if (distSq(peak, pred) <= (ref_side * 0.18f) * (ref_side * 0.18f) &&
                    peak_resp >= pred_resp - 0.03) {
                    found = peak;
                    resp  = peak_resp;
                }
            }
        }
        if (!located) {
            located = resolvePosition(gray_work_, pred, dt_sec, found, resp);
        }
        if (located && frame_tick_ % kScaleProbeEveryNFrames == 0) {
            probeScaleNcc(gray_work_, found);
        }
    }

    if (!located) {
        ++fail_streak_;
        if (fail_streak_ >= fail_limit) {
            return handleMiss(dt_sec, frame_center);
        }
        return makeLiveHoldOutput(frame_center);
    }

    fail_streak_ = 0;
    center_      = found;

    if (frame_tick_ <= lock_frames) {
        velocity_ = {};
    } else {
        const cv::Point2f instant_v = (center_ - prev_center_) / static_cast<float>(dt_sec);
        velocity_ = velocity_ * (1.0f - kVelocityEmaAlpha) + instant_v * kVelocityEmaAlpha;
    }
    prev_center_ = center_;

    const cv::Rect bbox = bboxFromCenter();
    if (!bboxMostlyInFrame(bbox, kMainWidth, kMainHeight)) {
        fail_streak_ = kVerifyFailToSearch;
        return handleMiss(dt_sec, frame_center);
    }

    out.valid      = true;
    out.live       = true;
    out.capturing  = true;
    out.track      = MarkerTrackMode::Live;
    out.ex         = center_.x - frame_center.x;
    out.ey         = center_.y - frame_center.y;
    out.bbox       = bbox;
    out.confidence = static_cast<float>(resp);
    return out;
}

}  // namespace bench
