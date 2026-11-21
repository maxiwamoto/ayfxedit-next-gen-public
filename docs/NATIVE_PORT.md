# Native Linux and macOS preview

This port uses Qt Widgets and Qt Multimedia with the same C++ AY renderer, converters, bank/project codecs and game exporters as the Windows release. The existing Win32 executable remains the stable Windows version.

## Build

Install a C++17 compiler, CMake 3.21+, Python 3, Qt 6.4+ Widgets, Multimedia and Concurrent development packages.

Ubuntu 24.04:
    sudo apt install build-essential cmake qt6-base-dev qt6-multimedia-dev libgl1-mesa-dev gstreamer1.0-plugins-good

macOS:
    brew install cmake qtbase qtmultimedia
    export CMAKE_PREFIX_PATH="$(brew --prefix qtbase):$(brew --prefix qtmultimedia)"

All platforms:
    cmake -S . -B build-portable -DCMAKE_BUILD_TYPE=Release
    cmake --build build-portable --parallel
    ctest --test-dir build-portable --output-on-failure

## Preview scope

Includes multichannel numeric frame editing, channel reassignment/removal, linked frame operations, undo/redo, loops, mute/solo, continuous AY preview, generators, two-point curves, pitch/vibrato, envelopes, metadata/search, minute autosaves, project save/load, WAV/MP3 conversion, capture extraction, exports, piano entry and the embedded offline guide.

It is a native preview, not a claim of pixel-for-pixel Win32 interface parity. The native grid uses editable hexadecimal values and checkboxes, with Alt-drag parameter-bar drawing and per-channel headings. The curve dialog currently uses two endpoints rather than the Windows freehand curve editor. Batch AFX import/export, waveform selection zoom and AFX/AFB file drag/drop are supported. Some inherited guide topics describe Windows controls; platform menu shortcuts take precedence.

The portable executable uses Qt's audio backend for playback and MP3 decoding. Installed codecs/backend availability can differ across Linux distributions. WAV conversion and AY rendering use the existing shared implementation. Select a local MP3/WAV of up to 64 MB and five minutes; convert a selection of up to 4096 frames. Projects retain the source excerpt and Windows-compatible settings.

Linux DEB packages require Ubuntu 24.04-compatible Qt libraries. Install with apt so dependencies are resolved. The TGZ is not a self-contained AppImage. macOS builds include Qt frameworks in an application bundle and use ad-hoc signatures; they are not Apple-notarized releases. No security settings are changed by the build.

## Validation

CTest runs the existing shared engine checks and a native GUI-model smoke check: hex editing, undo/redo, Unicode project paths, register preservation after save/load, linked timeline/loop edits, WAV and game exports. CPU and offscreen tests do not verify real sound-device output or subjective audio quality. Test the preview on a real Linux/macOS desktop before public promotion.

## Dependencies and credits

The Qt front end dynamically links Qt Core, GUI, Widgets, Multimedia and Concurrent. Qt is separately licensed; see THIRD_PARTY_NOTICES.md. The original authors' rights and credits remain unchanged.
