# Changes

## 1.0.0 — 2026-11-20

First public NEXT GEN source snapshot and portable Windows x64 release. Includes the latest local editor changes.

- Native Win32/C++17 interface with multichannel editing, linked timelines, mutes/solo, loops and undo/redo.
- Hardware envelope events and shared-register validation.
- WAV/MP3 workshop with trimming, zoom, spectrogram, sound controls, A/B preview, cancellation and saved source/settings.
- Reference and legacy audio preview, fractional clock timing, immediate first-frame playback and continuous chip state across loops.
- Curve designer, generators, pitch/vibrato tools, metadata, search, batch actions and recovery.
- Compact MSX/Spectrum SDK in tniASM and SDCC C, optional music mixing, and raw AY14/PSG11 register export.
- Restored keyboard focus and text selection when returning to the editor; disabled/destroyed focus targets safely return to the grid.
- Viewport scrolling avoids repainting surrounding editor controls or changing sound data.
- In-app guide and preserved original/new credits.
- Public packaging uses generated synthetic fixtures, portable assembler discovery and the current source only. Legacy sources remain upstream; obsolete executables and game-derived samples are excluded.

The portable build and release checksums are attached to the matching GitHub release. See README.md for build/test instructions and known limits.
