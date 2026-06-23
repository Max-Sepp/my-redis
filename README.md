# my-redis

A from-scratch reimplementation of [Redis](https://redis.io/) in modern C++, used as a vehicle
for learning how the internals actually work.

This repository holds **two implementations side by side**:

| Folder | What it is | Status |
| ------ | ---------- | ------ |
| [`v1/`](v1/) | The initial implementation, built by reasoning from first principles. | Archived for reference — builds and passes its tests, but no longer actively developed. |
| [`v2/`](v2/) | A redesign informed by studying how a real server actually works. | Active. |

## Why two folders?

**`v1/`** was a first pass: a complete, working server + client written with only a rough,
intuitive idea of how an in-memory data store like Redis is put together internally. It was
about getting something running end to end and learning by doing.

**`v2/`** is the redesign that came *after* digging into the internals of a production server —
specifically [Valkey](https://valkey.io/) (the open-source Redis fork) — and understanding how
the pieces really fit together. With that knowledge, the architecture is reconsidered from the
ground up rather than patched onto the original.

Rather than rewrite everything blindly, `v2/` keeps the pieces of `v1/` that were well built and
worth carrying forward, and reimplements the rest (notably the server) on a cleaner foundation.

### Reused building blocks

`v2/` is seeded with copies of these `v1/` packages:

- **`client/`** — the interactive RESP client (`connect`, `worker`, `string_helpers`, `main`).
- **`concurrent/`** — `ConcurrentQueue`, a header-only thread-safe queue.
- **`resp_value/`** — RESP protocol value parsing/serialization (`RespValue`, `RespValueQueue`,
  and the `resp_values` constructor helpers).
- **`network/`** — `send_all` (a dependency of the client; carried along so it builds).

In `v2/` these have been brought up to a consistent baseline:

- All code lives under a single **`namespace myredis`**.
- Identifiers and file names follow the **[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)**
  (e.g. `RespValue::serialize()` → `Serialize()`, `RespValue.cpp` → `resp_value.cc`).

`v1/` is left untouched, so the two trees can be compared directly.

## Building

A top-level `Makefile` builds both trees. All output is written under a single top-level
`build/` directory (`build/v1/<preset>/` and `build/v2/<preset>/`).

```bash
make debug        # build v1 and v2 (Debug)
make release      # build v1 and v2 (Release)
make v2-debug     # build only v2 (Debug)
make test         # build (debug) and run v1's test suites
make clean        # remove build/
make help         # list all targets
```

> The `v2` server is still being reimplemented — see the `TODO(v2)` in `v2/CMakeLists.txt`.

Each folder is also an independent CMake project if you prefer to build one directly:

```bash
cd v2 && cmake --preset debug && cmake --build --preset debug
# -> binaries land in build/v2/debug/ (currently: my_redis_client)
```

## Layout

```
.
├── v1/        # original implementation (server + client + tests)
└── v2/        # reimplementation
    └── src/
        ├── client/        # reused: interactive RESP client
        ├── concurrent/    # reused: ConcurrentQueue (header-only)
        ├── network/       # reused: send_all
        └── resp_value/    # reused: RESP parsing/serialization
```

## RESP protocol

Both implementations speak the [RESP protocol](https://redis.io/docs/reference/protocol-spec/),
e.g. `SET foo bar` → `+OK`, `GET foo` → `$3\r\nbar\r\n`, `DEL foo` → `:1`.
