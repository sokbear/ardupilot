#include "coord_map.h"

#include <algorithm>

namespace bench {

cv::Rect mapMainRectToPal(const cv::Rect& main, int main_w, int main_h, int pal_w, int pal_h)
{
    if (main.width <= 0 || main.height <= 0 || main_w <= 0 || main_h <= 0) {
        return {};
    }
    const float sx = static_cast<float>(pal_w) / static_cast<float>(main_w);
    const float sy = static_cast<float>(pal_h) / static_cast<float>(main_h);
    return {
        static_cast<int>(main.x * sx),
        static_cast<int>(main.y * sy),
        std::max(4, static_cast<int>(main.width * sx)),
        std::max(4, static_cast<int>(main.height * sy)),
    };
}

PalMarkerOverlay mapDetectionToPal(const MarkerDetection& main_det, int main_w, int main_h,
                                   int pal_w, int pal_h)
{
    PalMarkerOverlay out;
    if (!main_det.valid || main_det.bbox.width <= 0 || main_det.bbox.height <= 0) {
        return out;
    }
    out.valid     = true;
    out.capturing = main_det.capturing;
    out.live      = main_det.live;
    out.bbox      = mapMainRectToPal(main_det.bbox, main_w, main_h, pal_w, pal_h);
    return out;
}

}  // namespace bench
