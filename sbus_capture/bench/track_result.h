#pragma once

#include <opencv2/core.hpp>

namespace bench {

enum class MarkerTrackMode {
    None,
    Live,
    Predicted,  // MOSSE потерял, идёт поиск по шаблону (до kTrackReacquireTimeoutSec)
    Lost,       // таймаут 1 с — метка не найдена
};

struct MarkerDetection {
    bool            valid     = false;
    bool            live      = false;
    bool            capturing = false;  // true = цель видна (красная рамка)
    MarkerTrackMode track     = MarkerTrackMode::None;
    double ex    = 0.0;
    double ey    = 0.0;
    cv::Rect bbox;
    float  confidence = 0.0f;
};

}  // namespace bench
