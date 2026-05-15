#include "common.h"
#include "sender.h"
#include "receiver.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <string>
#include <climits>

volatile sig_atomic_t g_running = 1;

void setup_signal_handler() {
    struct sigaction sa{};
    sa.sa_handler = [](int) { g_running = 0; };
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

uint64_t parse_rate(const char* s) {
    char* end;
    double val = strtod(s, &end);
    if (val <= 0) return 0;
    if (strcmp(end, "k") == 0) return (uint64_t)(val * 1000);
    if (strcmp(end, "m") == 0) return (uint64_t)(val * 1000000);
    if (strcmp(end, "g") == 0) return (uint64_t)(val * 1000000000ULL);
    return (uint64_t)val; // raw bps
}

uint64_t parse_size(const char* s) {
    char* end;
    double val = strtod(s, &end);
    if (val <= 0) return 0;
    if (strcmp(end, "b") == 0) return (uint64_t)val;
    if (strcmp(end, "kb") == 0) return (uint64_t)(val * 1000);
    if (strcmp(end, "mb") == 0) return (uint64_t)(val * 1000000);
    if (strcmp(end, "gb") == 0) return (uint64_t)(val * 1000000000ULL);
    if (strcmp(end, "k") == 0) return (uint64_t)(val * 1000);
    if (strcmp(end, "m") == 0) return (uint64_t)(val * 1000000);
    if (strcmp(end, "g") == 0) return (uint64_t)(val * 1000000000ULL);
    return (uint64_t)val;
}

uint64_t parse_duration(const char* s) {
    char* end;
    double val = strtod(s, &end);
    if (val <= 0) return 0;
    if (strcmp(end, "s") == 0) return (uint64_t)val;
    if (strcmp(end, "m") == 0) return (uint64_t)(val * 60);
    if (strcmp(end, "h") == 0) return (uint64_t)(val * 3600);
    return (uint64_t)val; // assume seconds
}

void print_usage(const char* prog) {
    printf("Usage:\n");
    printf("  Sender - fixed rate mode:\n");
    printf("    %s send --dst <ip> --port <port> --proto <tcp|udp> --rate <rate> --duration <duration>\n", prog);
    printf("    Example: %s send --dst 192.168.1.100 --port 8080 --proto tcp --rate 10m --duration 30s\n", prog);
    printf("\n");
    printf("  Sender - burst mode:\n");
    printf("    %s send --dst <ip> --port <port> --proto <tcp|udp> --burst <size>\n", prog);
    printf("    Example: %s send --dst 192.168.1.100 --port 8080 --proto udp --burst 10mb\n", prog);
    printf("\n");
    printf("  Receiver:\n");
    printf("    %s recv --port <port> --proto <tcp|udp>\n", prog);
    printf("    Example: %s recv --port 8080 --proto tcp\n", prog);
    printf("\n");
    printf("Suffixes:\n");
    printf("  Rate:  k=Kbit/s, m=Mbit/s, g=Gbit/s  (e.g. 10m = 10 Mbit/s)\n");
    printf("  Size:  kb=KB, mb=MB, gb=GB           (e.g. 10mb = 10 MB)\n");
    printf("  Time:  s=seconds, m=minutes, h=hours  (e.g. 30s = 30 seconds)\n");
}

static bool has_prefix(const char* arg, const char* prefix) {
    return strncmp(arg, prefix, strlen(prefix)) == 0;
}

static const char* get_value(const char* arg) {
    const char* p = strchr(arg, '=');
    return p ? p + 1 : nullptr;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    setup_signal_handler();

    Config cfg;

    // Parse mode
    if (strcmp(argv[1], "send") == 0) {
        cfg.mode = Mode::Send;
    } else if (strcmp(argv[1], "recv") == 0) {
        cfg.mode = Mode::Recv;
    } else {
        print_usage(argv[0]);
        return 1;
    }

    // Parse remaining arguments
    bool has_rate = false, has_duration = false, has_burst = false;

    for (int i = 2; i < argc; i++) {
        const char* arg = argv[i];
        const char* val = nullptr;

        if (has_prefix(arg, "--dst=")) {
            val = get_value(arg);
            if (!val) { fprintf(stderr, "Missing value for --dst\n"); return 1; }
            cfg.dst_ip = val;
        } else if (has_prefix(arg, "--port=")) {
            val = get_value(arg);
            if (!val) { fprintf(stderr, "Missing value for --port\n"); return 1; }
            cfg.port = atoi(val);
        } else if (has_prefix(arg, "--proto=")) {
            val = get_value(arg);
            if (!val) { fprintf(stderr, "Missing value for --proto\n"); return 1; }
            if (strcmp(val, "tcp") == 0) cfg.proto = Proto::TCP;
            else if (strcmp(val, "udp") == 0) cfg.proto = Proto::UDP;
            else { fprintf(stderr, "Invalid proto: %s (use tcp or udp)\n", val); return 1; }
        } else if (has_prefix(arg, "--rate=")) {
            val = get_value(arg);
            if (!val) { fprintf(stderr, "Missing value for --rate\n"); return 1; }
            cfg.rate_bps = parse_rate(val);
            if (cfg.rate_bps == 0) { fprintf(stderr, "Invalid rate: %s\n", val); return 1; }
            has_rate = true;
        } else if (has_prefix(arg, "--duration=")) {
            val = get_value(arg);
            if (!val) { fprintf(stderr, "Missing value for --duration\n"); return 1; }
            cfg.duration_sec = parse_duration(val);
            if (cfg.duration_sec == 0) { fprintf(stderr, "Invalid duration: %s\n", val); return 1; }
            has_duration = true;
        } else if (has_prefix(arg, "--burst=")) {
            val = get_value(arg);
            if (!val) { fprintf(stderr, "Missing value for --burst\n"); return 1; }
            cfg.burst_bytes = parse_size(val);
            if (cfg.burst_bytes == 0) { fprintf(stderr, "Invalid burst size: %s\n", val); return 1; }
            has_burst = true;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", arg);
            print_usage(argv[0]);
            return 1;
        }
    }

    // Validate config
    if (cfg.mode == Mode::Send) {
        if (cfg.dst_ip.empty()) { fprintf(stderr, "Error: --dst is required for send mode\n"); return 1; }
        if (cfg.port == 0) { fprintf(stderr, "Error: --port is required\n"); return 1; }
        if (has_burst) {
            cfg.work_mode = WorkMode::Burst;
        } else if (has_rate && has_duration) {
            cfg.work_mode = WorkMode::Rate;
        } else {
            fprintf(stderr, "Error: specify either --rate + --duration, or --burst\n");
            return 1;
        }
    } else {
        if (cfg.port == 0) { fprintf(stderr, "Error: --port is required\n"); return 1; }
    }

    // Dispatch
    if (cfg.mode == Mode::Send) {
        run_sender(cfg);
    } else {
        run_receiver(cfg);
    }

    return 0;
}
