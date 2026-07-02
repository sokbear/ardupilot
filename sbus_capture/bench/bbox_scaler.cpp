#include "bbox_scaler.h"

#include "config.h"

#include <algorithm>
#include <cmath>
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

void tryScale(const cv::Mat& search_gray, const cv::Mat& templ, float scale,
              const cv::Size& init_size, float pixel_scale, float& best_scale,
              double& best_resp)
{
    const int tw =
        std::max(2, static_cast<int>(init_size.width * scale * pixel_scale));
    const int th =
        std::max(2, static_cast<int>(init_size.height * scale * pixel_scale));
    if (tw >= search_gray.cols || th >= search_gray.rows) {
        return;
    }

    cv::Mat resized;
    cv::resize(templ, resized, cv::Size(tw, th), 0, 0, cv::INTER_AREA);
    cv::Mat corr;
    cv::matchTemplate(search_gray, resized, corr, cv::TM_CCOEFF_NORMED);
    double resp = 0.0;
    cv::minMaxLoc(corr, nullptr, &resp);
    if (resp > best_resp) {
        best_resp  = resp;
        best_scale = scale;
    }
}

}  // namespace

void BboxScaler::reset()
{
    template_gray_ = cv::Mat();
    init_size_     = {};
    scale_target_  = 1.0f;
    ready_         = false;
}

void BboxScaler::init(const cv::Mat& bgr_full, const cv::Rect& roi_main)
{
    reset();
    if (bgr_full.empty() || roi_main.width <= 0 || roi_main.height <= 0) {
        return;
    }

    const cv::Rect roi = clampRect(roi_main, bgr_full.cols, bgr_full.rows);
    cv::Mat          roi_gray;
    cv::cvtColor(bgr_full(roi), roi_gray, cv::COLOR_BGR2GRAY);
    template_gray_ = roi_gray.clone();
    init_size_     = roi.size();
    scale_target_  = 1.0f;
    ready_         = true;
}

bool BboxScaler::measureScale(const cv::Mat& bgr_full, const cv::Point2f& center_main,
                              const cv::Size& bbox_size, float& scale_out)
{
    scale_out = scale_target_;
    if (!ready_ || bgr_full.empty() || init_size_.width <= 0 || init_size_.height <= 0) {
        return false;
    }

    const int ref_side = std::max(bbox_size.width, bbox_size.height);
    int       win_side = std::max(ref_side + 16,
                                  static_cast<int>(ref_side * kScaleRoiWindowFactor));
    win_side           = std::min(win_side, kScaleRoiMaxSide);

    cv::Rect search{
        static_cast<int>(center_main.x - win_side * 0.5f),
        static_cast<int>(center_main.y - win_side * 0.5f),
        win_side,
        win_side,
    };
    search = clampRect(search, bgr_full.cols, bgr_full.rows);
    if (search.width <= init_size_.width / 2 || search.height <= init_size_.height / 2) {
        return false;
    }

    cv::Mat search_gray;
    cv::cvtColor(bgr_full(search), search_gray, cv::COLOR_BGR2GRAY);

    const int   max_dim = std::max(search_gray.cols, search_gray.rows);
    const float downscale =
        (max_dim > kScaleSearchMaxSide)
            ? static_cast<float>(kScaleSearchMaxSide) / static_cast<float>(max_dim)
            : 1.0f;

    cv::Mat search_small;
    if (downscale < 0.999f) {
        cv::resize(search_gray, search_small, cv::Size(), downscale, downscale,
                   cv::INTER_AREA);
    } else {
        search_small = search_gray;
    }

    float  best_scale = scale_target_;
    double best_resp  = -1.0;

    for (int i = 0; i < kScaleLocalSteps; ++i) {
        const float t   = static_cast<float>(i) / static_cast<float>(kScaleLocalSteps - 1);
        const float mul = kScaleLocalMin + (kScaleLocalMax - kScaleLocalMin) * t;
        tryScale(search_small, template_gray_, scale_target_ * mul, init_size_, downscale,
                 best_scale, best_resp);
    }

    if (best_resp < kScaleMinResponse) {
        return false;
    }

    const float alpha =
        (best_scale >= scale_target_) ? kScaleGrowAlpha : kScaleShrinkAlpha;
    scale_target_ = std::clamp(scale_target_ + (best_scale - scale_target_) * alpha,
                               kScaleMinRatio, kScaleMaxRatio);
    scale_out     = scale_target_;
    return true;
}

}  // namespace bench
