#pragma once
#include <mutex>
#include <chrono>
#include "sbus.h"

// =====================================================================
// Потокобезопасный обмен между потоками RX / Vision / TX.
// Каждый блок защищён своим мьютексом; данные копируются целиком.
// =====================================================================

using Clock = std::chrono::steady_clock;

// Последний валидный кадр от приёмника (обновляет RX-поток).
struct RxShared {
    void set(const sbus::Frame& f) {
        std::lock_guard<std::mutex> lk(m_);
        frame_ = f;
        ts_ = Clock::now();
        valid_ = true;
    }
    // Возвращает true, если когда-либо был валидный кадр.
    bool get(sbus::Frame& f, Clock::time_point& ts) {
        std::lock_guard<std::mutex> lk(m_);
        f = frame_;
        ts = ts_;
        return valid_;
    }
private:
    std::mutex m_;
    sbus::Frame frame_{};
    Clock::time_point ts_{};
    bool valid_ = false;
};

// Результат детектора метки (обновляет Vision-поток).
struct VisionShared {
    void set(double ex, double ey, bool detected) {
        std::lock_guard<std::mutex> lk(m_);
        ex_ = ex;
        ey_ = ey;
        detected_ = detected;
        ts_ = Clock::now();
    }
    void get(double& ex, double& ey, bool& detected, Clock::time_point& ts) {
        std::lock_guard<std::mutex> lk(m_);
        ex = ex_;
        ey = ey_;
        detected = detected_;
        ts = ts_;
    }
private:
    std::mutex m_;
    double ex_ = 0.0;   // смещение метки по X от центра кадра, пиксели
    double ey_ = 0.0;   // смещение метки по Y от центра кадра, пиксели
    bool detected_ = false;
    Clock::time_point ts_{};
};
