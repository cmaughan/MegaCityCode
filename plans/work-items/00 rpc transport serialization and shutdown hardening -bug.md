# 00 RPC Transport Serialization And Shutdown Hardening

## Why This Exists

This is the highest-priority correctness risk in the current tree.

Current verified issues:

- `NvimRpc::request()` and `NvimRpc::notify()` can write from different threads with no write serialization
- the resize worker can still be in a blocking RPC call during shutdown
- clipboard copy/paste paths still rely on synchronous RPC from UI-facing flows

If writes interleave, msgpack frames can corrupt each other. If shutdown waits on in-flight blocking calls, exit can still feel brittle.

## Goal

Make the RPC transport safe under concurrent use and predictable under shutdown.

## Implementation Plan

1. Add explicit write serialization to `NvimRpc`.
   - add a dedicated write mutex or a single outbound writer queue
   - keep the first version simple: mutex first, queue only if needed later
2. Make shutdown semantics explicit.
   - mark transport closed before joining workers
   - unblock pending requests quickly on shutdown/read failure
   - ensure worker threads stop queuing new RPC after shutdown begins
3. Reduce UI-thread blocking.
   - route clipboard copy/paste and any remaining UI-only request paths through a worker or callback-based flow
   - keep synchronous RPC only where startup/test code truly needs it
4. Add logging around transport-close reasons.
   - distinguish timeout, read failure, shutdown, and child exit in logs

## Tests

- add a multithreaded RPC stress test mixing `request()` and `notify()`
- add a shutdown test with a request in flight
- add a direct timeout-vs-shutdown-vs-read-failure distinction test
- keep the existing RPC integration tests green

## Suggested Slice Order

1. write mutex
2. shutdown/unblock cleanup
3. clipboard/UI-path async conversion
4. stress/lifecycle tests

## Sub-Agent Split

- one agent on `spectre-nvim` transport changes
- one agent on app-side request callsites and clipboard flow
- merge only after the transport contract is settled
