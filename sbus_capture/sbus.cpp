#include "sbus.h"

namespace sbus {

bool decode(const uint8_t buf[FRAME_SIZE], Frame& out)
{
    if (buf[0] != HEADER) {
        return false;
    }

    // Распаковка 16 каналов по 11 бит из байтов [1..22].
    uint32_t acc = 0;
    int bits = 0;
    int ch = 0;
    for (int i = 1; i <= 22 && ch < NUM_CHANNELS; ++i) {
        acc |= static_cast<uint32_t>(buf[i]) << bits;
        bits += 8;
        while (bits >= 11 && ch < NUM_CHANNELS) {
            out.ch[ch++] = static_cast<uint16_t>(acc & 0x07FF);
            acc >>= 11;
            bits -= 11;
        }
    }

    const uint8_t f = buf[FLAGS_BYTE];
    out.ch17       = f & FLAG_CH17;
    out.ch18       = f & FLAG_CH18;
    out.frame_lost = f & FLAG_FRAME_LOST;
    out.failsafe   = f & FLAG_FAILSAFE;
    return true;
}

void encode(const Frame& in, uint8_t buf[FRAME_SIZE])
{
    for (int i = 0; i < FRAME_SIZE; ++i) {
        buf[i] = 0;
    }
    buf[0] = HEADER;

    // Упаковка 16 каналов по 11 бит в байты [1..22].
    uint32_t acc = 0;
    int bits = 0;
    int bytei = 1;
    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
        acc |= static_cast<uint32_t>(in.ch[ch] & 0x07FF) << bits;
        bits += 11;
        while (bits >= 8) {
            buf[bytei++] = static_cast<uint8_t>(acc & 0xFF);
            acc >>= 8;
            bits -= 8;
        }
    }
    // 16*11 = 176 бит = 22 байта ровно => bits == 0, bytei == 23.

    uint8_t f = 0;
    if (in.ch17)       f |= FLAG_CH17;
    if (in.ch18)       f |= FLAG_CH18;
    if (in.frame_lost) f |= FLAG_FRAME_LOST;
    if (in.failsafe)   f |= FLAG_FAILSAFE;
    buf[FLAGS_BYTE] = f;
    buf[24]         = FOOTER;
}

} // namespace sbus
