# easynet

网络灌包工具，用于网络带宽测试。支持 TCP/UDP 协议，双端配合测试上下行带宽。

## 编译

```bash
make
```

要求 C++17，依赖 POSIX socket（macOS / Linux 均可编译）。

## 使用方式

工具采用双端配合模式：一端作为**接收端**（recv），另一端作为**发送端**（send）。两台机器各自运行 easynet，互为收发。

### 接收端

在下行测试的目标机器上启动：

```bash
easynet recv --port=<端口> --proto=tcp|udp
```

接收端会持续监听，实时打印吞吐量统计，直到 Ctrl+C 退出。

### 发送端 — 固定速率模式

以恒定速率持续灌包一段时间：

```bash
easynet send --dst=<目标IP> --port=<端口> --proto=tcp|udp --rate=<速率> --duration=<时长>
```

**示例：** 以 10 Mbit/s 发送 TCP 流量，持续 30 秒

```bash
easynet send --dst=192.168.1.100 --port=8080 --proto=tcp --rate=10m --duration=30s
```

### 发送端 — 突发模式

发送一个指定大小的数据包：

```bash
easynet send --dst=<目标IP> --port=<端口> --proto=tcp|udp --burst=<大小>
```

**示例：** 向目标发送 10 MB 的 UDP 载荷

```bash
easynet send --dst=192.168.1.100 --port=8080 --proto=udp --burst=10mb
```

## 参数说明

| 参数 | 含义 | 取值 |
|------|------|------|
| `--dst` | 目标 IP 地址 | 如 `192.168.1.100` |
| `--port` | 端口号 | 1–65535 |
| `--proto` | 传输协议 | `tcp` 或 `udp` |
| `--rate` | 灌包速率 | 数字 + 后缀：`k`(Kbit/s) / `m`(Mbit/s) / `g`(Gbit/s) |
| `--duration` | 持续时间 | 数字 + 后缀：`s`(秒) / `m`(分钟) / `h`(小时) |
| `--burst` | 突发包大小 | 数字 + 后缀：`kb`(KB) / `mb`(MB) / `gb`(GB) |

注意：所有参数使用 `=` 分隔键值，如 `--rate=10m`。

## 工作模式详解

### 固定速率模式

以指定的恒定速率发送数据，适用于带宽稳定性测试。

- **速率控制：** 100Hz 发送频率，每 10ms 发送固定大小的数据块。块大小 = 速率(bps) / 8 × 0.01s
- **TCP：** 建立连接后持续写到对端，时间到后关闭连接
- **UDP：** 每个 tick 发送多个 1472 字节的数据报达到目标速率

### 突发模式

一次性发送指定大小的数据载荷，适用于单次大包传输测试。

- **TCP：** 连接后先发 4 字节（网络字节序）长度前缀，再发数据体。接收端据此知道数据边界
- **UDP：** 将大数据拆分为 1472 字节的数据报逐个发送

### TCP vs UDP

| 特性 | TCP | UDP |
|------|-----|-----|
| 可靠性 | 可靠传输，无丢包 | 可能丢包 |
| 流边界 | 字节流，无消息边界 | 数据报，保留边界 |
| 速率控制 | 受拥塞控制影响 | 接近设定速率 |
| 数据报大小 | 无限制（流） | 单包 ≤ 1472 字节 |
| 突发模式协议 | 4 字节长度前缀 | 简单拆分 |

## 典型场景

### 带宽压测

```bash
# 机器 B（接收端）
easynet recv --port=9999 --proto=tcp

# 机器 A（发送端）：100 Mbit/s 灌入 60 秒
easynet send --dst=<B的IP> --port=9999 --proto=tcp --rate=100m --duration=60s
```

### UDP 丢包测试

```bash
# 机器 B 监听 UDP
easynet recv --port=9999 --proto=udp

# 机器 A 发送 50 Mbit/s
easynet send --dst=<B的IP> --port=9999 --proto=udp --rate=50m --duration=30s
```

对比收发两端的统计可计算丢包率。

### 单包时延评估

```bash
# 机器 B
easynet recv --port=8888 --proto=tcp

# 机器 A 发 100MB 大包
easynet send --dst=<B的IP> --port=8888 --proto=tcp --burst=100mb
```

根据耗时和总字节数计算有效吞吐率。

## 输出示例

接收端实时输出：

```
[RX] 1 s | cur: 100.52 Mbit/s | total: 12.57 MB | pkts: 204
[RX] 2 s | cur: 101.00 Mbit/s | total: 25.19 MB | pkts: 406
Connection closed by peer.

--- TCP Receive Summary ---
  Duration: 3.00 s
  Total received: 37.50 MB (37500000 bytes)
  Packets: 604
  Avg rate: 99.93 Mbit/s
```

每行显示：当前速率 / 累计数据量 / 累计包数。结束后打印汇总（持续时间 / 总数据量 / 平均速率）。

## 项目结构

```
easynet/
├── Makefile / CMakeLists.txt    # C++17，链接 pthread
├── src/
│   ├── main.cpp                 # CLI 入口
│   ├── common.h                 # 配置结构体、统计、工具函数
│   ├── sender.cpp / sender.h    # 发送端（速率 + 突发）
│   ├── receiver.cpp / receiver.h # 接收端（统计上报）
│   ├── tcp.cpp / tcp.h          # TCP socket 封装
│   └── udp.cpp / udp.h          # UDP socket 封装
└── build/
    └── easynet                  # 编译产物
```
