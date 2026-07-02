#include "osd_renderer.h"
#include "config.h"

#include <algorithm>
#include <opencv2/imgproc.hpp>

#include <cstdio>

namespace bench {

namespace {

cv::Scalar osdCapturedColor()
{
    return {kOsdColorCapturedB, kOsdColorCapturedG, kOsdColorCapturedR};
}

cv::Scalar osdLostColor()
{
    return {kOsdColorLostB, kOsdColorLostG, kOsdColorLostR};
}

void drawMarkerBox(cv::Mat& img, const cv::Rect& box, const cv::Scalar& color)
{
    cv::rectangle(img, box, color, kOsdMarkerBoxThickness, cv::LINE_8);
}

}  // namespace

void OsdRenderer::render(cv::Mat& pal_bgr, const MarkerDetection& det_main,
                         const PalMarkerOverlay& pal_overlay, const OsdTelemetry& telem,
                         const RoiUiState& roi_ui) const
{
    if (pal_bgr.empty()) {
        return;
    }

    if (roi_ui.mouse_ok) {
        const cv::Point c = roi_ui.cursor_lores;
        cv::line(pal_bgr, {c.x - 8, c.y}, {c.x + 8, c.y}, {255, 255, 255}, 1, cv::LINE_8);
        cv::line(pal_bgr, {c.x, c.y - 8}, {c.x, c.y + 8}, {255, 255, 255}, 1, cv::LINE_8);
    }

    if (roi_ui.dragging && roi_ui.drag_rect_lores.width > 0 &&
        roi_ui.drag_rect_lores.height > 0) {
        cv::rectangle(pal_bgr, roi_ui.drag_rect_lores, {0, 255, 255}, 1, cv::LINE_8);
    }

    const cv::Point center(pal_bgr.cols / 2, pal_bgr.rows / 2);
    cv::line(pal_bgr, {center.x - 20, center.y}, {center.x + 20, center.y},
             {0, 255, 0}, 1, cv::LINE_8);
    cv::line(pal_bgr, {center.x, center.y - 20}, {center.x, center.y + 20},
             {0, 255, 0}, 1, cv::LINE_8);

    if (pal_overlay.valid && pal_overlay.capturing && pal_overlay.bbox.width > 0 &&
        pal_overlay.bbox.height > 0) {
        last_box_pal_ = pal_overlay.bbox;
        has_last_box_ = true;
    } else if (!pal_overlay.valid || (!pal_overlay.capturing && !pal_overlay.live)) {
        has_last_box_ = false;
    }

    if (has_last_box_ && (pal_overlay.capturing || pal_overlay.live)) {
        cv::Rect box = last_box_pal_;
        if (pal_overlay.valid && pal_overlay.bbox.width > 0 && pal_overlay.bbox.height > 0) {
            box = pal_overlay.bbox;
        }
        const cv::Scalar color =
            pal_overlay.capturing ? osdCapturedColor() : osdLostColor();
        drawMarkerBox(pal_bgr, box, color);
    }

    char line[128];
    std::snprintf(line, sizeof(line), "%s | %s", telem.status.c_str(), telem.mode.c_str());
    cv::putText(pal_bgr, line, {10, 24}, cv::FONT_HERSHEY_SIMPLEX, 0.55, {255, 255, 255}, 1,
                cv::LINE_8);

    if (det_main.capturing) {
        std::snprintf(line, sizeof(line), "CAP ex=%.0f ey=%.0f", det_main.ex, det_main.ey);
        cv::putText(pal_bgr, line, {10, 48}, cv::FONT_HERSHEY_SIMPLEX, 0.5, {255, 255, 255}, 1,
                    cv::LINE_8);
    } else if (has_last_box_) {
        cv::putText(pal_bgr, "LOS", {10, 48}, cv::FONT_HERSHEY_SIMPLEX, 0.5, {255, 255, 255}, 1,
                    cv::LINE_8);
    } else {
        cv::putText(pal_bgr, "SELECT ROI", {10, 48}, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    {128, 128, 128}, 1, cv::LINE_8);
    }

    std::snprintf(line, sizeof(line), "pan=%.1f tilt=%.1f%s", telem.pan_deg, telem.tilt_deg,
                  telem.gimbal_hw ? "" : " (sim)");
    cv::putText(pal_bgr, line, {10, 72}, cv::FONT_HERSHEY_SIMPLEX, 0.45, {180, 220, 255}, 1,
                cv::LINE_8);
}

}  // namespace bench
