#include "servo_gimbal.h"

#include "config.h"

#include <algorithm>
#include <iostream>

namespace bench {

namespace {

int angleToPulseUs(float angle_deg)
{
    const float clamped =
        std::clamp(angle_deg, -kServoTravelDeg, kServoTravelDeg);
    const float norm = (clamped + kServoTravelDeg) / (2.0f * kServoTravelDeg);
    return static_cast<int>(kServoPulseMinUs +
                            norm * (kServoPulseMaxUs - kServoPulseMinUs));
}

}  // namespace

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
        writeChannelPulseUs(kPanServoChannel, angleToPulseUs(pan_deg_));
        writeChannelPulseUs(kTiltServoChannel, angleToPulseUs(tilt_deg_));
    }
}

void ServoGimbal::writeChannelPulseUs(int channel, int pulse_us)
{
    if (pca_) {
        pca_->setChannelPulseUs(channel, pulse_us);
    }
}

}  // namespace bench
