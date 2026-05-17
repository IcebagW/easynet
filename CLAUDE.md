# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
# macOS / Linux
make              # → build/easynet

# Windows (MinGW via SSH: `ssh win`)
cd C:\Users\94900\easynet-src && cmake -B build -G "MinGW Makefiles" && cmake --build build

# CI (GitHub Actions) builds ubuntu/macos/windows on push & release
```

C++17. macOS `-lpthread`, Windows `ws2_32`. `Makefile` is POSIX-only; CMake works everywhere.

## Architecture

Single-binary network bandwidth testing tool. Two instances cooperate: **sender** injects traffic, **receiver** listens and reports throughput.

```
easynet send --dst=<ip> --port=<n> --proto=tcp|udp (--rate=<r> --duration=<d> | --burst=<sz>) [--threads=<n>]
easynet recv --port=<n> --proto=tcp|udp [--threads=<n>]
```

All CLI options use `--key=value` syntax. `--threads` range 1–64, splits rate evenly across independent sockets.

## Source layout

| File | Role |
|------|------|
| `main.cpp` | CLI parsing, `platform_init/cleanup`, mode dispatch |
| `common.h` | `Config`, `Stats`, `SOCKET_BUF_SIZE` constant, parse utilities, `setup_signal_handler` |
| `platform.h` | All `#ifdef _WIN32` — socket types, init/cleanup, error, shutdown (header-only) |
| `sender.cpp` | Multi-threaded sender: spawns N threads, each with own socket at rate/N. Rate: 100Hz sleep-based batching. Burst: length-prefixed (TCP) or fragmented (UDP). |
| `receiver.cpp` | TCP: accept loop with SO_RCVTIMEO poll, multi-connection + mutex-protected Stats. UDP: single-threaded recvfrom loop. |
| `tcp.cpp` | Socket wrappers: connect (SO_SNDBUF=2MB), bind+listen+accept (SO_RCVBUF=2MB), `send_full`/`recv_full`, length-prefixed framing for burst mode |
| `udp.cpp` | Socket wrappers: create (SO_SNDBUF+SO_RCVBUF=2MB), sendto, recvfrom |

## Key constants & tuning

- `UDP_PAYLOAD_MAX = 1472` — Ethernet MTU-safe datagram
- `SOCKET_BUF_SIZE = 2MB` — SO_SNDBUF/SO_RCVBUF on every socket (BDP limit ~5.3 Gbps per connection)
- `STATS_INTERVAL_SEC = 1` — throughput printed each second
- Rate control: 100Hz (10ms), `sleep_until`, chunk = `rate_bps / 800`

## Platform abstraction rules

Never use raw `close()`, `errno`, `perror()`, `ssize_t` in socket code. Always go through `platform.h` wrappers:

`platform_is_valid_socket(s)` / `platform_close_socket(s)` / `platform_shutdown_socket(s, how)` + `PLATFORM_SHUT_RDWR` / `platform_get_error()` / `platform_is_eintr()` / `platform_is_econnreset()` / `platform_strerror(err)` / `platform_print_error(msg)` / `platform_ssize_t` / `PLATFORM_INVALID_SOCKET` / `PLATFORM_SOCKET_ERROR`

## Windows-specific gotchas

1. **SO_RCVTIMEO inheritance**: Windows `accept()` inherits `SO_RCVTIMEO` from the listen socket. After `accept()`, must explicitly `setsockopt(client_fd, SO_RCVTIMEO, 0)` to clear it, otherwise `recv()` times out after 1s (WSAETIMEDOUT 10060).
2. **Admin SSH key path**: Administrator accounts use `C:\ProgramData\ssh\administrators_authorized_keys`, not `~/.ssh/authorized_keys`.
3. **SSH encoding**: Windows uses GBK, macOS UTF-8. Configure PowerShell profile with `[Console]::OutputEncoding = [Text.Encoding]::UTF8` and set SSH default shell to PowerShell.

## TCP framing

Burst mode prepends a 4-byte `htonl` length prefix. Rate mode streams until the sender closes the connection.

## Signal handling

`Ctrl+C` → `g_running = 0` → graceful shutdown with final stats. POSIX: `sigaction`, Windows: `SetConsoleCtrlHandler`.

## Testing

```bash
# Local loopback (macOS)
make && ./build/easynet recv --port=19999 --proto=tcp &
./build/easynet send --dst=127.0.0.1 --port=19999 --proto=tcp --rate=500m --duration=3s

# Cross-platform (macOS → Windows)
# 1. Start receiver on Windows: C:\Users\94900\easynet.exe recv --port=7353 --proto=tcp --threads=8
# 2. Sender on macOS:
./build/easynet send --dst=192.168.1.46 --port=7353 --proto=tcp --rate=1g --duration=10s --threads=8
```

WiFi limits: TCP ~264 Mbps (8 threads), UDP ~500 Mbps stable. Wired Ethernet expected to reach line rate.
