# AYFXEdit Studio — native Windows port

This is a local Win32/C++17 port based on Shiru's AYFXEdit source and the
Threetwosevensixseven/ayfxedit-improved fork (base commit e732307).
The upstream C++ Builder sources remain available in the original repository and inherited Git history; this branch contains the maintained native implementation.

## Run and build

- Run `start_native.bat`, or `native/dist/ayfxedit-next-gen.exe` directly.
- Build: double-click `build_native.bat`.
- Tests: run `test_native.bat` after building.
- Requires Windows x64. The executable uses only Windows system DLLs; the
  Visual C++ runtime is linked statically. C++ Builder, VCL and .NET are not needed.
- Building requires Visual Studio C++ Build Tools (2019 or newer) with a Windows SDK.
  The script locates the compiler with vswhere. No downloads happen during build.

## Built-in guide and About

**Help > User guide** (F1) opens a resizable offline manual with a contents tree.
Every File, Edit, Sound, Tools and Help command has its own article, alongside getting
started, grid/piano controls, keyboard shortcuts, timing and multichannel topics.
Articles wrap, scroll and can be selected/copied. The workshop's Help button
or F1 opens its conversion topic directly.

**Help > About AYFXEDIT - NEXT GEN** shows Max Iwamoto's NEXT GEN attribution,
the OCT.22.2026 identity date, and a copyright notice for the 2026 interface and
additions. It credits Shiru's AY Sound FX Editor v0.6 and
Threetwosevensixseven's ayfxedit-improved fork, with buttons for their websites
and detailed Credits. Upstream author and component acknowledgements are retained.

The compact workshop is 450 x 293 dialog units (previously 580 x 372), retaining
the waveform, conversion controls, comparison playback and Apply/Undo behavior.

## Working features

- Open/save AFX and AFB, including 256-effect banks; export banks without names.
- Add, duplicate, remove and rename effects; up to 4096 frames per effect.
- Hexadecimal tone/noise/volume editing; tone/noise toggles; draggable value bars.
- Frame selection, cut/copy/insert-paste, insert/delete/duplicate frames.
- Undo/redo, save prompts, file drag/drop and Unicode file paths.
- Returning from another app restores keyboard focus to the previous editor control, including the name field; unavailable controls fall back to the frame grid.
- Grouped effect library with 2 CH / 3 CH badges and A/B/C channel labels.
- Aligned editable A/B/C lanes for multichannel effects, with Play all and Solo.
- Modern bank sidebar, amplitude overview and live tone/noise/level/duration readouts.
- Single-channel AY preview and WAV export with Reference and Legacy models.
- MSX 1.789773 MHz, Spectrum 1.7734 MHz or 2 MHz; selectable 50/60 Hz playback.
- Piano buttons insert a note at the current frame, with octave selection.
- WAV / MP3 conversion workshop with waveform trimming, 1-3 AY channels and A/B preview.
- PSG register-dump import (auto/A/B/C).
- CSV and VT2 sample export (VT2 uses base note C4 and the original 63-frame limit).
- CSV export defaults to the current bank's folder and name (`sounds.afb` -> `sounds.csv`), or `Untitled.csv` for an unsaved bank. It exports only the active track; the save dialog allows another filename.

## Generic multichannel effects

Select an effect and click **Channels / loop...**. Give it any name, assign any bank
track to A/B/C, or choose **New blank track**. No game-specific naming is required.
Each track belongs to one effect/channel. Reassigning moves it; displaced tracks
remain available as standalone effects. All changes support Undo/Redo.

The AFB stays in the standard format. Save creates a companion file named
`your-bank.afb.ayfx-project` containing group names, track assignments, loop ranges,
clock and frame rate. Keep these two files together. Save As writes a new pair.
AFB-only readers still see individual tracks. The project records a fingerprint
of the bank; externally changed/reordered banks are rejected rather than silently
using stale channel assignments. Move the companion aside to open such a bank as
an ordinary AFB and reassign its channels. A failed companion save rolls back the
AFB replacement. Export bank without names remains an AFB-only export.

When no companion exists, names such as `0F_A_fire_whip` / `0F_C_fire_whip` are
recognized once as a convenience for old exports. After loading, group membership
is explicit and does not change when tracks are renamed. Castlevania's bank opens
as 60 effects / 73 tracks. Its existing `project.json` supplies 60 Hz/MSX clock and
original loop metadata; the editor's own companion takes precedence on later loads.
Keep original track names for the Castlevania **game importer**, which still uses
those names and its original `project.json` loop constraints. Generic editor group
names and loop edits do not change that external converter's rules.

## Editing and auditioning

- **- / +** buttons above the library select the previous/next grouped effect.
  Keyboard - / + (including numpad keys) work with the grid or library focused;
  text fields keep normal typing. Navigation stops at the first/last effect.

- Multichannel entries have **2 CH** / **3 CH** badges and channel combinations.
- **Remove A/B/C** above each assigned channel deletes only that track, preserves the other channels and supports Undo. The last bank track cannot be removed.
- Sound buttons use green **Sound ON** and red **MUTED** labels. To audition two channels, enable those two and mute the third, then **Play all**. To group separate tracks, assign them in **Channels / loop**; this keeps both AY voices.
- Click a channel header or numeric field to select the active track. Type hex
  values, click T/N flags, or drag volume bars. Metrics describe the active track.
- **Play all** / Enter combines channels; **Solo** auditions the active channel
  regardless of mute switches. WAV export uses the current mute mask, one pass.
- Click the colored **A/B/C: Sound ON / MUTED** button above a channel, or use **Sound > Mute A/B/C**. Changing
  mutes stops the current preview; press Play again to audition the new mix.
- **Edit > Link channel frame edits** is enabled by default. Insert, duplicate,
  delete, cut and paste shift all channels together. Multichannel copying preserves
  A/B/C data. Pasting a single track inserts silence into the other channels.
  Disable linking for intentional single-track timing changes. Value edits, rename,
  Clear, and Add/Copy/Remove track remain operations on the active track.
- **Channels / loop...** also sets loop start and exclusive end in decimal frames.
  Purple lines show the boundaries; the status bar shows their values. Playback
  plays the lead-in once, then repeats until Stop. Ctrl+Enter starts at the cursor.
  Linked timeline changes shift the markers; deleting the complete loop disables it.
- Conflicting audible shared-noise periods are red in the grid, with the first
  conflicting frame in the status bar. Playback rejects conflicts instead of
  silently choosing one channel's period.

Multichannel and loop previews use the Reference AY model. Shared noise remains
synchronized when channels are muted or soloed. A fixed 10,000 PCM units per channel
leaves mix headroom; standalone preview retains its previous gain. Loops retain oscillator, noise and envelope state across repetitions. WAV exports contain one pass, not an infinite loop.
Hardware envelopes are not represented by AFX's fixed volume data.

Clock and frame rate are stored in the companion, not in AFX/AFB themselves.
Changing clock does not retune the stored effect bytes. This is a sound-effect
editor, not a music sequencer.

## Audio preview

Reference preview is the default. It uses logarithmic fixed-volume steps of
approximately 3.01 dB, a 17-bit noise generator, and a windowed-sinc low-pass
resampler to 44.1 kHz. Legacy preview retains the inherited volume table and
renderer for comparison on standalone effects. Multichannel and loop previews
use Reference; WAV export follows the enabled preview model.
No per-effect normalization is applied.

The reference is informed by [openMSX AY8910 source](https://github.com/openMSX/openMSX/blob/master/src/sound/AY8910.cc).
It is not a complete openMSX audio pipeline or a measured analog-hardware model.
Noise is clocked at AY clock / (16 * max(1, R6)); the displayed noise value is
its shift clock, not a musical pitch. The inherited divider was already correct;
we have not globally raised noise pitch. Filtering reduces high-frequency aliasing.
The old table made many low/middle levels about 4–6 dB louder than this reference.

Castlevania II's chain whip remains the existing NES-to-AY conversion. Its
15 audible frames last 0.25 seconds at 60 Hz; playing at 50 Hz stretched them to
0.30 seconds. Nine audible frames use noise period 31, AY's slowest noise clock.
The editor improvements do not rewrite that effect or the game's sound bank.
A listening comparison with the original NES and the MSX emulator is still needed
before claiming an exact match or choosing further changes to the conversion.

## Keyboard

Click a numeric field and type hexadecimal digits. T/N toggle mixer flags.
Shift-click or Shift+arrow selects frames. Drag the bars to draw; right-drag clears.
Enter plays, Ctrl+Enter plays from the cursor, Space stops. Insert adds a frame,
Ctrl+Insert duplicates it, Delete removes the selection. Ctrl+C/X/V copies/cuts/
inserts frames. Ctrl+Z/Y undo/redo. Ctrl+S saves the bank. F1 shows help.

## Remaining differences from the original

Single-AY VGM extraction and batch file import/export are supported. VTX import
is not available. Piano entry writes one frame at a time; the original piano's
fill/step controls are not present. Use `start_native.bat` for the current build. UI is system-DPI-aware; per-monitor DPI changes
require reopening the editor.

## Source provenance and changes

`core.h` adapts the original AFX flags/delta encoding and AFB relative offsets
from `src/UnitMain.cpp`, and the AY model/volume table from `src/wav_render.h`.
The renderer now accumulates fractional AY ticks and applies frame zero immediately,
with exact 50/60 Hz scheduling. This deliberately fixes the original renderer's
integer clock truncation and delayed first frame, so WAV output is not bit-identical.
File parsing now checks bounds and rejects bank-offset overflow rather than wrapping.
Names are preserved as byte strings in AFB; the UI uses UTF-8 for new names.

The older single-channel importer in core.h retains the difference-minimum
pitch-detection approach from src/import_wav.h (John Montgomery's 5KTuner,
CC-BY 2008, as credited upstream). The workshop uses a new FFT peak analysis,
harmonic suppression, frame RMS envelope, and reference AY noise-profile fitting
in workshop.h. MP3 decoding uses the Windows Media Foundation Source Reader;
see Microsoft's [audio decoding documentation](https://learn.microsoft.com/en-us/windows/win32/medfound/tutorial--decoding-audio).
Shiru and the fork author retain credit for the original project. No new
project-wide license is asserted here.

## WAV / MP3 workshop

Choose **File > WAV / MP3 workshop...**, then open a local audio file.

1. Drag across the waveform, or enter Start / End times in milliseconds.
2. Choose one, two or three AY channels. Adjust Normalize, attack/fade-in,
   decay/fade-out, brightness and pitch (semitones).
3. Click **Convert**. Analysis and AY rendering run in a background worker;
   **Cancel conversion** discards that attempt.
4. Listen using **Original** and **AY result**. Switching keeps the playback
   position within the selection. Stop ends preview.
5. Apply as a new effect, or replace the selected effect's channels.
   The bank changes only on Apply; **Undo** restores it. Replacement preserves
   existing track names, clears the group's loop, and silences unused channels.
   Save the bank plus its companion project file to retain channel grouping.

The converter approximates audio using AY tones, logarithmic volume levels,
and the chip's single shared noise generator. The game SDK exports compact channel data; raw register export is also available. Neither includes PCM. Projects retain the selected original PCM source for later editing.
Short, distinct effects are better starting material than a complete mixed song.
Stereo sources are averaged to mono before analysis; AY channels are synthesized
tone/noise parts, not the original left/right channels.

Input: uncompressed 8/16-bit PCM WAV, mono/stereo, 4-192 kHz; or MP3 decoded by
Windows. Files are limited to 64 MB and five minutes. Select at most 4096 AY
frames (about 68 seconds at 60 Hz or 82 seconds at 50 Hz). MP3 encoder delay or
padding can remain at the edges; trim those using the waveform if needed.
MP3 needs Windows Media Foundation (Media Feature Pack on Windows N editions);
the application does not require FFmpeg or external codec downloads.


## Validation

`native/tests.cpp` verifies generated synthetic AFX files byte-for-byte and round-trips their generated bank. The original development suite additionally checked the upstream library (281 AFX files and 28 banks); those game-derived fixtures are not bundled here. It also tests 256-entry banks, 4096-frame
effects, malformed data, offset overflow, AY pitch, 50/60 Hz duration, stereo WAV
and PSG import. Reference audio checks cover all 32 noise periods, the full
131071-state noise cycle, volume ratios, and ultrasonic alias rejection.
`test_native.bat "path/to/castlevania2-effects.afb"` also checks companion timing
and exports Reference/Legacy chain-whip WAVs for comparison. It also checks the
60-effect grouping, channel selection/edit/undo, and combined fire-whip playback. The native window smoke test exercises keyboard/bar editing,
undo, paste, bank duplication and Windows audio startup, and writes rendered BMPs
into `native/build/`. Audio startup success does not substitute for listening tests.

Generic-project validation covers arbitrary names, save/load, stale companion
rejection, failed-save rollback, reassignment and track removal. GUI checks cover
the channel dialog, linked insertion/deletion/paste and undo, rename persistence,
mutes, and queued Windows loop playback. Loop segment length, noise-conflict frame
identification and muted output are also checked independently.


Workshop tests cover trim timing, pitch, normalization, fades, multiple tones,
noise bandwidth, brightness, silence, cancellation, application and seeking.
The GUI smoke test checks preview/cancel without mutation, Apply and Undo.
To additionally test a real local MP3 through both the decoder and GUI, run
test_native.bat with the Castlevania bank path as argument 1 and the MP3 path
as argument 2 (quote both paths).
This also verifies Unicode paths, uppercase extensions and MP3 conversion.
Malformed MP3 rejection is always tested. The test runner invalidates stale
results and treats any nonzero process exit, including a crash, as a failure.


The GUI smoke test checks menu-to-guide coverage, opens every article, captures
the guide at two sizes, and renders About and the compact workshop. It also
verifies workshop controls are enabled again when conversion completes.


F1 regression checks exercise the actual accelerator dispatch with the grid,
name field and clock selector focused, plus a queued F1 key in a workshop input.
Menu accelerators are distinguished from combo-box notifications; workshop
controls explicitly route F1 to the conversion guide. Workshop field labels
are right-aligned beside their inputs, and the brightness value sits by its label.


## NEXT GEN tools and SDK

Tools includes curve drawing, preset generators, envelope/pitch editing,
capture extraction, SDK export, metadata/search, batch operations, source reopening
and recovery. F1 documents every command and its limitations.

Project v2 adds envelope events, priority, tags/favourites and selected source
PCM/recipe. v1 projects still load. Classic AFX/AFB bytes are unchanged; extended
content requires the sidecar. Source excerpts live on the first converted track.
Undo snapshots copy source data; long excerpts increase memory use. Per-source
limit is 64 MB; project limit is 256 MB. Autosave writes a separate session
snapshot each minute under LOCALAPPDATA/AYFXEDIT-NEXT-GEN/Recovery. Recovery
opens as an untitled project and cannot accidentally overwrite the original bank.

Continuous loop preview refills PCM blocks from a persistent AY chip model.
Oscillator, noise LFSR, envelope and filter state continue across loop boundaries.

`runtime/` contains ROM-safe ASM and portable C players. The build embeds their
sources in the EXE so export works outside the checkout. `tools/format_asm.py`
checks maintained and generated tniASM sources, excluding SDCC compiler output.
Run `test_players.bat` after native tests generate the fixture bank. See the
[runtime README](../runtime/README.md) for format, ABI, hardware ownership,
memory mapping and integration requirements.

Both compiled players execute on a Z80 CPU model, comparing PSG writes with
authored register frames. Tests cover 1/2/3 channels, envelope retriggers, loops,
priority rejection, long hold runs, music mixing/restoration, port addresses,
GPIO bits and interrupt neutrality. Target-game and physical hardware integration
are not covered by these CPU tests.

## Raw register export

File > Export AY registers exports the selected group to `.psg11` or `.ayregs`,
plus a `.json` sidecar. See [the format contract](../docs/register-export.md).
There is one current editor executable, `native/dist/ayfxedit-next-gen.exe`.
The obsolete player implementations and old binaries are available in Git history.
