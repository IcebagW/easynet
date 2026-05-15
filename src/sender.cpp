#include "sender.h"
#include "tcp.h"
#include "udp.h"
#include "common.h"

#include <chrono>
#include <thread>
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

// --- TCP Sender ---

static void send_rate_tcp(const Config& cfg) {
    platform_socket_t fd = tcp_connect(cfg.dst_ip.c_str(), cfg.port);
    if (!platform_is_valid_socket(fd)) return;

    uint64_t chunk = cfg.rate_bps / 8 * INTERVAL.count() / 1000;
    uint8_t* buf = new uint8_t[chunk];
    memset(buf, 0xAA, chunk);

    Stats stats;
    stats.init();
    auto end_time = stats.start_time + std::chrono::seconds(cfg.duration_sec);
    auto next_tick = stats.start_time;

    printf("Sending TCP at %.2f Mbit/s for %llu s...\n", cfg.rate_bps / 1e6, (unsigned long long)cfg.duration_sec);
    printf("Chunk size: %llu bytes per %lld ms\n", (unsigned long long)chunk, (long long)INTERVAL.count());

    while (g_running) {
        auto now = std::chrono::steady_clock::now();
        if (now >= end_time) break;

        if (!tcp_send_full(fd, buf, chunk)) break;
        stats.print(chunk, "TX");

        next_tick += INTERVAL;
        auto sleep_until = next_tick > now ? next_tick : now + INTERVAL;
        std::this_thread::sleep_until(sleep_until);
    }

    delete[] buf;
    tcp_close(fd);
    print_final(stats, "TCP Rate Send");
}

static void send_burst_tcp(const Config& cfg) {
    platform_socket_t fd = tcp_connect(cfg.dst_ip.c_str(), cfg.port);
    if (!platform_is_valid_socket(fd)) return;

    uint8_t* buf = new uint8_t[cfg.burst_bytes];
    memset(buf, 0xBB, cfg.burst_bytes);

    Stats stats;
    stats.init();

    printf("Sending TCP burst: %.2f MB...\n", cfg.burst_bytes / 1e6);
    if (tcp_send_len_prefixed(fd, buf, cfg.burst_bytes)) {
        stats.print(cfg.burst_bytes + 4, "TX");
    }

    delete[] buf;
    tcp_close(fd);
    print_final(stats, "TCP Burst Send");
}

// --- UDP Sender ---

static void send_rate_udp(const Config& cfg) {
    platform_socket_t fd = udp_create_socket();
    if (!platform_is_valid_socket(fd)) return;

    uint64_t chunk = cfg.rate_bps / 8 * INTERVAL.count() / 1000;
    uint8_t* buf = new uint8_t[UDP_PAYLOAD_MAX];
    memset(buf, 0xAA, UDP_PAYLOAD_MAX);

    Stats stats;
    stats.init();
    auto end_time = stats.start_time + std::chrono::seconds(cfg.duration_sec);
    auto next_tick = stats.start_time;

    printf("Sending UDP at %.2f Mbit/s for %llu s...\n", cfg.rate_bps / 1e6, (unsigned long long)cfg.duration_sec);
    printf("Datagram size: %d bytes, %llu bytes per tick\n", UDP_PAYLOAD_MAX, (unsigned long long)chunk);

    while (g_running) {
        auto now = std::chrono::steady_clock::now();
        if (now >= end_time) break;

        uint64_t sent = 0;
        while (sent < chunk) {
            size_t sz = std::min<uint64_t>(UDP_PAYLOAD_MAX, chunk - sent);
            if (!udp_sendto(fd, buf, sz, cfg.dst_ip.c_str(), cfg.port)) break;
            sent += sz;
        }
        stats.print(sent, "TX");

        next_tick += INTERVAL;
        auto sleep_until = next_tick > now ? next_tick : now + INTERVAL;
        std::this_thread::sleep_until(sleep_until);
    }

    delete[] buf;
    udp_close(fd);
    print_final(stats, "UDP Rate Send");
}

static void send_burst_udp(const Config& cfg) {
    platform_socket_t fd = udp_create_socket();
    if (!platform_is_valid_socket(fd)) return;

    uint8_t* buf = new uint8_t[cfg.burst_bytes];
    memset(buf, 0xBB, cfg.burst_bytes);

    Stats stats;
    stats.init();

    printf("Sending UDP burst: %.2f MB (%llu datagrams)...\n",
           cfg.burst_bytes / 1e6,
           (unsigned long long)((cfg.burst_bytes + UDP_PAYLOAD_MAX - 1) / UDP_PAYLOAD_MAX));

    uint64_t offset = 0;
    while (offset < cfg.burst_bytes) {
        size_t sz = std::min<uint64_t>(UDP_PAYLOAD_MAX, cfg.burst_bytes - offset);
        if (!udp_sendto(fd, buf + offset, sz, cfg.dst_ip.c_str(), cfg.port)) break;
        offset += sz;
        stats.print(sz, "TX");
    }

    delete[] buf;
    udp_close(fd);
    print_final(stats, "UDP Burst Send");
}

// --- Dispatcher ---

void run_sender(const Config& cfg) {
    if (cfg.proto == Proto::TCP) {
        if (cfg.work_mode == WorkMode::Rate) {
            send_rate_tcp(cfg);
        } else {
            send_burst_tcp(cfg);
        }
    } else {
        if (cfg.work_mode == WorkMode::Rate) {
            send_rate_udp(cfg);
        } else {
            send_burst_udp(cfg);
        }
    }
}
