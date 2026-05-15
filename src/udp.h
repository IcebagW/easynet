#pragma once

#include "platform.h"
#include <cstdint>
#include <cstddef>

platform_socket_t udp_create_socket();
int  udp_bind(platform_socket_t fd, int port);
void udp_close(platform_socket_t fd);
bool udp_sendto(platform_socket_t fd, const void* buf, size_t len, const char* ip, int port);
platform_ssize_t udp_recvfrom(platform_socket_t fd, void* buf, size_t len, sockaddr_in* src);
