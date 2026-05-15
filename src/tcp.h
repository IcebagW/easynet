#pragma once

#include "platform.h"
#include <cstdint>
#include <cstddef>

platform_socket_t tcp_connect(const char* ip, int port);
platform_socket_t tcp_bind_listen(int port);
platform_socket_t tcp_accept(platform_socket_t server_fd);
void tcp_close(platform_socket_t fd);
bool tcp_send_full(platform_socket_t fd, const void* buf, size_t len);
bool tcp_recv_full(platform_socket_t fd, void* buf, size_t len);
bool tcp_send_len_prefixed(platform_socket_t fd, const void* buf, uint32_t len);
bool tcp_recv_len_prefixed(platform_socket_t fd, uint8_t*& buf, uint32_t& len);
