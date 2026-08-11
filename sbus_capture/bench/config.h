#pragma once

// Стенд этап 1: Pi 5 + камера на гимбале MG946R + PAL composite.
namespace bench {

// Версия сборки bench — увеличивать при каждом изменении кода перед деплоем.
inline constexpr const char* kBenchVersion = "1.0.9";

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

// Служебная статистика (PERF) на HDMI-консоль: прямая запись в TTY,
// не зависит от journald. Пустая строка пути = выключено.
inline constexpr const char* kConsoleStatsPath = "/dev/tty1";

constexpr float kVelocityEmaAlpha = 0.35f;

// Камера: меньше буферов → ниже задержка (DMA-буферы libcamera).
constexpr int kCameraBufferCount   = 3;
constexpr int kCameraPublishSlots  = 3;  // triple-buffer: без clone 1080p в grabLatest

// IMX708 RGB888/sRGB: байты в буфере = RGB. При main=NV12 не используется.
// false = камера отдаёт BGR888 (Шаг 5); true = RGB888 → конверсия в трекере.
constexpr bool kCameraRgb888BytesAreRgb = false;

// === NanoTrack (обучаемое ядро) ===
inline constexpr const char* kNanoBackbonePath = "models/nanotrack_backbone_sim.onnx";
inline constexpr const char* kNanoNeckheadPath = "models/nanotrack_head_sim.onnx";
constexpr float kNanoScoreThreshold = 0.60f;

// Пределы линейного масштаба рамки относительно исходного ROI,
// по метрике площади: s = sqrt(S/S0). Грубые перила; основная защита
// от дрейфа — NCC-верификатор.
constexpr float kScaleMinRatio         = 0.10f;
constexpr float kScaleMaxRatio         = 8.0f;
// Предел стороны рамки как доли стороны кадра.
constexpr float kBboxMaxFrameSideRatio = 0.85f;
constexpr bool  kPyramidEnable               = true;
constexpr float kPyramidUpSearchPx           = 640.0f;
constexpr float kPyramidDownSearchPx         = 440.0f;
constexpr int   kPyramidSwitchCooldownFrames = 30;
constexpr float kTrackMinVisibleFraction = 0.45f;
constexpr double kTrackReacquireTimeoutSec = 2.0;
constexpr double kTrackSearchGiveUpSec    = 3.0;

// Мышь: выделение ROI на PAL-экране (lores 720×576).
constexpr int kRoiMinSelectSidePx = 24;
constexpr const char* kI2cBusPath     = "/dev/i2c-1";
constexpr int         kPca9685Address = 0x40;
constexpr int         kPanServoChannel  = 0;
constexpr int         kTiltServoChannel = 1;
constexpr int         kPca9685PwmHz     = 50;

constexpr int kServoPulseMinUs = 1000;
constexpr int kServoPulseMaxUs = 2000;
constexpr float kServoTravelDeg = 90.0f;

// PID: ошибка в пикселях -> шаг угла за кадр (градусы).
constexpr double kPanPidKp  = 0.04;
constexpr double kPanPidKi  = 0.002;
constexpr double kPanPidKd  = 0.01;
constexpr double kPanPidMaxStepDeg = 3.0;

constexpr double kTiltPidKp  = 0.04;
constexpr double kTiltPidKi  = 0.002;
constexpr double kTiltPidKd  = 0.01;
constexpr double kTiltPidMaxStepDeg = 3.0;

// === TargetVerifier: независимая проверка цели поверх NanoTrack ===
constexpr int   kVerifyCanonSize          = 96;
constexpr float kVerifyTemplateAdaptAlpha = 0.05f;
constexpr float kVerifySearchInflate      = 1.30f;
constexpr float kVerifyNccDropMargin      = 0.30f;
constexpr float kVerifyBaselineAlpha      = 0.05f;
constexpr int   kVerifyFailStreak         = 45;
constexpr float kVerifyHardFloor          = 0.05f;
constexpr int   kVerifyHardFailStreak     = 12;
// 0 = выкл; 1 = теневой (только лог); 2 = боевой.
constexpr int kVerifyMode = 2;

}  // namespace bench
