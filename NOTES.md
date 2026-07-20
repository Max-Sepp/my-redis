# Benchmark notes: v1 vs v2

Performance comparison of the two `my_redis_server` implementations under a SET/GET
workload. The point isn't to compete with real Redis (both are learning servers), but to
see how the two internal designs behave and whether the v2 rebuild actually bought anything.

## What's being compared

- v1: a single epoll accept loop feeding a pool of worker threads (`RequestExecutor`).
- v2: a pool of IO threads parse client bytes into RESP requests and hand them to a single
  command-executor thread over an `eventfd`. That single executor owns the store, so command
  execution needs no locking.

Both speak RESP and support `SET`/`GET`/`DEL`; `SET`/`GET` is the comparable subset.

## Setup

- Host: Intel Core i5-10210U (4 cores / 8 threads), Fedora, Linux 6.x.
- Compiler: GCC 15.2.1, CMake **release** preset (optimized) for both trees.
- Tool: `valkey-benchmark` 8.1.8 (redis-benchmark compatible), loopback, 3-byte values.
- Method: for each cell, a warm-up run is discarded, then a measured run of 30k to 50k ops
  per command (200k for the v2 pipelined-at-scale cell). Servers run one at a time; v1 on
  6379, v2 on 6380. Numbers are single runs on an idle laptop, so treat them as
  order-of-magnitude, not precise.
- Commit: `822ee06`.

Matrix: concurrency `-c ∈ {1, 10, 50}` × pipelining `-P ∈ {1, 16}`.

## Results

Throughput in requests/sec (higher is better); p50 latency in ms in parentheses.

| Workload            | v1 SET          | v1 GET          | v2 SET          | v2 GET          |
| ------------------- | --------------- | --------------- | --------------- | --------------- |
| c=1,  P=1           | 3,110 (0.30)    | 3,273 (0.30)    | 6,015 (0.13)    | 7,620 (0.11)    |
| c=10, P=1           | 24,793 (0.24)   | 38,119 (0.21)   | 41,494 (0.16)   | 38,344 (0.16)   |
| c=50, P=1           | **crash**       | **crash**       | 39,463 (0.59)   | 46,555 (0.58)   |
| c=1,  P=16          | ~388 (0.56)     | ~collapse       | **hang**        | **hang**        |
| c=50, P=16          | **crash**       | **crash**       | 18,877 (1.21)   | 18,829 (1.23)   |

"crash" = server segfaulted (SIGSEGV, core dumped). "hang" = benchmark never completed;
server stayed alive but stopped making progress. "collapse" = completes, but throughput
falls off a cliff.

## What the numbers say

Single connection, v2 is about twice as fast. At c=1/P=1 the workload is latency-bound: one
request, wait for the reply, repeat. v2 turns each round trip around in roughly half the time
(~0.12 ms vs ~0.30 ms p50), so it does about twice the throughput. The gap comes from
per-request overhead on v1's path rather than a throughput ceiling.

Under moderate concurrency they converge. At c=10 both land near ~38k GET. v1's worker thread
pool parallelizes across connections, so v1 catches up (GET is a dead heat; v1 SET is still
slower, ~25k vs ~41k). This is the one regime where v1 looks fine.

At high concurrency v1 falls over. At c=50 v1 segfaults (reproducibly under sustained load,
intermittently under short bursts, since it's a race rather than a clean capacity limit). v2
handles the same load and reaches ~40k SET / ~47k GET. So the c=50 result is about safety,
not throughput: v1 crashes where v2 keeps serving.

Pipelining is broken in both servers, in different ways.

- v1: pipelining *hurts*. With `-P 16` on one connection throughput collapses to ~390 rps,
  ~8× slower than the same connection without pipelining. v1 doesn't drain a batch of queued
  commands efficiently.
- v2: single-connection pipelining hangs outright (the benchmark never finishes; the server
  is still alive). It only makes progress once many connections are feeding it (c=50/P=16
  works, at ~19k). Even then pipelining is a net *loss* for v2: c=50/P=1 does ~40k, and
  turning pipelining on drops it to ~19k. The single-executor plus per-command `eventfd`
  handoff doesn't benefit from batching the way a real server's pipeline does.

## Takeaways

- The v2 rebuild's real win is robustness and single-connection latency, not peak throughput.
  It survives c=50 where v1 crashes, and it's about twice as fast on a lone connection.
- v1's main weakness is a concurrency race that segfaults under load. Its per-request path is
  also heavier, so single-connection latency runs about twice v2's. The thread-pool model
  otherwise scales acceptably at moderate concurrency.
- Pipelining barely works in either server. For v1 it's an 8× regression; for v2 it stalls on
  a single connection and loses throughput even when it runs. If pipelined throughput matters,
  this is the biggest gap between these servers and a production one, and the clearest thing
  left to fix, especially v2's per-command executor handoff. That handoff is the throughput
  ceiling: peak here is ~47k rps, where real Redis does far more.

## Caveats

- Loopback only, tiny (3-byte) values, snapshotting disabled, single idle host, single runs.
- Absolute numbers are not portable; the *relative* differences and the crash/hang/collapse
  behaviors are the durable findings.
- v1's c=50 crash is a race. It doesn't fail on every run, but it fails often enough under
  sustained load that no stable number could be recorded.
- Reproduce with the release binaries under `build/v{1,2}/release/` and
  `valkey-benchmark -t set,get -n <N> -c <C> -P <P> -d 3` (v2 with `-p 6380`).
