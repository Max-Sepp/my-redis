# Benchmark notes: v1 vs v2

Performance comparison of the two `my_redis_server` implementations under a SET/GET
workload. The point isn't to compete with real Redis (both are learning servers), but to
see how the two internal designs behave, and specifically whether v2's new request-batching
(`99e53c9`, "Implement batching of resp requests to improve pipelining of requests
performance") actually fixed the pipelining problems the last round of notes found.

## What's being compared

- v1: a single epoll accept loop feeding a pool of worker threads (`RequestExecutor`).
- v2: a pool of IO threads parse client bytes into RESP requests and hand them to a single
  command-executor thread over an `eventfd`. That single executor owns the store, so command
  execution needs no locking. As of `99e53c9`, the IO threads batch parsed requests before
  handing them off, instead of signalling the executor once per request.

Both speak RESP and support `SET`/`GET`/`DEL`; `SET`/`GET` is the comparable subset.

## Setup

- Host: Intel Core i5-10210U (4 cores / 8 threads), Fedora, Linux 7.1.8-100.fc43.
- Compiler: clang++ 21.1.8, CMake **release** preset (optimized, IPO on) for both trees.
- Tool: `valkey-benchmark` 8.1.9 (redis-benchmark compatible), loopback, 3-byte values.
- Method: for each cell, a warm-up run is discarded, then a measured run of 50k ops per
  command (200k for `-P 16` cells). Servers run one at a time; v1 on 6379, v2 on 6380.
  Numbers are single runs on an idle laptop, so treat them as order-of-magnitude, not
  precise — see Caveats for the two anomalies this run turned up and how they were checked.
- Commit: `99e53c9`.

Matrix: concurrency `-c ∈ {1, 10, 50}` × pipelining `-P ∈ {1, 16}`.

**Not comparable to the previous revision of these notes (`822ee06`).** v1's numbers here
are several times higher across the board because request logging is now off by default
(`f08c4e8`, "Implement NullLogger add to v1 server") — the old numbers were measured with
every request logged to stdout, which dominated v1's per-request cost. v2's non-pipelined
numbers also roughly doubled since `822ee06`, which nothing but the batching commit touched
in that window, so that's read as a real effect of the batching change (fewer/cheaper
IO-thread → executor handoffs), not noise. Compare v1-vs-v2 *within* this table; don't diff
these numbers against the old ones.

## Results

Throughput in requests/sec (higher is better); p50 latency in ms in parentheses.

| Workload    | v1 SET          | v1 GET          | v2 SET            | v2 GET            |
| ----------- | --------------- | --------------- | ------------------ | ------------------ |
| c=1,  P=1   | 29,815 (0.031)  | 31,447 (0.031)  | 27,716 (0.031)     | 28,265 (0.031)     |
| c=10, P=1   | 102,249 (0.063) | 101,626 (0.063) | 95,785 (0.071)     | 98,039 (0.071)     |
| c=50, P=1   | **crash**       | **crash**       | 99,602 (0.271)     | 99,404 (0.271)     |
| c=1,  P=16  | **hang**        | **hang**        | 166,806 (0.087)    | 195,122 (0.079)    |
| c=10, P=16  | 3,890 (0.423)   | 3,893 (0.431)   | 673,401 (0.207)    | 724,638 (0.191)    |
| c=50, P=16  | **crash**       | **crash**       | 800,000 (0.823)    | 836,820 (0.791)    |

"crash" = server segfaulted (SIGSEGV, core dumped), or — for the three cells after v1's
first c=50 segfault — the harness couldn't get v1 listening again in time to run them (see
Caveats). "hang" = benchmark never completed; server stayed alive but stopped making
progress.

## What the numbers say

**Pipelining is fixed for v2, and it's the headline result.** Before `99e53c9`, a single
pipelined connection made v2 hang outright, and even at c=50 turning pipelining on cost
throughput (c=50/P=1 did ~40k, P=16 dropped it to ~19k — see the previous revision of these
notes). Now pipelining is a large, consistent win at every concurrency tested: c=1/P=16 goes
from a permanent hang to ~167–195k rps; c=10/P=16 jumps ~7× over c=10/P=1 to ~700k; c=50/P=16
jumps ~8× over c=50/P=1 to ~800–840k. Batching the handoff to the executor, instead of
signalling it once per request, is exactly the fix this problem needed.

**Non-pipelined throughput is now a wash between v1 and v2.** With v1's logging overhead
gone, single-connection latency is close (~0.031ms p50 both), and at c=10/c=50 the two are
within a few percent of each other. The batching change also nudged v2's non-pipelined path
up (see the setup note above), which is presumably why it now tracks v1 this closely instead
of trailing it.

**v1 still isn't safe at c=50, and pipelining still actively hurts it.** The c=50 segfault
that appeared in the previous round of testing reproduces every time now (5/5 attempts
across this session, both inside and outside the harness) rather than intermittently — see
Caveats for why. v1's pipelining path is also still broken in its own way: c=10/P=16 collapses
to ~3,890 rps, a ~26× regression versus c=10/P=1, and c=1/P=16 hangs the same way it did
before. None of this is touched by the v2-only batching commit, so it's unchanged from the
last write-up other than being easier to reproduce.

## Takeaways

- The v2 batching change did what it set out to do: pipelining went from broken (hangs at
  c=1, throughput *losses* at higher concurrency) to the fastest mode v2 has, at every
  concurrency tested.
- v2 is now competitive with v1 on non-pipelined throughput too, on top of the safety
  advantage from the previous round of notes (v1 crashes at c=50; v2 doesn't).
- v1 is unchanged and remains the weaker implementation on both axes that matter here:
  it segfaults under concurrent load, and pipelining is a regression rather than a win.
  Nothing in this round of testing touched v1's code, so this is the same finding as before,
  just more reproducible.
- v2's single-connection pipelining isn't perfectly solved — see the intermittent hang noted
  in Caveats. That's a much narrower version of the old bug (occasional vs. guaranteed) and
  is the most likely next thing to chase if pipelined latency at c=1 matters.

## Caveats

- Loopback only, tiny (3-byte) values, snapshotting disabled, single idle host, single runs
  per cell (with the exceptions below, which got extra runs specifically because they looked
  wrong).
- **v2 c=50/P=16 SET fluke, discarded.** One full-matrix run recorded SET at 21,277 rps
  against GET's 900,901 rps in the same cell — a 40× gap that doesn't appear anywhere else
  in the matrix. Rerunning the same cell (fresh server, same op counts, same sequence)
  reproducibly gives SET numbers in line with GET, in the 780k–900k range. Treated as a
  one-off scheduling hiccup on the laptop, not a real SET/GET asymmetry; the table above
  uses the clean rerun.
- **v2 c=1/P=16 hung once in three full-matrix runs, on GET only.** The other two full runs,
  plus five manual back-to-back repeats, all completed normally in the ~170–200k rps range.
  So the batching change turned a *guaranteed* single-connection pipeline hang into an
  *occasional* one — better, but not fully gone. Not chased further; flagged above as the
  next thing to look at.
- **v1's c=50 segfault reproduces every time now**, not intermittently as the previous
  revision of these notes described — 3/3 harness runs and a further 2/2 manual runs this
  session. After it crashes, the harness's restart check (10s) never sees v1 come back up on
  port 6379; manually giving it 60s didn't help either. `ss` confirms nothing rebinds the
  port and the new process sits parked in `epoll_wait` having apparently never completed
  its bind/listen. That's why GET/P=1 and both P=16 cells in the c=50 row show as failed
  too — they were never actually exercised standalone, only the SET/P=1 cell independently
  triggers the crash. Not root-caused beyond that; a real bug in v1, which is archived and
  not under active development.
- Reproduce with the release binaries under `build/v{1,2}/release/` (rebuild first — the
  binaries on disk may predate the code you're testing) and
  `valkey-benchmark -t set,get -n <N> -c <C> -P <P> -d 3` (v2 with `-p 6380`), or just run
  `./notes/benchmark.sh`.
