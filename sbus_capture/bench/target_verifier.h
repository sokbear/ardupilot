#pragma once

#include <opencv2/core.hpp>

namespace bench {

struct VerifyResult {
    bool  ok        = false;  // итоговый вердикт (с учётом демпфирования)
    bool  frame_ok  = false;  // сырой вердикт этого кадра
    float ncc_score = 0.0f;   // максимум корреляции по окну поиска
    float baseline  = 0.0f;   // текущая базовая линия NCC
};

// Верификация "то ли это, что мы отслеживаем" поверх NanoTrack.
// Критерий ОТНОСИТЕЛЬНЫЙ: не абсолютный порог NCC (он не работает для
// неконтрастных меток), а падение NCC ниже собственной базовой линии
// цели (EMA по принятым кадрам) более чем на kVerifyNccDropMargin,
// устойчивое kVerifyFailStreak кадров подряд.
// Сопоставление устойчиво к джиттеру рамки: эталон ищется в окне,
// раздутом на kVerifySearchInflate, берётся максимум корреляции.
class TargetVerifier {
public:
    void init(const cv::Mat& bgr_frame, const cv::Rect& bbox);
    VerifyResult verify(const cv::Mat& bgr_frame, const cv::Rect& bbox);
    void reset();
    bool active() const { return active_; }

private:
    // Серый патч произвольного прямоугольника, приведённый к size×size, CV_32F.
    static cv::Mat extractGrayCanon(const cv::Mat& bgr_frame, const cv::Rect& r,
                                    int size);

    cv::Mat reference_;            // kVerifyCanonSize², CV_32F — эталон
    float   baseline_    = -2.0f;  // <-1 = ещё не установлена
    int     fail_streak_ = 0;
    int     hard_streak_ = 0;  // подряд кадров с NCC ниже kVerifyHardFloor
    bool    active_      = false;
};

}  // namespace bench
