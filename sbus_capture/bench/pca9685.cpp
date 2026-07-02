#include "pca9685.h"

#include <cerrno>
#include <algorithm>
#include <cstring>
#include <iostream>

#if defined(__linux__)
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace bench {

namespace {

constexpr uint8_t kRegMode1   = 0x00;
constexpr uint8_t kRegMode2   = 0x01;
constexpr uint8_t kRegPrescale = 0xFE;
constexpr uint8_t kRegLed0    = 0x06;

constexpr uint8_t kMode1Sleep    = 0x10;
constexpr uint8_t kMode1Ai       = 0x20;
constexpr uint8_t kMode1Restart   = 0x80;
constexpr uint8_t kMode2OutDrv   = 0x04;

constexpr int kOscHz     = 25000000;
constexpr int kPwmSteps  = 4096;

}  // namespace

bool Pca9685::open(const char* i2c_bus_path, int address)
{
#if !defined(__linux__)
    (void)i2c_bus_path;
    (void)address;
    std::cerr << "pca9685: I2C only supported on Linux\n";
    return false;
#else
    fd_ = ::open(i2c_bus_path, O_RDWR);
    if (fd_ < 0) {
        std::cerr << "pca9685: cannot open " << i2c_bus_path << ": "
                  << std::strerror(errno) << '\n';
        return false;
    }

    if (::ioctl(fd_, I2C_SLAVE, address) < 0) {
        std::cerr << "pca9685: I2C_SLAVE 0x" << std::hex << address << std::dec
                  << " failed: " << std::strerror(errno) << '\n';
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    if (!writeReg(kRegMode2, kMode2OutDrv)) {
        close();
        return false;
    }

    if (!setPwmFrequencyHz(pwm_hz_)) {
        close();
        return false;
    }

    return true;
#endif
}

void Pca9685::close()
{
#if defined(__linux__)
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
}

bool Pca9685::setPwmFrequencyHz(int hz)
{
    if (hz < 24 || hz > 1600) {
        return false;
    }
    pwm_hz_ = hz;

#if !defined(__linux__)
    return false;
#else
    const int prescale =
        static_cast<int>(static_cast<double>(kOscHz) / (kPwmSteps * hz) + 0.5) - 1;
    if (prescale < 3 || prescale > 255) {
        std::cerr << "pca9685: invalid prescale " << prescale << '\n';
        return false;
    }

    uint8_t mode1 = 0;
    if (!readReg(kRegMode1, mode1)) {
        return false;
    }

    const uint8_t sleep_mode = (mode1 & ~kMode1Restart) | kMode1Sleep;
    if (!writeReg(kRegMode1, sleep_mode)) {
        return false;
    }

    if (!writeReg(kRegPrescale, static_cast<uint8_t>(prescale))) {
        return false;
    }

    if (!writeReg(kRegMode1, (mode1 & ~kMode1Sleep) | kMode1Ai)) {
        return false;
    }

    // Datasheet: min 500 us after wake before next change.
    usleep(500);

    return writeReg(kRegMode1, (mode1 & ~kMode1Sleep) | kMode1Ai | kMode1Restart);
#endif
}

bool Pca9685::setChannelPulseUs(int channel, int pulse_us)
{
    if (channel < 0 || channel > 15) {
        return false;
    }
    pulse_us = std::max(0, std::min(pulse_us, 1000000 / pwm_hz_));

    const double period_us = 1000000.0 / static_cast<double>(pwm_hz_);
    const auto off_ticks = static_cast<uint16_t>(
        static_cast<double>(pulse_us) / period_us * static_cast<double>(kPwmSteps));
    return setChannelRaw(channel, off_ticks);
}

bool Pca9685::setChannelRaw(int channel, uint16_t off_ticks)
{
#if !defined(__linux__)
    (void)channel;
    (void)off_ticks;
    return false;
#else
    if (fd_ < 0) {
        return false;
    }

    const uint8_t reg = static_cast<uint8_t>(kRegLed0 + 4 * channel);
    const uint8_t buf[5] = {
        reg,
        0x00,
        0x00,
        static_cast<uint8_t>(off_ticks & 0xFF),
        static_cast<uint8_t>((off_ticks >> 8) & 0x0F),
    };
    if (::write(fd_, buf, sizeof(buf)) != static_cast<ssize_t>(sizeof(buf))) {
        std::cerr << "pca9685: channel " << channel << " write failed\n";
        return false;
    }
    return true;
#endif
}

bool Pca9685::writeReg(uint8_t reg, uint8_t value)
{
#if !defined(__linux__)
    (void)reg;
    (void)value;
    return false;
#else
    if (fd_ < 0) {
        return false;
    }
    const uint8_t buf[2] = {reg, value};
    if (::write(fd_, buf, sizeof(buf)) != 2) {
        std::cerr << "pca9685: write reg 0x" << std::hex << static_cast<int>(reg)
                  << std::dec << " failed\n";
        return false;
    }
    return true;
#endif
}

bool Pca9685::readReg(uint8_t reg, uint8_t& value)
{
#if !defined(__linux__)
    (void)reg;
    (void)value;
    return false;
#else
    if (fd_ < 0) {
        return false;
    }
    if (::write(fd_, &reg, 1) != 1) {
        return false;
    }
    if (::read(fd_, &value, 1) != 1) {
        return false;
    }
    return true;
#endif
}

}  // namespace bench
