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

// OSD: сообщение о потере метки по центру экрана.
constexpr double kOsdLostMessageSec   = 3.0;
constexpr double kOsdLostMessageScale = 1.35;
constexpr int    kOsdLostMessageThick = 3;
constexpr int    kOsdMarkerBoxThickness = 1;
// BGR в OpenCV; composite DRM (RG24) получает RGB — конверсия в composite_out::present.
constexpr int kOsdColorCapturedB = 0;
constexpr int kOsdColorCapturedG = 0;
constexpr int kOsdColorCapturedR = 255;
constexpr int kOsdColorLostB     = 255;
constexpr int kOsdColorLostG     = 0;
constexpr int kOsdColorLostR     = 0;
constexpr int kOsdColorCursorB   = 255;
constexpr int kOsdColorCursorG   = 0;
constexpr int kOsdColorCursorR   = 0;

constexpr float kVelocityEmaAlpha = 0.35f;

// Камера: меньше буферов → ниже задержка (DMA-буферы libcamera).
constexpr int kCameraBufferCount   = 3;
constexpr int kCameraPublishSlots  = 3;  // triple-buffer: без clone 1080p в grabLatest

// IMX708 RGB888/sRGB: байты в буфере = RGB. При main=NV12 не используется.
constexpr bool kCameraRgb888BytesAreRgb = true;

// Трекер: init-сегментация + сегментация в зоне при треке (масштаб и центр).
constexpr int   kTemplateRefMaxSide     = 96;
constexpr int   kTrackExtractMaxSide    = 128;
constexpr int   kNccSearchMaxSide       = 96;
constexpr float kWeightSigmaFactor      = 0.28f;
constexpr float kSearchWindowFactor     = 1.08f;
constexpr float kSearchMaxDriftFactor   = 0.16f;
constexpr float kLatchMaxPredDistFactor = 0.12f;
constexpr int   kCenterLockFrames       = 10;
constexpr int   kCenterLockTemplate     = 14;
constexpr float kSegMaxZoneFillRatio    = 0.88f;
constexpr float kSegMinRoiSideRatio     = 0.18f;
constexpr float kSegMinRoiAreaRatio     = 0.04f;
constexpr float kSegPreferDistFactor    = 0.45f;
constexpr float kSegMinCircularity      = 0.55f;
constexpr float kSegMinZoneAreaFactor   = 0.002f;
constexpr int   kSegMinAbsAreaPx        = 36;
constexpr float kSegTrackZoneFactor     = 1.55f;
constexpr float kSegTrackZoneFactorLarge = 1.85f;
constexpr float kSegLargeScaleThreshold = 2.2f;
constexpr float kSegReacquireZoneFactor = 2.4f;
constexpr float kSegMaxCenterStepFactor = 0.32f;
constexpr float kScaleSmoothAlpha       = 0.32f;
constexpr float kScaleMaxStepRatio      = 0.14f;
constexpr float kScaleMeasureMaxJump    = 1.28f;
constexpr float kScaleMinRatio          = 0.25f;
constexpr float kScaleMaxRatio          = 4.5f;
constexpr float kBboxMaxFrameSideRatio  = 0.42f;
constexpr float kMinContrastForSeg      = 22.0f;
constexpr float kSearchWindowTemplate   = 1.14f;
constexpr float kSearchDriftTemplate    = 0.22f;
constexpr float kScaleProbeDownMul      = 0.90f;
constexpr float kScaleProbeUpMul        = 1.12f;
constexpr int   kScaleProbeEveryNFrames = 6;
constexpr float kTrackMinResponse       = 0.32f;
constexpr float kTrackMinResponseTemplate = 0.20f;
constexpr int   kVerifyFailToSearch     = 18;
constexpr int   kVerifyFailTemplate     = 35;
constexpr float kSegInitMaxCenterShift  = 0.20f;
constexpr float kSegInitMinAreaRatio    = 0.06f;
constexpr float kTrackMinVisibleFraction = 0.45f;
constexpr int   kRelocateEveryNFrames   = 2;
constexpr double kTrackReacquireTimeoutSec = 2.0;
constexpr double kTrackSearchGiveUpSec    = 3.0;
constexpr int    kScaleMinSidePx        = 12;
// MOSSE (низкий контраст): своё окно фильтра, отдельно от scale_ на OSD.
constexpr float  kMosseReinitScaleThreshold = 0.15f;
constexpr int    kMosseFailLimit            = 35;
constexpr int    kMosseSoftFailFrames       = 12;
constexpr int    kCenterLockMosse           = 0;
constexpr float  kMosseMaxCenterJumpFactor  = 0.28f;
constexpr float  kMosseScaleRoiFactor       = 2.0f;
constexpr int    kMosseScaleRoiMaxSide      = 384;
constexpr int    kMosseScaleSearchMaxSide   = 256;
constexpr int    kMosseScaleLocalSteps      = 5;
constexpr float  kMosseScaleLocalMin        = 0.65f;
constexpr float  kMosseScaleLocalMax        = 1.85f;
constexpr float  kMosseScaleGrowAlpha       = 0.55f;
constexpr float  kMosseScaleShrinkAlpha     = 0.35f;
constexpr float  kMosseScaleMinResponse     = 0.12f;

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
