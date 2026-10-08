# Firmware / prod.keys compatibility — Nintendo Switch (original)

_Last checked: 2026-10-08._

## Most recent firmware: 23.0.1

- Nintendo's [official system update page](https://www.nintendo.com/en-gb/Support/Troubleshooting/Nintendo-Switch-System-Update-Information-1445507.html) lists **23.0.1** (released 2026-10-01 in the UK; Nintendo Americas lists 2026-09-30).
- Switchbrew documents [23.0.0](https://switchbrew.org/wiki/23.0.0) introduced **master_key_16** (hex) and [NCA key generation](https://switchbrew.org/wiki/NCA_Format) **0x17**. The matching per-NCA key index is **0x16**. The 23.0.1 patch release is not documented as introducing a newer key generation.
- HOS **22.x** uses **master_key_15** / key index **0x15**; earlier firmware versions use earlier generations.

## Implemented compatibility work

The fork's standalone [`strato-full-core`](../../tree/strato-full-core) branch contains the Strato emulator main sources directly, plus candidate fixes:

- **#2200**: keystore indexed key arrays expanded from 20 to 32 entries (index range 0x00–0x1f, including HOS 23 key index 0x16). The C++ loader now guards out-of-range key generation and malformed suffix indexes, and accepts CRLF, comments, and final lines without trailing newline.
- **#2201**: firmware version reading skips unrelated unreadable NCA archives, reports missing keys, and validates file input more strictly. Does not embed any keys.
- **#2202**: font extraction handles unknown file names and corrupted/unsupported unrelated archives without aborting whole import.
- The tools repo contains [`keyset_audit.py`](../tools/keyset_audit.py), which reports presence of recognized key-generation **names**, never key bytes.

To audit your legally obtained keyset **locally only**:

```powershell
python tools/keyset_audit.py "C:\path\to\prod.keys"
```

**Never upload or commit `prod.keys`, `title.keys`, keys from your console, commercial ROMs or copyrighted Nintendo firmware archives.**

## Not yet completed

- The Strato main branch predates HOS 22/23. It still needs **ZBIC NSO decompression** for content using the new compression flag in HOS 22+, see [tracking issue #5](../../issues/5).
- HLE services, NCA variants, controller/UI, firmware applets, audio, memory behavior and Vulkan compatibility also need regression tests.
- A parser accepting `master_key_16` / key-area-key index `0x16` does **not** prove cryptographic validity of key data or successful execution of firmware 23.0.1 applications.
- This fork does **not** boot the full proprietary Switch firmware as a real console; it emulates services and runs suitable applications. Firmware import is for data used by software, not proof of firmware boot.
- Do not advertise **full compatibility with every firmware** until tested; future Nintendo updates can change the formats again.

## Test requirements

- [x] Patch dry-run against pinned upstream sources
- [x] Unit tests with synthetic, non-secret key names and bytes
- [ ] Build working emulator branch with patches (GitHub Actions)
- [ ] Confirm a **user-supplied** HOS 23.0.1 keyset is parsed (no key data in logs)
- [ ] Verify authorized HOS 23.0.1 firmware version import
- [ ] Validate NSO ZBIC decoder and HLE service compatibility
- [ ] Verify regression against HOS 1–22 with independent fixtures and device logs

## Why keep source independent?

The [`strato-full-core`](../../tree/strato-full-core) branch mirrors native Strato code as a first-party snapshot, not a Gitlink to Strato. GitHub Actions additionally attempts to vendor all recursive third-party dependencies so that build prerequisites remain available even if original third-party repos disappear. Check the mirroring workflow completion: **until that step succeeds, some dependency Gitlinks remain**.

Licenses and existing upstream copyright notices must be preserved.
