#pragma once

#include "platform.h"
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <chrono>

constexpr int UDP_PAYLOAD_MAX = 1472;  // Ethernet MTU 1500 - IP 20 - UDP 8
constexpr int STATS_INTERVAL_SEC = 1;  // Print stats every second

enum class Mode { Send, Recv };
enum class Proto { TCP, UDP };
enum class WorkMode { Rate, Burst };

struct Config {
    Mode mode = Mode::Send;
    Proto proto = Proto::TCP;
    WorkMode work_mode = WorkMode::Rate;
    std::string dst_ip;
    int port = 0;
    uint64_t rate_bps = 0;       // bits per second (rate mode)
    uint64_t duration_sec = 0;   // seconds (rate mode)
    uint64_t burst_bytes = 0;    // bytes (burst mode)
    int threads = 1;              // number of sender threads / receiver connections
};

struct Stats {
    uint64_t total_bytes = 0;
    uint64_t total_packets = 0;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_report_time;
    uint64_t last_report_bytes = 0;

    void init() {
        start_time = std::chrono::steady_clock::now();
        last_report_time = start_time;
        total_bytes = 0;
        total_packets = 0;
        last_report_bytes = 0;
    }

    void print(uint64_t bytes, const char* dir) {
        total_bytes += bytes;
        total_packets++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_report_time).count();
        if (elapsed >= STATS_INTERVAL_SEC) {
            double cur_rate = (total_bytes - last_report_bytes) * 8.0 / elapsed / 1e6;
            auto total_sec = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            printf("[%s] %llu s | cur: %.2f Mbit/s | total: %.2f MB | pkts: %llu\n",
                   dir, (unsigned long long)total_sec, cur_rate,
                   total_bytes / 1e6, (unsigned long long)total_packets);
            last_report_time = now;
            last_report_bytes = total_bytes;
        }
    }

    void merge(const Stats& other) {
        if (total_packets == 0 || other.start_time < start_time)
            start_time = other.start_time;
        total_bytes += other.total_bytes;
        total_packets += other.total_packets;
    }
};

extern volatile sig_atomic_t g_running;

uint64_t parse_rate(const char* s);       // "10m" -> 10,000,000
uint64_t parse_size(const char* s);       // "10mb" -> 10,000,000
uint64_t parse_duration(const char* s);   // "30s" -> 30
void setup_signal_handler();
void print_usage(const char* prog);

// Inline: one small #ifdef for signal handling
inline void setup_signal_handler() {
#ifdef _WIN32
    SetConsoleCtrlHandler([](DWORD t) -> BOOL {
        if (t == CTRL_C_EVENT || t == CTRL_BREAK_EVENT) { g_running = 0; return TRUE; }
        return FALSE;
    }, TRUE);
#else
    struct sigaction sa{};
    sa.sa_handler = [](int) { g_running = 0; };
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
#endif
}
