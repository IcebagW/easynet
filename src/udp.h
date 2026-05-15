#pragma once

#include <cstdint>
#include <cstddef>
#include <netinet/in.h>

int  udp_create_socket();
int  udp_bind(int fd, int port);
void udp_close(int fd);
bool udp_sendto(int fd, const void* buf, size_t len, const char* ip, int port);
ssize_t udp_recvfrom(int fd, void* buf, size_t len, sockaddr_in* src);
