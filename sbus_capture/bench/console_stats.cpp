#include "console_stats.h"

#include "config.h"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace bench {

ConsoleStats::ConsoleStats(const char* tty_path)
{
    if (tty_path == nullptr || tty_path[0] == '\0') {
        disabled_ = true;
        return;
    }
    tty_path_ = tty_path;
    tryOpen();
}

ConsoleStats::~ConsoleStats()
{
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

void ConsoleStats::tryOpen()
{
    if (disabled_ || fd_ >= 0 || tty_path_.empty()) {
        return;
    }

    fd_ = ::open(tty_path_.c_str(), O_WRONLY | O_NOCTTY | O_CLOEXEC);
    if (fd_ < 0) {
        if (!logged_fail_) {
            std::fprintf(stderr,
                         "bench: console stats disabled (open %s failed: %s)\n",
                         tty_path_.c_str(), std::strerror(errno));
            logged_fail_ = true;
        }
        return;
    }

    if (!logged_ok_) {
        std::fprintf(stderr, "bench: console stats -> %s\n", tty_path_.c_str());
        logged_ok_ = true;
    }
}

void ConsoleStats::write(const ConsoleStatsSample& s)
{
    tryOpen();
    if (fd_ < 0) {
        return;
    }

    char ts[16];
    const std::time_t now = std::time(nullptr);
    std::tm         tm_buf{};
    localtime_r(&now, &tm_buf);
    std::strftime(ts, sizeof(ts), "%H:%M:%S", &tm_buf);

    char line[256];
    int  n = std::snprintf(
        line, sizeof(line),
        "%s ver %s | loop %5.1f fps | grab %.2f/%.2f upd %.2f/%.2f "
        "pub %.2f/%.2f ms | box %dx%d score %.2f | trk %u pal %u\n",
        ts, kBenchVersion, s.loop_fps, s.grab_avg, s.grab_max, s.upd_avg,
        s.upd_max, s.pub_avg, s.pub_max, s.box_w, s.box_h,
        static_cast<double>(s.score), s.track_frames, s.pal_frames);
    if (n <= 0) {
        return;
    }
    if (n > static_cast<int>(sizeof(line)) - 1) {
        n = static_cast<int>(sizeof(line)) - 1;
    }

    ssize_t w = ::write(fd_, line, static_cast<size_t>(n));
    if (w < 0 && errno == EINTR) {
        (void)::write(fd_, line, static_cast<size_t>(n));
    }
}

}  // namespace bench
