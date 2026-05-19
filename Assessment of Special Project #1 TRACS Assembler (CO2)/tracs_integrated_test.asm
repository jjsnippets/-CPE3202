; ============================================================
; tracs_integrated_test.asm
; TRACS Integrated Test Assembly Program
;
; Derived from:
;   P2a-PFE-Integrated-TRACS-Test-Code-v1.0.3.pdf
;
; This exercises all 24 TRACS instructions.
; Program memory starts at 0x000 (no ORG directive means default 0x000).
;
; Addresses (byte) assigned sequentially from 0x000:
;   0x000  WB   0x15
;   0x002  WM   0x400
;   0x004  WB   0x05
;   0x006  WACC
;   0x008  WB   0x08
;   0x00A  ADD
;   0x00C  RM   0x400
;   0x00E  MUL
;   0x010  RACC
;   0x012  WM   0x401
;   0x014  WIB  0x0B
;   0x016  WIO  0x000
;   0x018  WB   0x10
;   0x01A  SUB
;   0x01C  RACC
;   0x01E  WIO  0x001
;   0x020  SHL
;   0x022  SHL
;   0x024  RM   0x401         <- note: listed at 0x024 in PDF (minor ordering note)
;   0x026  SHR
;   0x028  OR
;   0x02A  NOT
;   0x02C  RIO  0x01F
;   0x02E  SWAP
;   0x030  XOR
;   0x032  WB   0xFF
;   0x034  AND
;   0x036  RM   0x401
;   0x038  BRE  skip1
;   0x03A  WB   0x2E
;   0x03C  BRGT skip2
;   0x03E  AND
;   0x040  BRLT loop_start
;   0x042  WB   0x00          ; unreachable
;   0x044  WACC               ; unreachable
; loop_start (0x046):
;   0x046  WB   0x03
;   0x048  WACC
; loop:
;   0x04A  WB   0x00
;   0x04C  BRE  done
;   0x04E  WB   0x01
;   0x050  SUB
;   0x052  BR   loop
; done (0x054):
;   0x054  EOP
; ============================================================

        ORG  0x000

        WB   0x15           ; MBR = 0x15
        WM   0x400          ; memory[0x400] = 0x15
        WB   0x05           ; MBR = 0x05
        WACC                ; ACC = 0x05
        WB   0x08           ; MBR = 0x08
        ADD                 ; ACC = 0x05 + 0x08 = 0x0D
        RM   0x400          ; MBR = 0x15
        MUL                 ; ACC = 0x0D * 0x15 = 0x11 (low byte)
        RACC                ; MBR = 0x11
        WM   0x401          ; memory[0x401] = 0x11
        WIB  0x0B           ; IOBR = 0x0B
        WIO  0x000          ; ioBuffer[0x000] = 0x0B
        WB   0x10           ; MBR = 0x10
        SUB                 ; ACC = 0x11 - 0x10 = 0x01
        RACC                ; MBR = 0x01
        WIO  0x001          ; ioBuffer[0x001] = 0x01
        SHL                 ; ACC = 0x02
        SHL                 ; ACC = 0x04
        SHR                 ; ACC = 0x02
        RM   0x401          ; MBR = 0x11
        OR                  ; ACC = 0x02 | 0x11 = 0x13
        NOT                 ; ACC = ~0x13 = 0xEC
        RIO  0x01F          ; IOBR = ioBuffer[0x01F]
        SWAP                ; MBR <-> IOBR
        XOR                 ; ACC = 0xEC ^ MBR
        WB   0xFF           ; MBR = 0xFF
        AND                 ; ACC = ACC & 0xFF
        RM   0x401          ; MBR = 0x11
skip1   BRE  skip2          ; if ACC == MBR jump to skip2
        WB   0x2E           ; MBR = 0x2E
skip2   BRGT done           ; if ACC > MBR jump to done
        AND                 ; ACC = ACC & MBR
        BRLT loop_start     ; if ACC < MBR jump to loop_start
        WB   0x00           ; unreachable
        WACC                ; unreachable
loop_start WB  0x03         ; MBR = 0x03
        WACC                ; ACC = 0x03
loop    WB   0x00           ; MBR = 0x00
        BRE  done           ; if ACC == 0 exit loop
        WB   0x01           ; MBR = 0x01
        SUB                 ; ACC = ACC - 1
        BR   loop           ; unconditional jump to loop
done    EOP                 ; end of program
