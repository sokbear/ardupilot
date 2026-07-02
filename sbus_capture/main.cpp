#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include "config.h"
#include "sbus.h"
#include "serial_port.h"
#include "shared_state.h"
#include "vision.h"
#include "control.h"

namespace {

std::atomic<bool> g_running{true};

void on_signal(int) { g_running = false; }

// Поток чтения SBUS от приёмника: побайтовая сборка кадров.
// Начало кадра определяется по межкадровому разрыву (>= GAP) и заголовку 0x0F.
void rx_loop(SerialPort& in_port, RxShared& rx)
{
    constexpr int GAP_US = 2000; // HAL_SBUS_FRAME_GAP в ArduPilot

    uint8_t frame[sbus::FRAME_SIZE];
    int ofs = 0;
    auto last_byte = Clock::now();

    uint8_t chunk[64];
    while (g_running) {
        ssize_t n = in_port.read_bytes(chunk, sizeof(chunk));
        if (n <= 0) {
            continue;
        }
        for (ssize_t i = 0; i < n; ++i) {
            const auto now = Clock::now();
            const bool gap =
                std::chrono::duration_cast<std::chrono::microseconds>(now - last_byte)
                    .count() >= GAP_US;
            last_byte = now;

            const uint8_t b = chunk[i];

            if (gap) {
                ofs = 0; // разрыв => начало нового кадра
            }
            if (ofs == 0 && b != sbus::HEADER) {
                continue; // ждём заголовок
            }

            frame[ofs++] = b;

            if (ofs == sbus::FRAME_SIZE) {
                sbus::Frame f;
                if (sbus::decode(frame, f)) {
                    rx.set(f);
                }
                ofs = 0;
            }
        }
    }
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr,
            "Использование: %s <rx_uart> <tx_uart>\n"
            "  rx_uart  - порт от приёмника (TBS Nano),   напр. /dev/ttyAMA0\n"
            "  tx_uart  - порт к полётнику (ArduPilot),    напр. /dev/ttyAMA1\n",
            argv[0]);
        return 1;
    }
    const std::string rx_dev = argv[1];
    const std::string tx_dev = argv[2];

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    SerialPort in_port, out_port;
    if (!in_port.open(rx_dev))  return 2;
    if (!out_port.open(tx_dev)) return 2;

    RxShared      rx;
    VisionShared  vis;

    // Поток зрения (заглушка -> заменить реальным детектором).
    VisionThread vision(vis);
    vision.start();

    // Управляющий поток (FSM + генерация SBUS). RT-приоритет ставится внутри.
    ControlThread control(rx, vis, out_port);
    control.start();

    std::fprintf(stderr, "sbus_capture запущен. RX=%s TX=%s. Ctrl-C для выхода.\n",
                 rx_dev.c_str(), tx_dev.c_str());

    // Чтение приёмника — в основном потоке.
    rx_loop(in_port, rx);

    control.stop();
    vision.stop();
    in_port.close();
    out_port.close();
    return 0;
}
