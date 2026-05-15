# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
# macOS / Linux
make              # → build/easynet
make clean

# Windows (Visual Studio or MinGW)
mkdir build && cd build && cmake .. && cmake --build .
```

C++17. POSIX links `-lpthread`, Windows links `ws2_32`. Makefile is POSIX-only; CMake works everywhere.

## Architecture

`easynet` is a single-binary network bandwidth testing tool. Two instances cooperate: a **sender** injects traffic, a **receiver** listens and reports throughput. All CLI options use `--key=value` syntax.

```
easynet send --dst=<ip> --port=<n> --proto=tcp|udp (--rate=<r> --duration=<d> | --burst=<sz>)
easynet recv --port=<n> --proto=tcp|udp
```

## Platform abstraction — `platform.h`

All OS differences live in `src/platform.h` via `#ifdef _WIN32`. The rest of the codebase uses cross-platform types/functions and never `#ifdef`s itself (except `common.h` for signal handling, which fundamentally differs between POSIX `sigaction` and Win32 `SetConsoleCtrlHandler`).

| Platform API | POSIX | Windows |
|---|---|---|
| socket type | `int` | `SOCKET` |
| invalid check | `s >= 0` | `s != INVALID_SOCKET` |
| close | `close()` | `closesocket()` |
| shutdown | `SHUT_RDWR` | `SD_BOTH` |
| last error | `errno` | `WSAGetLastError()` |
| interrupted | `errno == EINTR` | `WSAGetLastError() == WSAEINTR` |
| init/cleanup | no-op | `WSAStartup` / `WSACleanup` |

**Rule:** Never use raw `close()`, `errno`, `perror()`, `strerror()` in socket code. Always go through:
- `platform_is_valid_socket(s)` — instead of `s >= 0` or `s < 0`
- `platform_close_socket(s)` — instead of `close()` / `closesocket()`
- `platform_shutdown_socket(s, how)` + `PLATFORM_SHUT_RDWR` — instead of raw `shutdown()`
- `platform_get_error()` — instead of `errno`
- `platform_is_eintr()` — instead of `errno == EINTR`
- `platform_strerror(err)` / `platform_print_error(msg)` — instead of `strerror()` / `perror()`
- `platform_ssize_t` — instead of `ssize_t` (which doesn't exist on Windows)
- `PLATFORM_INVALID_SOCKET` / `PLATFORM_SOCKET_ERROR` — instead of `-1`

## Source layout

| File | Role |
|------|------|
| `main.cpp` | CLI parsing (`has_prefix`/`get_value`), `platform_init/cleanup`, mode dispatch |
| `common.h` | `Config` and `Stats` structs, parse utilities, inline `setup_signal_handler` (one `#ifdef`), `g_running` |
| `platform.h` | All `#ifdef _WIN32` — socket types, init/cleanup, close, error, shutdown abstractions (header-only) |
| `sender.cpp` | Rate mode (10ms tick sleep-based batching) and burst mode, dispatched by protocol |
| `receiver.cpp` | TCP: accept → recv loop until EOF; UDP: bind → recvfrom loop until signal |
| `tcp.cpp` | Socket wrappers: connect, bind+listen+accept, `send_full`/`recv_full` (handle partial I/O), length-prefixed send/recv for burst framing |
| `udp.cpp` | Socket wrappers: create, bind, sendto, recvfrom |

## Key constants

- `UDP_PAYLOAD_MAX = 1472` — fits Ethernet MTU (1500 − IP header 20 − UDP header 8)
- `STATS_INTERVAL_SEC = 1` — per-second throughput reports
- Rate control: 100 Hz (10ms intervals), `sleep_until` for pacing, chunk = `rate_bps / 8 * 0.01`

## TCP framing

Burst mode prepends a 4-byte `htonl` length prefix so the receiver knows exact payload size. Rate mode streams until the sender closes the connection.

## Signal handling

`Ctrl+C` sets `g_running = 0`. All loops check this flag for graceful shutdown with final stats. POSIX uses `sigaction(SIGINT|SIGTERM)`, Windows uses `SetConsoleCtrlHandler(CTRL_C_EVENT|CTRL_BREAK_EVENT)`.
