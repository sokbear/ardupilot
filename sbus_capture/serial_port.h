#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

// =====================================================================
// Линуксовый UART для SBUS: 100000 бод, 8 бит данных, even parity, 2 stop.
//
// ВАЖНО про инверсию: SBUS — инвертированный сигнал. У UART Raspberry Pi
// аппаратной инверсии нет, поэтому ОБЕ линии (RX от приёмника и TX к
// полётнику) нужно инвертировать внешним инвертором / конвертером.
// Здесь порт конфигурируется как обычный 8E2 100000 — предполагается,
// что сигнал уже приведён к не-инвертированному виду железом.
//
// Нестандартная скорость 100000 задаётся через termios2/BOTHER.
// =====================================================================

class SerialPort {
public:
    SerialPort() = default;
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    // Открывает и конфигурирует порт (100000 8E2, raw). false при ошибке.
    bool open(const std::string& device);

    void close();

    bool is_open() const { return fd_ >= 0; }

    // Чтение до n байт. Возвращает число прочитанных (0 при таймауте, -1 ошибка).
    ssize_t read_bytes(uint8_t* buf, size_t n);

    // Запись n байт целиком. true при успехе.
    bool write_all(const uint8_t* buf, size_t n);

private:
    int fd_ = -1;
};
