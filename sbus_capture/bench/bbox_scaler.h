#pragma once

#include <opencv2/core.hpp>

namespace bench {

// Оценка масштаба по шаблону ROI: matchTemplate только в окне ~2× bbox (FullHD).
class BboxScaler {
public:
    void reset();
    void init(const cv::Mat& bgr_full, const cv::Rect& roi_main);
    // Возвращает true, если найдено надёжное совпадение; scale_out — целевой масштаб.
    bool measureScale(const cv::Mat& bgr_full, const cv::Point2f& center_main,
                      const cv::Size& bbox_size, float& scale_out);

    float scaleTarget() const { return scale_target_; }

private:
    cv::Mat  template_gray_;
    cv::Size init_size_;
    float    scale_target_ = 1.0f;
    bool     ready_        = false;
};

}  // namespace bench
