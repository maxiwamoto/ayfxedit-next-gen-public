#pragma once
// The guide is compiled into the application; no browser or internet is required.
#ifdef _WIN32
#include <richedit.h>
#endif
struct HelpTopic {const wchar_t* section;const wchar_t* title;const wchar_t* body;int command;};
enum {GuideStart=-1,GuideGrid=-2,GuideKeys=-3,GuideChannels=-4,GuideTiming=-5,GuideCredits=-6};
const HelpTopic helpTopics[]={
 {L"File",L"Export AY registers",LR"(Exports the complete selected effect group, using all assigned channels regardless of preview mute/solo. Choose .psg11 for the Castlevania II converter (R0-R10, 11 bytes per frame) or .ayregs for the full AY state (R0-R13, 14 bytes per frame). A matching filename.json records clock, tick rate, channel mask, frame count, priority and loop boundaries. The loop is stored once, not repeated.

PSG11 supports fixed-volume effects and shared noise. It cannot store hardware envelopes, so export rejects envelope effects rather than discarding their events. Use AY14 or the compact SDK instead. In AY14, R13=255 means no write this frame; 0-15 means retrigger, even when equal to the previous shape. R14/R15 I/O registers are excluded.

These are raw frame records, not standard .psg log files. The Castlevania tools can delta-encode PSG11; bank placement and game IDs remain the game's responsibility. Full AY14 needs an envelope-aware consumer. No additional register player is included in this editor.)",ExportRegisters},

 {L"Tools",L"Workshop zoom and analysis",LR"(Zoom selection enlarges the selected waveform range; Zoom out restores the entire source. Dragging selects times inside the current view. Spectrogram switches from waveform to a cached frequency display with time along X and logarithmic 80 Hz to 10 kHz along Y. Bright colours indicate stronger energy; the white outline marks the selection. Analysis is computed only when the view changes.

Tone sensitivity and noise sensitivity range from 0 to 100 (default 50). Higher tone sensitivity accepts weaker tonal peaks. Higher noise sensitivity more readily allocates a noise channel. These are separate from Brightness, which shifts fitted noise periods and spectral weighting. Convert after changing any analysis setting. Use Envelope and pitch after Apply for manual frame-range pitch corrections; Curve designer can replace a pitch or volume contour.

Apply saves the original selected audio plus conversion settings in the project. Reopen saved audio source restores them later. Loop previews preserve oscillator, noise and envelope state through each loop; WAV exports render a finite pass.)",-20},
 {L"Tools",L"Curve designer",LR"(Select frames in the main grid, then open Tools > Curve designer. A single selected frame expands to the rest of the active track. Enter the inclusive range as start:end in decimal. Choose volume, tone period or noise period. Click to add a point, drag to reposition, and right-click to delete. The top of the graph is the maximum value: volume 15, tone 4095 or noise 31. Higher tone periods mean lower pitch. Choose Linear or Smooth interpolation. Preview auditions a copy; Apply writes the curve to the active track as one undo step. Cancel leaves the effect unchanged. Volume curves switch the selected cells to fixed volume.)",Curves},
 {L"Tools",L"Sound generator",LR"(Create laser, explosion, jump, pickup, footsteps or engine sounds. Frames controls duration at the selected 50/60 Hz tick rate. Channels selects 1, 2 or 3 tracks. Start/end frequencies control the pitch sweep; pickup and engine presets use their own pitch patterns. Noise amount controls noisy components when the preset uses them. Seed creates repeatable variations. Preview listens without changing the bank. Apply adds a new grouped effect. All generated frames remain editable.)",Generate},
 {L"Tools",L"Envelope and pitch",LR"(Select frames and choose Tools > Envelope and pitch. This edits the active track. Software-controlled volume changes at the project tick rate. A hardware envelope can change volume between ticks, producing smooth fades or fast modulation, but it is not automatically a more faithful sound. All three channels share one envelope generator with a limited set of shapes.

Hardware envelope replaces fixed volume with the AY envelope generator. Period is a 16-bit divider; shapes 0-15 are the AY register 13 shapes. Retrigger can preserve existing events, trigger once at the selection start, trigger every frame or clear events. AY has one envelope shared by all channels. Conflicting periods or retriggers across active channels are rejected. Use matching values when sharing it.

Transpose and vibrato provide manual pitch corrections. Vibrato depth is in semitones and cycles spans the selected range. Preview validates and plays the group. Apply is undoable. The status line shows ENV when the track uses envelope events. Classic AFX/AFB alone cannot represent hardware envelopes: keep the project sidecar or export the game SDK.)",Envelope},
 {L"Tools",L"Find in library",LR"(Search grouped names, track names and tags. Favourites only restricts results to groups with a favourite track. Preview reports the next match; Apply selects it. Search wraps around the current bank. Tags and favourites are edited in Effect details.)",FindLibrary},
 {L"Tools",L"Tags, favourite and priority",LR"(Tags and favourite status belong to the active track and are saved in the project sidecar. Favourite groups display an asterisk in the library. Priority belongs to the whole grouped effect. The game player accepts equal or higher priority sounds and rejects a lower priority sound while one is active. Values range from 0 to 255; default is 128.)",Metadata},
 {L"Tools",L"Extract PSG / VGM capture",LR"(Import a plain PSG log or uncompressed .vgm file containing one AY chip. Select the first frame, exclusive end frame, and channel mask (A=1, B=2, C=4; sum them, for example A+C=5). The range is limited to 4096 frames. All selected channels are extracted together, including noise and envelope writes. A mask with gaps is compacted to the new group channels.

VGM is sampled at the current 50/60 Hz rate. Multiple register writes within a tick are quantized, so this is not a cycle-exact VGM player. Tone/envelope periods are scaled to the editor target clock. PSG logs have no reliable clock metadata and are interpreted at the selected clock. Up to five minutes can be inspected. Dual-AY, .vgz and unsupported commands are rejected. Preview listens before Apply creates a group.)",CaptureImport},
 {L"Tools",L"Export game SDK",LR"(Choose a NEW folder name. Export writes the bank, named IDs, ROM statistics, formatted tniASM sources, SDCC C sources, MSX/Spectrum port modules, examples and build scripts. The executable includes the compact player templates; no source checkout is needed. Changed channel fields are packed, unchanged frames use repeat counts, and identical effects share data. Source WAV/MP3 files are never included in a game bank.

Run build_tniasm.bat with TNIASM pointing to your tniASM executable. It uses compat.asm, as our MSX projects do. Run build_sdcc.bat with SDCC pointing to sdcc.exe. Modules are callable examples, not bootable ROMs. See README.md in the export for entry points, memory mapping, clock/tick settings and interrupt integration.

Small games can use example_small_msx.asm or example_small_spectrum.asm: 324/332 bytes of MSX/Spectrum code and 23 RAM bytes, with no music mixer. The full integration uses 509/517 code bytes, 37 RAM bytes and the optional mixer. Code sizes include PSG I/O and exclude effect data/example entry vectors. Both preserve noise and hardware envelopes. PSG/VGM imports export through this same compact format.

One effect can use 1, 2 or 3 assigned channels. Music survives on unused channels. Noise and envelope are shared AY resources, so conflicting music components are muted while the effect owns them. One prioritized effect group plays at a time. Loop data is stored once. Banks must fit the mapped address window; split large banks. ZX Spectrum requires the 128K AY ports or a compatible AY expansion.)",ExportSDK},
 {L"Tools",L"Batch import audio / AFX",LR"(Select several .afx, .wav or .mp3 files. Choose the audio channel count. Audio is normalized and converted from its start, limited to 4096 frames per file at the current rate. AFX files are imported unchanged. Each file becomes a new group. Apply stages the whole batch and commits only when all files succeed; Undo removes the batch. The source audio selection is retained for converted files. Use the workshop for individual trim, fades and sensitivity adjustments.)",BatchImport},
 {L"Tools",L"Batch export WAV + project",LR"(Choose a NEW folder. Every grouped effect is rendered into one WAV using its full timeline once, without infinite loop repetition. Filenames include an index and sanitized name. A complete library.afb and .ayfx-project pair preserves editable tracks and extended metadata. Existing folders are not overwritten. Use Export game SDK for executable Z80 playback data.)",BatchExport},
 {L"Tools",L"Recover autosaved project",LR"(Dirty projects are autosaved once per minute under %LOCALAPPDATA%\AYFXEDIT-NEXT-GEN\Recovery, using a separate filename for each editor session. After a crash, use this command to open a snapshot. Recovery becomes an untitled dirty project so Save As creates your chosen destination. Normal close removes only that session's snapshot after the save/discard decision. Other sessions and older crash snapshots remain available. The editor reports autosave failures in its status line.)",Recovery},
 {L"Tools",L"Reopen saved audio source",LR"(Workshop Apply stores the selected original mono PCM audio and conversion settings on the first track of the grouped effect. This command opens that source again, restoring channels, fades, pitch, brightness, normalization and sensitivity. Trim times now start at zero because the saved source is the selected excerpt. Use Replace selected effect channels when applying a revised conversion. This data lives in .ayfx-project; keep it with the AFB. Batch imports preserve their selected source too.)",ReopenSource},
 {L"Start here",L"Getting started",LR"(Open an AFB bank or AFX effect from File. Select an effect in the library, then click a frame to edit it.

1. Choose the AY clock and 50 / 60 Hz rate to match your target machine.
2. Enter tone, noise and volume values, or import audio through the WAV / MP3 workshop.
3. Press Enter to listen and Space to stop.
4. Save your bank. Keep its .ayfx-project companion beside the AFB to preserve groups, loops and timing.

Browse the File, Edit, Sound and Tools branches for every menu command. The keyboard and editing topics explain the grid and toolbar.

An AFX is one channel track. An AFB stores up to 256 tracks. A grouped effect can use one, two or three tracks, with up to 4096 frames per track.)",GuideStart},
 {L"Start here",L"Grid, library and piano",LR"(Select an effect in the left library. The - / + buttons move between grouped effects; 2 CH / 3 CH badges show multichannel entries.

Click a numeric field and type hexadecimal digits. Tone ranges from 000 to FFF, noise from 00 to 1F, and volume from 0 to F. Higher tone periods mean lower pitches. T and N enable tone and noise.

Drag a value bar to draw changes. Right-drag a bar to clear values. Click frame numbers to move without changing sound. Shift-click or Shift+Up/Down extends the selection. Frame labels are hexadecimal.

For multichannel effects, click a channel header or field to choose the active track. The colored A/B/C: Sound ON or MUTED buttons above the grid toggle preview sound. Green means audible; red means muted. Labels identify the state without relying on color. Numeric edits affect that track only.

To combine two existing tracks into one layered effect, open Channels / loop and assign them to A and B (or C). Play all mixes the enabled channels; mute the third channel to audition a pair. This preserves separate AY voices; it does not merge two simultaneous tones into one hardware channel.

The name field renames the active track. + Track adds a blank standalone track; Copy duplicates the active track; Remove A/B/C above each assigned channel removes that specific track, leaving the other tracks intact. Undo restores it. The last bank track is retained, with its removal button disabled. These are not whole-group duplication controls.

Choose an octave and click a piano key to write a note at the current frame. Piano entry writes one frame at a time.)",GuideGrid},
 {L"Start here",L"Keyboard shortcuts",LR"(File and help
Ctrl+N   New bank
Ctrl+O   Open bank / effect
Ctrl+S   Save bank
F1           User guide

With the editor grid focused
Enter                 Play
Ctrl+Enter        Play from cursor
Space                 Stop
Ctrl+Z / Ctrl+Y   Undo / Redo
Ctrl+C / X / V     Copy / Cut / Insert pasted frames
Ctrl+A                Select all frames
Insert                  Insert blank frame
Ctrl+Insert         Duplicate frame
Delete                 Delete selected frames
Up / Down          Move frame
Left / Right         Move numeric field
Page Up / Down  Move through frames
Home / End        First / last frame
Shift+Up / Down  Extend selection
0-9, A-F               Enter hexadecimal values
T / N                    Toggle tone / noise

With the grid or library focused
+ / -   Next / previous grouped effect (including numpad)

Text fields retain normal typing. In dialogs, Tab moves between controls and Escape closes or cancels. In this guide, arrow keys browse the topic tree; Tab moves into the article for scrolling and copying.)",GuideKeys},
 {L"Start here",L"Multichannel effects and noise",LR"(Use Edit > Effect channels and loop to assign bank tracks to A, B and C. Each track belongs to one effect/channel; names do not determine grouping after assignment.

Play combines enabled channels. Solo auditions the active track. Linked timeline editing keeps insertions and deletions aligned across channels; value edits still affect only the active track.

The AY chip has one shared noise generator. Audible channels using noise must request the same noise period in a frame. Conflicts appear in red; playback rejects them. Fix the conflicting values, or disable noise on a channel that should use tone only.

Save creates an AFB plus a .ayfx-project file with group assignments, names, loops, clock and rate. Keep both together. An AFB-only player sees individual tracks and needs its own logic for multichannel effects.

When loading old banks without a companion, names such as 0F_A_fire_whip and 0F_C_fire_whip can be grouped once. Later grouping is explicit; renaming a track does not regroup it.)",GuideChannels},
 {L"Start here",L"Clock, rate and preview model",LR"(Clock
Choose MSX (1.789773 MHz), Spectrum (1.7734 MHz) or 2 MHz. Changing the clock changes playback pitch; it does not retune stored tone values.

Frame rate
50 Hz means 20 ms per frame; 60 Hz means about 16.67 ms. Use the rate expected by your game's player. Clock and rate are stored in the companion project, not in bare AFX/AFB data.

Preview model
Reference AY is the default. It models logarithmic volume steps and shared noise with filtering. Legacy AYFX is available for comparison on standalone effects. Multichannel and loop previews use Reference.

No per-effect normalization is applied in normal editor playback. The workshop has its own optional Normalize control. AFX stores fixed volume values, not AY hardware envelopes.)",GuideTiming},
 {L"File",L"New bank",L"Ctrl+N\n\nStart a new bank containing one empty track. The editor prompts if the current bank has unsaved changes. Starting a new bank clears the previous undo history.",New},
 {L"File",L"Open bank / effect...",L"Ctrl+O\n\nOpen an AFB bank or AFX effect. Opening replaces the current bank after the unsaved-changes prompt. You can also drop an AFB or AFX file onto the editor.\n\nAn AFB's companion .ayfx-project restores groups, loops and timing. If the companion does not match the bank, loading is rejected to avoid using stale assignments. Keep matching files together; for an intentionally changed external bank, move the old companion aside and reassign channels.",Open},
 {L"File",L"Save bank",L"Ctrl+S\n\nSave the current bank with track names and a companion .ayfx-project file. A bank without a path opens the Save As dialog.\n\nThe companion stores multichannel grouping, group names, loop ranges, clock/rate, hardware-envelope events, priorities, tags/favourites and any saved source audio/conversion recipe. Both files are needed to reopen the complete editor project.",Save},
 {L"File",L"Save bank as...",L"Choose a new AFB filename and save the bank plus its companion project file. The new path becomes the current save destination.\n\nThe editor validates the bank and group data before saving. If companion replacement fails, the old bank file is restored.",SaveAs},
 {L"File",L"Export bank without names...",L"Write an AFB without track names. This is a separate bank-only export; it does not write a companion or change your normal save destination.\n\nUseful for a game build that needs compact track data. Grouping, loops and timing must be handled by the game's player. Hardware-envelope effects are rejected because a bare AFB cannot preserve their events.",SaveBare},
 {L"File",L"Load current effect...",L"Load a single AFX into the active channel track, replacing its data. The filename becomes the track name. Other bank tracks remain unchanged.\n\nUse Undo to restore the previous track. Use Open bank / effect if you intend to replace the whole bank instead.",LoadFx},
 {L"File",L"Save current effect...",L"Export only the active channel track as an AFX file. A multichannel group requires separate tracks; this command does not bundle A/B/C or their grouping metadata. Hardware-envelope effects are rejected; save the bank with its project sidecar or use AY14/the compact SDK.",SaveFx},
 {L"File",L"WAV / MP3 workshop...",LR"(Convert a short audio clip into editable AY tone, noise and volume frames.

1. Load WAV or MP3. Drag the waveform to select a range, or enter Start and End in milliseconds. Full range selects the complete source.
2. Choose 1-3 AY channels. These are synthesized tone/noise parts, not original stereo channels.
3. Adjust the controls below, then click Convert.
4. Compare Original and AY result. Switching retains the position within the selection. Stop ends preview.
5. Apply as a new effect or replace the selected effect's channels. Apply supports Undo; Cancel leaves the bank unchanged.

Normalize scales the selected clip's envelope. Turn it off to retain source level differences.
Fade-in / Fade-out set attack and decay times in milliseconds (0-2000).
Pitch shifts tones by -24 to +24 semitones.
Brightness changes spectral emphasis and noise bandwidth; higher values are brighter.

Changing settings invalidates the AY preview until you Convert again. Conversion runs in the background; Cancel conversion stops the attempt. Replacement preserves existing track names, clears the group's loop and silences spare channels.

Input: 8/16-bit PCM WAV or MP3, mono/stereo, up to 64 MB and five minutes. Stereo is averaged to mono. Select at most 4096 AY frames. MP3 may include encoder padding; trim it if needed. Windows Media Foundation is required for MP3.

This is an AY approximation, not PCM playback. Short, distinct effects usually translate better than mixed music.)",ImportWav},
 {L"File",L"Import PSG dump...",L"Import AY register data from a PSG file into the active track. This replaces the active track and supports Undo.\n\nBefore importing, choose Sound > PSG import: channel A, B, C or automatic. Automatic chooses the first channel whose volume changes and skips its initial pause.\n\nThe imported data uses fixed volume, tone, noise and mixer state. AY hardware envelopes are not represented. Use Tools > Extract PSG / VGM capture for multichannel VGM or envelope-aware PSG import. VTX is not supported.",ImportPsg},
 {L"File",L"Export WAV...",L"Render the selected effect to a 44.1 kHz, 16-bit mono WAV. A grouped effect combines enabled channels, using the current mute mask, clock and frame rate.\n\nExport contains one pass, not an endless loop. It follows the applicable preview model; multichannel and loop previews use Reference. Shared-noise conflicts must be corrected before rendering.",ExportWav},
 {L"File",L"Export CSV...",L"Export the active track's frames as text rows: tone-enabled, noise-enabled, tone period, noise period, volume. Periods and volume are hexadecimal. Only the active track is exported. The save dialog defaults to the bank's folder and filename with a .csv extension (sounds.afb becomes sounds.csv); an unsaved bank uses Untitled.csv. You can choose a different name or folder. Hardware-envelope effects are rejected; use AY14 register export or the compact SDK to preserve them.",ExportCsv},
 {L"File",L"Export VT2 sample (C4)...",L"Export the active track as a Vortex Tracker II sample text file, using base note C4 and the selected AY clock. The format is limited to 63 frames; longer effects are truncated for this export. Hardware-envelope effects are rejected.",ExportVT},
 {L"File",L"Exit",L"Close the editor. Unsaved bank changes trigger a save/discard/cancel prompt. Stop or close any modal dialog before leaving the editor.",Quit},
 {L"Edit",L"Undo",L"Ctrl+Z (grid focused)\n\nRestore the previous edit, including grouped track assignments and workshop Apply. Navigation and preview do not create edits.",Undo},
 {L"Edit",L"Redo",L"Ctrl+Y (grid focused)\n\nReapply an undone edit. Making a new edit after Undo clears the redo branch.",Redo},
 {L"Edit",L"Cut frames",L"Ctrl+X (grid focused)\n\nCopy the selected frames, then delete them and shift later frames earlier. With linked editing enabled, the operation keeps the group's channels aligned. Without a range selection, the current frame is used.",Cut},
 {L"Edit",L"Copy frames",L"Ctrl+C (grid focused)\n\nCopy the selected frames to the editor's internal frame clipboard. With linked editing enabled, copying preserves the group's A/B/C data. Without a range selection, the current frame is used.",Copy},
 {L"Edit",L"Insert pasted frames",L"Ctrl+V (grid focused)\n\nInsert copied frames at the cursor, shifting later data. With linked editing enabled, all channels shift together; pasting a single track inserts silence into the other channels. The track limit is 4096 frames.",Paste},
 {L"Edit",L"Select all frames",L"Ctrl+A (grid focused)\n\nSelect the active track from frame zero through its last audible frame. Linked frame operations use this range across the group. Click a frame without Shift to start a new selection.",SelectAll},
 {L"Edit",L"Insert frame",L"Insert (grid focused)\n\nInsert a blank frame at the cursor and shift later frames. With Link channel frame edits enabled, all channels shift together. This operation supports Undo.",InsertRow},
 {L"Edit",L"Duplicate frame",L"Ctrl+Insert (grid focused)\n\nInsert a copy of the current frame and shift later frames. Linked editing duplicates the corresponding frame on each assigned channel.",CloneRow},
 {L"Edit",L"Delete frames",L"Delete (grid focused)\n\nRemove the selected frames, or the current frame when no range is selected. Later data shifts earlier. Linked editing applies the same shift across the group; removing an entire loop disables it.",DeleteRows},
 {L"Edit",L"Clear current effect",L"Clear all frames of the active channel track. Its name is retained and other tracks in the group are unchanged. Undo restores the cleared data.",ClearFx},
 {L"Edit",L"Previous effect",L"- (grid or library focused)\n\nSelect the previous grouped effect. Navigation stops at the beginning of the library. It does not step through individual tracks inside the current group.",PreviousEffect},
 {L"Edit",L"Next effect",L"+ (grid or library focused)\n\nSelect the next grouped effect. Navigation stops at the end of the library. The numpad + key also works; text fields retain normal typing.",NextEffect},
 {L"Edit",L"Effect channels and loop...",LR"(Name the group and assign bank tracks to A, B and C. Choose New blank track to create a channel. Reassigning an existing track moves it from its previous group; displaced tracks remain available as standalone effects.

Enable Repeat loop during playback and enter decimal First frame and End (exclusive). For example, 10 to 20 repeats frames 10 through 19. The lead-in plays once, then the loop repeats until Stop. Purple grid lines mark the boundaries.

Apply supports Undo. Group and loop metadata is saved in the companion .ayfx-project file. A bare AFB has no group or loop metadata.)",Channels},
 {L"Edit",L"Link channel frame edits",L"Enabled by default. Insert, duplicate, delete, cut and paste keep all assigned channels aligned. Linked timeline changes also shift loop markers.\n\nTurn this off for intentional single-track timing changes. Numeric values, rename, Clear, and the library's Add/Copy buttons operate on tracks. Remove A/B/C above the grid deletes only the named channel track and supports Undo.",Linked},
 {L"Sound",L"Play",L"Enter (grid focused)\n\nPlay the selected effect from frame zero, combining its enabled channels. A configured loop repeats until Stop. Playback uses the selected clock and frame rate.",Play},
 {L"Sound",L"Play from cursor",L"Ctrl+Enter (grid focused)\n\nStart playback at the current frame. The group's enabled channels play together, and a configured loop repeats until Stop.",PlayHere},
 {L"Sound",L"Stop",L"Space (grid focused)\n\nStop the current editor preview, including repeated loops. Workshop preview has its own Stop button.",Stop},
 {L"Sound",L"Solo active channel",L"Audition only the active track from the beginning. Solo ignores the mute switches, so you can inspect a muted track without changing the mix.",Solo},
 {L"Sound",L"Mute A",L"Toggle channel A in the audition/WAV-export mix. Changing a mute stops the current preview; press Play again to hear the new mix. Muting does not delete or edit track data. Game SDK and raw register exports include all assigned channels regardless of preview mutes.",MuteA},
 {L"Sound",L"Mute B",L"Toggle channel B in the audition/WAV-export mix. Changing a mute stops the current preview; press Play again. The colored B: Sound ON / B: MUTED button above channel B performs the same operation.",MuteB},
 {L"Sound",L"Mute C",L"Toggle channel C in the audition/WAV-export mix. Changing a mute stops the current preview; press Play again. WAV export uses the current mute mask; Solo ignores it.",MuteC},
 {L"Sound",L"PSG import: channel A",L"Use channel A from the next imported PSG dump. This selects an import source; it does not select the editor's active track or change playback.",ChooseA},
 {L"Sound",L"PSG import: channel B",L"Use channel B from the next imported PSG dump. Then choose File > Import PSG dump to replace the active track with that data.",ChooseB},
 {L"Sound",L"PSG import: channel C",L"Use channel C from the next imported PSG dump. Then choose File > Import PSG dump to replace the active track with that data.",ChooseC},
 {L"Sound",L"PSG import: automatic",L"Let PSG import select the first channel whose volume changes, skipping its leading pause. This is the default. Choose an explicit A/B/C source when automatic selection does not pick the intended effect.",ChooseAuto},
 {L"Sound",L"Linear tone bars",L"Toggle the standalone grid's tone-period bar between linear and logarithmic display. Only the bar scale changes; stored tone periods and playback remain the same.",Linear},
 {L"Help",L"User guide",L"F1\n\nBrowse the topic tree for every File, Edit, Sound, Tools and Help command. Start here contains keyboard shortcuts, grid controls, timing and multichannel concepts.\n\nResize this window for more reading space. Articles wrap and scroll; their text can be selected and copied. The guide is included in the executable and works offline. The workshop's Help button opens its conversion topic directly.",Help},
 {L"Help",L"About AYFXEDIT - NEXT GEN",L"Show the editor's identity, copyright and source attribution. About links to Shiru's original project website and Threetwosevensixseven's improved fork. The Credits button opens the detailed acknowledgements in this guide.",About},
 {L"Help",L"Credits and source attribution",LR"(AYFXEDIT - NEXT GEN
By Max Iwamoto
OCT.22.2026
Copyright (C) 2026 Max Iwamoto - NEXT GEN interface and additions.

Based on AY Sound FX Editor v0.6 by Shiru, through Threetwosevensixseven's ayfxedit-improved fork (base commit e732307). The original C++ Builder sources remain available in the upstream repository linked below.

Original AYFXEdit:
https://shiru.untergrund.net/software.shtml

Improved fork:
https://github.com/Threetwosevensixseven/ayfxedit-improved

Additional source acknowledgements:
- The inherited WAV importer credits John Montgomery's 5KTuner, CC-BY 2008, integrated upstream in 2017. The new workshop uses a separate FFT-based converter.
- The original renderer's noise code credits HackerKAY.
- The reference AY preview was informed by openMSX AY8910 source; it is not the complete openMSX audio pipeline.
- MP3 decoding uses Microsoft Windows Media Foundation.

The game SDK players and extended project format are NEXT GEN additions. The optional test suite uses Ivan Kosarev's MIT-licensed z80 emulator; it is not included in the application.

Original code and contributions remain attributed to their respective authors. NEXT GEN attribution does not replace upstream credits or license notices.)",GuideCredits}
};
constexpr size_t helpTopicCount=sizeof(helpTopics)/sizeof(helpTopics[0]);
