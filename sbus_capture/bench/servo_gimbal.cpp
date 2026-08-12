#include "servo_gimbal.h"

#include "config.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace bench {

int ServoGimbal::pulseUsForAngleDeg(float angle_deg)
{
    const float clamped =
        std::clamp(angle_deg, -kServoTravelDeg, kServoTravelDeg);
    const float half_span =
        static_cast<float>(kServoPulseMaxUs - kServoPulseMinUs) * 0.5f;
    const float us_per_deg = half_span / kServoTravelDeg;
    // +угол → меньший PWM (900 мкс), −угол → больший (2100 мкс).
    const float pulse =
        static_cast<float>(kServoPulseCenterUs) - clamped * us_per_deg;
    return static_cast<int>(std::lround(pulse));
}

bool ServoGimbal::open()
{
    auto driver = std::make_unique<Pca9685>();
    if (!driver->open(kI2cBusPath, kPca9685Address)) {
        std::cerr << "gimbal: PCA9685 not found on " << kI2cBusPath
                  << " addr=0x" << std::hex << kPca9685Address << std::dec << '\n';
        hardware_active_ = false;
        return true;
    }

    pca_             = std::move(driver);
    hardware_active_ = true;
    setAnglesDeg(0.0f, 0.0f);
    std::cout << "gimbal: MG946R via PCA9685 ch" << kPanServoChannel
              << " (pan) ch" << kTiltServoChannel << " (tilt)\n";
    return true;
}

void ServoGimbal::close()
{
    if (pca_) {
        writeChannelPulseUs(kPanServoChannel, 0);
        writeChannelPulseUs(kTiltServoChannel, 0);
        pca_->close();
        pca_.reset();
    }
    hardware_active_ = false;
}

void ServoGimbal::setAnglesDeg(float pan_deg, float tilt_deg)
{
    pan_deg_  = std::clamp(pan_deg, -kServoTravelDeg, kServoTravelDeg);
    tilt_deg_ = std::clamp(tilt_deg, -kServoTravelDeg, kServoTravelDeg);

    if (pca_) {
        const int pan_pulse  = pulseUsForAngleDeg(pan_deg_);
        const int tilt_pulse = pulseUsForAngleDeg(tilt_deg_);
        writeChannelPulseUs(kPanServoChannel, pan_pulse);
        writeChannelPulseUs(kTiltServoChannel, tilt_pulse);
    }
}

void ServoGimbal::writeChannelPulseUs(int channel, int pulse_us)
{
    if (pca_) {
        pca_->setChannelPulseUs(channel, pulse_us);
    }
}

}  // namespace bench
