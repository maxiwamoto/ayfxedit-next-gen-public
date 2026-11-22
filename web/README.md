# AYFXEDIT Web Studio — testing preview

A standalone browser interface around the existing C++ AY engine. Desktop source and releases are unchanged. The browser has no account, upload API, analytics or external runtime CDN; audio and project data stay local. IndexedDB recovery is convenient, not a replacement for downloaded backups.

## Included

- Three coloured channel timelines with volume/tone/noise drawing, frame inspection, piano notes, mute/solo and hardware envelopes.
- Linked selection editing, insert/repeat/copy/paste/clear/delete, undo/redo, channel management, loops, tags, favourites and priorities.
- Native sound presets and WAV/MP3 conversion in a cancellable worker, with original/AY preview and retained source excerpt/settings.
- AFX, AFB plus companion project, project ZIP, PSG and uncompressed single-AY VGM input.
- Editable project ZIP, classic AFX, WAV, AY register stream and native Z80/SDCC game SDK exports.
- Local recovery, keyboard shortcuts, built-in help and responsive layout.

This is not full desktop parity. Spectrograms, batch export, CSV/VT2 export, several advanced transformations and some library management controls are not included yet. Capture import is limited to 4096 frames. Audio input is mono/stereo, at most five minutes/64 MB; choose a short excerpt for conversion. ZIP input has a 256 MB expanded size limit. Native project formats retain their existing limits.

## Build and run

Install official Emscripten SDK 7.0.1, Node/npm and Python 3. Activate the SDK environment so `em++` is available, or set EMXX to its compiler. The local Windows checkout also detects native/build/emsdk.

From the repository root:

```text
npm ci --prefix web
python web/build.py
node web/tests.mjs
python web/deploy.py C:/xampp/htdocs/max_iwamoto_me/ayfxedit-web
```

Serve the resulting folder over HTTP on localhost or HTTPS on the public site. Opening index.html with file:// is unsupported. No PHP or Node server is required by the editor itself. The containing website uses PHP for its tools/project pages. Serve .mjs as JavaScript and .wasm as application/wasm; the included Apache .htaccess supplies those MIME types where overrides are enabled. Upload all deployed files together. Do not upload node_modules, emsdk, C++ build files or private project files.

Use modern browsers with WebAssembly, module workers, Web Audio and IndexedDB. A first click on Play unlocks audio. Keep the tab foreground for reliable preview scheduling; browser/OS codec support determines MP3 decoding. Download project ZIPs and extract the AFB and companion together for desktop editing.

## Validation — 2026-11-22

- Emscripten build succeeds. Node WASM tests cover project/ZIP round trips, Unicode, linked loops and insertion bounds, channel operations, invalid data/envelope rejection, conversion, continuous playback and SDK export.
- Test fixture WAV and AY register stream are byte-identical to the native desktop fixture (WAV SHA-256 69a818725ad998216ad3952a4fb0474e509dd4587303229cb9c1fc81e42ce4af).
- Local Chromium browser checks: startup, play/stop controls, period editing/undo, desktop AFB+sidecar import, WAV conversion/AY preview/add effect, local recovery after reload, and responsive layout at 390 x 844 (no horizontal page overflow).
- Save triggers project ZIP generation successfully. Automated download-event capture stalled in the embedded test browser, so physical downloaded-file verification there remains pending. The produced ZIP bytes and reopening are verified separately by Node tests.
- No real Safari, Firefox, Linux/macOS audio-device or listening verification yet. This remains a testing preview.

## Layout and architecture

index.html/studio.css/app.js: browser interface. model.js: editing transactions. client.js/worker.js: Web Audio scheduling and worker boundary. bridge.cpp: validation and native engine adapter. The actual synthesis, conversion, codecs and SDK generator reuse native headers. The player streams continuous AY state across chunks/loops rather than repeating disconnected WAV renders.

The website deployment copies only runtime files using deploy.py. dist/ is generated and ignored by Git. vendor/ contains pinned fflate plus required notices. See CREDITS.md and THIRD_PARTY_NOTICES.md; this port does not assert a new blanket licence for inherited code.
