# Desktop → Web feature map

Comparison date: 2026-11-22. References: native/main.cpp, native/help_content.h, native/tools_ui.h, native/workshop_ui.h, portable/main.cpp and portable/workshop_dialog.cpp. Windows is the superset; the Qt preview has fewer tools in some areas. This maps functional workflows, not identical window layouts or operating-system integration.

| Standalone feature | Web control / implementation |
| --- | --- |
| New/Open/Save/Save As; unsaved-change prompt | Open/Save project; Tools → New bank / Save project as. ZIP contains matching AFB and companion. |
| AFX active-track load/save; bare bank | Tools → Load active track; Export → AFX / Bank without names. |
| WAV, CSV, VT2, AY14/PSG11, SDK | Export menu; existing native encoders and renderers. |
| Batch WAV + project / classic AFX | Export → All effects / All tracks; ZIP replaces folder output. |
| Batch AFX/WAV/MP3 import | Tools → Batch import, fully staged; one undo step. |
| Classic PSG automatic/A/B/C | Tools → Classic PSG import; native importPSG. |
| PSG/VGM extraction | Tools → Extract, up to five-minute log; selected range ≤4096 frames and mask; target-clock scaling. |
| Numeric frame fields; tone/noise/envelope | Numeric frame grid, hexadecimal; inspector decimal; same field limits. |
| Selection, bars, right-drag clearing, linear/log tone | Timelines, Draw, Tone scale. Numeric grid pages all assigned channels together. |
| Linked insert/repeat/cut/copy/paste/delete; clear | Toolbar and keyboard; loop boundary adjustment; overflow rejected. |
| Undo/redo; select all; next/previous | Toolbar and shortcuts. Browser input fields retain ordinary editing keys. |
| Add/copy/remove track or group; clear active track | Library +, duplicate/remove; channel ×; Tools → Track details. |
| Explicit channel assignment, displaced tracks, blank voices | Channels…; shared native assignChannels preserves displaced tracks. |
| Group/track names, tags, favourites, priority | Heading; Track details; Tags & priority; favourites filter; Channels. |
| Reference/legacy preview, from-cursor, mute/solo | Transport model selector; From selection; channel controls. Reference forced for loops/multichannel/envelopes. |
| Noise/envelope conflict diagnostics | Inspector warning, red numeric-grid rows, native validation before playback/export. |
| Loops and stateful playback | Loop / Channels; persistent native AYStream through chunks and loop boundaries. |
| Piano, octaves 0–8 | Inspector piano and full octave selector. |
| Multipoint linear/smooth curve, range, preview | Curve… graph plus numeric point editing; native applyCurve. |
| Generator presets, seed, preview | Create a sound; native generateSound. |
| Transpose/vibrato/envelope/retrigger policies | Pitch / envelope…; native modulation and envelope rules. |
| WAV/MP3 source load, waveform, trim, zoom | Workshop; native decoder for supported PCM WAV, browser decoder for MP3. |
| Spectrogram and analysis sensitivities | Workshop cached 256×64 spectrum, same native FFT and logarithmic frequency mapping. |
| Position-preserving Original/AY preview | Workshop A/B buttons retain offset; Stop resets. |
| Convert/cancel; normalization/fades/pitch/brightness | Workshop worker, original native conversion settings. |
| Apply new / replace channels; preserved names | Workshop destination selector; native applyConverted clears loops and silences spare channels. |
| Reopen retained source/settings; load other audio | Tools → Reopen saved audio; workshop Load other audio. |
| Per-session recovery | Separate IndexedDB key per tab; Tools → Recovery snapshots; legacy single-slot data still readable. |
| Guide, contextual F1, credits | 26 searchable topics; F1 opens relevant tool topic; original credits retained. |

## Browser-specific behaviour

- Save prepares a download; it cannot verify that a browser wrote it to disk or silently overwrite arbitrary local files. Save As chooses the suggested filename. Folder exports arrive as ZIPs.
- Browser storage is profile/site scoped and can be evicted; downloaded projects remain the durable backup. No account or server upload is used.
- Operating-system audio devices and MP3 decoding are browser-managed. Browser-reserved shortcuts may take precedence. Closing the tab replaces Exit.
- Layout, track selection and playback buttons differ visually. Reference and legacy sound processing use the shared implementation. Exact native GUI placement is not reproduced.

## Verification boundary

Automated native/WASM comparisons, scheduler tests and local Chromium UI checks passed; details in VALIDATION.md. Real Safari/Firefox and Linux/macOS audio-device/listening checks remain. Therefore this build implements the identified feature inventory, but is not labelled universally certified or publicly released.
