#include "udp.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cstdio>

int udp_create_socket() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    return fd;
}

int udp_bind(int fd, int port) {
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "bind port %d failed: %s\n", port, strerror(errno));
        return -1;
    }

    printf("Listening on port %d (UDP)...\n", port);
    return 0;
}

void udp_close(int fd) {
    if (fd >= 0) close(fd);
}

bool udp_sendto(int fd, const void* buf, size_t len, const char* ip, int port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IP: %s\n", ip);
        return false;
    }

    ssize_t n = sendto(fd, buf, len, 0, (sockaddr*)&addr, sizeof(addr));
    if (n < 0) {
        if (errno == EINTR) return true; // retryable
        fprintf(stderr, "sendto error: %s\n", strerror(errno));
        return false;
    }
    return true;
}

ssize_t udp_recvfrom(int fd, void* buf, size_t len, sockaddr_in* src) {
    socklen_t addr_len = sizeof(sockaddr_in);
    ssize_t n = recvfrom(fd, buf, len, 0, (sockaddr*)src, &addr_len);
    if (n < 0) {
        if (errno == EINTR) return 0;
        perror("recvfrom");
    }
    return n;
}
