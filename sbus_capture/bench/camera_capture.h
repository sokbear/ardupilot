#pragma once

#include <opencv2/core.hpp>

#include <cstdint>

namespace bench {

// Кадр с камеры FullHD (RGB888 или NV12).
struct CameraFrame {
    cv::Mat main_y;    // Y-плоскость (NV12 main)
    cv::Mat main_uv;   // UV NV12 (h/2 × w)
    cv::Mat main_rgb;  // RGB888/sRGB от ISP
    cv::Mat main_bgr;  // кэш BGR (ensureMainBgr)
    uint64_t seq = 0;
};

// Захват FullHD через libcamera (@kMainFps, см. config.h).
class CameraCapture {
public:
    CameraCapture() = default;
    ~CameraCapture();

    CameraCapture(const CameraCapture&) = delete;
    CameraCapture& operator=(const CameraCapture&) = delete;

    bool open();
    void close();

    bool grabLatest(CameraFrame& out, int timeout_ms = 2);
    void ensureMainBgr(CameraFrame& frame);

    bool isOpen() const { return open_; }
    bool hasRgbMainStream() const { return has_rgb_main_; }

private:
    struct Impl;
    Impl* impl_ = nullptr;
    bool  open_ = false;
    bool  has_rgb_main_ = false;
};

// FullHD BGR → PAL BGR (720×576, @kLoresFps).
cv::Mat makeLoresFrame(const cv::Mat& main_bgr);
cv::Mat makeLoresFromFrame(const CameraFrame& frame);

}  // namespace bench
