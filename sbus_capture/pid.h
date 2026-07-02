#pragma once
#include <algorithm>

// =====================================================================
// Простой PID-регулятор с ограничением выхода и анти-виндапом.
// Используется в видео-контуре: вход — пиксельная ошибка, выход —
// отклонение наклона (в единицах SBUS) от центра.
// =====================================================================

class Pid {
public:
    Pid(double kp, double ki, double kd, double out_limit)
        : kp_(kp), ki_(ki), kd_(kd), out_limit_(out_limit) {}

    void reset() {
        integral_ = 0.0;
        prev_error_ = 0.0;
        has_prev_ = false;
    }

    // error — текущая ошибка, dt — шаг в секундах.
    double update(double error, double dt) {
        if (dt <= 0.0) dt = 1e-3;

        double p = kp_ * error;

        integral_ += error * dt;
        // анти-виндап: ограничиваем интегральный вклад пределом выхода
        if (ki_ > 0.0) {
            const double i_limit = out_limit_ / ki_;
            integral_ = std::clamp(integral_, -i_limit, i_limit);
        }
        double i = ki_ * integral_;

        double d = 0.0;
        if (has_prev_) {
            d = kd_ * (error - prev_error_) / dt;
        }
        prev_error_ = error;
        has_prev_ = true;

        double out = p + i + d;
        return std::clamp(out, -out_limit_, out_limit_);
    }

private:
    double kp_, ki_, kd_, out_limit_;
    double integral_ = 0.0;
    double prev_error_ = 0.0;
    bool has_prev_ = false;
};
