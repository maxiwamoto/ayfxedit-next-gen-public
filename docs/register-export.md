# AY register interchange

Use File > Export AY registers. Export includes all assigned A/B/C channels of
the selected group, independently of preview mute/solo. It stores the lead-in and
one loop pass, ending at the loop's exclusive end; without a loop, it ends at the
last audible/envelope frame. Silence-only effects contain one frame.

- **.psg11**: raw R0 through R10, exactly 11 bytes per frame. R7 upper bits are
  80h, matching the existing Castlevania II PSG11 pipeline. No header or padding.
  Hardware-envelope volume and shape events are rejected before writing files.
- **.ayregs** (AY14): raw R0 through R13, exactly 14 bytes per frame. R7 has only
  mixer bits; I/O pins are not part of the effect. R13=FFh means *do not write*.
  R13=00h..0Fh is a trigger, even if equal to the preceding frame's shape.
  Do not treat FFh as a shape or deduplicate consecutive equal triggers.

Both use a sidecar named `<data filename>.json`: format/version, group name,
registers per frame, frame count, AY clock, tick rate, channel mask, priority and
loop boundaries. Loop fields are null without a loop. Registers with 16-bit
values occupy the AY's low-byte/high-byte registers in their usual order.
No R14/R15 I/O-port data is exported. These files are **not** standard `.psg` logs.

The CLI uses the same export implementation:

```
native/dist/ayfxedit-next-gen.exe --export-registers bank.afb chain.psg11 13
```

The optional final argument is the zero-based grouped effect index (default 0).
Failure returns exit code 2 and writes `<output>.error.txt`.

## Castlevania II handoff

The existing `tools/convert_usa_audio.py` in the Castlevania II repository
provides `delta_encode(frames)` and `delta_decode(data,count)` for 11-byte frames.
Split the `.psg11` file into 11-byte records, encode using that tool, and retain
the sidecar's timing/channel/loop metadata when placing blocks in the game bank.
The codec alone does not install a bank or assign game sound IDs. The game's
existing AFB staging/import route remains available as well.

That pipeline uses software-controlled volume, not AY hardware envelopes. AY14
requires an envelope-aware consumer; the editor's compact SDK already supplies
one. No separate register-stream player is maintained here.

Verify the actual external codec without copying it into this repository:

```
python tools/test_register_export.py --castlevania-tools C:/development/castlevania_2/tools
```

Run `test_native.bat` first to produce the register fixtures. The script imports
the converter read-only and checks an encode/decode round trip; it never writes
to the Castlevania project or changes its ROM.
