# Design notes

## Normal update

The store appends a 48-byte record. Bytes 0–43 contain metadata, data, and a
CRC. Bytes 44–47 are a commit marker written last. A reset before the marker
is complete leaves the prior committed record readable. An invalid or partial
record is skipped, but occupies its physical slot until compaction. Writes of
an unchanged value are skipped to reduce flash wear.

## Compaction

When all 21 slots are occupied, the store reads the latest valid value for
each key into a bounded stack buffer. It erases the inactive sector, writes its
header body, copies live records, then writes the sector commit marker last.
The old sector is not erased. If any step fails before commit, boot still
selects the old sector. If failure happens after sector commit but during the
new user record, the new sector contains the old values and remains bootable.

Epochs select the newer valid sector. This implementation stops compaction at
`UINT32_MAX` rather than wrapping epochs; a production design would define a
safe epoch rollover policy. Mount treats nonblank flash with no valid sector as
corrupt and does not erase it automatically.

## Invariants and RAM

- The active sector is never erased during an update.
- A record is visible only with valid magic, key/length, CRC, and full commit.
- A sector is selectable only with valid header magic, CRC, and full commit.
- At most eight 32-byte values are staged on the stack during compaction.

The store has bounded flash and RAM use, no heap allocation, and no background
threads. It is designed to be explainable and testable, not to imitate all
features of a production filesystem.
