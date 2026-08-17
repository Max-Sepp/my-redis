# notes/

Benchmark tooling for the v1-vs-v2 `my_redis_server` comparison. The written-up
findings live in [`NOTES.md`](NOTES.md); this folder holds the script that
collects the raw numbers.

## `benchmark.sh`

Runs the full SET/GET matrix — servers `{v1, v2}` × concurrency `-c {1,10,50}` ×
pipelining `-P {1,16}` — with a discarded warm-up before each measured
`valkey-benchmark` run, and dumps the results to `results/`.

Each cell is classified as `ok`, `crash` (server died mid-run), `hang` (exceeded
`--timeout`), or `error` (no result parsed). A crash restarts the server before
the next cell so one bad cell doesn't poison the rest.

### Prerequisites

- Release builds present: `build/v1/release/my_redis_server` and
  `build/v2/release/my_redis_server` (configure + build the `release` preset in
  each of `v1/` and `v2/`).
- `valkey-benchmark` (or redis-benchmark, adjust the binary name) on `PATH`.
- Ports 6379 (v1, hard-coded) and 6380 (v2) free.

### Usage

```sh
./notes/benchmark.sh                      # full matrix, both servers
./notes/benchmark.sh --quick              # smaller op counts, for a fast sanity run
./notes/benchmark.sh --only v2            # one server only (v1|v2)
./notes/benchmark.sh --format csv         # csv | json | both (default both)
./notes/benchmark.sh --timeout 90         # seconds before a cell is called a hang
```

### Output

Written to `results/` (git-ignored), timestamped, with `latest.*` symlinks:

- `benchmark-<stamp>.csv` — one row per cell:
  `server,command,concurrency,pipeline,n_ops,rps,p50_ms,status`
- `benchmark-<stamp>.json` — same rows plus a `meta` block (host, CPU, kernel,
  tool version, git commit, parameters).
- `benchmark-<stamp>.meta.txt` — the run metadata on its own.

Numbers are single runs on whatever host you run them on; treat them as
order-of-magnitude and prefer the relative v1-vs-v2 differences, as `NOTES.md`
explains.
