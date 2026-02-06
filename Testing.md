# store --- Testing Plan

## General Testing

-   Initialize store with default and custom TOML configurations; verify
    superblock correctness.
-   Create files, write fixed data, read back, and validate
    byte-for-byte equality.
-   Append multiple records sequentially; ensure EOF tracking and offset
    correctness.
-   Validate error handling for non-existent files and invalid configs.
-   Restart store after normal shutdown; verify metadata and data
    integrity.

## Performance Testing

-   Measure sequential write and append throughput and compare with:
    -   plain file I/O using `dd`
    -   SQLite in WAL mode (as a rough baseline)
-   Measure read latency for small vs large records.
-   Compare compression on vs off (CPU cost vs disk savings).
-   Track syscall counts and I/O patterns using `strace`.

## Memory Testing

-   Monitor RSS and VSZ during sustained write and read workloads.
-   Detect memory leaks using long-running loops with repeated
    operations.
-   Test behavior under constrained memory (via `ulimit`).
-   Validate that buffers are released after operations.

## Fool Testing

-   Kill process mid-write and mid-append; restart and validate
    consistency.
-   Fill disk to capacity and attempt further writes.
-   Toggle compression flags inconsistently between restarts.
-   Simulate power loss by abrupt termination during metadata updates.
