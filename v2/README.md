# my-redis v2

The active redesign. See the [top-level README](../README.md) for background and build
instructions.

## TODO

- [ ] **Remove the O(n) front erase in `Connection::Flush`** (`src/server/connection.cc`).
  `out_buffer_.erase(0, sent)` shifts every unsent byte down on each partial send, so draining an
  N-byte backlog to a slow client that accepts k bytes per `EPOLLOUT` copies roughly N²/k bytes.
  Options, from least to most change:
  1. **Read offset with lazy compaction (do this first).** Track `out_offset_`, send from
     `data() + out_offset_`, `clear()` when the offset reaches the end, and compact only once it
     passes half the buffer. Each compaction moves at most as many bytes as were already sent, so
     the cost averages out to O(1) per byte.
  2. **Deque of chunks plus `writev` (what Redis/Valkey do).** No byte is moved after it is copied
     in, and many small responses go out in one syscall. Use fixed-size blocks or a free list to
     avoid an allocation per chunk (Valkey's `clientReplyBlock`). This fits the inbox, which now
     splits large responses across records.
  3. **Growable byte ring buffer.** O(1) at both ends with no per-chunk allocation, and a wrapped
     region is two `iovec`s, but growing means reallocating and unwrapping. Only worth it if
     allocation shows up in profiles.
