#include "target_verifier.h"

#include "config.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace bench {

cv::Mat TargetVerifier::extractGrayCanon(const cv::Mat& bgr_frame, const cv::Rect& r,
                                          int size)
{
    const cv::Rect full(0, 0, bgr_frame.cols, bgr_frame.rows);
    const cv::Rect c = r & full;
    if (c.width < 2 || c.height < 2) {
        return {};
    }

    cv::Mat gray;
    cv::cvtColor(bgr_frame(c), gray, cv::COLOR_BGR2GRAY);

    cv::Mat canon;
    const int interp =
        (c.width > size || c.height > size) ? cv::INTER_AREA : cv::INTER_LINEAR;
    cv::resize(gray, canon, cv::Size(size, size), 0, 0, interp);

    cv::Mat canon_f;
    canon.convertTo(canon_f, CV_32F);
    return canon_f;
}

void TargetVerifier::init(const cv::Mat& bgr_frame, const cv::Rect& bbox)
{
    reference_   = extractGrayCanon(bgr_frame, bbox, kVerifyCanonSize);
    baseline_    = -2.0f;
    fail_streak_ = 0;
    hard_streak_ = 0;
    active_      = !reference_.empty();
}

void TargetVerifier::reset()
{
    reference_.release();
    baseline_    = -2.0f;
    fail_streak_ = 0;
    hard_streak_ = 0;
    active_      = false;
}

VerifyResult TargetVerifier::verify(const cv::Mat& bgr_frame, const cv::Rect& bbox)
{
    VerifyResult res;
    if (!active_ || reference_.empty()) {
        res.ok       = true;
        res.frame_ok = true;
        return res;
    }

    // Окно поиска: рамка, раздутая вокруг центра на kVerifySearchInflate.
    const float cx = bbox.x + bbox.width * 0.5f;
    const float cy = bbox.y + bbox.height * 0.5f;
    const int   sw = static_cast<int>(std::lround(bbox.width * kVerifySearchInflate));
    const int   sh = static_cast<int>(std::lround(bbox.height * kVerifySearchInflate));
    const cv::Rect search_rect(static_cast<int>(std::lround(cx - sw * 0.5f)),
                               static_cast<int>(std::lround(cy - sh * 0.5f)), sw, sh);

    const int search_size =
        static_cast<int>(std::lround(kVerifyCanonSize * kVerifySearchInflate));
    const cv::Mat search = extractGrayCanon(bgr_frame, search_rect, search_size);
    if (search.empty()) {
        res.frame_ok = false;
        ++fail_streak_;
        ++hard_streak_;
        res.ok = fail_streak_ < kVerifyFailStreak &&
                 hard_streak_ < kVerifyHardFailStreak;
        res.baseline = baseline_;
        return res;
    }

    // Максимум нормированной корреляции по окну — допуск на джиттер рамки.
    cv::Mat corr;
    cv::matchTemplate(search, reference_, corr, cv::TM_CCOEFF_NORMED);
    double    maxv = 0.0;
    cv::Point maxloc;
    cv::minMaxLoc(corr, nullptr, &maxv, nullptr, &maxloc);
    res.ncc_score = static_cast<float>(maxv);

    // Первый кадр после init: устанавливаем базовую линию.
    if (baseline_ < -1.0f) {
        baseline_ = res.ncc_score;
    }
    res.baseline = baseline_;

    // Относительный критерий: провал = падение ниже базовой линии на запас.
    res.frame_ok = res.ncc_score >= baseline_ - kVerifyNccDropMargin;

    if (res.frame_ok) {
        fail_streak_ = 0;
        hard_streak_ = 0;
        // Базовая линия следует за фактическим уровнем корреляции цели.
        baseline_ = baseline_ * (1.0f - kVerifyBaselineAlpha) +
                    res.ncc_score * kVerifyBaselineAlpha;

        // Адаптация эталона — по выровненному патчу (в точке максимума),
        // только на кадрах не хуже базовой линии.
        if (res.ncc_score >= baseline_) {
            const cv::Rect aligned(maxloc.x, maxloc.y, kVerifyCanonSize,
                                   kVerifyCanonSize);
            if (aligned.x >= 0 && aligned.y >= 0 &&
                aligned.x + aligned.width <= search.cols &&
                aligned.y + aligned.height <= search.rows) {
                reference_ = reference_ * (1.0f - kVerifyTemplateAdaptAlpha) +
                             search(aligned) * kVerifyTemplateAdaptAlpha;
            }
        }
    } else {
        ++fail_streak_;
        if (res.ncc_score < kVerifyHardFloor) {
            ++hard_streak_;   // глубокий провал — сигнатура перекрытия
        } else {
            hard_streak_ = 0; // неглубокий (смаз) — быстрый путь не копится
        }
    }

    res.ok = res.frame_ok || (fail_streak_ < kVerifyFailStreak &&
                              hard_streak_ < kVerifyHardFailStreak);
    return res;
}

}  // namespace bench
