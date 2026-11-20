; AYFXEDIT NEXT GEN compact channel player, tniASM.
; Include record_layout.asm (music) or standalone_layout.asm, then one I/O file.
; START: HL=entry (from AYFX_BANK table); carry=priority rejection.
; TICK: call once per exported frame, before optional MIX and WRITE.
; Optional MIX is in mixer.asm: HL=music image or zero. R13=FFh means no write.
; All functions clobber AF/BC/DE/HL; TICK preserves IX. No EI/DI or BIOS calls.
; Bank, code and state must remain mapped during calls.

AYFX_INIT:
	LD	HL,AYFX_STATE
	LD	DE,AYFX_STATE + 0001h
	LD	BC,AYFX_STATE_SIZE - 01h
	LD	(HL),00h
	LDIR
	LD	A,3Fh
	LD	(AYFX_REGS + 07h),A
	LD	(AYFX_OUTPUT + 07h),A
	LD	A,FFh
	LD	(AYFX_REGS + 0Dh),A
	LD	(AYFX_OUTPUT + 0Dh),A
	CALL	AYFX_READ_GPIO
	LD	(AYFX_GPIO),A
	RET

AYFX_START:
	INC	HL
	LD	B,(HL)
	DEC	HL
	LD	A,(AYFX_ACTIVE)
	OR	A
	JR	Z,.accept
	LD	A,(AYFX_PRIORITY)
	CP	B
	JR	C,.accept
	JR	Z,.accept
	SCF
	RET

.accept:
	LD	A,B
	LD	(AYFX_PRIORITY),A
	LD	A,(HL)
	LD	(AYFX_MASK),A
	PUSH	HL
	INC	HL
	INC	HL
	LD	E,(HL)
	INC	HL
	LD	D,(HL)
	POP	HL
	PUSH	HL
	LD	A,D
	AND	E
	INC	A
	JR	Z,.no_loop
	ADD	HL,DE
	JR	.store_loop

.no_loop:
	LD	HL,0000h

.store_loop:
	LD	(AYFX_LOOP),HL
	POP	HL
	LD	DE,0004h
	ADD	HL,DE
	LD	(AYFX_PTR),HL
	XOR	A
	LD	(AYFX_WAIT),A
	INC	A
	LD	(AYFX_ACTIVE),A
	OR	A
	RET

AYFX_STOP:
	XOR	A
	LD	(AYFX_ACTIVE),A
	LD	(AYFX_WAIT),A
	LD	(AYFX_REGS + 08h),A
	LD	(AYFX_REGS + 09h),A
	LD	(AYFX_REGS + 0Ah),A
	DEC	A
	LD	(AYFX_REGS + 0Dh),A
	LD	A,3Fh
	LD	(AYFX_REGS + 07h),A
	RET

AYFX_TICK:
	LD	A,FFh
	LD	(AYFX_REGS + 0Dh),A
	LD	A,(AYFX_ACTIVE)
	OR	A
	RET	Z
	LD	A,(AYFX_WAIT)
	OR	A
	JR	Z,.read
	DEC	A
	LD	(AYFX_WAIT),A
	RET

.read:
	LD	HL,(AYFX_PTR)
	LD	A,(HL)
	INC	HL
	OR	A
	JR	NZ,.command
	LD	HL,(AYFX_LOOP)
	LD	A,H
	OR	L
	JP	Z,AYFX_STOP
	LD	A,(HL)
	INC	HL

.command:
	BIT	07h,A
	JR	Z,.channels
	AND	7Fh
	LD	(AYFX_WAIT),A
	JR	.save

.channels:
	LD	B,A
	LD	C,03h
	LD	DE,AYFX_REGS
	PUSH	IX
	LD	IX,AYFX_REGS + 08h

.channel:
	SRL	B
	JR	NC,.skip_channel
	LD	A,(HL)
	INC	HL
	PUSH	AF
	AND	1Fh
	LD	(IX+AYFX_REG_VALUE),A
	POP	AF
	BIT	05h,A
	CALL	NZ,.byte
	INC	DE
	BIT	06h,A
	CALL	NZ,.byte
	DEC	DE

.skip_channel:
	INC	DE
	INC	DE
	INC	IX
	DEC	C
	JR	NZ,.channel
	POP	IX
	SRL	B
	CALL	C,.byte
	LD	DE,AYFX_REGS + 0Bh
	SRL	B
	JR	NC,.shape
	CALL	.byte
	INC	DE
	CALL	.byte

.shape:
	LD	DE,AYFX_REGS + 0Dh
	SRL	B
	CALL	C,.byte
	LD	DE,AYFX_REGS + 07h
	SRL	B
	CALL	C,.byte

.save:
	LD	(AYFX_PTR),HL
	RET

; Copy one field byte without disturbing packed flags in A or B.
.byte:
	PUSH	AF
	LD	A,(HL)
	INC	HL
	LD	(DE),A
	POP	AF
	RET

AYFX_WRITE:
	LD	HL,AYFX_OUTPUT
	LD	D,00h

.register:
	LD	E,(HL)
	LD	A,D
	CP	07h
	JR	NZ,.write
	LD	A,(AYFX_GPIO)
	LD	B,A
	LD	A,E
	AND	3Fh
	OR	B
	LD	E,A

.write:
	CALL	AYFX_WRITE_REG
	INC	HL
	INC	D
	LD	A,D
	CP	0Dh
	JR	NZ,.register
	LD	E,(HL)
	LD	A,E
	CP	FFh
	RET	Z
	JP	AYFX_WRITE_REG	; io_msx.asm or io_spectrum.asm
