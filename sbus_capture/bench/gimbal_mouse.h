#pragma once

#include "config.h"

#include <algorithm>

namespace bench {

struct GimbalMouseAngles {
    float pan_deg  = 0.0f;
    float tilt_deg = 0.0f;
};

// Курсор на PAL-экране (lores): центр = 0°; вправо → pan + (по часовой);
// влево → pan −; вверх → tilt +; вниз → tilt −. Линейно по краю экрана → ±travel.
inline GimbalMouseAngles gimbalAnglesFromCursor(int cursor_x, int cursor_y,
                                                int screen_w, int screen_h)
{
    const float half_w = static_cast<float>(screen_w) * 0.5f;
    const float half_h = static_cast<float>(screen_h) * 0.5f;
    if (half_w <= 0.0f || half_h <= 0.0f) {
        return {};
    }

    const float nx = std::clamp((static_cast<float>(cursor_x) - half_w) / half_w,
                                -1.0f, 1.0f);
    const float ny = std::clamp((half_h - static_cast<float>(cursor_y)) / half_h,
                                -1.0f, 1.0f);

    return {nx * kServoTravelDeg, ny * kServoTravelDeg};
}

}  // namespace bench
