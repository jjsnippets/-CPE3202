// CPE 3202 COMPUTER ORGANIZATION AND ARCHITECTURE
// Group 2      MW 7:30AM - 10:30AM LB CEAC2 TC
// Estose, Jude Vicris; Sarcol, Joshua; Silmaro, Jame Paul Jr.      BS CpE - 3  2026/04/26
// Laboratory Exercise #4: The ALU v2.0 (CO2)

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MEMSIZE 0x0800
#define IOSIZE  0x0020
#define PRINTCYCLE 1

#define FLAG_ZF 0x01
#define FLAG_CF 0x02
#define FLAG_SF 0x04
#define FLAG_OF 0x80

// Opcodes
typedef enum inst_opcode {
    // Arithmetic and Logical
    SHR  = 0b10101,     // ACC <- ACC >> 1,  CF <- ACC<0>
    SHL  = 0b10110,     // ACC <- ACC << 1,  CF <- ACC<7>
    XOR  = 0b10111,     // ACC <- ACC ^ BUS
    NOT  = 0b11000,     // ACC <- !ACC
    OR   = 0b11001,     // ACC <- ACC | BUS
    AND  = 0b11010,     // ACC <- ACC & BUS
    MUL  = 0b11011,     // ACC <- ACC * BUS
    SUB  = 0b11101,     // ACC <- ACC - BUS
    ADD  = 0b11110,     // ACC <- ACC + BUS

    // Data movement
    WM   = 0b00001,     // BUS <- MBR 
    RM   = 0b00010,     // MBR <- BUS
    RIO  = 0b00100,     // IOBR <- BUS
    WIO  = 0b00101,     // BUS <- IOBR
    WB   = 0b00110,     // MBR <- literal
    WIB  = 0b00111,     // IOBR <- literal
    WACC = 0b01001,     // ACC <- BUS
    RACC = 0b01011,     // BUS <- ACC
    SWAP = 0b01110,     // MBR <- IOBR,  IOBR <- MBR

    // Program control
    BR   = 0b00011,     // PC <- addr
    BRLT = 0b10001,     // if ACC < BUS (SF == 1) then PC <- addr
    BRGT = 0b10010,     // if ACC > BUS (SF == 0) then PC <- addr 
    BRNE = 0b10011,     // if ACC != BUS (ZF == 0) then PC <- addr
    BRE  = 0b10100,     // if ACC == BUS (ZF == 1) then PC <- addr
    EOP  = 0b11111      // End of program
} INST_OPCODE;

// Main memory and I/O buffer
static uint8_t dataMemory[MEMSIZE];     // Main memory
static uint8_t IOBuffer[IOSIZE];        // I/O memory

// Busese
volatile uint16_t ADDR;     // Address bus (11)
volatile uint8_t BUS;       // Data bus (8)
volatile uint8_t CONTROL;   // Control (Opcode) bus (5)

// Control signals
volatile uint8_t IOM;       // IO/Memory access
volatile uint8_t RW;        // Read/Write
volatile uint8_t OE;        // Output enable
volatile uint8_t FLAGS;     // OF:x:x:x:x:SF:CF:ZF

// Function Prototypes
int CU(void);
int ALU(void);
void MainMemory(void);
void IOMemory(void);
void initMemory(void);

unsigned char twosComp(unsigned char data);
void setFlagsArithmetic(unsigned int result);
void setFlagZeroOnly(unsigned char value);
void setFlagCarryOnly(unsigned char carry);

void loadWord(uint16_t addr, uint16_t word);
const char *instructionName(uint8_t opcode);

int main(void) {
    initMemory();

    if (CU() == 1) {
        printf("Program run successfully.\n");
    } else {
        printf("Program execution failed.\n");
    }

    return 0;
}

void initMemory(void) {
    memset(dataMemory, 0, sizeof(dataMemory));
    memset(IOBuffer, 0, sizeof(IOBuffer));
    FLAGS = 0;
    BUS = 0;
    ADDR = 0;
    CONTROL = 0;
    IOM = 0;
    RW = 0;
    OE = 0;

    #if PRINTCYCLE == 1
        printf("Initializing Main Memory...\n\n");
    #endif

    // Memory instructions
    // Store 0x15 at dataMemory[0x400]
    loadWord(0x000, (WB   << 11) | 0x015);  // MBR = 0x15
    loadWord(0x002, (WM   << 11) | 0x400);  // dataMemory[0x400] = 0x15
    
    // 5 + 8
    // Load 5 to ACC
    loadWord(0x004, (WB   << 11) | 0x005);  // MBR = 0x05
    loadWord(0x006, (WACC << 11));          // ACC = 0x05

    // Load 8 to BUS
    loadWord(0x008, (WB   << 11) | 0x008);  // MBR = 0x08
    
    // Perform the addition
    loadWord(0x00A, (ADD  << 11));          // ACC = (0x05) + (0x08) = 0x0D     ZF=0, CF=0, OF=0, SF=0

    // Previous sum * dataMemory[0x400]
    // Read dataMemory[0x400] and store to BUS
    loadWord(0x00C, (RM   << 11) | 0x400);  // MBR = 0x15

    // Perform the multiplication
    loadWord(0x00E, (MUL  << 11));          // ACC = (0x0D) x (0x15) = 0x11     ZF=0, CF=1, OF=1, SF=0
    
    // Store product at dataMemory[0x401]
    // Move result to BUS
    loadWord(0x010, (RACC << 11));          // MBR = 0x11
    loadWord(0x012, (WM   << 11) | 0x401);  // dataMemory[0x401] = 0x11


    // IO instructions
    // Store 0x0B at IOBuffer[0x00]
    loadWord(0x014, (WIB  << 11) | 0x00B);  // IOBR = 0x0B
    loadWord(0x016, (WIO  << 11) | 0x000);  // IOBuffer[0x00] = 0x0B

    // Previous product - 0x10
    // Load 0x10 to BUS
    loadWord(0x018, (WB   << 11) | 0x010);  // MBR = 0x10

    // Perform the subtraction
    loadWord(0x01A, (SUB  << 11));          // ACC = (0x11) - (0x10) = 0x01     ZF=0, CF=0, OF=0, SF=0
    
    // Use difference as address to store 0x0B literal
    loadWord(0x01C, (RACC << 11));          // MBR = 0x01
    loadWord(0x01E, (WIO  << 11) | 0x001);  // IOBuffer[0x01] = 0x0B


    // Logical instructions
    // Logical shifting of difference
    loadWord(0x020, (SHL  << 11));
    loadWord(0x022, (SHL  << 11));
    loadWord(0x024, (RM   << 11) | 0x401);  // Load product to MBR
    loadWord(0x026, (SHR  << 11));          // ACC = 0x02
    
    // Logical OR of previous result and product
    loadWord(0x028, (OR   << 11));          // ACC = (0x02) OR (0x11) = 0x13    ZF=0, CF=0, OF=0, SF=0
    
    // Logical NOT of previous result
    loadWord(0x02A, (NOT  << 11));          // ACC = NOT (0x13) = 0xEC          ZF=0, CF=0, OF=0, SF=0
    
    // Load IOBuffer[0x01] to IOBR
    loadWord(0x02C, (RIO  << 11) | 0x001);  // IOBR = 0x0B

    // Swap
    // Last MBR write is at 0x024 (loading of product)
    loadWord(0x02E, (SWAP << 11));          // MBR = 0x0B, IOBR = 0x11

    // Logical XOR of previous result and the swap
    loadWord(0x030, (XOR  << 11));          // ACC = (0xEC) XOR (0x0B) = 0xE7   ZF=0, CF=0, OF=0, SF=0
    
    // Flag-clear AND
    loadWord(0x032, (WB   << 11) | 0x0FF);  // MBR = 0xFF
    loadWord(0x034, (AND  << 11));          // ACC = (0xE7) AND (0xFF) = 0xE7   ZF=0, CF=0, OF=0, SF=0
    

    // Branching instructions
    // If previous result == product, then goto 0x03C
    // Load product to BUS
    loadWord(0x036, (RM   << 11) | 0x401);  // MBR = 0x11
    loadWord(0x038, (BRE  << 11) | 0x03C);  // ACC = (0xE7) - (0x11) = 0xD6     ZF=0, CF=0, OF=0, SF=0
    // Branch not taken

    // Mangled if-else block
    // If subtraction from previous compare > 0xF0, then goto 0x40
    // Load 0xF0 to BUS
    loadWord(0x03A, (WB   << 11) | 0x0F0);  // MBR = 0xF0
    loadWord(0x03C, (BRGT << 11) | 0x040);  // ACC = (0xD6) - (0xF0) = 0xE6     ZF=0, CF=1, OF=1, SF=1
    
    // If subtraction from previous compare < 0xF0, then goto 0x44
    loadWord(0x03E, (BRLT << 11) | 0x044);  // ACC = (0xE6) - (0xF0) = 0xF6     ZF=0, CF=1, OF=1, SF=1
    
    // Unreachable instructions
    loadWord(0x040, (WB   << 11) | 0x000);
    loadWord(0x042, (WACC << 11));

    
    // For loop

    loadWord(0x044, (WB   << 11) | 0x003);
    loadWord(0x046, (WACC << 11));
    loadWord(0x048, (WB   << 11) | 0x000);
    loadWord(0x04A, (BRE  << 11) | 0x052);
    loadWord(0x04C, (WB   << 11) | 0x001);
    loadWord(0x04E, (SUB  << 11));
    loadWord(0x050, (BR   << 11) | 0x048);
    loadWord(0x052, (EOP  << 11));
}

int CU(void) {
    uint16_t PC = 0;
    uint16_t IR = 0;
    uint16_t MAR = 0;
    uint8_t MBR = 0;
    uint16_t IOAR = 0;
    uint8_t IOBR = 0;

    uint8_t INCREMENT = 0;
    uint8_t FETCH = 0;
    uint8_t IO = 0;
    uint8_t MEMORY = 0;

    uint8_t run = 1;
    uint8_t isError = 0;

    do {
        #if PRINTCYCLE == 1
                uint16_t PC_before = PC;
        #endif

        IOM = 1; RW = 0; OE = 1;
        FETCH = 1; IO = 0; MEMORY = 0;

        ADDR = PC;
        MainMemory();
        if (FETCH == 1) {
            IR = ((uint16_t)BUS) << 8;
            INCREMENT = 1;
            if (INCREMENT == 1) PC++;
            ADDR = PC;
            INCREMENT = 0;
        }

        MainMemory();
        if (FETCH == 1) {
            IR |= BUS;
            INCREMENT = 1;
            if (INCREMENT == 1) PC++;
            INCREMENT = 0;
        }
        FETCH = 0;

        {
            uint8_t opcode = (IR >> 11) & 0x1F;
            uint16_t operand = IR & 0x07FF;

            switch (opcode) {
                case WM:
                    MAR = operand;
                    CONTROL = opcode;
                    IOM = 1; RW = 1; OE = 1;
                    FETCH = 0; MEMORY = 1; IO = 0;
                    if (MEMORY == 1) ADDR = MAR;
                    if (MEMORY == 1) BUS = MBR;
                    MainMemory();
                    break;

                case RM:
                    MAR = operand;
                    CONTROL = opcode;
                    IOM = 1; RW = 0; OE = 1;
                    FETCH = 0; MEMORY = 1; IO = 0;
                    if (MEMORY == 1) ADDR = MAR;
                    MainMemory();
                    if (MEMORY == 1) MBR = BUS;
                    break;

                case BR:
                    MAR = operand;
                    CONTROL = opcode;
                    IOM = 0; RW = 0; OE = 0;
                    FETCH = 0; MEMORY = 0; IO = 0;
                    PC = MAR;
                    break;

                case RIO:
                    IOAR = operand;
                    CONTROL = opcode;
                    IOM = 0; RW = 0; OE = 1;
                    FETCH = 0; MEMORY = 0; IO = 1;
                    if (IO == 1) ADDR = IOAR;
                    IOMemory();
                    if (IO == 1) IOBR = BUS;
                    break;

                case WIO:
                    IOAR = operand;
                    CONTROL = opcode;
                    IOM = 0; RW = 1; OE = 1;
                    FETCH = 0; MEMORY = 0; IO = 1;
                    if (IO == 1) ADDR = IOAR;
                    if (IO == 1) BUS = IOBR;
                    IOMemory();
                    break;

                case WB:
                    MBR = (uint8_t)operand;
                    CONTROL = opcode;
                    IOM = 0; RW = 0; OE = 0;
                    FETCH = 0; MEMORY = 0; IO = 0;
                    break;

                case WIB:
                    IOBR = (uint8_t)operand;
                    CONTROL = opcode;
                    IOM = 0; RW = 0; OE = 0;
                    FETCH = 0; MEMORY = 0; IO = 0;
                    break;

                case WACC:
                    CONTROL = opcode;
                    IOM = 0; RW = 0; OE = 0;
                    FETCH = 0; MEMORY = 1; IO = 0;
                    if (MEMORY == 1) BUS = MBR;
                    if (ALU() != 0) {
                        isError = 1;
                        run = 0;
                    }
                    break;

                case RACC:
                    CONTROL = opcode;
                    IOM = 0; RW = 0; OE = 0;
                    FETCH = 0; MEMORY = 1; IO = 0;
                    if (ALU() != 0) {
                        isError = 1;
                        run = 0;
                    } else if (MEMORY == 1) {
                        MBR = BUS;
                    }
                    break;

                case SWAP: {
                    uint8_t temp = MBR;
                    CONTROL = opcode;
                    IOM = 0; RW = 0; OE = 0;
                    FETCH = 0; MEMORY = 0; IO = 0;
                    MBR = IOBR;
                    IOBR = temp;
                    break;
                }

                case ADD:
                case SUB:
                case MUL:
                case AND:
                case OR:
                case XOR:
                    CONTROL = opcode;
                    IOM = 0; RW = 0; OE = 0;
                    FETCH = 0; MEMORY = 1; IO = 0;
                    if (MEMORY == 1) BUS = MBR;
                    if (ALU() != 0) {
                        isError = 1;
                        run = 0;
                    }
                    break;

                case NOT:
                case SHL:
                case SHR:
                    CONTROL = opcode;
                    IOM = 0; RW = 0; OE = 0;
                    FETCH = 0; MEMORY = 0; IO = 0;
                    if (ALU() != 0) {
                        isError = 1;
                        run = 0;
                    }
                    break;

                case BRE:
                case BRNE:
                case BRGT:
                case BRLT:
                    CONTROL = opcode;
                    IOM = 0; RW = 0; OE = 0;
                    FETCH = 0; MEMORY = 1; IO = 0;
                    if (MEMORY == 1) BUS = MBR;
                    if (ALU() != 0) {
                        isError = 1;
                        run = 0;
                        break;
                    }

                    if ((opcode == BRE  && (FLAGS & FLAG_ZF)) ||
                        (opcode == BRNE && !(FLAGS & FLAG_ZF)) ||
                        (opcode == BRGT && !(FLAGS & FLAG_SF) && !(FLAGS & FLAG_ZF)) ||
                        (opcode == BRLT &&  (FLAGS & FLAG_SF))) {
                        PC = operand;
                    }
                    break;

                case EOP:
                    CONTROL = opcode;
                    IOM = 0; RW = 0; OE = 0;
                    FETCH = 0; MEMORY = 0; IO = 0;
                    run = 0;
                    break;

                default:
                    CONTROL = opcode;
                    IOM = 0; RW = 0; OE = 0;
                    FETCH = 0; MEMORY = 0; IO = 0;
                    isError = 1;
                    run = 0;
                    break;
            }

        #if PRINTCYCLE == 1
                printf("====================================\n");
                printf("%-5s: 0x%02X | %-6s: 0x%03X\n\n", "BUS", BUS, "ADDR", ADDR);
                printf("%-20s: 0x%03X\n", "PC", PC_before);
                printf("%-20s: 0x%04X\n", "IR", IR);
                printf("%-20s: %s (0x%02X)\n", "Instruction", instructionName(CONTROL), CONTROL);
                printf("%-20s: 0x%03X\n\n", "Operand", operand);
                printf("%-20s: 0x%03X\n", "MAR", MAR);
                printf("%-20s: 0x%02X\n", "MBR", MBR);
                printf("%-20s: 0x%02X\n\n", "Memory[MAR]", dataMemory[MAR]);
                printf("%-20s: 0x%03X\n", "IOAR", IOAR);
                printf("%-20s: 0x%02X\n", "IOBR", IOBR);
                printf("%-20s: 0x%02X\n\n", "IOBuffer[IOAR]", IOBuffer[IOAR]);
                printf("%-20s: 0x%02X\n", "FLAGS", FLAGS);
                printf("%-20s: ZF=%u CF=%u SF=%u OF=%u\n\n",
                        "Flag Bits",
                        (FLAGS & FLAG_ZF) ? 1 : 0,
                        (FLAGS & FLAG_CF) ? 1 : 0,
                        (FLAGS & FLAG_SF) ? 1 : 0,
                        (FLAGS & FLAG_OF) ? 1 : 0);
        #endif
        }
    } while (run);

    return isError ? 0 : 1;
}

int ALU(void) {
    static int ACC = 0;
    unsigned int temp_ACC = (unsigned char)ACC;
#if PRINTCYCLE == 1
    unsigned char acc_before = (unsigned char)ACC;
    unsigned char bus_before = BUS;
#endif

    switch (CONTROL) {
        case WACC:
            ACC = BUS;
            break;

        case RACC:
            BUS = (unsigned char)ACC;
            break;

        case ADD:
            temp_ACC = (unsigned char)ACC + BUS;
            ACC = (unsigned char)temp_ACC;
            setFlagsArithmetic(temp_ACC);
            break;

        case SUB:
            temp_ACC = (unsigned char)ACC + twosComp(BUS);
            ACC = (unsigned char)temp_ACC;
            setFlagsArithmetic(temp_ACC);
            break;

        case MUL:
            temp_ACC = (unsigned char)ACC * BUS;
            ACC = (unsigned char)temp_ACC;
            setFlagsArithmetic(temp_ACC);
            break;

        case AND:
            ACC = (unsigned char)ACC & BUS;
            setFlagZeroOnly((unsigned char)ACC);
            break;

        case OR:
            ACC = (unsigned char)ACC | BUS;
            setFlagZeroOnly((unsigned char)ACC);
            break;

        case NOT:
            ACC = (unsigned char)(~((unsigned char)ACC));
            setFlagZeroOnly((unsigned char)ACC);
            break;

        case XOR:
            ACC = (unsigned char)ACC ^ BUS;
            setFlagZeroOnly((unsigned char)ACC);
            break;

        case SHL:
            setFlagCarryOnly((((unsigned char)ACC) & 0x80) ? 1 : 0);
            ACC = ((unsigned char)ACC << 1) & 0xFF;
            break;

        case SHR:
            setFlagCarryOnly((((unsigned char)ACC) & 0x01) ? 1 : 0);
            ACC = ((unsigned char)ACC >> 1) & 0xFF;
            break;

        case BRE:
        case BRNE:
        case BRGT:
        case BRLT:
            temp_ACC = (unsigned char)ACC + twosComp(BUS);
            ACC = (unsigned char)temp_ACC;
            setFlagsArithmetic(temp_ACC);
            break;

        default:
            return -1;
    }

#if PRINTCYCLE == 1
    printf("ALU Trace             : %-4s | ACC(before)=0x%02X | BUS=0x%02X | ACC(after)=0x%02X | FLAGS=0x%02X\n",
           instructionName(CONTROL), acc_before, bus_before, (unsigned char)ACC, FLAGS);
#endif

    return 0;
}

void MainMemory(void) {
    if (IOM == 1) {
        if (RW == 0 && OE == 1) {
            BUS = dataMemory[ADDR];
        } else if (RW == 1 && OE == 1) {
            dataMemory[ADDR] = BUS;
        }
    }
}

void IOMemory(void) {
    if (IOM == 0) {
        if (RW == 0 && OE == 1) {
            BUS = IOBuffer[ADDR];
        } else if (RW == 1 && OE == 1) {
            IOBuffer[ADDR] = BUS;
        }
    }
}

void loadWord(uint16_t addr, uint16_t word) {
    if (addr + 1 < MEMSIZE) {
        dataMemory[addr] = (word >> 8) & 0xFF;
        dataMemory[addr + 1] = word & 0xFF;
    }
}

unsigned char twosComp(unsigned char data) {
    return (unsigned char)(~data + 1);
}

void setFlagsArithmetic(unsigned int result) {
    FLAGS &= ~(FLAG_ZF | FLAG_CF | FLAG_SF | FLAG_OF);

    if (((unsigned char)result) == 0x00) {
        FLAGS |= FLAG_ZF;
    }
    if (result > 0x00FF) {
        FLAGS |= FLAG_CF;
        FLAGS |= FLAG_OF;
    }
    if (((unsigned char)result) & 0x80) {
        FLAGS |= FLAG_SF;
    }
}

void setFlagZeroOnly(unsigned char value) {
    FLAGS &= ~FLAG_ZF;
    if (value == 0x00) {
        FLAGS |= FLAG_ZF;
    }
}

void setFlagCarryOnly(unsigned char carry) {
    FLAGS &= ~FLAG_CF;
    if (carry) {
        FLAGS |= FLAG_CF;
    }
}

const char *instructionName(uint8_t opcode) {
    switch (opcode) {
        case WM:   return "WM";
        case RM:   return "RM";
        case BR:   return "BR";
        case RIO:  return "RIO";
        case WIO:  return "WIO";
        case WB:   return "WB";
        case WIB:  return "WIB";
        case WACC: return "WACC";
        case RACC: return "RACC";
        case SWAP: return "SWAP";
        case BRLT: return "BRLT";
        case BRGT: return "BRGT";
        case BRNE: return "BRNE";
        case BRE:  return "BRE";
        case SHR:  return "SHR";
        case SHL:  return "SHL";
        case XOR:  return "XOR";
        case NOT:  return "NOT";
        case OR:   return "OR";
        case AND:  return "AND";
        case MUL:  return "MUL";
        case SUB:  return "SUB";
        case ADD:  return "ADD";
        case EOP:  return "EOP";
        default:   return "INVALID";
    }
}
