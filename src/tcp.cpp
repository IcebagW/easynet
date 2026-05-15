#include "tcp.h"

platform_socket_t tcp_connect(const char* ip, int port) {
    platform_socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (!platform_is_valid_socket(fd)) {
        platform_print_error("socket");
        return PLATFORM_INVALID_SOCKET;
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IP address: %s\n", ip);
        platform_close_socket(fd);
        return PLATFORM_INVALID_SOCKET;
    }

    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) == PLATFORM_SOCKET_ERROR) {
        fprintf(stderr, "connect to %s:%d failed: %s\n",
                ip, port, platform_strerror(platform_get_error()));
        platform_close_socket(fd);
        return PLATFORM_INVALID_SOCKET;
    }

    printf("Connected to %s:%d (TCP)\n", ip, port);
    return fd;
}

platform_socket_t tcp_bind_listen(int port) {
    platform_socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (!platform_is_valid_socket(fd)) {
        platform_print_error("socket");
        return PLATFORM_INVALID_SOCKET;
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) == PLATFORM_SOCKET_ERROR) {
        fprintf(stderr, "bind port %d failed: %s\n",
                port, platform_strerror(platform_get_error()));
        platform_close_socket(fd);
        return PLATFORM_INVALID_SOCKET;
    }

    if (listen(fd, 1) == PLATFORM_SOCKET_ERROR) {
        platform_print_error("listen");
        platform_close_socket(fd);
        return PLATFORM_INVALID_SOCKET;
    }

    printf("Listening on port %d (TCP)...\n", port);
    return fd;
}

platform_socket_t tcp_accept(platform_socket_t server_fd) {
    sockaddr_in client{};
    socklen_t len = sizeof(client);
    platform_socket_t fd = accept(server_fd, (sockaddr*)&client, &len);
    if (!platform_is_valid_socket(fd)) {
        platform_print_error("accept");
        return PLATFORM_INVALID_SOCKET;
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client.sin_addr, ip, sizeof(ip));
    printf("Accepted connection from %s:%d\n", ip, ntohs(client.sin_port));
    return fd;
}

void tcp_close(platform_socket_t fd) {
    if (platform_is_valid_socket(fd)) {
        platform_shutdown_socket(fd, PLATFORM_SHUT_RDWR);
        platform_close_socket(fd);
    }
}

bool tcp_send_full(platform_socket_t fd, const void* buf, size_t len) {
    const char* ptr = (const char*)buf;
    size_t remaining = len;
    while (remaining > 0) {
        platform_ssize_t n = send(fd, ptr, (int)remaining, 0);
        if (n == PLATFORM_SOCKET_ERROR) {
            if (platform_is_eintr()) continue;
            fprintf(stderr, "send error: %s\n", platform_strerror(platform_get_error()));
            return false;
        }
        ptr += n;
        remaining -= n;
    }
    return true;
}

bool tcp_recv_full(platform_socket_t fd, void* buf, size_t len) {
    char* ptr = (char*)buf;
    size_t remaining = len;
    while (remaining > 0) {
        platform_ssize_t n = recv(fd, ptr, (int)remaining, 0);
        if (n == PLATFORM_SOCKET_ERROR) {
            if (platform_is_eintr()) continue;
            fprintf(stderr, "recv error: %s\n", platform_strerror(platform_get_error()));
            return false;
        }
        if (n == 0) return false; // EOF
        ptr += n;
        remaining -= n;
    }
    return true;
}

bool tcp_send_len_prefixed(platform_socket_t fd, const void* buf, uint32_t len) {
    uint32_t net_len = htonl(len);
    if (!tcp_send_full(fd, &net_len, 4)) return false;
    if (!tcp_send_full(fd, buf, len)) return false;
    return true;
}

bool tcp_recv_len_prefixed(platform_socket_t fd, uint8_t*& buf, uint32_t& len) {
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
