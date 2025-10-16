# my-redis

A high-performance, in-memory key-value data store inspired by [Redis](https://redis.io/), implemented in modern C++.
This project aims to provide a lightweight, fast, and extensible database solution suitable for cache, message broker,
and real-time analytics workloads.

## Features

- **In-memory storage** for ultra-fast data access
- **Basic Redis commands**: `GET`, `SET`, `DEL`, and more
- **Persistence**: Optional snapshotting and AOF (Append Only File) support (planned)
- **Network server**: Accepts client connections over TCP
- **Extensible architecture**: Easily add new commands and data structures
- **Single-threaded event loop** for simplicity and performance

## Getting Started

### Prerequisites

- **C++17 or later**
- **CMake** (version 3.21+)
- **Linux, macOS, or Windows** (tested on Linux)

### Build

```bash
git clone https://github.com/Max-Sepp/my-redis.git
cd my-redis
mkdir build && cd build
cmake ..
make
```

### Run

```bash
./my_redis
```

The server listens on the default port `6379`. You can connect with `telnet`, `nc`, or a Redis client library.

### Example Usage (RESP format)

The server speaks the [RESP protocol](https://redis.io/docs/reference/protocol-spec/):

```
*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n
+OK\r\n
*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n
$3\r\nbar\r\n
*2\r\n$3\r\nDEL\r\n$3\r\nfoo\r\n
:1\r\n
```

This corresponds to the following commands:

- `SET foo bar` → `+OK`
- `GET foo` → `$3\r\nbar`
- `DEL foo` → `:1` (number of keys deleted)

## Roadmap

- [x] Basic command support
- [ ] Persistence (RDB/AOF)
- [ ] Pub/Sub messaging
- [ ] Multi-threaded event loop