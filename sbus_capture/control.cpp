#include "control.h"
#include "config.h"
#include "pid.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <pthread.h>
#include <sched.h>

namespace {

inline uint16_t clamp_safe(double v)
{
    return static_cast<uint16_t>(
        std::clamp(v, double(cfg::SBUS_MIN_SAFE), double(cfg::SBUS_MAX_SAFE)));
}

inline int us_elapsed(Clock::time_point a, Clock::time_point b)
{
    return static_cast<int>(
        std::chrono::duration_cast<std::chrono::microseconds>(b - a).count());
}

} // namespace

void ControlThread::start()
{
    running_ = true;
    th_ = std::thread(&ControlThread::run, this);
}

void ControlThread::stop()
{
    running_ = false;
    if (th_.joinable()) {
        th_.join();
    }
}

void ControlThread::run()
{
    // realtime-приоритет для стабильного периода выдачи SBUS
    sched_param sp{};
    sp.sched_priority = 80;
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
        std::fprintf(stderr,
            "warn: TX-поток без SCHED_FIFO (нужен root/CAP_SYS_NICE)\n");
    }

    auto next = Clock::now();
    const auto period = std::chrono::microseconds(cfg::TX_PERIOD_US);
    while (running_) {
        step();
        next += period;
        std::this_thread::sleep_until(next);
    }
}

void ControlThread::step()
{
    static Pid pid_roll(cfg::KP_ROLL,  cfg::KI_ROLL,  cfg::KD_ROLL,  cfg::PID_OUTPUT_LIMIT);
    static Pid pid_pitch(cfg::KP_PITCH, cfg::KI_PITCH, cfg::KD_PITCH, cfg::PID_OUTPUT_LIMIT);
    static Clock::time_point last_step = Clock::now();

    const Clock::time_point now = Clock::now();
    const double dt = std::max(1e-3, us_elapsed(last_step, now) / 1e6);
    last_step = now;

    // --- свежие данные от потоков ---
    sbus::Frame rc;
    Clock::time_point rc_ts;
    const bool have_rc = rx_.get(rc, rc_ts);

    double ex, ey;
    bool detected;
    Clock::time_point vis_ts;
    vis_.get(ex, ey, detected, vis_ts);

    // --- БЕЗОПАСНОСТЬ: нет пульта -> failsafe pass ---
    const bool rc_stale = !have_rc || us_elapsed(rc_ts, now) > cfg::RX_TIMEOUT_US;
    if (rc_stale || (have_rc && rc.failsafe)) {
        mode_ = Mode::FAILSAFE_PASS;
        sbus::Frame out = have_rc ? rc : sbus::Frame{};
        out.failsafe = true;            // пусть сработает штатный failsafe FC
        uint8_t buf[sbus::FRAME_SIZE];
        sbus::encode(out, buf);
        out_port_.write_all(buf, sbus::FRAME_SIZE);
        pid_roll.reset();
        pid_pitch.reset();
        return;
    }

    // --- определение режима по тумблеру CH7 ---
    const bool capture_req = rc.ch[cfg::CH_CAPTURE] > cfg::CAPTURE_ON_THRESHOLD;
    const bool vision_fresh = detected && (us_elapsed(vis_ts, now) < cfg::VISION_LOST_US);

    Mode m = mode_.load();
    if (m == Mode::FAILSAFE_PASS) {
        m = Mode::TRANSPARENT; // связь восстановилась
    }

    if (m == Mode::CAPTURE) {
        bool exit_capture = !capture_req || !vision_fresh;

        if (cfg::ENABLE_STICK_OVERRIDE) {
            const int droll  = std::abs(int(rc.ch[cfg::CH_ROLL])  - int(cfg::SBUS_CENTER));
            const int dpitch = std::abs(int(rc.ch[cfg::CH_PITCH]) - int(cfg::SBUS_CENTER));
            if (droll > cfg::STICK_OVERRIDE_DEADBAND ||
                dpitch > cfg::STICK_OVERRIDE_DEADBAND) {
                exit_capture = true;
            }
        }

        if (exit_capture) {
            m = Mode::TRANSPARENT;
            pid_roll.reset();
            pid_pitch.reset();
        }
    } else { // TRANSPARENT
        if (capture_req && vision_fresh) {
            m = Mode::CAPTURE;
            pid_roll.reset();
            pid_pitch.reset();
        }
    }
    mode_ = m;

    // --- формирование выходного кадра ---
    sbus::Frame out = rc;   // по умолчанию всё от оператора (throttle/yaw/aux/flightmode)

    if (m == Mode::CAPTURE) {
        // Видео-PID: пиксельная ошибка -> отклонение наклона от центра.
        // Камера стабилизирована по pitch+roll => ex,ey ~ угловой пеленг.
        const double roll_out  = cfg::SIGN_ROLL_FROM_EX  * pid_roll.update(ex, dt);
        const double pitch_out = cfg::SIGN_PITCH_FROM_EY * pid_pitch.update(ey, dt);

        // (При установке камеры под азимутом != 0 заменить на поворот 2x2:
        //   roll_out  = cos*PIDx - sin*PIDy;  pitch_out = sin*PIDx + cos*PIDy;)

        // Зажимаем ТОЛЬКО сгенерированные малиной каналы (защита от "мягкого
        // failsafe" FC по каналам 1-4 <= 875us). Каналы оператора (throttle/
        // yaw/aux) пробрасываются как есть, иначе сломается ход стиков и
        // arm/disarm стиками.
        out.ch[cfg::CH_ROLL]  = clamp_safe(double(cfg::SBUS_CENTER) + roll_out);
        out.ch[cfg::CH_PITCH] = clamp_safe(double(cfg::SBUS_CENTER) + pitch_out);
    }

    out.failsafe = false;   // сюда попадаем только при живой связи (см. выше)
    out.frame_lost = false;

    uint8_t buf[sbus::FRAME_SIZE];
    sbus::encode(out, buf);
    out_port_.write_all(buf, sbus::FRAME_SIZE);
}
