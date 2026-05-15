#include "tcp.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cstdio>

int tcp_connect(const char* ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IP address: %s\n", ip);
        close(fd);
        return -1;
    }

    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "connect to %s:%d failed: %s\n", ip, port, strerror(errno));
        close(fd);
        return -1;
    }

    printf("Connected to %s:%d (TCP)\n", ip, port);
    return fd;
}

int tcp_bind_listen(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "bind port %d failed: %s\n", port, strerror(errno));
        close(fd);
        return -1;
    }

    if (listen(fd, 1) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    printf("Listening on port %d (TCP)...\n", port);
    return fd;
}

int tcp_accept(int server_fd) {
    sockaddr_in client{};
    socklen_t len = sizeof(client);
    int fd = accept(server_fd, (sockaddr*)&client, &len);
    if (fd < 0) {
        perror("accept");
        return -1;
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client.sin_addr, ip, sizeof(ip));
    printf("Accepted connection from %s:%d\n", ip, ntohs(client.sin_port));
    return fd;
}

void tcp_close(int fd) {
    if (fd >= 0) {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
}

bool tcp_send_full(int fd, const void* buf, size_t len) {
    const char* ptr = (const char*)buf;
    size_t remaining = len;
    while (remaining > 0) {
        ssize_t n = send(fd, ptr, remaining, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "send error: %s\n", strerror(errno));
            return false;
        }
        ptr += n;
        remaining -= n;
    }
    return true;
}

bool tcp_recv_full(int fd, void* buf, size_t len) {
    char* ptr = (char*)buf;
    size_t remaining = len;
    while (remaining > 0) {
        ssize_t n = recv(fd, ptr, remaining, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "recv error: %s\n", strerror(errno));
            return false;
        }
        if (n == 0) return false; // EOF
        ptr += n;
        remaining -= n;
    }
    return true;
}

bool tcp_send_len_prefixed(int fd, const void* buf, uint32_t len) {
    uint32_t net_len = htonl(len);
    if (!tcp_send_full(fd, &net_len, 4)) return false;
    if (!tcp_send_full(fd, buf, len)) return false;
    return true;
}

bool tcp_recv_len_prefixed(int fd, uint8_t*& buf, uint32_t& len) {
    uint32_t net_len;
    if (!tcp_recv_full(fd, &net_len, 4)) return false;
    len = ntohl(net_len);
    buf = new uint8_t[len];
    if (!tcp_recv_full(fd, buf, len)) {
        delete[] buf;
        buf = nullptr;
        return false;
    }
    return true;
}
