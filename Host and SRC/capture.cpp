#include "hb/packet.hpp"
#include <cerrno>
#include <csignal>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <poll.h>
#include <string>
#include <termios.h>
#include <unistd.h>

static volatile std::sig_atomic_t g_stop = 0;
static void on_signal(int) { g_stop = 1; }

static bool write_command(int fd, const char *command, std::size_t length) {
    std::size_t sent = 0;
    while (sent < length) {
        const ssize_t n = ::write(fd, command + sent, length - sent);
        if (n > 0) { sent += static_cast<std::size_t>(n); continue; }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            pollfd pfd{fd, POLLOUT, 0};
            if (::poll(&pfd, 1, 1000) > 0) continue;
        }
        return false;
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage: hb_capture <serial-device> <output.hbci> [duration-seconds]\n";
        return 2;
    }
    int duration = 0;
    if (argc == 4) {
        char *end = nullptr;
        const long value = std::strtol(argv[3], &end, 10);
        if (end == argv[3] || *end != '\0' || value <= 0 || value > 86400) {
            std::cerr << "duration must be 1..86400 seconds\n"; return 2;
        }
        duration = static_cast<int>(value);
    }

    const int fd = ::open(argv[1], O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) { std::perror("open serial device"); return 2; }
    termios settings{};
    if (tcgetattr(fd, &settings) != 0) { std::perror("tcgetattr"); ::close(fd); return 2; }
    cfmakeraw(&settings);
    cfsetispeed(&settings, B115200);
    cfsetospeed(&settings, B115200);
    settings.c_cflag |= (CLOCAL | CREAD);
    settings.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    settings.c_cflag |= CS8;
    settings.c_cc[VMIN] = 0;
    settings.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &settings) != 0) {
        std::perror("tcsetattr"); ::close(fd); return 2;
    }
    (void)tcflush(fd, TCIOFLUSH);

    std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
    if (!output) { std::cerr << "Cannot open output file\n"; ::close(fd); return 2; }
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    if (!write_command(fd, "START\n", 6u)) {
        std::cerr << "Failed to send START command\n"; ::close(fd); return 3;
    }
    std::cout << "Capture started. Press Ctrl-C to stop.\n";

    const auto start = std::chrono::steady_clock::now();
    std::uint64_t bytes_written = 0;
    char buffer[4096];
    while (!g_stop) {
        if (duration > 0 &&
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count() >= duration) break;
        pollfd pfd{fd, POLLIN, 0};
        const int wait_ms = duration > 0 ? 100 : 250;
        const int ready = ::poll(&pfd, 1, wait_ms);
        if (ready < 0) { if (errno == EINTR) continue; std::perror("poll"); break; }
        if (ready == 0) continue;
        if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            std::cerr << "Serial device disconnected or reported an I/O error\n"; break;
        }
        const ssize_t n = ::read(fd, buffer, sizeof buffer);
        if (n > 0) {
            output.write(buffer, n);
            if (!output) { std::cerr << "Output write failed\n"; break; }
            bytes_written += static_cast<std::uint64_t>(n);
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            std::perror("read"); break;
        }
    }
    (void)write_command(fd, "STOP\n", 5u);
    output.flush();
    ::close(fd);
    std::cout << "Capture stopped; bytes written: " << bytes_written << "\n";
    std::cout << "Decode with: hb_decode " << argv[2] << " capture.csv\n";
    return output ? 0 : 3;
}
