#include "display_hub.h"

namespace bench {

bool DisplayHub::publish(const cv::Mat& main_rgb, uint64_t seq, clock::time_point now,
                         const DisplaySnapshot& meta)
{
    if (main_rgb.empty()) {
        return false;
    }

    if (!frame_schedule_init_) {
        frame_schedule_init_ = true;
        next_frame_publish_  = now;
    }
    if (now < next_frame_publish_) {
        return false;
    }
    next_frame_publish_ = now + std::chrono::milliseconds(1000 / kLoresFps);

    const int slot = write_slot_;
    if (slots_[slot].main_rgb.rows != main_rgb.rows || slots_[slot].main_rgb.cols != main_rgb.cols ||
        slots_[slot].main_rgb.type() != main_rgb.type()) {
        slots_[slot].main_rgb.create(main_rgb.size(), main_rgb.type());
    }
    main_rgb.copyTo(slots_[slot].main_rgb);
    slots_[slot].seq             = seq;
    slots_[slot].det             = meta.det;
    slots_[slot].telem           = meta.telem;
    slots_[slot].roi_ui          = meta.roi_ui;
    slots_[slot].tracking_active = meta.tracking_active;
    slots_[slot].track_center    = meta.track_center;
    slots_[slot].init_bbox_size  = meta.init_bbox_size;

    published_.store(slot, std::memory_order_release);
    write_slot_ = (slot + 1) % kSlots;
    return true;
}

bool DisplayHub::consume(DisplaySnapshot& out)
{
    const int idx = published_.load(std::memory_order_acquire);
    if (idx < 0 || slots_[idx].main_rgb.empty()) {
        return false;
    }

    const DisplaySnapshot& src = slots_[idx];
    out.main_rgb        = src.main_rgb.clone();
    out.seq             = src.seq;
    out.det             = src.det;
    out.telem           = src.telem;
    out.roi_ui          = src.roi_ui;
    out.tracking_active = src.tracking_active;
    out.track_center    = src.track_center;
    out.init_bbox_size  = src.init_bbox_size;
    return true;
}

}  // namespace bench
