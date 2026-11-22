# Third-party notices

This distribution is derived from Shiru's AY Sound FX Editor v0.6 through Threetwosevensixseven's ayfxedit-improved, upstream revision `e7323074a7c5df12afe4a7e644591d8be6c8c42a`.

The referenced upstream snapshot provides source and binaries but contains no project-wide licence file. No MIT, GPL, public-domain or other blanket licence is asserted for that inherited code or icon by this fork. Original rights and notices are retained. Consult the original authors about permissions beyond those they have provided.

The inherited tuner attribution reads:

> tuner code is C version of Java 5KTuner by John Montgomery, CC-BY 2008, integrated into AYFXedit in 2017

The upstream notice does not identify a Creative Commons version. This fork retains the notice without inventing one. The retained pitch-detection approach has modified RIFF parsing/resampling; the NEXT GEN workshop uses a separate FFT-based converter.

Copyright (C) 2026 Max Iwamoto applies to the NEXT GEN interface and additions, not to the underlying authors' work. See CREDITS.md for source-level provenance, upstream links and additional acknowledgements.

Optional test/build dependencies are installed separately under their own terms. They and their licences are not embedded into the Windows editor. Windows APIs are supplied by the operating system.

## Native Linux/macOS preview

The optional Qt front end dynamically links Qt Core, GUI, Widgets, Multimedia and Concurrent, Copyright The Qt Company Ltd. and other contributors. These Qt modules are available under LGPLv3 (or a commercial licence); their terms apply to Qt, not as a replacement licence for this project's inherited code. Qt source and licence texts: https://code.qt.io/cgit/qt/ and https://www.qt.io/licensing/open-source-lgpl-obligations . The native source and build instructions allow rebuilding against a compatible replacement Qt library. Linux packages use system Qt packages; macOS bundles retain Qt framework resources. Qt Multimedia may use separately licensed system or bundled codec components; retain their notices when packaging. This preview does not modify Qt.

## Web Studio

The browser build contains fflate (Arjun Barrett, MIT) and Emscripten runtime components (MIT/UIUC). Complete notices are in vendor/fflate-LICENSE.txt and vendor/emscripten-LICENSE.txt. Qt is not used by the web build. The inherited editor attribution above remains applicable.
