#include "gimbal_tracker.h"

#include "config.h"

namespace bench {

GimbalTracker::GimbalTracker(ServoGimbal& gimbal)
    : gimbal_(gimbal),
      pan_pid_(kPanPidKp, kPanPidKi, kPanPidKd, kPanPidMaxStepDeg),
      tilt_pid_(kTiltPidKp, kTiltPidKi, kTiltPidKd, kTiltPidMaxStepDeg)
{
}

void GimbalTracker::reset()
{
    pan_pid_.reset();
    tilt_pid_.reset();
    gimbal_.setAnglesDeg(0.0f, 0.0f);
}

GimbalState GimbalTracker::update(const MarkerDetection& det, double dt_sec)
{
    GimbalState state;
    state.pan_deg  = gimbal_.panDeg();
    state.tilt_deg = gimbal_.tiltDeg();

    if (!det.valid) {
        state.tracking = false;
        return state;
    }

    // ex>0 — метка правее центра -> pan вправо (азимут).
    // ey>0 — метка ниже центра -> tilt вниз.
    const float pan_step  = static_cast<float>(pan_pid_.update(det.ex, dt_sec));
    const float tilt_step = static_cast<float>(tilt_pid_.update(det.ey, dt_sec));

    const float pan  = gimbal_.panDeg() + pan_step;
    const float tilt = gimbal_.tiltDeg() + tilt_step;
    gimbal_.setAnglesDeg(pan, tilt);

    state.pan_deg  = gimbal_.panDeg();
    state.tilt_deg = gimbal_.tiltDeg();
    state.tracking = true;
    return state;
}

}  // namespace bench
