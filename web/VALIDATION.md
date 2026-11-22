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

## Remaining release limits

This is not a full standalone replacement yet. Missing browser tools include multipoint/smooth curves, selected-frame transpose/vibrato, spectrogram, position-preserving A/B comparison, saved-source reopening/replacement, batch operations, existing-track reassignment and CSV/VT2/PSG11 export. Recovery uses one local browser slot instead of native session files.

Real Safari/Firefox, Linux/macOS audio-device and subjective listening checks remain. The embedded test browser cannot capture downloads; successful normal-browser downloading was confirmed by the user and the actual supplied ZIP was tested. No production website upload or public release was performed.

Sources for the comparison: native/help_content.h, native/main.cpp, portable/main.cpp, portable/workshop_dialog.cpp, and the current web implementation.
