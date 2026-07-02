#pragma once

#include "pca9685.h"

#include <memory>

namespace bench {

// Двухосевой гимбал MG946R через PCA9685 (I2C PWM).
class ServoGimbal {
public:
    bool open();
    void close();

    void setAnglesDeg(float pan_deg, float tilt_deg);

    float panDeg() const { return pan_deg_; }
    float tiltDeg() const { return tilt_deg_; }

    bool hardwareActive() const { return hardware_active_; }

private:
    void writeChannelPulseUs(int channel, int pulse_us);

    std::unique_ptr<Pca9685> pca_;
    bool  hardware_active_ = false;
    float pan_deg_         = 0.0f;
    float tilt_deg_        = 0.0f;
};

}  // namespace bench
