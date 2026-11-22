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

## Remaining release checks

Real Safari/Firefox, Linux/macOS audio-device and subjective listening checks remain. Browser MP3 decoding can differ from platform decoders. The embedded test browser cannot capture downloads; normal-browser project ZIP download/reopening was previously confirmed by the user and their exact file was audited. Browser equivalents for filesystem operations are documented in PARITY.md. No production website upload or public release was performed.
