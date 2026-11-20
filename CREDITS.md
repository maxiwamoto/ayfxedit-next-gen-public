# Credits and provenance

## AYFXEDIT — NEXT GEN

Copyright (C) 2026 **Max Iwamoto** — NEXT GEN interface and additions.

Native Windows editor, multichannel project workflow, audio workshop, tools/guide, compact game SDK, register export and regression verification. These additions build on the work credited below; they do not replace the original authors' attribution.

## Original AYFXEdit and improved fork

- **Shiru** — AY Sound FX Editor, effect/bank formats, original editor, renderer and players. [Original project](https://shiru.untergrund.net/software.shtml).
- **Threetwosevensixseven** — [ayfxedit-improved](https://github.com/Threetwosevensixseven/ayfxedit-improved), including its Unicode/C++ Builder port and improvements. NEXT GEN starts from commit `e7323074a7c5df12afe4a7e644591d8be6c8c42a`.
- **John Montgomery** — 5KTuner, credited upstream as **CC-BY 2008**. The inherited single-channel WAV importer retains its difference-minimum pitch-detection approach; the new workshop uses separate FFT-based analysis.
- **HackerKAY** — noise code credited by the original renderer.

`native/core.h` adapts AFX flags/delta encoding and AFB offsets from upstream `src/UnitMain.cpp`, the legacy AY model/table from `src/wav_render.h`, and the inherited importer approach from `src/import_wav.h`. NEXT GEN adds bounds checks, fractional timing, frame-zero corrections and a separate reference preview. `native/assets/ayfx.ico` is the inherited project icon.

The original acknowledgements also thank **Alone Coder**, **Sergey Bulba** (AY Emulator, Vortex Tracker and documentation), **SMT** (UnrealSpeccy and VTX player), **Copper Feet / Bytex** (asm80), and the authors of the original sound-effect library. The original game-derived library is not bundled in this release.

## Additional references and tools

- [openMSX AY8910](https://github.com/openMSX/openMSX/blob/master/src/sound/AY8910.cc) informed the reference AY preview. This is not the full openMSX audio pipeline or a measured analog hardware model.
- Microsoft Windows Media Foundation supplies MP3 decoding.
- Ivan Kosarev's MIT-licensed `z80` emulator is an optional test dependency, not bundled into the editor.
- tniASM and SDCC are external SDK build tools, not redistributed in this package.

See [third-party notices](THIRD_PARTY_NOTICES.md). Original copyrights and notices remain with their authors.
