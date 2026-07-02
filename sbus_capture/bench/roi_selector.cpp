#include "roi_selector.h"

#include "config.h"

#include <algorithm>
#include <iostream>

namespace bench {

void RoiSelector::setScreenSize(int lores_w, int lores_h)
{
    lores_w_ = lores_w > 0 ? lores_w : kLoresWidth;
    lores_h_ = lores_h > 0 ? lores_h : kLoresHeight;
}

cv::Rect RoiSelector::normalizeRect(cv::Point a, cv::Point b)
{
    const int x = std::min(a.x, b.x);
    const int y = std::min(a.y, b.y);
    const int w = std::abs(a.x - b.x);
    const int h = std::abs(a.y - b.y);
    return {x, y, w, h};
}

cv::Rect RoiSelector::loresToMain(const cv::Rect& lores, int main_w, int main_h) const
{
    if (lores.width <= 0 || lores.height <= 0) {
        return {};
    }
    const float sx = static_cast<float>(main_w) / static_cast<float>(lores_w_);
    const float sy = static_cast<float>(main_h) / static_cast<float>(lores_h_);
    return {
        static_cast<int>(lores.x * sx),
        static_cast<int>(lores.y * sy),
        std::max(1, static_cast<int>(lores.width * sx)),
        std::max(1, static_cast<int>(lores.height * sy)),
    };
}

void RoiSelector::clearRoi()
{
    roi_main_           = {};
    roi_lores_          = {};
    ui_.roi_locked      = false;
    ui_.roi_lores       = {};
    ui_.dragging        = false;
    ui_.drag_rect_lores = {};
}

void RoiSelector::update(const MouseInput& mouse, int main_w, int main_h)
{
    ui_.mouse_ok = mouse.isOpen();
    if (!mouse.isOpen()) {
        return;
    }

    ui_.cursor_lores = {mouse.cursorX(), mouse.cursorY()};

    if (mouse.rightPressed()) {
        clearRoi();
        return;
    }

    if (mouse.leftPressed()) {
        drag_start_         = ui_.cursor_lores;
        ui_.dragging        = true;
        ui_.drag_rect_lores = {drag_start_.x, drag_start_.y, 0, 0};
    }

    if (ui_.dragging && mouse.leftDown()) {
        ui_.drag_rect_lores = normalizeRect(drag_start_, ui_.cursor_lores);
    }

    if (mouse.leftReleased() && ui_.dragging) {
        ui_.dragging = false;
        cv::Rect sel = normalizeRect(drag_start_, ui_.cursor_lores);
        if (sel.width >= kRoiMinSelectSidePx && sel.height >= kRoiMinSelectSidePx) {
            roi_lores_     = sel;
            roi_main_      = loresToMain(sel, main_w, main_h);
            ui_.roi_locked = true;
            ui_.roi_lores  = sel;
            std::cout << "bench: ROI locked " << roi_main_.x << ',' << roi_main_.y << ' '
                      << roi_main_.width << 'x' << roi_main_.height << '\n';
        } else {
            ui_.drag_rect_lores = {};
        }
    }
}

}  // namespace bench
