#include "vision.h"
#include <chrono>

void VisionThread::start()
{
    running_ = true;
    th_ = std::thread(&VisionThread::run, this);
}

void VisionThread::stop()
{
    running_ = false;
    if (th_.joinable()) {
        th_.join();
    }
}

void VisionThread::run()
{
    // ------------------------------------------------------------------
    // TODO: заменить тело цикла на реальный пайплайн зрения:
    //
    //   1. захватить кадр с камеры (V4L2 / libcamera / OpenCV VideoCapture);
    //   2. найти метку (AprilTag/ArUco/цветовой/шаблонный детектор);
    //   3. вычислить смещение центра метки от центра кадра (ex, ey);
    //   4. out_.set(ex, ey, true)  либо  out_.set(0, 0, false), если не видно.
    //
    // Частота ~30-60 Гц. Этот поток НЕ таймингокритичен (в отличие от TX).
    // ------------------------------------------------------------------
    using namespace std::chrono_literals;
    while (running_) {
        // Заглушка: метка не обнаружена.
        out_.set(0.0, 0.0, /*detected=*/false);
        std::this_thread::sleep_for(20ms); // ~50 Гц
    }
}
