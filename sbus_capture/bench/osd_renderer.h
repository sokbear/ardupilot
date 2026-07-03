#pragma once

#include "coord_map.h"
#include "roi_selector.h"
#include "track_result.h"

#include <opencv2/core.hpp>
#include <string>

namespace bench {

struct OsdTelemetry {
    std::string status = "BENCH";
    std::string mode   = "TRACK";
    float pan_deg      = 0.0f;
    float tilt_deg     = 0.0f;
    bool  gimbal_hw    = false;
    bool  lost_banner  = false;  // «Потеря метки» по центру экрана
};

class OsdRenderer {
public:
    // det_main — координаты в FullHD; pal_overlay — рамка в PAL (после mapDetectionToPal).
    void render(cv::Mat& pal_bgr, const MarkerDetection& det_main,
                const PalMarkerOverlay& pal_overlay, const OsdTelemetry& telem,
                const RoiUiState& roi_ui) const;

private:
    mutable cv::Rect last_box_pal_;
    mutable bool     has_last_box_ = false;
};

}  // namespace bench
