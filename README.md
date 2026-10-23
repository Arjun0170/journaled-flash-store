# Journaled Flash Store

A small, software-only key-value store built over a simulated NOR flash driver
in C11. It supports eight keys with values up to 32 bytes, append-only records,
deletion tombstones, CRC-32 validation, and two-sector compaction. Every sector
and record is committed only after its data is written.

The design follows the same broad idea used by embedded nonvolatile stores:
write data first, then publish metadata that makes it visible. See the
[Zephyr NVS design](https://docs.zephyrproject.org/latest/services/storage/nvs/nvs.html)
for another implementation of that principle. This code is independent and
uses its own deliberately small on-flash format.

## Run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/flash_store_sim
```

The simulator saves a value, cuts power partway through an update, remounts,
retries, and then writes enough values to trigger sector rotation. Tests inject
power loss at all 48 byte boundaries of a normal record and all 160 byte
boundaries of a two-key compaction plus update.

## Flash model and format

The model has two 1024-byte sectors. Erase changes bits to 1; programming can
only change 1 to 0. A byte budget can stop a write or erase at a chosen point.
Erase counts are tracked per sector.

Each sector starts with a 16-byte header: magic, epoch, header CRC-32, and a
commit marker. It then holds 21 fixed 48-byte records. A record contains magic,
key, length, 32 data bytes, CRC-32, and a commit marker. A zero-length record
is a deletion tombstone. The latest valid record for a key wins.

When a sector fills, live values are copied to the other sector, and the new
sector header is committed last. The previous sector is kept intact. On mount,
the highest-epoch committed sector is selected. If neither sector is valid but
flash is nonblank, mount reports corruption instead of silently formatting it.

## Code map

| Path | Purpose |
| --- | --- |
| `src/store.c` | Flash format, CRC, append, lookup, deletion, compaction, mount |
| `sim/flash_model.c` | NOR-like bit rules, partial writes and erases, wear counters |
| `tests/test_main.c` | 208 write-cut points, erase cuts, recovery, CRC, reuse |
| `sim/main.c` | Human-readable failure/retry/rotation demonstration |
| `docs/design.md` | Recovery invariant and tradeoffs |
| `docs/test-plan.md` | Reproducible cases |

## Limits

This is an educational driver and storage model, not a drop-in filesystem.
CRC-32 detects accidental corruption but is not authentication. The two-sector
rotation spreads erases but does not implement sophisticated wear leveling.
Real flash may impose program alignment, page limits, erase timing, and
power-failure behavior beyond this byte-level model. No physical device was
used. MIT licensed; see `LICENSE`.
