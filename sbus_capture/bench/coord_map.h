#pragma once

#include "track_result.h"

#include <opencv2/core.hpp>

namespace bench {

// Рамка и ошибки трека в координатах PAL (720×576).
struct PalMarkerOverlay {
    cv::Rect bbox;
    bool     valid     = false;
    bool     capturing = false;
    bool     live      = false;
};

cv::Rect mapMainRectToPal(const cv::Rect& main, int main_w, int main_h, int pal_w, int pal_h);

PalMarkerOverlay mapDetectionToPal(const MarkerDetection& main_det, int main_w, int main_h,
                                   int pal_w, int pal_h);

}  // namespace bench
