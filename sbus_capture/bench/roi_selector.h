#pragma once

#include "mouse_input.h"

#include <opencv2/core.hpp>

namespace bench {

struct RoiUiState {
    bool     mouse_ok     = false;
    cv::Point cursor_lores;
    bool     dragging     = false;
    cv::Rect drag_rect_lores;
    bool     roi_locked   = false;
    cv::Rect roi_lores;
};

// Выделение области на PAL-кадре (720×576) мышью: ЛКМ — рамка, ПКМ — сброс.
class RoiSelector {
public:
    void setScreenSize(int lores_w, int lores_h);

    // Вызывать каждый кадр после MouseInput::poll().
    void update(const MouseInput& mouse, int main_w, int main_h);

    bool hasTrackRoi() const { return roi_main_.width > 0 && roi_main_.height > 0; }
    cv::Rect trackRoiMain() const { return roi_main_; }
    void clearRoi();

    const RoiUiState& ui() const { return ui_; }

private:
    static cv::Rect normalizeRect(cv::Point a, cv::Point b);
    cv::Rect loresToMain(const cv::Rect& lores, int main_w, int main_h) const;

    RoiUiState ui_;
    cv::Point  drag_start_ {};
    cv::Rect   roi_main_;
    cv::Rect   roi_lores_;
    int        lores_w_ = 720;
    int        lores_h_ = 576;
};

}  // namespace bench
