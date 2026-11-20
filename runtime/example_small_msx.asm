; Raw callable player module, origin 8000h, RAM record C000h.
; Entry vector: INIT, START (HL=effect), TICK/WRITE, STOP.
; This is a game integration module, not a bootable ROM.
	%symfile	"example_small_msx.sym"
	ORG	8000h

AYFX_STATE:  EQU	C000h

	JP	AYFX_INIT	; player.asm
	JP	AYFX_START	; player.asm
	JP	EXAMPLE_FRAME
	JP	AYFX_STOP	; player.asm

EXAMPLE_FRAME:
	CALL	AYFX_TICK
	JP	AYFX_WRITE	; player.asm

	%include	"standalone_layout.asm"
	%include	"player.asm"
	%include	"io_msx.asm"
	%include	"effects.asm"
