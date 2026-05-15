#include "receiver.h"
#include "tcp.h"
#include "udp.h"

#include <cstdio>
#include <cstring>

static void print_final(const Stats& stats, const char* label) {
    auto now = std::chrono::steady_clock::now();
    auto sec = std::chrono::duration_cast<std::chrono::milliseconds>(now - stats.start_time).count() / 1000.0;
    double avg_rate = sec > 0 ? stats.total_bytes * 8.0 / sec / 1e6 : 0;
    printf("\n--- %s Summary ---\n", label);
    printf("  Duration: %.2f s\n", sec);
    printf("  Total received: %.2f MB (%llu bytes)\n", stats.total_bytes / 1e6, (unsigned long long)stats.total_bytes);
    printf("  Packets: %llu\n", (unsigned long long)stats.total_packets);
    printf("  Avg rate: %.2f Mbit/s\n", avg_rate);
}

// --- TCP Receiver ---

static void recv_tcp(const Config& cfg) {
    platform_socket_t server_fd = tcp_bind_listen(cfg.port);
    if (!platform_is_valid_socket(server_fd)) return;

    platform_socket_t client_fd = tcp_accept(server_fd);
    if (!platform_is_valid_socket(client_fd)) {
        platform_close_socket(server_fd);
        return;
    }

    Stats stats;
    stats.init();

    uint8_t buf[65536];
    printf("Receiving TCP data...\n");

    while (g_running) {
        platform_ssize_t n = recv(client_fd, (char*)buf, sizeof(buf), 0);
        if (n == PLATFORM_SOCKET_ERROR) {
            if (platform_is_eintr()) continue;
            platform_print_error("recv");
            break;
        }
        if (n == 0) {
            printf("Connection closed by peer.\n");
            break;
        }
        stats.print((uint64_t)n, "RX");
    }

    tcp_close(client_fd);
    tcp_close(server_fd);
    print_final(stats, "TCP Receive");
}

// --- UDP Receiver ---

static void recv_udp(const Config& cfg) {
    platform_socket_t fd = udp_create_socket();
    if (!platform_is_valid_socket(fd)) return;

    if (udp_bind(fd, cfg.port) < 0) {
        udp_close(fd);
        return;
    }

    Stats stats;
    stats.init();

    uint8_t buf[65536];
    sockaddr_in src;
    printf("Receiving UDP data...\n");

    while (g_running) {
        platform_ssize_t n = udp_recvfrom(fd, buf, sizeof(buf), &src);
        if (n == PLATFORM_SOCKET_ERROR) break;
        if (n == 0) continue;
        stats.print((uint64_t)n, "RX");
    }

    udp_close(fd);
    print_final(stats, "UDP Receive");
}

// --- Dispatcher ---

void run_receiver(const Config& cfg) {
    if (cfg.proto == Proto::TCP) {
        recv_tcp(cfg);
    } else {
        recv_udp(cfg);
    }
}
