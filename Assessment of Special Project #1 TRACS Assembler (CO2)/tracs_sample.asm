; ============================================================
; tracs_sample.asm
; TRACS Sample / Test Assembly Program
;
; Derived from the "TRACS Assembly Program" example in:
;   - 02-TRACS-Assembler-Briefing-v1.0-2526.pdf (slides 12-20)
;   - 03-CpE3202-3.3-Introduction-to-TRACS.pdf (slide 12)
;
; Program overview:
;   Stores 5 and 3 into data memory at 0x400 and 0x401.
;   Adds them; stores sum (8) at 0x402.
;   Loops back to "here" (SHL) if ACC > 5 (branch not taken the
;   first time since ACC=8 > 5, so it jumps to here).
;   At "here": shifts ACC left once, stores result at 0x405.
;
; Expected output (starting at byte address 0x010):
;   0x010: 0x0305  (WB  0x05  => opcode 0x06 << 11 | 0x05 = 0x3005)
;          high=0x30, low=0x05
;   ...see README for full expected output and verification.
; ============================================================

        ORG  0x010

start   WB   0x05           ; write 0x05 to MBR
        WM   0x400          ; write MBR to memory[0x400]
        WB   0x03           ; write 0x03 to MBR
        WM   0x401          ; write MBR to memory[0x401]
        RM   0x400          ; read memory[0x400] into MBR
        WACC                ; ACC = MBR (ACC = 0x05)
        RM   0x401          ; read memory[0x401] into MBR
        ADD                 ; ACC = ACC + MBR (0x05 + 0x03 = 0x08)
        RACC                ; MBR = ACC
        WM   0x402          ; memory[0x402] = ACC (= 0x08)
        WB   0x00           ; MBR = 0x00
        RM   0x402          ; MBR = memory[0x402] (= 0x08)
        WACC                ; ACC = 0x08
        WB   0x05           ; MBR = 0x05
        BRGT here           ; if ACC > MBR jump to "here" (0x08 > 0x05 => taken)
        RACC                ; (unreachable if branch taken)
        WM   0x403          ; (unreachable if branch taken)

here    SHL                 ; ACC <<= 1  (0x08 << 1 = 0x10)
        RACC                ; MBR = ACC (= 0x10)
        WM   0x405          ; memory[0x405] = 0x10

        EOP                 ; end of program
