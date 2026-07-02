#include "serial_port.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <asm/termbits.h>   // termios2, BOTHER (не включать вместе с <termios.h>)
#include <cstdio>
#include <cerrno>
#include <cstring>

SerialPort::~SerialPort()
{
    close();
}

bool SerialPort::open(const std::string& device)
{
    fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0) {
        std::fprintf(stderr, "SerialPort: cannot open %s: %s\n",
                     device.c_str(), std::strerror(errno));
        return false;
    }

    struct termios2 tio{};
    if (ioctl(fd_, TCGETS2, &tio) != 0) {
        std::fprintf(stderr, "SerialPort: TCGETS2 failed: %s\n", std::strerror(errno));
        close();
        return false;
    }

    // Управляющие флаги: 8 бит данных, even parity (PARENB без PARODD),
    // 2 стоп-бита (CSTOPB), локальный, приём включён, своя скорость (BOTHER).
    tio.c_cflag &= ~CBAUD;
    tio.c_cflag |= BOTHER;
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;
    tio.c_cflag |= PARENB;     // включить контроль чётности
    tio.c_cflag &= ~PARODD;    // even
    tio.c_cflag |= CSTOPB;     // 2 стоп-бита
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~CRTSCTS;   // без аппаратного flow control

    // Вход: чтобы parity не правил байты, отключаем разбор/маркировку.
    tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
                     INLCR | IGNCR | ICRNL | IXON | INPCK | IGNPAR);

    // Локальные/выходные флаги: raw-режим.
    tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tio.c_oflag &= ~OPOST;

    // Кастомная скорость 100000 в обе стороны.
    tio.c_ispeed = 100000;
    tio.c_ospeed = 100000;

    // Поведение чтения: вернуться по таймауту, не блокироваться навсегда.
    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 1;       // 0.1с таймаут чтения

    if (ioctl(fd_, TCSETS2, &tio) != 0) {
        std::fprintf(stderr, "SerialPort: TCSETS2 failed: %s\n", std::strerror(errno));
        close();
        return false;
    }

    return true;
}

void SerialPort::close()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

ssize_t SerialPort::read_bytes(uint8_t* buf, size_t n)
{
    if (fd_ < 0) return -1;
    return ::read(fd_, buf, n);
}

bool SerialPort::write_all(const uint8_t* buf, size_t n)
{
    if (fd_ < 0) return false;
    size_t written = 0;
    while (written < n) {
        ssize_t r = ::write(fd_, buf + written, n - written);
        if (r < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        written += static_cast<size_t>(r);
    }
    return true;
}
