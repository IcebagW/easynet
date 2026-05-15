#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/types.h>

int  tcp_connect(const char* ip, int port);
int  tcp_bind_listen(int port);
int  tcp_accept(int server_fd);
void tcp_close(int fd);
bool tcp_send_full(int fd, const void* buf, size_t len);
bool tcp_recv_full(int fd, void* buf, size_t len);
bool tcp_send_len_prefixed(int fd, const void* buf, uint32_t len);
bool tcp_recv_len_prefixed(int fd, uint8_t*& buf, uint32_t& len);
