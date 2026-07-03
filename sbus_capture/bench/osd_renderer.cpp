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

cv::Scalar osdCursorColor()
{
    return {kOsdColorCursorB, kOsdColorCursorG, kOsdColorCursorR};
}

void drawMarkerBox(cv::Mat& img, const cv::Rect& box, const cv::Scalar& color)
{
    cv::rectangle(img, box, color, kOsdMarkerBoxThickness, cv::LINE_8);
}

void drawCenterLostBanner(cv::Mat& img)
{
    constexpr const char* kLine1 = "MARK LOST";
    constexpr const char* kLine2 = "POTERYA METKI";
    const double            scale     = kOsdLostMessageScale;
    const int               thickness = kOsdLostMessageThick;

    int baseline = 0;
    const cv::Size ts1 =
        cv::getTextSize(kLine1, cv::FONT_HERSHEY_DUPLEX, scale, thickness, &baseline);
    const cv::Size ts2 =
        cv::getTextSize(kLine2, cv::FONT_HERSHEY_DUPLEX, scale * 0.72, thickness - 1, &baseline);

    const int block_w = std::max(ts1.width, ts2.width) + 48;
    const int block_h = ts1.height + ts2.height + 36;
    const cv::Rect bg((img.cols - block_w) / 2, (img.rows - block_h) / 2, block_w, block_h);
    cv::rectangle(img, bg, {0, 0, 0}, cv::FILLED);
    cv::rectangle(img, bg, {0, 0, 255}, 3, cv::LINE_8);

    const cv::Point org1(bg.x + (bg.width - ts1.width) / 2, bg.y + ts1.height + 12);
    const cv::Point org2(bg.x + (bg.width - ts2.width) / 2, org1.y + ts2.height + 10);

    cv::putText(img, kLine1, org1, cv::FONT_HERSHEY_DUPLEX, scale, {0, 255, 255}, thickness,
                cv::LINE_8);
    cv::putText(img, kLine2, org2, cv::FONT_HERSHEY_DUPLEX, scale * 0.72, {255, 255, 255},
                thickness - 1, cv::LINE_8);
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
        cv::line(pal_bgr, {c.x - 8, c.y}, {c.x + 8, c.y}, osdCursorColor(), 1, cv::LINE_8);
        cv::line(pal_bgr, {c.x, c.y - 8}, {c.x, c.y + 8}, osdCursorColor(), 1, cv::LINE_8);
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

    if (pal_overlay.valid && pal_overlay.bbox.width > 0 && pal_overlay.bbox.height > 0 &&
        (det_main.track == MarkerTrackMode::Live ||
         det_main.track == MarkerTrackMode::Predicted)) {
        last_box_pal_ = pal_overlay.bbox;
        has_last_box_ = true;
    } else if (det_main.track == MarkerTrackMode::Lost ||
               det_main.track == MarkerTrackMode::None) {
        has_last_box_ = false;
    }

    if (has_last_box_) {
        cv::Rect box = last_box_pal_;
        if (pal_overlay.valid && pal_overlay.bbox.width > 0 && pal_overlay.bbox.height > 0) {
            box = pal_overlay.bbox;
        }
        cv::Scalar color = osdCapturedColor();
        if (det_main.track == MarkerTrackMode::Predicted) {
            color = {0, 255, 255};
        }
        drawMarkerBox(pal_bgr, box, color);
    }

    char line[128];
    std::snprintf(line, sizeof(line), "%s | %s", telem.status.c_str(), telem.mode.c_str());
    cv::putText(pal_bgr, line, {10, 24}, cv::FONT_HERSHEY_SIMPLEX, 0.55, {255, 255, 255}, 1,
                cv::LINE_8);

    if (telem.lost_banner) {
        drawCenterLostBanner(pal_bgr);
    }

    if (det_main.capturing) {
        std::snprintf(line, sizeof(line), "CAP ex=%.0f ey=%.0f", det_main.ex, det_main.ey);
        cv::putText(pal_bgr, line, {10, 48}, cv::FONT_HERSHEY_SIMPLEX, 0.5, {255, 255, 255}, 1,
                    cv::LINE_8);
    } else if (det_main.track == MarkerTrackMode::Lost) {
        cv::putText(pal_bgr, "LOST", {10, 48}, cv::FONT_HERSHEY_SIMPLEX, 0.5, {0, 0, 255}, 1,
                    cv::LINE_8);
    } else if (det_main.track == MarkerTrackMode::Predicted && has_last_box_) {
        cv::putText(pal_bgr, "SEARCH", {10, 48}, cv::FONT_HERSHEY_SIMPLEX, 0.5, {0, 255, 255}, 1,
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
