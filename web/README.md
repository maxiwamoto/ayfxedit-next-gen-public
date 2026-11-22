# AYFXEDIT Web Studio — testing preview

A standalone browser interface around the existing C++ AY engine. Desktop source and releases are unchanged. The browser has no account, upload API, analytics or external runtime CDN; audio and project data stay local. IndexedDB recovery is convenient, not a replacement for downloaded backups.

## Included

The web interface now covers the functional command set of the Windows editor and native Linux/macOS preview. The command-by-command mapping and browser equivalents are in [PARITY.md](PARITY.md).

- Three-channel timelines and a hexadecimal numeric frame grid, shared-register diagnostics, linear/log tone display, linked frame edits, cut/copy/paste, undo/redo and full-range piano entry.
- Multipoint linear/smooth curves with preview; transpose, vibrato, hardware envelopes and retrigger policies; seeded generator preview.
- Existing-track reassignment, group/track management, favourites filtering, tags, loops and priorities.
- Workshop with native PCM WAV decoding, MP3 decoding through Web Audio, waveform/spectrogram, zoom, position-preserving A/B listening, source reopening and channel replacement.
- Atomic batch AFX/audio import, ranged/channel-selective PSG/VGM extraction and classic PSG active-track replacement.
- Editable project ZIP, bare AFB, AFX, WAV, CSV, VT2, AY14, PSG11, batch WAV/project, batch AFX and native game SDK exports.
- Reference/legacy audition, separate browser-session recovery snapshots, desktop editing shortcuts and 26 searchable Help topics.

Browser equivalents use downloads/ZIPs instead of silently writing native files/folders, and IndexedDB instead of application recovery directories. This is a local testing preview, not a claim of certification across every browser/audio device. MP3 decoding is browser-dependent. Audio/capture input is limited to five minutes/64 MB; selected effects remain limited to 4096 frames. ZIP input has a 256 MB expanded-size limit.

## Build and run

Install official Emscripten SDK 7.0.1, Node/npm and Python 3. Activate the SDK environment so `em++` is available, or set EMXX to its compiler. The local Windows checkout also detects native/build/emsdk.

From the repository root:

```text
npm ci --prefix web
python web/build.py
npm test --prefix web
python web/deploy.py C:/xampp/htdocs/max_iwamoto_me/ayfxedit-web
```

Serve the resulting folder over HTTP on localhost or HTTPS on the public site. Opening index.html with file:// is unsupported. No PHP or Node server is required by the editor itself. The containing website uses PHP for its tools/project pages. Serve .mjs as JavaScript and .wasm as application/wasm; the included Apache .htaccess supplies those MIME types where overrides are enabled. Upload all deployed files together. Do not upload node_modules, emsdk, C++ build files or private project files.

Use modern browsers with WebAssembly, module workers, Web Audio and IndexedDB. A first click on Play unlocks audio. Keep the tab foreground for reliable preview scheduling; browser/OS codec support determines MP3 decoding. Download project ZIPs and extract the AFB and companion together for desktop editing.

## Validation — 2026-11-22

- Emscripten build succeeds. Node WASM tests cover project/ZIP round trips, Unicode, linked loops and insertion bounds, channel operations, invalid data/envelope rejection, conversion, continuous playback and SDK export.
- Test fixture WAV and AY register stream are byte-identical to the native desktop fixture (WAV SHA-256 69a818725ad998216ad3952a4fb0474e509dd4587303229cb9c1fc81e42ce4af).
- Local Chromium browser checks: startup, play/stop controls, period editing/undo, desktop AFB+sidecar import, WAV conversion/AY preview/add effect, local recovery after reload, and responsive layout at 390 x 844 (no horizontal page overflow).
- The user downloaded My sounds.ayfx.zip in a normal browser and supplied that exact file. It reopened in Web Studio with all 6 groups, 14 tracks and 86,922 source-audio bytes. Native and WebAssembly save/reopen preserve bank and companion bytes. The later desktop Chrome regression pass also downloaded and reopened projects directly and compared the downloaded batch archives with native output.
- Expanded native comparison: 192 WAV outputs, 24 persistent stream cases, 147 SDK files, plus AFX/register/metadata checks match byte-for-byte across MSX/Spectrum/2 MHz clocks, 50/60 Hz, mute masks, loops and envelopes.
- Browser checks cover every export control, playback start/automatic stop, project ZIP import, searchable help and F1 from a workshop field. Help closes back to the intact workshop; conversion then completes. Unsupported AFX envelope exports now show their error inside the dialog.
- Audio scheduler unit checks cover continuous chunk scheduling, automatic stop, cancellation and underrun reporting.
- No real Safari, Firefox, Linux/macOS audio-device or listening verification yet. This remains a testing preview.

## Layout and architecture

index.html/studio.css/app.js: main interface. advanced.js/workshop.js: native-feature controls and audio workflows. model.js: editing transactions. client.js/worker.js: Web Audio scheduling and worker boundary. bridge.cpp: validation and native engine adapter. The actual synthesis, conversion, codecs and SDK generator reuse native headers. The player streams continuous AY state across chunks/loops rather than repeating disconnected WAV renders.

The website deployment copies only runtime files using deploy.py. dist/ is generated and ignored by Git. vendor/ contains pinned fflate plus required notices. See CREDITS.md and THIRD_PARTY_NOTICES.md; this port does not assert a new blanket licence for inherited code.

## Repeat the deeper export audit

From the repository root, with CMake and a native C++17 compiler available:

```text
cmake -S web/native-audit -B native/build/web-audit-native
cmake --build native/build/web-audit-native --config Release
node web/client-tests.mjs
node web/audit.mjs "C:/path/to/downloaded-project.ayfx.zip"
```

On Windows the audit defaults to native/build/web-audit-native/Release/web-native-verify.exe. Set WEB_NATIVE_VERIFY to the executable path for another generator/platform. The ZIP argument is optional; the synthetic clock/rate matrix always runs. Audit output uses a new ignored native/build/web-audit-* folder. It does not modify the source ZIP. Native files, streams and SDK output are compared to the browser's real WASM adapter. This is numerical verification; it does not record or listen to an OS audio device.

The updated testing/upload status is documented in [VALIDATION.md](VALIDATION.md). Deployment adds one content-derived revision to the complete module/worker/WASM graph so browsers do not mix engine versions.

Latest regression pass: real Chrome project downloads/reopening and batch ZIP contents verified against native output. Edited-project matrix: 208 WAVs, 26 streams and 147 SDK files. Run `npm test --prefix web` for all six suites, including worker cancellation and asynchronous save regressions. See VALIDATION.md for workflow evidence and remaining platform checks.
