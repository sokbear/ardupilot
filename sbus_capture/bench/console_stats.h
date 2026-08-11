#pragma once

#include <string>

namespace bench {

// Однострочная сводка производительности трек-цикла, раз в секунду.
struct ConsoleStatsSample {
    double loop_fps  = 0.0;
    double grab_avg  = 0.0, grab_max = 0.0;
    double upd_avg   = 0.0, upd_max  = 0.0;
    double pub_avg   = 0.0, pub_max  = 0.0;
    int    box_w     = 0,   box_h    = 0;
    float  score     = 0.0f;  // 0 = нет захвата
    unsigned track_frames = 0;
    unsigned pal_frames   = 0;
};

// Пишет статистику напрямую в TTY (HDMI-консоль), минуя stdout/journal.
// Открытие ленивое с повтором (TTY может быть занят getty при старте).
class ConsoleStats {
public:
    explicit ConsoleStats(const char* tty_path);
    ~ConsoleStats();

    ConsoleStats(const ConsoleStats&)            = delete;
    ConsoleStats& operator=(const ConsoleStats&) = delete;

    bool enabled() const { return fd_ >= 0; }
    void write(const ConsoleStatsSample& s);

private:
    void tryOpen();

    std::string tty_path_;
    int         fd_           = -1;
    bool        disabled_     = false;
    bool        logged_fail_  = false;
    bool        logged_ok_    = false;
};

}  // namespace bench
