; Spectrum 128/+2/+3 and compatible external AY interface.
; Plain 48K Spectrum needs an AY expansion; the beeper is a different device.
AYFX_READ_GPIO:
	LD	BC,FFFDh
	LD	A,07h
	OUT	(C),A
	IN	A,(C)
	AND	C0h
	RET

; D=register, E=value. Preserves HL, DE.
AYFX_WRITE_REG:
	PUSH	BC
	LD	BC,FFFDh
	OUT	(C),D
	LD	B,BFh
	OUT	(C),E
	POP	BC
	RET
