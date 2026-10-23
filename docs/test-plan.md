# Test plan

Run the commands in the README. `ctest` must report one passing suite.

| Case | Procedure | Expected |
| --- | --- | --- |
| CRC reference | CRC-32 of `123456789` | `0xCBF43926` |
| Fresh flash | Mount erased sectors | Sector 0 formatted |
| Put/get/delete | Write, read, delete a key | Newest valid record controls result |
| Duplicate value | Write same bytes again | No new record programmed |
| Bad arguments | Invalid key or length | Rejected |
| Damaged header | Corrupt only sector header | Mount reports corruption, does not erase |
| Interrupted append | Stop at each of 48 byte positions | Old value readable after remount |
| Interrupted compaction | Stop at each of 160 write positions | Both live old values readable after remount |
| Interrupted erase | Stop target erase at selected positions | Active sector remains readable |
| Corrupt latest value | Flip a bit in the newest record | Earlier valid value returned |
| Repeated updates | Write 100 changing values | Sector rotation preserves latest value |

The write-cut loops test every byte position for the chosen record and
two-key compaction scenario. They do not prove behavior for every possible
key/value history or every physical flash failure mode.
