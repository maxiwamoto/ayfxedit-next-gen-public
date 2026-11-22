# Web preview validation — 2026-11-22

## Completed

The exact ZIP downloaded by the user in a normal browser passed ZIP CRC validation and reopened through Web Studio's Open chooser. It contains 6 grouped effects / 14 tracks. Its conversion source excerpt (86,922 bytes) is retained. Resaving/reloading both native and WASM codecs produces identical bank and companion bytes.

The native verifier is compiled independently with MSVC. The test matrix combines the user's project with six synthetic banks (three clocks × two rates), including single-channel tones, three-channel noise, loops and explicitly retriggered envelopes. All 192 WAVs, 24 continuous stream cases, 147 SDK files, applicable AFX exports, AY14 streams and JSON metadata are byte-identical. Split stream chunks equal uninterrupted rendering. All-muted WAVs are silent and WAV headers/lengths are validated.

Browser checks cover project import, all export controls, correctly named download links, play/automatic stop, WAV conversion, F1 from a workshop field, returning to the intact workshop, guide search/navigation, and visible rejection of unsupported envelope-to-AFX exports. Earlier tests also cover mobile editor layout and local recovery. Automated audio scheduler tests check cancellation and underrun handling.

## Fixes made during testing

- A persistent download link allows retrying a prepared file. Save now says prepared rather than claiming the browser wrote it to disk.
- Audio underruns now reach the error handler instead of silently stopping due to a cancellation-token change.
- Errors such as unsupported AFX envelope export are visible inside their modal dialog.
- The old short help paragraph was replaced with 21 searchable topics, tables, walkthroughs, shortcuts, troubleshooting, credits and a feature-parity section. Help uses its own dialog and does not discard an open workshop. Workshop F1 selects its topic automatically.

## Feature-parity expansion

Implemented the inventory in PARITY.md, including numeric editing, advanced native tools, batch workflows, complete exports, legacy audio, capture extraction, workshop analysis/source revision and per-session recovery. The standalone source/release binaries were not modified.

The expanded independent native audit passed on the user's supplied ZIP plus six clock/rate fixtures: 192 WAV renders, 24 continuous stream cases, 147 SDK files and 24 full-timeline batch WAVs. CSV, VT2, PSG11 data/metadata, legacy WAV, seeded generators, smooth multipoint curves and transpose/vibrato project outputs also match the native verifier byte-for-byte where applicable. Latest report: native/build/web-audit-1795371786338/report.json (ignored build artifact).

Focused tests cover deterministic/different seeds, duplicate-assignment rejection, displaced-track preservation, replacement names/source data/spare-channel silence, envelope retrigger policies, rejection of lossy exports, 5100-frame capture slicing, channel masks, VGM target-clock retuning, spectrum peak/range validation and last-track removal rules. Audio scheduling tests also check A/B offset preservation and reset on Stop.

Local browser checks cover curve preview/apply/undo, pitch edits, invalid duplicate assignment, hexadecimal grid, capture frames 4500–4510 with mask A+C, spectrogram, conversion and A/B controls, replace/reopen source with restored recipe, batch import failure with no partial changes, successful AFX+WAV batch and one-step undo, and complete library ZIP preparation. Additional final checks cover legacy playback, native PCM WAV conversion, contextual workshop Help, every new export button, separate tab recovery snapshots, hexadecimal keystroke editing with undo, and editor/grid/curve layouts at 390 × 844 without page overflow. No browser console errors were reported. Existing project/codec and scheduler regressions still pass.

Deployment revisions the whole module/worker/WASM dependency graph to prevent cached old code mixing with a newly deployed engine. PCM WAV decoding now uses the native decoder at its original sample rate instead of browser resampling before conversion.

## Full browser regression pass — 2026-11-22

Desktop Chrome was used for actual file downloads as well as UI tests. Nothing was published. Runtime revision: `2d58b821d4b2`.

- Opened the user's original ZIP, saved it under a QA filename, reopened that downloaded ZIP and compared both contained files byte-for-byte with the original. All 6 groups / 14 tracks and 86,922 bytes of retained source audio survive.
- Imported a valid AFX followed by a malformed AFX: the error is visible and the bank stays at 6 groups. Cancelled a mixed AFX / 60-second WAV batch with no partial changes. A successful AFX / stereo 48 kHz WAV batch produces 8 groups; Undo restores 6 and Redo restores 8.
- Changed the new effect to a 10–100 frame loop with priority 211, saved, reloaded and reopened the actual download. All settings survived, along with 18 tracks and 351,566 source-audio bytes. Continuous loop playback remained active beyond one pass and stopped on command.
- Downloaded the full library WAV ZIP and AFX ZIP. CRCs pass. All 6 WAV files, the bundled editable project and all 14 AFX files match the independent native verifier exactly.
- Downloaded individual WAV, AFX, CSV, VT2, AY14 and PSG11 exports plus the game SDK. Their bytes match the native verifier, including all 21 SDK files. The bare AFB download was generated; its codec is covered by engine regressions.
- Reopened retained WAV source, rendered its spectrogram, reconverted it and switched Original / AY previews. Loaded a 188.21-second MP3 and converted a two-second excerpt to 100 frames. Cancelled a longer conversion and successfully retried a one-second excerpt. Stop and automatic completion return playback indicators to idle.
- Corrected curve positions with two decimal places now validate and Apply successfully; Undo restores the original values. Browser console errors/warnings: none in the final pass.

The final independent native audit uses the newly edited/downloaded project and six clock/rate fixtures: **208 WAVs, 26 streaming cases, 147 SDK files and 26 batch WAVs** pass. Report: `native/build/web-audit-1795373171110/report.json`. The original user project was also audited at `native/build/web-audit-1795372630424/report.json`. These are ignored local artifacts.

Fixes from this pass:

- Curve position inputs now accept the same two-decimal precision produced by drawing points.
- Closing a worker rejects calls even at the startup/await boundary, preventing an indefinitely pending operation. Four cancellation cases are covered by `worker-tests.mjs`.
- Save completion cannot clear recovery for a different opened project or newer edits. Recovery-cleanup failure no longer turns a prepared download into an apparent save failure. `project-io-tests.mjs` executes the real save handler against deferred operations.
- Batch import stops prior playback and locks the channel count during processing.
- Original / AY preview buttons reset when audio ends naturally.

`npm test --prefix web` passes all six suites. Standalone synthesis/conversion code and release binaries are unchanged.

## Remaining release checks

Real Safari/Firefox, Linux/macOS audio-device and subjective listening checks remain. Browser MP3 decoding can differ from platform decoders. Desktop Chrome downloads, reopening and downloaded-file native comparisons are now verified directly; the earlier embedded-browser download limitation does not apply to this pass. Browser equivalents for filesystem operations are documented in PARITY.md. No production website upload or public release was performed.
