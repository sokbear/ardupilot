#pragma once

namespace bench {

// USB-мышь через /dev/input/event* (evdev), без X11.
class MouseInput {
public:
    bool open();
    void close();
    bool isOpen() const { return fd_ >= 0; }

    void setScreenSize(int width, int height);
    void poll();

    int  cursorX() const { return static_cast<int>(x_); }
    int  cursorY() const { return static_cast<int>(y_); }
    bool leftDown() const { return left_down_; }
    bool leftPressed() const { return left_pressed_; }
    bool leftReleased() const { return left_released_; }
    bool rightPressed() const { return right_pressed_; }

    void clearEdges();

private:
    int   fd_             = -1;
    float x_              = 0.0f;
    float y_              = 0.0f;
    int   screen_w_       = 720;
    int   screen_h_       = 576;
    bool  left_down_      = false;
    bool  left_pressed_   = false;
    bool  left_released_  = false;
    bool  right_pressed_  = false;
};

}  // namespace bench
