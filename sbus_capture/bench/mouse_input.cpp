#include "mouse_input.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unistd.h>

#include <linux/input.h>
#include <sys/ioctl.h>

namespace bench {

namespace {

bool isMouseDevice(int fd)
{
    unsigned long evbit = 0;
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit) < 0) {
        return false;
    }
    if (!(evbit & (1u << EV_REL))) {
        return false;
    }

    unsigned long relbit = 0;
    if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relbit)), &relbit) < 0) {
        return false;
    }
    if (!(relbit & (1u << REL_X))) {
        return false;
    }

    char name[256] = {};
    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0) {
        const std::string n(name);
        if (n.find("hdmi") != std::string::npos ||
            n.find("HDMI") != std::string::npos ||
            n.find("vc4") != std::string::npos) {
            return false;
        }
        if (n.find("mouse") != std::string::npos ||
            n.find("Mouse") != std::string::npos ||
            n.find("Logitech") != std::string::npos) {
            return true;
        }
    }
    return true;
}

}  // namespace

bool MouseInput::open()
{
    if (fd_ >= 0) {
        return true;
    }

    for (int i = 0; i < 32; ++i) {
        const std::string path = "/dev/input/event" + std::to_string(i);
        const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            continue;
        }
        if (!isMouseDevice(fd)) {
            ::close(fd);
            continue;
        }

        fd_ = fd;
        x_  = static_cast<float>(screen_w_ / 2);
        y_  = static_cast<float>(screen_h_ / 2);
        std::cout << "bench: mouse " << path << '\n';
        return true;
    }

    std::cerr << "bench: USB mouse not found (plug mouse, user in group input)\n";
    return false;
}

void MouseInput::close()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

void MouseInput::setScreenSize(int width, int height)
{
    screen_w_ = width > 0 ? width : 720;
    screen_h_ = height > 0 ? height : 576;
    if (x_ <= 0.0f && y_ <= 0.0f) {
        x_ = static_cast<float>(screen_w_ / 2);
        y_ = static_cast<float>(screen_h_ / 2);
    }
}

void MouseInput::clearEdges()
{
    left_pressed_  = false;
    left_released_ = false;
    right_pressed_ = false;
}

void MouseInput::poll()
{
    if (fd_ < 0) {
        return;
    }

    input_event ev {};
    while (true) {
        const ssize_t n = ::read(fd_, &ev, sizeof(ev));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            std::cerr << "bench: mouse read error: " << std::strerror(errno) << '\n';
            break;
        }
        if (n != static_cast<ssize_t>(sizeof(ev))) {
            break;
        }

        if (ev.type == EV_REL) {
            if (ev.code == REL_X) {
                x_ += static_cast<float>(ev.value);
            } else if (ev.code == REL_Y) {
                y_ += static_cast<float>(ev.value);
            }
        } else if (ev.type == EV_KEY) {
            if (ev.code == BTN_LEFT) {
                const bool down = ev.value != 0;
                if (down && !left_down_) {
                    left_pressed_ = true;
                }
                if (!down && left_down_) {
                    left_released_ = true;
                }
                left_down_ = down;
            } else if (ev.code == BTN_RIGHT && ev.value != 0) {
                right_pressed_ = true;
            }
        }
    }

    if (x_ < 0.0f) {
        x_ = 0.0f;
    }
    if (y_ < 0.0f) {
        y_ = 0.0f;
    }
    if (x_ >= static_cast<float>(screen_w_)) {
        x_ = static_cast<float>(screen_w_ - 1);
    }
    if (y_ >= static_cast<float>(screen_h_)) {
        y_ = static_cast<float>(screen_h_ - 1);
    }
}

}  // namespace bench
