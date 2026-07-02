#pragma once
#include <atomic>
#include <thread>
#include "shared_state.h"
#include "serial_port.h"

// =====================================================================
// Управляющий поток: конечный автомат режимов + генерация SBUS в FC.
// Работает на фиксированном периоде (cfg::TX_PERIOD_US), желательно с
// realtime-приоритетом (см. main.cpp).
// =====================================================================

enum class Mode {
    TRANSPARENT,    // ретрансляция кадра оператора 1:1
    CAPTURE,        // малина управляет roll/pitch по видео
    FAILSAFE_PASS,  // пропадание связи с пультом -> пробрасываем failsafe в FC
};

class ControlThread {
public:
    ControlThread(RxShared& rx, VisionShared& vis, SerialPort& out_port)
        : rx_(rx), vis_(vis), out_port_(out_port) {}

    void start();
    void stop();

    Mode mode() const { return mode_.load(); }

private:
    void run();
    void step();

    RxShared&     rx_;
    VisionShared& vis_;
    SerialPort&   out_port_;

    std::thread th_;
    std::atomic<bool> running_{false};
    std::atomic<Mode> mode_{Mode::TRANSPARENT};
};
