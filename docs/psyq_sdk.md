# Sony SDK in RE1 Dual Shock (simple overview)

Which Sony PsyQ libraries RE1 Dual Shock was built with, and where they sit in the main program.

## Which Sony SDK?

RE1 Dual Shock was built mostly with **Sony PsyQ SDK 3.7**, plus a few libraries from other
versions:

| Part | SDK version | Why it makes sense |
|---|---|---|
| Graphics, CD, memory card, geometry, system | **3.7** | The main SDK of the 1997 Director's Cut era |
| Sound (`LIBSND`, `LIBSPU`) | **3.3** (with 2 newer pieces) | The sound code from the 1996 original was kept |
| Controller (`LIBPAD`) | **4.2** | Added for the Dual Shock's analog sticks and vibration |

## What's Sony code and what's Capcom code?

The main program (`SLUS_007.47`) is laid out like this:

```
0x80010000 ┌──────────────────────────────┐
           │ Capcom game code   (~326 KB) │  ← we decompile this
0x80061880 ├──────────────────────────────┤
           │ Sony libraries     (~188 KB) │  ← named and skipped (Sony's code)
0x8008F89C ├──────────────────────────────┤
           │ Data               (~140 KB) │
0x800B2800 └──────────────────────────────┘
```

- **687 Sony functions** are named automatically.
- The 12 overlay files in `PROG2/` are **pure Capcom code** (only a 24-byte Sony start-up
  stub each).

## Files

- `dualshock/config/symbol_addrs_psyq.txt`: all Sony function names with their addresses
  (splat reads it).
- `dualshock/config/psyq_report_SLUS_007.47.md`: the full match report.
- To regenerate: `python tools/psyq_match.py` (see the detailed doc).

More detail is in [psyq_sdk_detailed.md](psyq_sdk_detailed.md).

## Correction

The function at **0x8008C37C** was matched as `SpuRead`, but it is **`SpuWrite`** (it calls
`_spu_write`; the two are near-identical code). Fixed in `dualshock/config/symbol_addrs_psyq.txt`
(`SpuWrite = 0x8008C37C`).
