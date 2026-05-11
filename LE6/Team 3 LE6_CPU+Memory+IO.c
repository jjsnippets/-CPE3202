// CPE 3202 COMPUTER ORGANIZATION AND ARCHITECTURE
// Group 2      MW 7:30AM - 10:30AM LB CEAC2 TC
// Estose, Jude Vicris; Sarcol, Joshua; Silmaro, Jame Paul Jr.      BS CpE - 3  2026/05/11
// Laboratory Exercise #6: Input/Output

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define PRINTDEBUG 0        // Verbose printing of internal CPU state for every instruction
#define PRINTIO 1           // Printing of IO to terminal (7-segment)
#define PRINTIOTEST 0       // Printing of IO to terminal (Test code)

// FLAGS register definitions   OF:x:x:x:x:SF:CF:ZF
#define FLAG_ZF 0x01        // Zero flag
#define FLAG_CF 0x02        // Carry flag
#define FLAG_SF 0x04        // Sign flag
#define FLAG_OF 0x80        // Overflow flag

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
long A1[32], A2[32], A3[32], A4[32], A5[32], A6[32], A7[32], A8[32];    // Main Memory  chip group A
long B1[32], B2[32], B3[32], B4[32], B5[32], B6[32], B7[32], B8[32];    //              chip group B
unsigned char iOData[32];                                               // 16 Latches and 16 Buffers

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

uint8_t twosComp(uint8_t data);
void setFlags(uint16_t result);

void loadWord(uint16_t addr, uint16_t word);

#if PRINTDEBUG == 1
    const uint8_t *instructionName(uint8_t opcode);
    uint8_t readMemoryDebug(uint16_t address);
#endif

#if PRINTIO == 1
    void SevenSegment(void);
#endif

#if PRINTIOTEST == 1
    void InputSim(void);
#endif

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
    memset(iOData, 0, sizeof(iOData)); 
    FLAGS = 0;
    BUS = 0;
    ADDR = 0;
    CONTROL = 0;
    IOM = 0;
    RW = 0;
    OE = 0;

    #if PRINTDEBUG == 1
        printf("Initializing Main Memory...\n\n");
    #endif

    // Port configuration and I/O testing
    loadWord(0x000, (WIB << 11) | 0x0CA);   // IOBR = 0xCA (Test data)
    loadWord(0x002, (WIO << 11) | 0x001);   // Latch 0x01 = 0xCA

    loadWord(0x004, (RIO << 11) | 0x01F);   // Read at buffer 0x1F = 0x53 (Reversed bits)
    loadWord(0x006, (SWAP << 11));          // IOBR = 0x00, MBR = 0x53
    loadWord(0x008, (WM << 11) | 0x406);    // Memory[0x406] = 0x53
    loadWord(0x00A, (RM << 11) | 0x406);    // MBR = 0x53 (Test read)

    // Countdown from 9 to 0
    // Equivalent c-pseudocode
    //    i = 9;
    //    while (i != 0) { 
    //        i--;              // '0' is displayed first before loop breaks
    //        OUT = i;          // "OUT" to 7-segment display
    //    };

    // Initialization
    loadWord(0x00C, (WB << 11) | 0x009);    // MBR = 0x09 (Initial counter value)
    loadWord(0x00E, (WACC << 11));          // ACC = 0x09
    loadWord(0x010, (SWAP << 11));          // IOBR = 0x09, MBR = 0x00 (irrelevant)
    loadWord(0x012, (WIO << 11) | 0x000);   // Latch 0x00 = 0x09, which also displays '9'

    // Loop condition check
    loadWord(0x014, (WB << 11) | 0x000);    // MBR = 0x00 (Condition check value)
    loadWord(0x016, (BRE << 11) | 0x024);   // When ACC == 0, then break loop (jump to 0x024: EOP)
    
    // Decrement counter
    loadWord(0x018, (WB << 11) | 0x001);    // MBR = 1 (Decrement step)
    loadWord(0x01A, (SUB << 11));           // ACC = ACC - MBR (1)
    
    // Display updated counter value
    loadWord(0x01C, (RACC << 11));          // MBR = ACC
    loadWord(0x01E, (SWAP << 11));          // IOBR = MBR (new value), MBR = IOBR (old value, which needs to be overwritten later)
    loadWord(0x020, (WIO << 11) | 0x000);   // Latch 0x00 = IOBR, which displays the updated counter value
    
    // Jump back to loading of stopping condition
    loadWord(0x022, (BR << 11) | 0x014);    // To loop condition check (0x014)
    
    // End of program
    loadWord(0x024, (EOP << 11));
}

int CU(void) {
    // Internal registers
    uint16_t PC = 0;        // Program Counter
    uint16_t IR = 0;        // Instruction Register
    uint16_t MAR = 0;       // Memory Address Register
    uint8_t MBR = 0;        // Memory Buffer Register
    uint16_t IOAR = 0;      // I/O Address Register
    uint8_t IOBR = 0;       // I/O Buffer Register

    // Internal control lines
    uint8_t INCREMENT = 0;  // Increment Control line
    uint8_t FETCH = 0;      // Fetch Control line
    uint8_t IO = 0;         // IO Control line
    uint8_t MEMORY = 0;     // Memory Control line

    // Program execution state
    uint8_t run = 1;        // Running state
    uint8_t isError = 0;    // Error state

    do {
        #if PRINTDEBUG == 1
            uint16_t PC_before = PC;
        #endif

        // Fetch Operation
        OE = 1; IOM = 1; RW = 0;        // Memory read
        FETCH = 1; IO = 0; MEMORY = 0;  // Select IR 

        // Fetch upper byte
        ADDR = PC; MainMemory();        // Fetch upper byte
        if (FETCH == 1) {
            IR = ((uint16_t)BUS) << 8;  // Load upper instruction byte
            
            INCREMENT = 1;
            if (INCREMENT == 1) PC++;   // Point to lower byte
            ADDR = PC;
            INCREMENT = 0;
        }

        // Fetch lower byte
        MainMemory();                   // Fetch lower byte
        if (FETCH == 1) {
            IR |= BUS;                  // Load lower instruction byte
            
            INCREMENT = 1;
            if (INCREMENT == 1) PC++;   // Point to next instruction
            INCREMENT = 0;
        }

        
        // Decode and Execute
        FETCH = 0;                              // Deassert FETCH
        uint8_t opcode = (IR >> 11) & 0x1F;     // Extract opcode (5 bits)
        uint16_t operand = IR & 0x07FF;         // Extract operand (11 bits)
        CONTROL = opcode;                       // Current instruction to be executed

        switch (opcode) {
            // Arithmetic and Logical Instructions
            // Two-operand operations
            case ADD:
            case SUB:
            case MUL:
            case AND:
            case OR:
            case XOR:
                IOM = 0; RW = 0; OE = 0;        // No external memory access
                FETCH = 0; MEMORY = 1; IO = 0;  // Select MBR/MAR
                
                if (MEMORY == 1) BUS = MBR;     // Transfer to external data BUS
                if (ALU() != 0) {               // Relevant ALU() call dictated by CONTROL
                    isError = 1;
                    run = 0;
                }
                break;
            
            // Single-operand operations
            case NOT:
            case SHL:
            case SHR:
                IOM = 0; RW = 0; OE = 0;        // No external memory access
                FETCH = 0; MEMORY = 0; IO = 0;  // No internal registers to be selected
                
                if (ALU() != 0) {               // Relevant ALU() call dictated by CONTROL
                    isError = 1;
                    run = 0;
                }
                break;


            // Data Movement Instructions
            case WM:    // Write data in MBR to memory at address pointed to by MAR
                OE = 1; IOM = 1; RW = 1;        // Memory write
                FETCH = 0; MEMORY = 1; IO = 0;  // Select MBR/MAR

                MAR = operand;                  // MAR <- operand
                if (MEMORY == 1) ADDR = MAR;    // Set ADDR bus to MAR/operand
                if (MEMORY == 1) BUS = MBR;     // Set data bus to MBR
                MainMemory();                   // Memory[MAR] = MBR
                break;

            case RM:    // Read data from memory with the specified address, stores data to MBR
                OE = 1; IOM = 1; RW = 0;        // Memory read
                FETCH = 0; MEMORY = 1; IO = 0;  // Select MBR/MAR

                MAR = operand;                  // MAR <- operand
                if (MEMORY == 1) ADDR = MAR;    // Set ADDR bus to MAR/operand
                MainMemory();                   // Read result is at BUS
                if (MEMORY == 1) MBR = BUS;     // MBR = Memory[MAR]
                break;

            case RIO:   // Read data from IO memory with the specified address, stores data to IOBR
                OE = 1; IOM = 0; RW = 0;        // IO read
                FETCH = 0; MEMORY = 0; IO = 1;  // Select IOBR/IOAR 

                IOAR = operand;                 // IOAR <- operand
                if (IO == 1) ADDR = IOAR;       // Set ADDR bus to IOAR/operand
                IOMemory();                     // Read result is at BUS
                if (IO == 1) IOBR = BUS;        // IOBR = IOdata[IOAR]
                break;

            case WIO:   // Write data in IOBR to memory at address pointed to by IOAR
                OE = 1; IOM = 0; RW = 1;        // IO write
                FETCH = 0; MEMORY = 0; IO = 1;  // Select IOBR/IOAR

                IOAR = operand;                 // IOAR <- operand
                if (IO == 1) ADDR = IOAR;       // Set ADDR bus to IOAR/operand
                if (IO == 1) BUS = IOBR;        // Set data bus to IOBR
                IOMemory();                     // IOdata[IOAR] = IOBR
                #if PRINTIOTEST == 1
                    InputSim();                 // Print connectivity test
                #endif
                #if PRINTIO == 1
                    SevenSegment();             // Display to 7-segment
                #endif
                break;

            case WB:    // Write literal value to MBR
                OE = 0; IOM = 0; RW = 0;        // No external memory access
                FETCH = 0; MEMORY = 0; IO = 0;  // Internal to CU only

                MBR = (uint8_t)operand;         // MBR <- operand
                break;

            case WIB:   // Write literal value to IOBR
                OE = 0; IOM = 0; RW = 0;        // No external memory access
                FETCH = 0; MEMORY = 0; IO = 0;  // Internal to CU only

                IOBR = (uint8_t)operand;        // IOBR <- operand
                break;

            case WACC:  // Write data on BUS to ACC
                OE = 0; IOM = 0; RW = 0;        // No external memory access
                FETCH = 0; MEMORY = 1; IO = 0;  // Select MBR/MAR

                if (MEMORY == 1) BUS = MBR;     // Transfer to external data BUS
                if (ALU() != 0) {               // ACC <- BUS
                    isError = 1;
                    run = 0;
                }
                break;

            case RACC:  // Read ACC to bus
                OE = 0; IOM = 0; RW = 0;        // No external memory access
                FETCH = 0; MEMORY = 1; IO = 0;  // Select MBR/MAR
                
                if (ALU() != 0) {               // BUS <- ACC
                    isError = 1;
                    run = 0;
                }
                if (MEMORY == 1) MBR = BUS;     // Transfer to internal MBR
                break;

            case SWAP:  // Swap data of MBR and IOBR
                OE = 0; IOM = 0; RW = 0;        // No external memory access
                FETCH = 0; MEMORY = 0; IO = 0;  // Internal to CU only
                
                uint8_t temp = MBR;             // Perform swap
                MBR = IOBR;                     
                IOBR = temp;
                break;


            // Program Control Instructions
            // Non-conditional branch
            case BR:    // Branch to specified address
                OE = 0; IOM = 0; RW = 0;        // No external memory access
                FETCH = 0; MEMORY = 0; IO = 0;  // Internal to CU only

                MAR = operand;                  // MAR <- operand
                PC = MAR;                       // Perform jump
                break;

            // Conditional branches
            case BRE:   // Compare ACC and BUS, if equal branch to address specified
            case BRNE:  // Compare ACC and BUS, if not equal branch to address specified
            case BRGT:  // Compare ACC and BUS, if ACC > BUS, branch to address specified
            case BRLT:  // Compare ACC and BUS, if ACC < BUS, branch to address specified
                OE = 0; IOM = 0; RW = 0;        // No external memory access
                FETCH = 0; MEMORY = 1; IO = 0;  // Select MBR/MAR

                if (MEMORY == 1) BUS = MBR;     // Transfer to external data BUS (usually from previous instruction)
                if (ALU() != 0) {               // Relevant ALU() call dictated by CONTROL
                    isError = 1;
                    run = 0;
                    break;
                }

                // If condition is met
                if ((opcode == BRE  && (FLAGS & FLAG_ZF)) ||                        // ACC - BUS = 0, so ZF == 1
                    (opcode == BRNE && !(FLAGS & FLAG_ZF)) ||                       // ACC - BUS != 0, so ZF == 0
                    (opcode == BRGT && !(FLAGS & FLAG_SF) && !(FLAGS & FLAG_ZF)) || // ACC - BUS > 0, so SF == 0 and ZF == 0
                    (opcode == BRLT &&  (FLAGS & FLAG_SF))) {                       // ACC - BUS < 0, sp SF == 1
                    PC = operand;               // Then perform jump
                }
                break;

            case EOP:   // End of program, no more instructions, CU will halt
                IOM = 0; RW = 0; OE = 0;        // Deassert control signals
                FETCH = 0; MEMORY = 0; IO = 0;

                run = 0;                        // Signal CU to stop
                break;

            default:    // Invalid opcode
                IOM = 0; RW = 0; OE = 0;        // Deassert control signals
                FETCH = 0; MEMORY = 0; IO = 0;

                isError = 1;                    // Signal an error
                run = 0;                        // Halt CU
                break;
        }

    #if PRINTDEBUG == 1
            printf("====================================\n");
            printf("%-5s: 0x%02X | %-6s: 0x%03X\n\n", "BUS", BUS, "ADDR", ADDR);
            printf("%-20s: 0x%03X\n", "PC", PC_before);
            printf("%-20s: 0x%04X\n", "IR", IR);
            printf("%-20s: %s (0x%02X)\n", "Instruction", instructionName(CONTROL), CONTROL);
            printf("%-20s: 0x%03X\n\n", "Operand", operand);
            printf("%-20s: 0x%03X\n", "MAR", MAR);
            printf("%-20s: 0x%02X\n", "MBR", MBR);
            printf("%-20s: 0x%02X\n\n", "Memory[MAR]", readMemoryDebug(MAR));
            printf("%-20s: 0x%03X\n", "IOAR", IOAR);
            printf("%-20s: 0x%02X\n", "IOBR", IOBR);
            printf("%-20s: 0x%02X\n\n", "iOData[IOAR]", iOData[IOAR]);
            printf("%-20s: 0x%02X\n", "FLAGS", FLAGS);
            printf("%-20s: ZF=%u CF=%u SF=%u OF=%u\n\n",
                    "Flag Bits",
                    (FLAGS & FLAG_ZF) ? 1 : 0,
                    (FLAGS & FLAG_CF) ? 1 : 0,
                    (FLAGS & FLAG_SF) ? 1 : 0,
                    (FLAGS & FLAG_OF) ? 1 : 0);
    #endif
        
    } while (run);

    return isError ? 0 : 1;
}

int ALU(void) {
    static int ACC = 0;
    uint16_t temp_ACC = (uint8_t)ACC;

    #if PRINTDEBUG == 1
        uint8_t acc_before = (uint8_t)ACC;
        uint8_t bus_before = BUS;
    #endif

    switch (CONTROL) {
        // Arithmetic and Logical Instructions
        case ADD:
            temp_ACC = (uint8_t)ACC + BUS;
            ACC = (uint8_t)temp_ACC;
            setFlags(temp_ACC);
            break;

        case SUB:
            temp_ACC = (uint8_t)ACC + twosComp(BUS);
            ACC = (uint8_t)temp_ACC;
            setFlags(temp_ACC);
            break;

        case MUL:
            temp_ACC = (uint8_t)ACC * BUS;
            ACC = (uint8_t)temp_ACC;
            setFlags(temp_ACC);
            break;

        case AND:
            ACC = (uint8_t)ACC & BUS;
            setFlags((uint8_t)ACC);
            break;

        case OR:
            ACC = (uint8_t)ACC | BUS;
            setFlags((uint8_t)ACC);
            break;

        case NOT:
            ACC = (uint8_t)(~((uint8_t)ACC));
            setFlags((uint8_t)ACC);
            break;

        case XOR:
            ACC = (uint8_t)ACC ^ BUS;
            setFlags((uint8_t)ACC);
            break;

        case SHL:
            setFlags((((uint8_t)ACC) & 0x80) ? 1 : 0);
            ACC = ((uint8_t)ACC << 1) & 0xFF;
            break;

        case SHR:
            setFlags((((uint8_t)ACC) & 0x01) ? 1 : 0);
            ACC = ((uint8_t)ACC >> 1) & 0xFF;
            break;


        // Data Movement Instructions
        case WACC:
            ACC = BUS;
            break;

        case RACC:
            BUS = (uint8_t)ACC;
            break;


        // Program Control Instructions
        case BRE:
        case BRNE:
        case BRGT:
        case BRLT:
            temp_ACC = (uint8_t)ACC + twosComp(BUS);    // Perform subtraction
            ACC = (uint8_t)temp_ACC;
            setFlags(temp_ACC);
            break;

        // Invalid opcode
        default:    
            return -1;
    }

    #if PRINTDEBUG == 1
        printf("ALU Trace            : %-4s | ACC(before)=0x%02X | BUS=0x%02X | ACC(after)=0x%02X | FLAGS=0x%02X\n",
            instructionName(CONTROL), acc_before, bus_before, (uint8_t)ACC, FLAGS);
    #endif

    return 0;
}

void MainMemory(void) {
    if (IOM == 1 && OE == 1) {
        // Decode address data
        int col = ADDR & 0x001F;            // Column: First 5 bits
        int row = (ADDR >> 5) & 0x001F;     // Row: Next 5 bits
        int cs = ADDR >> 10;                // Chip Select: MSb (11th bit)

        // For memory reads
        if (RW == 0) {
            if (cs == 0) {  // chip group A for cs = 0
                // The entire byte is concatenated at once
                BUS = ((A8[row] >> col) & 1) << 7 |
                      ((A7[row] >> col) & 1) << 6 |
                      ((A6[row] >> col) & 1) << 5 |
                      ((A5[row] >> col) & 1) << 4 |
                      ((A4[row] >> col) & 1) << 3 |
                      ((A3[row] >> col) & 1) << 2 |
                      ((A2[row] >> col) & 1) << 1 |
                      ((A1[row] >> col) & 1);
            } else {        // chip group B for cs = 1
                BUS = ((B8[row] >> col) & 1) << 7 |
                      ((B7[row] >> col) & 1) << 6 |
                      ((B6[row] >> col) & 1) << 5 |
                      ((B5[row] >> col) & 1) << 4 |
                      ((B4[row] >> col) & 1) << 3 |
                      ((B3[row] >> col) & 1) << 2 |
                      ((B2[row] >> col) & 1) << 1 |
                      ((B1[row] >> col) & 1);
            }

        // For memory writes
        } else if (RW == 1) {
            if (cs == 0) {  // chip group A for cs = 0
                // The relevant bit in the BUS is "inserted" in the right place
                // by masking off the bit in the chip (column) then ORing afterwards the bit in the BUS
                A1[row] = (A1[row] & ~(1L << col)) | (((BUS >> 0) & 1L) << col);
                A2[row] = (A2[row] & ~(1L << col)) | (((BUS >> 1) & 1L) << col);
                A3[row] = (A3[row] & ~(1L << col)) | (((BUS >> 2) & 1L) << col);
                A4[row] = (A4[row] & ~(1L << col)) | (((BUS >> 3) & 1L) << col);
                A5[row] = (A5[row] & ~(1L << col)) | (((BUS >> 4) & 1L) << col);
                A6[row] = (A6[row] & ~(1L << col)) | (((BUS >> 5) & 1L) << col);
                A7[row] = (A7[row] & ~(1L << col)) | (((BUS >> 6) & 1L) << col);
                A8[row] = (A8[row] & ~(1L << col)) | (((BUS >> 7) & 1L) << col);
            } else {        // chip group B for cs = 1
                B1[row] = (B1[row] & ~(1L << col)) | (((BUS >> 0) & 1L) << col);
                B2[row] = (B2[row] & ~(1L << col)) | (((BUS >> 1) & 1L) << col);
                B3[row] = (B3[row] & ~(1L << col)) | (((BUS >> 2) & 1L) << col);
                B4[row] = (B4[row] & ~(1L << col)) | (((BUS >> 3) & 1L) << col);
                B5[row] = (B5[row] & ~(1L << col)) | (((BUS >> 4) & 1L) << col);
                B6[row] = (B6[row] & ~(1L << col)) | (((BUS >> 5) & 1L) << col);
                B7[row] = (B7[row] & ~(1L << col)) | (((BUS >> 6) & 1L) << col);
                B8[row] = (B8[row] & ~(1L << col)) | (((BUS >> 7) & 1L) << col);
            }
        }
    }
}

void IOMemory(void) {
    if (OE) {
        if (RW == 1 && IOM == 0) {      // If memory write and IO access
            if (ADDR >= 0x000 && ADDR <= 0x00F)
                iOData[ADDR] = BUS;     // IO write

        } else {                        // If memory read
            if (ADDR >= 0x010 && ADDR <= 0x01F)
                BUS = iOData[ADDR];     // IO read 

        }
    }
}

// Helper function to 2 bytes of the same instruction at once
void loadWord(uint16_t addr, uint16_t word) {
    if (addr + 1 < 0x0800) {        // If within main memory addressable range
        // High byte
        IOM = 1; RW = 1; OE = 1;
        ADDR = addr;
        BUS = (word >> 8) & 0xFF;
        MainMemory();
        // Low byte
        IOM = 1; RW = 1; OE = 1;
        ADDR = addr + 1;
        BUS = word & 0xFF;
        MainMemory();
        // Reset control signals
        IOM = 0; RW = 0; OE = 0;
    }
}

uint8_t twosComp(uint8_t data) {
    return (uint8_t)(~data + 1);
}

void setFlags(uint16_t result)
{
    switch (CONTROL) {
        // Arithmetic and Branch Instructions
        case ADD:
        case SUB:
        case MUL:
        case BRE:
        case BRNE:
        case BRGT:
        case BRLT:
            // Update all flags
            FLAGS &= ~(FLAG_ZF | FLAG_CF | FLAG_SF | FLAG_OF);

            if (((uint8_t)result) == 0x00)      // set ZF if result is 0
                FLAGS |= FLAG_ZF;

            if (result > 0x00FF) {              // set CF and OF if result is greater than 8-bits
                FLAGS |= FLAG_CF;
                FLAGS |= FLAG_OF;
            }

            if (((uint8_t)result) & 0x80)       // set SF when 8th bit is set
                FLAGS |= FLAG_SF;
            break;

        // Logical Instructions
        case AND:
        case OR:
        case NOT:
        case XOR:
            // Update only ZF
            FLAGS &= ~FLAG_ZF;
            if (((uint8_t)result) == 0x00)      // Set ZF 
            {
                FLAGS |= FLAG_ZF;
            }
            break;

        // Shift Instructions
        case SHL:
        case SHR:
            // Update only CF
            FLAGS &= ~FLAG_CF;

            if (result)                         // Set CF depending on the passed value from ALU()
                FLAGS |= FLAG_CF;
                
            break;
    }
}

#if PRINTDEBUG == 1
    // Helper function for printing out the opcode name
    const uint8_t *instructionName(uint8_t opcode) {
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

    // Helper function for returning the data at the specified return address
    uint8_t readMemoryDebug(uint16_t address) {
        int col = address & 0x001F;
        int row = (address >> 5) & 0x001F;
        int cs = address >> 10;

        if (cs == 0) {
            return ((A8[row] >> col) & 1) << 7 |
                ((A7[row] >> col) & 1) << 6 |
                ((A6[row] >> col) & 1) << 5 |
                ((A5[row] >> col) & 1) << 4 |
                ((A4[row] >> col) & 1) << 3 |
                ((A3[row] >> col) & 1) << 2 |
                ((A2[row] >> col) & 1) << 1 |
                ((A1[row] >> col) & 1);
        } else {
            return ((B8[row] >> col) & 1) << 7 |
                ((B7[row] >> col) & 1) << 6 |
                ((B6[row] >> col) & 1) << 5 |
                ((B5[row] >> col) & 1) << 4 |
                ((B4[row] >> col) & 1) << 3 |
                ((B3[row] >> col) & 1) << 2 |
                ((B2[row] >> col) & 1) << 1 |
                ((B1[row] >> col) & 1);
        }
    }
#endif

#if PRINTIO == 1
    void SevenSegment(void) {
        if(iOData[ADDR]==0x01)
        {
            printf("       X\n");
            printf("       X\n");
            printf("       X\n");
            printf("       X\n");
            printf("       X\n");
            printf("       X\n");
            printf("       X\n");
            printf("\n");
        } 
        else if(iOData[ADDR]==0x02)
        {
            printf("   XXXXX\n");
            printf("       X\n");
            printf("       X\n");
            printf("   XXXXX\n");
            printf("   X    \n");
            printf("   X    \n");
            printf("   XXXXX\n");
            printf("\n");
        } 
        else if(iOData[ADDR]==0x03)
        {
            printf("   XXXXX\n");
            printf("       X\n");
            printf("       X\n");
            printf("   XXXXX\n");
            printf("       X\n");
            printf("       X\n");
            printf("   XXXXX\n");
            printf("\n");
        } 
        else if(iOData[ADDR]==0x04)
        {
            printf("   X   X\n");
            printf("   X   X\n");
            printf("   X   X\n");
            printf("   XXXXX\n");
            printf("       X\n");
            printf("       X\n");
            printf("       X\n");
            printf("\n");
        }
        else if(iOData[ADDR]==0x05)
        {
            printf("   XXXXX\n");
            printf("   X    \n");
            printf("   X    \n");
            printf("   XXXXX\n");
            printf("       X\n");
            printf("       X\n");
            printf("   XXXXX\n");
            printf("\n");
        } 
        else if(iOData[ADDR]==0x06)
        {
            printf("   XXXXX\n");
            printf("   X    \n");
            printf("   X    \n");
            printf("   XXXXX\n");
            printf("   X   X\n");
            printf("   X   X\n");
            printf("   XXXXX\n");
            printf("\n");
        }
        else if(iOData[ADDR]==0x07)
        {
            printf("   XXXXX\n");
            printf("       X\n");
            printf("       X\n");
            printf("       X\n");
            printf("       X\n");
            printf("       X\n");
            printf("       X\n");
            printf("\n");
        } 
        else if(iOData[ADDR]==0x08)
        {
            printf("   XXXXX\n");
            printf("   X   X\n");
            printf("   X   X\n");
            printf("   XXXXX\n");
            printf("   X   X\n");
            printf("   X   X\n");
            printf("   XXXXX\n");
            printf("\n");
        }
        else if(iOData[ADDR]==0x09)
        {
            printf("   XXXXX\n");
            printf("   X   X\n");
            printf("   X   X\n");
            printf("   XXXXX\n");
            printf("       X\n");
            printf("       X\n");
            printf("   XXXXX\n");
            printf("\n");
        }
        else if(iOData[ADDR]==0x00)
        {
            printf("   XXXXX\n");
            printf("   X   X\n");
            printf("   X   X\n");
            printf("   X   X\n");
            printf("   X   X\n");
            printf("   X   X\n");
            printf("   XXXXX\n");
            printf("\n");
        }
    }
#endif

#if PRINTIOTEST == 1
    void InputSim(void) {
        unsigned char data;
        for(int i=7; i>=0; i--) {
            // Load source data
            data = iOData[0x001];                   

            // Mask bits
            data = data >> i;                       
            data = data & 0x01;
            data = data << (7 - i);

            // Write bit to destination buffer
            iOData[0x01F] = iOData[0x01F] | data;
        }
    }
#endif