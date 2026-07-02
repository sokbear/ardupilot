#pragma once

#include <opencv2/core.hpp>

namespace bench {

// Вывод PAL-кадра 720×576 на composite (DRM/KMS, коннектор Composite-1).
class CompositeOutput {
public:
    CompositeOutput() = default;
    ~CompositeOutput();

    CompositeOutput(const CompositeOutput&) = delete;
    CompositeOutput& operator=(const CompositeOutput&) = delete;

    bool open();
    void close();

    bool present(const cv::Mat& lores_bgr);

    bool isOpen() const { return open_; }

private:
    struct Impl;
    Impl* impl_ = nullptr;
    bool  open_ = false;
};

}  // namespace bench
