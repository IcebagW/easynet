#include "sender.h"
#include "tcp.h"
#include "udp.h"
#include "common.h"

#include <chrono>
#include <thread>
#include <vector>
#include <cstring>

// 100Hz send frequency
static constexpr auto INTERVAL = std::chrono::milliseconds(10);

static void print_final(const Stats& stats, const char* label) {
    auto now = std::chrono::steady_clock::now();
    auto sec = std::chrono::duration_cast<std::chrono::milliseconds>(now - stats.start_time).count() / 1000.0;
    double avg_rate = sec > 0 ? stats.total_bytes * 8.0 / sec / 1e6 : 0;
    printf("\n--- %s Summary ---\n", label);
    printf("  Duration: %.2f s\n", sec);
    printf("  Total sent: %.2f MB (%llu bytes)\n", stats.total_bytes / 1e6, (unsigned long long)stats.total_bytes);
    printf("  Packets: %llu\n", (unsigned long long)stats.total_packets);
    printf("  Avg rate: %.2f Mbit/s\n", avg_rate);
}

// --- Single-thread sender helpers (take Stats output param) ---

static void send_rate_tcp_impl(const Config& cfg, Stats& out) {
    platform_socket_t fd = tcp_connect(cfg.dst_ip.c_str(), cfg.port);
    if (!platform_is_valid_socket(fd)) return;

    uint64_t chunk = cfg.rate_bps / 8 * INTERVAL.count() / 1000;
    uint8_t* buf = new uint8_t[chunk];
    memset(buf, 0xAA, chunk);

    out.init();
    auto end_time = out.start_time + std::chrono::seconds(cfg.duration_sec);
    auto next_tick = out.start_time;

    printf("[TID %zu] Sending TCP at %.2f Mbit/s, chunk %llu bytes\n",
           std::hash<std::thread::id>{}(std::this_thread::get_id()) % 10000,
           cfg.rate_bps / 1e6, (unsigned long long)chunk);

    while (g_running) {
        auto now = std::chrono::steady_clock::now();
        if (now >= end_time) break;

        if (!tcp_send_full(fd, buf, chunk)) break;
        out.print(chunk, "TX");

        next_tick += INTERVAL;
        auto sleep_until = next_tick > now ? next_tick : now + INTERVAL;
        std::this_thread::sleep_until(sleep_until);
    }

    delete[] buf;
    tcp_close(fd);
}

static void send_burst_tcp_impl(const Config& cfg, Stats& out) {
    platform_socket_t fd = tcp_connect(cfg.dst_ip.c_str(), cfg.port);
    if (!platform_is_valid_socket(fd)) return;

    uint8_t* buf = new uint8_t[cfg.burst_bytes];
    memset(buf, 0xBB, cfg.burst_bytes);

    out.init();

    if (tcp_send_len_prefixed(fd, buf, (uint32_t)cfg.burst_bytes)) {
        out.print(cfg.burst_bytes + 4, "TX");
    }

    delete[] buf;
    tcp_close(fd);
}

static void send_rate_udp_impl(const Config& cfg, Stats& out) {
    platform_socket_t fd = udp_create_socket();
    if (!platform_is_valid_socket(fd)) return;

    uint64_t chunk = cfg.rate_bps / 8 * INTERVAL.count() / 1000;
    uint8_t* buf = new uint8_t[UDP_PAYLOAD_MAX];
    memset(buf, 0xAA, UDP_PAYLOAD_MAX);

    out.init();
    auto end_time = out.start_time + std::chrono::seconds(cfg.duration_sec);
    auto next_tick = out.start_time;

    while (g_running) {
        auto now = std::chrono::steady_clock::now();
        if (now >= end_time) break;

        uint64_t sent = 0;
        while (sent < chunk) {
            size_t sz = std::min<uint64_t>(UDP_PAYLOAD_MAX, chunk - sent);
            if (!udp_sendto(fd, buf, sz, cfg.dst_ip.c_str(), cfg.port)) break;
            sent += sz;
        }
        out.print(sent, "TX");

        next_tick += INTERVAL;
        auto sleep_until = next_tick > now ? next_tick : now + INTERVAL;
        std::this_thread::sleep_until(sleep_until);
    }

    delete[] buf;
    udp_close(fd);
}

static void send_burst_udp_impl(const Config& cfg, Stats& out) {
    platform_socket_t fd = udp_create_socket();
    if (!platform_is_valid_socket(fd)) return;

    uint8_t* buf = new uint8_t[cfg.burst_bytes];
    memset(buf, 0xBB, cfg.burst_bytes);

    out.init();

    uint64_t offset = 0;
    while (offset < cfg.burst_bytes) {
        size_t sz = std::min<uint64_t>(UDP_PAYLOAD_MAX, cfg.burst_bytes - offset);
        if (!udp_sendto(fd, buf + offset, sz, cfg.dst_ip.c_str(), cfg.port)) break;
        offset += sz;
        out.print(sz, "TX");
    }

    delete[] buf;
    udp_close(fd);
}

// --- Threaded runner ---

static void run_sender_thread(const Config& cfg, Stats& out) {
    if (cfg.proto == Proto::TCP) {
        if (cfg.work_mode == WorkMode::Rate) {
            send_rate_tcp_impl(cfg, out);
        } else {
            send_burst_tcp_impl(cfg, out);
        }
    } else {
        if (cfg.work_mode == WorkMode::Rate) {
            send_rate_udp_impl(cfg, out);
        } else {
            send_burst_udp_impl(cfg, out);
        }
    }
}

void run_sender(const Config& cfg) {
    int n = cfg.threads;
    if (n <= 0) n = 1;

    // Per-thread sub-config: divide rate/burst evenly
    Config sub = cfg;
    sub.threads = 1;  // prevent infinite recursion
    if (cfg.work_mode == WorkMode::Rate) {
        sub.rate_bps = cfg.rate_bps / n;
    } else {
        sub.burst_bytes = cfg.burst_bytes / n;
    }

    printf("Starting %d sender thread(s)\n", n);
    if (cfg.work_mode == WorkMode::Rate) {
        printf("Total rate: %.2f Mbit/s (%.2f Mbit/s per thread), duration: %llu s\n",
               cfg.rate_bps / 1e6, sub.rate_bps / 1e6, (unsigned long long)cfg.duration_sec);
    } else {
        printf("Total burst: %.2f MB (%.2f MB per thread)\n",
               cfg.burst_bytes / 1e6, sub.burst_bytes / 1e6);
    }

    // Start threads
    std::vector<std::thread> threads;
    std::vector<Stats> stats_vec(n);
    for (int i = 0; i < n; i++) {
        threads.emplace_back([i, &sub, &stats_vec]() {
            run_sender_thread(sub, stats_vec[i]);
        });
    }

    // Wait for all threads
    Stats total;
    for (int i = 0; i < n; i++) {
        threads[i].join();
        total.merge(stats_vec[i]);
    }

    // Print aggregate summary
    const char* proto = cfg.proto == Proto::TCP ? "TCP" : "UDP";
    const char* mode = cfg.work_mode == WorkMode::Rate ? "Rate Send" : "Burst Send";
    char label[64];
    snprintf(label, sizeof(label), "%s %s (%d threads)", proto, mode, n);
    print_final(total, label);
}
