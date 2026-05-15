# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
make          # build → build/easynet
make clean    # remove build/
```

Also `cmake .. && make` from `build/` if cmake is available. C++17, links `-lpthread`.

## Architecture

`easynet` is a single-binary network packet injection tool for bandwidth testing. Two instances cooperate: a **sender** injects traffic, a **receiver** listens and reports throughput.

**CLI convention:** All options use `--key=value` syntax (not `--key value`).

```
easynet send --dst=<ip> --port=<n> --proto=tcp|udp (--rate=<r> --duration=<d> | --burst=<sz>)
easynet recv --port=<n> --proto=tcp|udp
```

**Source layout:**

| File | Role |
|------|------|
| `main.cpp` | CLI parsing (`--key=value` via `has_prefix`/`get_value`), mode dispatch |
| `common.h` | `Config` and `Stats` structs, parse utilities (`parse_rate/size/duration`), signal handler, `g_running` flag |
| `sender.cpp` | Rate mode (10ms tick sleep-based batching) and burst mode, dispatched by protocol |
| `receiver.cpp` | TCP: accept → recv loop until EOF; UDP: bind → recvfrom loop until signal |
| `tcp.cpp` | Raw POSIX socket wrappers: connect, bind+listen+accept, send_full/recv_full (handle partial I/O), length-prefixed send/recv for burst mode |
| `udp.cpp` | Raw POSIX socket wrappers: create, bind, sendto, recvfrom |

**Key constants in `common.h`:**
- `UDP_PAYLOAD_MAX = 1472` — fits Ethernet MTU (1500 − IP 20 − UDP 8)
- `STATS_INTERVAL_SEC = 1` — throughput printed every second

**Rate control:** Sleep-based batching at 100 Hz (10ms interval). Each tick sends `rate_bps / 8 * 0.01` bytes. Uses `std::chrono::steady_clock` with `sleep_until` for accurate pacing.

**TCP framing:** Burst mode prepends a 4-byte network byte order (`htonl`) length prefix so the receiver knows exact payload size. Rate mode just streams until the sender closes the connection.

**Signal handling:** `SIGINT`/`SIGTERM` set `g_running = 0`; all loops check this flag for graceful shutdown with final stats.

**Stats:** Both sender and receiver print per-second current throughput (Mbit/s), total bytes, and packet count. A final summary shows average rate over the full run.
