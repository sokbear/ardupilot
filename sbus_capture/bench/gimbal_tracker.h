#pragma once

#include "track_result.h"
#include "pid.h"
#include "servo_gimbal.h"

namespace bench {

struct GimbalState {
    float pan_deg  = 0.0f;
    float tilt_deg = 0.0f;
    bool  tracking = false;
};

// Визуальное слежение: ex/ey -> углы pan/tilt гимбала.
class GimbalTracker {
public:
    explicit GimbalTracker(ServoGimbal& gimbal);

    GimbalState update(const MarkerDetection& det, double dt_sec);

    void reset();

private:
    ServoGimbal& gimbal_;
    Pid          pan_pid_;
    Pid          tilt_pid_;
};

}  // namespace bench
