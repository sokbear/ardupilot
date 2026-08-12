#include "gimbal_tracker.h"

#include "config.h"

#include <cmath>
#include <iostream>

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
    smoothed_ex_      = 0.0;
    smoothed_ey_      = 0.0;
    has_smoothed_     = false;
    return_to_center_ = true;
    cmd_accum_sec_    = 0.0;
    gimbal_.setAnglesDeg(0.0f, 0.0f);
}

void GimbalTracker::armTracking()
{
    return_to_center_ = false;
    pan_pid_.reset();
    tilt_pid_.reset();
    smoothed_ex_  = 0.0;
    smoothed_ey_  = 0.0;
    has_smoothed_ = false;
    cmd_accum_sec_ = 0.0;
}

GimbalState GimbalTracker::update(const MarkerDetection& det, double dt_sec)
{
    GimbalState state;
    state.pan_deg  = gimbal_.panDeg();
    state.tilt_deg = gimbal_.tiltDeg();

    if (return_to_center_) {
        gimbal_.setAnglesDeg(0.0f, 0.0f);
        pan_pid_.reset();
        tilt_pid_.reset();
        has_smoothed_ = false;
        state.pan_deg    = 0.0f;
        state.tilt_deg   = 0.0f;
        state.tracking   = false;
        return state;
    }

    // Управляем только при уверенном Live-захвате; иначе держим угол и сбрасываем PID.
    if (!det.valid || !det.capturing ||
        det.track != MarkerTrackMode::Live) {
        pan_pid_.reset();
        tilt_pid_.reset();
        has_smoothed_ = false;
        state.tracking = false;
        return state;
    }

    double ex = det.ex;
    double ey = det.ey;
    if (std::abs(ex) < kGimbalPidDeadbandPx) {
        ex = 0.0;
    }
    if (std::abs(ey) < kGimbalPidDeadbandPx) {
        ey = 0.0;
    }

    if (!has_smoothed_) {
        smoothed_ex_  = ex;
        smoothed_ey_  = ey;
        has_smoothed_ = true;
    } else {
        const double a = static_cast<double>(kGimbalErrorEmaAlpha);
        smoothed_ex_ = smoothed_ex_ * (1.0 - a) + ex * a;
        smoothed_ey_ = smoothed_ey_ * (1.0 - a) + ey * a;
    }

    if (smoothed_ex_ == 0.0 && smoothed_ey_ == 0.0) {
        pan_pid_.reset();
        tilt_pid_.reset();
        state.tracking = true;
        return state;
    }

    // ex>0 — метка правее центра -> pan по часовой (+).
    // ey>0 — метка ниже центра -> tilt вниз (−).
    // Выход PID — требуемая угловая поправка к цели (град), позиционный контур.
    const float pan_corr =
        static_cast<float>(pan_pid_.update(smoothed_ex_, dt_sec));
    const float tilt_corr =
        static_cast<float>(tilt_pid_.update(-smoothed_ey_, dt_sec));

    const float pan  = gimbal_.panDeg() + pan_corr;
    const float tilt = gimbal_.tiltDeg() + tilt_corr;

    cmd_accum_sec_ += dt_sec;
    if (cmd_accum_sec_ >= kGimbalCommandPeriodSec) {
        cmd_accum_sec_ = 0.0;
        gimbal_.setAnglesDeg(pan, tilt);
    }

    if (++telem_counter_ >= 10) {
        telem_counter_ = 0;
        std::cout << "bench: gimbal ex=" << det.ex << " ey=" << det.ey
                  << " sx=" << smoothed_ex_ << " sy=" << smoothed_ey_
                  << " pan_step=" << pan_corr << " tilt_step=" << tilt_corr
                  << " pan=" << pan << " tilt=" << tilt << "\n";
    }

    state.pan_deg  = gimbal_.panDeg();
    state.tilt_deg = gimbal_.tiltDeg();
    state.tracking = true;
    return state;
}

}  // namespace bench
