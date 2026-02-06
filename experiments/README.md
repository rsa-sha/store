# store --- Experiments

This directory contains **deliberate experiments** run against the
`store` storage engine to understand behavior under stress, failure, and
edge conditions.

These are **not automated tests**. Each experiment is meant to: -
validate assumptions - surface failure modes - document unexpected
behavior - guide future design decisions

## How to use this directory

Each experiment should be documented in its own file using the following
structure:

### Experiment Name

Short descriptive title.

### Goal

What assumption or behavior is being tested?

### Setup

Configuration, flags, disk size, compression mode, etc.

### Steps

Exact commands or actions performed.

### Expected Outcome

What you believed would happen.

### Actual Outcome

What actually happened.

### Observations

Anything surprising, confusing, or unclear.

### Follow-ups

Ideas for fixes, mitigations, or further experiments.

## Example Experiments

-   Crash during append
-   Disk full during metadata update
-   Compression flag mismatch across restarts
-   Concurrent readers during write
-   Restart after partial superblock write

This directory is intentionally informal. Accuracy and honesty matter
more than completeness.
