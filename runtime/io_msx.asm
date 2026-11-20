; MSX: preserve R7 GPIO-direction bits, never touch R14/R15.
; Call in the same serialized PSG owner as the music player.
AYFX_READ_GPIO:
	LD	A,07h
	OUT	(A0h),A
	IN	A,(A2h)
	AND	C0h
	RET

; D=register, E=value. Preserves HL, DE.
AYFX_WRITE_REG:
	LD	A,D
	OUT	(A0h),A
	LD	A,E
	OUT	(A1h),A
	RET
