; AYFXEDIT NEXT GEN runtime state. Code is ROM-safe; these 37 bytes must be RAM.
; Define AYFX_STATE before including this file. Calls must be serialized with
; your music/interrupt owner. No routine changes IFF or installs an interrupt.

AYFX_PTR:         EQU	AYFX_STATE
AYFX_LOOP:        EQU	AYFX_STATE + 02h
AYFX_WAIT:        EQU	AYFX_STATE + 04h
AYFX_ACTIVE:      EQU	AYFX_STATE + 05h
AYFX_PRIORITY:    EQU	AYFX_STATE + 06h
AYFX_MASK:        EQU	AYFX_STATE + 07h
AYFX_REGS:        EQU	AYFX_STATE + 08h
AYFX_OUTPUT:      EQU	AYFX_STATE + 16h
AYFX_GPIO:        EQU	AYFX_STATE + 24h
AYFX_STATE_SIZE:  EQU	25h
AYFX_REG_VALUE:   EQU	00h
