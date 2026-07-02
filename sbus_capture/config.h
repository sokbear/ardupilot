#pragma once
#include <cstdint>

// =====================================================================
// Конфигурация системы "SBUS capture" (man-in-the-middle на Raspberry Pi)
//
//   TBS Nano (RX) --SBUS--> [ Pi ] --SBUS--> Полётник (ArduPilot)
//
// Все каналы индексируются от 0: ch[0] == CH1 на пульте.
// =====================================================================

namespace cfg {

// --- Назначение каналов (0-based индексы) ---
constexpr int CH_ROLL       = 0;  // CH1 - крен
constexpr int CH_PITCH      = 1;  // CH2 - тангаж
constexpr int CH_THROTTLE   = 2;  // CH3 - газ/высота (всегда от оператора)
constexpr int CH_YAW        = 3;  // CH4 - рыскание   (всегда от оператора)
constexpr int CH_FLIGHTMODE = 4;  // CH5 - режим полёта коптера
constexpr int CH_CAPTURE    = 6;  // CH7 - тумблер режима "захват"
// CH8+ (index 7..) - команды/механизмы, всегда пробрасываются от оператора.

// --- Порог тумблера захвата (в сырых единицах SBUS, 0..2047) ---
// Захват активен, когда CH7 выше порога.
constexpr uint16_t CAPTURE_ON_THRESHOLD = 1200;

// --- Сырые значения SBUS (11 бит: 0..2047) ---
// Маппинг ArduPilot: us = raw * 1000/1600 + 875  => raw 200..1800 == 1000..2000us.
// "Мягкий failsafe" FC срабатывает при us <= 875, т.е. при raw <= 0.
// Поэтому управляемые каналы держим в безопасном коридоре с запасом.
constexpr uint16_t SBUS_CENTER   = 1024;  // ~1515us
constexpr uint16_t SBUS_MIN_SAFE = 200;   // ~1000us
constexpr uint16_t SBUS_MAX_SAFE = 1800;  // ~2000us

// --- Тайминги ---
constexpr int    TX_PERIOD_US     = 14000;  // период выдачи кадра в FC (~14 мс)
constexpr int    RX_TIMEOUT_US    = 60000;  // нет кадров от RX дольше -> failsafe pass
constexpr int    VISION_LOST_US   = 800000; // метка не видна дольше -> выход из захвата

// --- Видео-контур: усиление "пиксель -> наклон" (в единицах SBUS на пиксель) ---
// Подбирается на стенде. ex,ey - смещение метки от центра кадра в пикселях.
constexpr double KP_ROLL  = 1.2;
constexpr double KI_ROLL  = 0.0;
constexpr double KD_ROLL  = 0.25;
constexpr double KP_PITCH = 1.2;
constexpr double KI_PITCH = 0.0;
constexpr double KD_PITCH = 0.25;

// Ограничение выхода видео-PID (отклонение от центра в единицах SBUS).
constexpr double PID_OUTPUT_LIMIT = 600.0;

// --- Калибровка осей "картинка -> (roll, pitch)" ---
// Зависит от установки камеры по азимуту относительно носа коптера.
// Простейший случай: знаки. Полный случай - поворот 2x2 (см. control.cpp).
constexpr double SIGN_ROLL_FROM_EX  = +1.0;
constexpr double SIGN_PITCH_FROM_EY = +1.0;

// --- Опциональный возврат управления по стику (по решению - выключен) ---
// Если true: резкое отклонение оператором roll/pitch выводит из захвата.
constexpr bool   ENABLE_STICK_OVERRIDE   = false;
constexpr uint16_t STICK_OVERRIDE_DEADBAND = 250; // raw, отклонение от центра

} // namespace cfg
