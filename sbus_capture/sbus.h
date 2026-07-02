#pragma once
#include <cstdint>
#include <array>

// =====================================================================
// Кодек кадра SBUS (Futaba), совместимый с декодером ArduPilot.
//
// Кадр: 25 байт.
//   [0]      = 0x0F  (заголовок)
//   [1..22]  = 16 каналов по 11 бит, little-endian упаковка (176 бит = 22 байта)
//   [23]     = флаги: бит0 ch17, бит1 ch18, бит2 frame_lost, бит3 failsafe
//   [24]     = 0x00  (футер)
//
// Параметры линии: 100000 бод, 8E2, инвертированный сигнал
// (инверсия — на аппаратном уровне, см. serial_port / README).
// =====================================================================

namespace sbus {

constexpr int      FRAME_SIZE   = 25;
constexpr uint8_t  HEADER       = 0x0F;
constexpr uint8_t  FOOTER       = 0x00;
constexpr int      FLAGS_BYTE   = 23;
constexpr int      NUM_CHANNELS = 16;

constexpr uint8_t  FLAG_CH17       = 1 << 0;
constexpr uint8_t  FLAG_CH18       = 1 << 1;
constexpr uint8_t  FLAG_FRAME_LOST = 1 << 2;
constexpr uint8_t  FLAG_FAILSAFE   = 1 << 3;

struct Frame {
    std::array<uint16_t, NUM_CHANNELS> ch{}; // сырые значения 0..2047
    bool ch17 = false;
    bool ch18 = false;
    bool frame_lost = false;
    bool failsafe = false;
};

// Декодирует 25-байтный буфер. Возвращает false при неверном заголовке.
bool decode(const uint8_t buf[FRAME_SIZE], Frame& out);

// Кодирует Frame в 25-байтный буфер (заголовок/футер/флаги проставляются здесь).
void encode(const Frame& in, uint8_t buf[FRAME_SIZE]);

} // namespace sbus
