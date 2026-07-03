#pragma once

#include "config.h"
#include "osd_renderer.h"
#include "roi_selector.h"
#include "track_result.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <opencv2/core.hpp>

namespace bench {

struct DisplaySnapshot {
    uint64_t seq = 0;
    cv::Mat  main_rgb;
    MarkerDetection det;
    OsdTelemetry    telem;
    RoiUiState      roi_ui;
    bool              tracking_active = false;
    cv::Point2f       track_center    {};
    cv::Size          init_bbox_size  {};
};

// Triple-buffer: кадр + метаданные публикуются атомарно (track @25 Hz publish).
class DisplayHub {
public:
    using clock = std::chrono::steady_clock;

    // Возвращает true, если кадр опубликован (≈25 Hz).
    bool publish(const cv::Mat& main_rgb, uint64_t seq, clock::time_point now,
                 const DisplaySnapshot& meta);

    bool consume(DisplaySnapshot& out);

private:
    static constexpr int kSlots = 3;

    std::array<DisplaySnapshot, kSlots> slots_{};
    std::atomic<int>                    published_{-1};
    int                                 write_slot_ = 0;

    clock::time_point next_frame_publish_{};
    bool              frame_schedule_init_ = false;
};

}  // namespace bench
