# Public release validation

Release 1.0.0, 2026-11-20. Built from this public source snapshot with MSVC x64, Windows SDK and Python 3.

- `build_native.bat`: passed, portable x64 executable using the static C runtime.
- `test_native.bat`: passed native regression suite and GUI smoke tests, using 8 generated AFX effects and 1 generated bank (no game asset collections).
- Native coverage includes codec round-trips, 256-entry banks, 4096-frame limits, malformed input, 50/60 Hz timing, noise/LFSR checks, envelopes, multichannel loops, workshop controls, project save/rollback and register export.
- GUI coverage includes focus restoration after activation, safe focus fallback, viewport scrolling, editing/undo, channel dialogs, guide and audio startup.
- `python tools/format_asm.py --check`: passed for 22 maintained/generated ASM files.
- `test_players.bat`: all six compiled implementations passed register-trace, loop, priority, envelope, GPIO and interrupt-neutrality checks.

| Player | Target | Peak tested T-states per frame |
|---|---|---:|
| tniASM with mixer | MSX | 4534 |
| tniASM small | MSX | 3170 |
| SDCC C | MSX | 16910 |
| tniASM with mixer | Spectrum | 4982 |
| tniASM small | Spectrum | 3618 |
| SDCC C | Spectrum | 17469 |

These CPU-model measurements cover the generated regression fixtures. They do not model platform contention or establish the cost in every game. Audio-startup checks do not replace subjective listening tests. The optional external MP3/game-bank fixtures were not supplied for this clean-package run.
