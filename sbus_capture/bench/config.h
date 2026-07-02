#pragma once

// Стенд этап 1: Pi 5 + камера на гимбале MG946R + PAL composite.
namespace bench {

constexpr int kMainWidth  = 1920;
constexpr int kMainHeight = 1080;
constexpr int kMainFps    = 60;

// Camera Module 3 (IMX708): непрерывный AF через libcamera controls::AfMode.
constexpr bool kCameraAfContinuous = true;

constexpr int kLoresWidth  = 720;
constexpr int kLoresHeight = 576;
constexpr int kLoresFps    = 25;

// OSD
constexpr int kOsdMarkerBoxThickness = 1;
// BGR в OpenCV; composite DRM (RG24) получает RGB — конверсия в composite_out::present.
constexpr int kOsdColorCapturedB = 0;
constexpr int kOsdColorCapturedG = 0;
constexpr int kOsdColorCapturedR = 255;
constexpr int kOsdColorLostB     = 255;
constexpr int kOsdColorLostG     = 0;
constexpr int kOsdColorLostR     = 0;

constexpr float kVelocityEmaAlpha = 0.35f;  // сглаживание скорости цели (MOSSE)

// Камера: меньше буферов → ниже задержка (DMA-буферы libcamera).
constexpr int kCameraBufferCount   = 3;
constexpr int kCameraPublishSlots  = 3;  // triple-buffer: без clone 1080p в grabLatest

// IMX708 RGB888/sRGB: байты в буфере = RGB. При main=NV12 не используется.
constexpr bool kCameraRgb888BytesAreRgb = true;

constexpr int kMosseMaxMissFrames   = 18;
constexpr int   kTrackLossClearFrames = 45;

// Масштаб: matchTemplate в ROI (cap + downscale), refine @ PAL 25 Hz; MOSSE @ 60 Hz FullHD.
constexpr float kScaleRoiWindowFactor = 2.0f;
constexpr int   kScaleRoiMaxSide      = 384;   // cap окна поиска (px)
constexpr int   kScaleSearchMaxSide   = 256;   // matchTemplate на уменьшенном ROI
constexpr int   kScaleLocalSteps      = 5;
constexpr float kScaleLocalMin        = 0.65f;
constexpr float kScaleLocalMax        = 1.85f;
constexpr float kScaleGrowAlpha       = 0.72f;
constexpr float kScaleShrinkAlpha     = 0.35f;
constexpr float kScaleDisplayAlpha    = 0.62f;  // догон target между PAL-тиками (~40 ms)
constexpr float kScaleMinRatio        = 0.35f;
constexpr float kScaleMaxRatio        = 2.5f;
constexpr int   kScaleMinSidePx       = 12;
constexpr float kScaleMinResponse     = 0.28f;
constexpr float kScaleReinitThreshold = 0.14f;

// Мышь: выделение ROI на PAL-экране (lores 720×576).
constexpr int kRoiMinSelectSidePx = 24;  // мин. сторона рамки выделения, px
constexpr const char* kI2cBusPath     = "/dev/i2c-1";
constexpr int         kPca9685Address = 0x40;  // адрес по умолчанию (A0–A5 = GND)
constexpr int         kPanServoChannel  = 0;   // канал PCA9685: азимут
constexpr int         kTiltServoChannel = 1;   // канал PCA9685: наклон
constexpr int         kPca9685PwmHz     = 50;

constexpr int kServoPulseMinUs = 1000;
constexpr int kServoPulseMaxUs = 2000;
constexpr float kServoTravelDeg = 90.0f;  // ±90° от центра (подстроить механику)

// PID: ошибка в пикселях -> шаг угла за кадр (градусы).
constexpr double kPanPidKp  = 0.04;
constexpr double kPanPidKi  = 0.002;
constexpr double kPanPidKd  = 0.01;
constexpr double kPanPidMaxStepDeg = 3.0;

constexpr double kTiltPidKp  = 0.04;
constexpr double kTiltPidKi  = 0.002;
constexpr double kTiltPidKd  = 0.01;
constexpr double kTiltPidMaxStepDeg = 3.0;

}  // namespace bench
