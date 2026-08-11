#pragma once

#include "config.h"

#include <cstdint>

namespace bench {

// Драйвер PCA9685 (16-канальный I2C PWM, 50 Hz для серво).
class Pca9685 {
public:
    bool open(const char* i2c_bus_path, int address);
    void close();

    bool isOpen() const { return fd_ >= 0; }

    bool setPwmFrequencyHz(int hz);
    bool setChannelPulseUs(int channel, int pulse_us);

private:
    bool writeReg(uint8_t reg, uint8_t value);
    bool readReg(uint8_t reg, uint8_t& value);
    bool setChannelRaw(int channel, uint16_t off_ticks);

    int fd_ = -1;
    int pwm_hz_ = kPca9685PwmHz;
};

}  // namespace bench
