; (Test Assembly Program and Machine Code)
; program starts here
WB      0x15        ; MBR = 0x15
WM      0x400       ; dataMemory[0x400] : 0x15
WB      0x05        ; MBR = 0x05
WACC                ; ACC = 0x05
WB      0x08        ; MBR = 0x08
ADD                 ; ACC = (0x05) + (0x08) = 0x0D ZF=0, CF=0, OF=0, SF=0
RM      0x400       ; MBR = 0x15
MUL                 ; ACC = (0x0D) x (0x15) = 0x11 ZF=0, CF=1, OF=1, SF=0
RACC                ; MBR = 0x11
WM      0x401       ; dataMemory[0x401] : 0x11
WIB     0x0B        ; IOBR = 0x0B
WIO     0x000       ; ioBuffer[0x000] : 0x0B
WB      0x10        ; MBR = 0x10
SUB                 ; ACC = (0x11) - (0x10) = 0x01 ZF=0, CF=0, OF=0, SF=0
RACC                ; MBR = 0x01
WIO     0x001       ; ioBuffer[0x001] : 0x0B
SHL                 ; ACC = (0x01) << 1 = 0x02 ZF=0, CF=0, OF=0, SF=0
SHL                 ; ACC = (0x02) << 1 = 0x04 ZF=0, CF=0, OF=0, SF=0
SHR                 ; ACC = (0x04) >> 1 = 0x02 ZF=0, CF=0, OF=0, SF=0
RM      0x401       ; MBR = 0x11
OR                  ; ACC = (0x02) OR (0x11) = 0x13 ZF=0, CF=0, OF=0, SF=0
NOT                 ; ACC = NOT (0x13) = 0xEC ZF=0, CF=0, OF=0, SF=0
RIO     0x01F       ; IOBR = 0xD0
SWAP                ; MBR = 0xD0, IOBR = 0x11
XOR                 ; ACC = (0xEC) XOR (0xD0) = 0x3C ZF=0, CF=0, OF=0, SF=0
WB      0xFF        ; MBR = 0xFF
AND                 ; ACC = (0x3C) AND (0xFF) = 0x3C ZF=0, CF=0, OF=0, SF=0
RM      0x401       ; MBR = 0x11
BRE     0x03C       ; ACC = (0x3C) - (0x11) = 0x2B ZF=0, CF=0, OF=0, SF=0
WB      0x2E        ; MBR = 0x2E
BRGT    0x042       ; ACC = (0x2B) - (0x2E) = 0xFD ZF=0, CF=0, OF=0, SF=1
AND                 ; ACC = (0xFD) AND (0x2E) = 0x2C ZF=0, CF=0, OF=0, SF=0
BRLT    0x046       ; ACC = (0x2C) - (0x2E) = 0xFE ZF=0, CF=0, OF=0, SF=1
WB      0x00        ; unreachable
WACC                ; unreachable
WB      0x03        ; MBR = 0x03
WACC                ; ACC = 0x03

; This part is a controlled loop
WB      0x00        ; MBR = 0x00
BRE     0x054       ; ACC = (0x03) - (0x00) = 0x03 ZF=0, CF=0, OF=0, SF=0
                            ; ACC = (0x02) - (0x00) = 0x02 ZF=0, CF=0, OF=0, SF=0
                            ; ACC = (0x01) - (0x00) = 0x01 ZF=0, CF=0, OF=0, SF=0
                            ; ACC = (0x00) - (0x00) = 0x00 ZF=1, CF=0, OF=0, SF=0
                            ; PC <- 0x52 (when ACC == MBR and ZF=1)
WB      0x01        ; MBR = 0x01
SUB                 ; ACC = (0x03) - (0x01) = 0x02 ZF=0, CF=0, OF=0, SF=0
                            ; ACC = (0x02) - (0x01) = 0x01 ZF=0, CF=0, OF=0, SF=0
                            ; ACC = (0x01) - (0x01) = 0x00 ZF=1, CF=0, OF=0, SF=0
BR      0x04A       ; PC <- 0x048 (loop)
EOP