# Synolsis V35 — Horizon SVC 0x18 compatibility test

## Regression observed

V30 introduced `if (!numHandles || numHandles > 0x40) return result::OutOfRange` in
`kernel/svc.cpp:WaitSynchronization`. This is **not** the Horizon behavior:
0 objects is a legal cancellable/timed wait; only counts outside 0..64 fail.
The emulation log from V34 reported `SetTerminateResult: 60929` (0xEE01,
Kernel OutOfRange), followed by guest `Break` and thread shutdown. This
is a plausible relationship **but not proof** the result came from SVC 0x18.

## Source-backed comparison

- Nintendo Switch Brew, SVC WaitSynchronization:
  https://switchbrew.org/wiki/SVC#WaitSynchronization
  (0 objects wait until timeout/cancellation; counts above 0x40 return 0xEE01).
- Eden mirror, `src/core/hle/kernel/svc/svc_synchronization.cpp`:
  https://github.com/eden-emulator/mirror/blob/master/src/core/hle/kernel/svc/svc_synchronization.cpp
  (`0 <= num_handles && num_handles <= ArgumentHandleCountMax`, only
  copies guest handle pointers when `num_handles > 0`).
- Citron mirror:
  https://github.com/pkgforge-community/git.citron-emu.org-Citron-Citron/blob/main/src/core/hle/kernel/svc/svc_synchronization.cpp
  (same check and empty-list path).
- Do **not** wholesale copy Eden/Citron HLE code: those use a different
  kernel/object scheduler architecture from Skyline/Strato. Reproduce
  documented behavior with the existing scheduler, not a large rebase.

## V35 changes

- Accept 0..64 handles and preserve the `>64` OutOfRange result.
- Zero-length lists do not require a non-null guest pointer and no longer
  index `waitHandles[0]` for tracing.
- Empty waits still use the original scheduler timeout/cancellation path:
  `TimedOut` for timeout 0 / expired positive timeout, `Cancelled` on
  cancel, no artificial wake-up. No change to valid 1..64 object waits.
- Add limited diagnostics on empty waits and invalid counts.
- Compile `tests/wait_synchronization_policy_test.cpp` as part of the
  dedicated CI to prevent reintroducing the regression.
- Retain V34 guest present/fence/Vulkan-result diagnostics to distinguish
  separate graphics and guest-exit failures.

## Device A/B acceptance

On same save/title, handheld mode, graphics settings and Turnip driver,
compare the V34 vs V35 screenshots and `emulation.log`. Look for
`WaitSynchronization empty handles:`, `invalid handle count:`,
`Synolsis present begin/fence ready/present result`, `Break`,
`SetTerminateResult`. Capture logcat crash after any Android process closure.

**This build is a compatibility repair, not proof that NieR or Nine Sols
black-screen rendering, complete NVDEC/VIC, QLauncher, or internal-resolution
scaling has been fixed.**
