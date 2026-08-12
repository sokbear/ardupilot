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

    // Разрешить PID после успешного init ROI (после ПКМ — удерживаем центр).
    void armTracking();

private:
    ServoGimbal& gimbal_;
    Pid          pan_pid_;
    Pid          tilt_pid_;
    double       smoothed_ex_        = 0.0;
    double       smoothed_ey_        = 0.0;
    bool         has_smoothed_       = false;
    bool         return_to_center_   = true;
    int          telem_counter_      = 0;
    double       cmd_accum_sec_      = 0.0;   // накопитель времени между командами серве
};

}  // namespace bench
