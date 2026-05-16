#include "udp.h"
#include "common.h"

platform_socket_t udp_create_socket() {
    platform_socket_t fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (!platform_is_valid_socket(fd)) {
        platform_print_error("socket");
        return PLATFORM_INVALID_SOCKET;
    }
    int buf_size = SOCKET_BUF_SIZE;
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const char*)&buf_size, sizeof(buf_size));
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const char*)&buf_size, sizeof(buf_size));
    return fd;
}

int udp_bind(platform_socket_t fd, int port) {
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) == PLATFORM_SOCKET_ERROR) {
        fprintf(stderr, "bind port %d failed: %s\n",
                port, platform_strerror(platform_get_error()));
        return -1;
    }

    printf("Listening on port %d (UDP)...\n", port);
    return 0;
}

void udp_close(platform_socket_t fd) {
    if (platform_is_valid_socket(fd)) platform_close_socket(fd);
}

bool udp_sendto(platform_socket_t fd, const void* buf, size_t len, const char* ip, int port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IP: %s\n", ip);
        return false;
    }

    platform_ssize_t n = sendto(fd, (const char*)buf, (int)len, 0, (sockaddr*)&addr, sizeof(addr));
    if (n == PLATFORM_SOCKET_ERROR) {
        if (platform_is_eintr()) return true;
        fprintf(stderr, "sendto error: %s\n", platform_strerror(platform_get_error()));
        return false;
    }
    return true;
}

platform_ssize_t udp_recvfrom(platform_socket_t fd, void* buf, size_t len, sockaddr_in* src) {
    socklen_t addr_len = sizeof(sockaddr_in);
    platform_ssize_t n = recvfrom(fd, (char*)buf, (int)len, 0, (sockaddr*)src, &addr_len);
    if (n == PLATFORM_SOCKET_ERROR) {
        if (platform_is_eintr()) return 0;
        platform_print_error("recvfrom");
    }
    return n;
}
