#pragma once

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <cstdio>

    using platform_socket_t = SOCKET;
    using platform_ssize_t = int;

    #define PLATFORM_INVALID_SOCKET  INVALID_SOCKET
    #define PLATFORM_SOCKET_ERROR    SOCKET_ERROR
    #define PLATFORM_SHUT_RDWR       SD_BOTH

    inline int platform_init() {
        WSADATA wsa;
        return WSAStartup(MAKEWORD(2, 2), &wsa);
    }
    inline void platform_cleanup() { WSACleanup(); }

    inline bool platform_is_valid_socket(platform_socket_t s) { return s != INVALID_SOCKET; }
    inline int  platform_close_socket(platform_socket_t s)    { return closesocket(s); }
    inline int  platform_shutdown_socket(platform_socket_t s, int how) { return shutdown(s, how); }

    inline int  platform_get_error()          { return WSAGetLastError(); }
    inline bool platform_is_eintr()           { return WSAGetLastError() == WSAEINTR; }
    inline bool platform_is_econnreset()      { return WSAGetLastError() == WSAECONNRESET; }

    inline const char* platform_strerror(int err) {
        static thread_local char buf[256];
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, err, 0, buf, sizeof(buf), nullptr);
        return buf;
    }
    inline void platform_print_error(const char* msg) {
        fprintf(stderr, "%s: %s\n", msg, platform_strerror(WSAGetLastError()));
    }

#else  // POSIX
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <cerrno>
    #include <cstring>
    #include <cstdio>
    #include <csignal>

    using platform_socket_t = int;
    using platform_ssize_t = ssize_t;

    #define PLATFORM_INVALID_SOCKET  (-1)
    #define PLATFORM_SOCKET_ERROR    (-1)
    #define PLATFORM_SHUT_RDWR       SHUT_RDWR

    inline int  platform_init()                   { return 0; }
    inline void platform_cleanup()                {}
    inline bool platform_is_valid_socket(platform_socket_t s) { return s >= 0; }
    inline int  platform_close_socket(platform_socket_t s)    { return close(s); }
    inline int  platform_shutdown_socket(platform_socket_t s, int how) { return shutdown(s, how); }

    inline int         platform_get_error()      { return errno; }
    inline bool        platform_is_eintr()       { return errno == EINTR; }
    inline bool        platform_is_econnreset()  { return errno == ECONNRESET; }
    inline const char* platform_strerror(int err) { return strerror(err); }
    inline void        platform_print_error(const char* msg) { perror(msg); }
#endif
