// CPE 3202 COMPUTER ORGANIZATION AND ARCHITECTURE
// Group 2      MW 7:30AM - 10:30AM LB CEAC2 TC
// Estose, Jude Vicris; Sarcol, Joshua; Silmaro, Jame Paul Jr.      BS CpE - 3  2026/05/11
// Laboratory Exercise #5: The Memory

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
long A1[32], A2[32], A3[32], A4[32], A5[32], A6[32], A7[32], A8[32];    // Main Memory  chip group A
long B1[32], B2[32], B3[32], B4[32], B5[32], B6[32], B7[32], B8[32];    //              chip group B
static uint8_t IOBuffer[IOSIZE];                                        // I/O memory

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

#if PRINTCYCLE == 1
    const uint8_t *instructionName(uint8_t opcode);
    uint8_t readMemoryDebug(uint16_t address);
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
    // Store 0x15 at memory address 0x400
    loadWord(0x000, (WB   << 11) | 0x015);  // MBR = 0x15
    loadWord(0x002, (WM   << 11) | 0x400);  // memory address 0x400 = 0x15
    
    // 5 + 8
    // Load 5 to ACC
    loadWord(0x004, (WB   << 11) | 0x005);  // MBR = 0x05
    loadWord(0x006, (WACC << 11));          // ACC = 0x05

    // Load 8 to BUS
    loadWord(0x008, (WB   << 11) | 0x008);  // MBR = 0x08
    
    // Perform the addition
    loadWord(0x00A, (ADD  << 11));          // ACC = (0x05) + (0x08) = 0x0D     ZF=0, CF=0, OF=0, SF=0

    // Previous sum * memory address 0x400
    // Read memory address 0x400 and store to BUS
    loadWord(0x00C, (RM   << 11) | 0x400);  // MBR = 0x15

    // Perform the multiplication
    loadWord(0x00E, (MUL  << 11));          // ACC = (0x0D) x (0x15) = 0x11     ZF=0, CF=1, OF=1, SF=0
    
    // Store product at memory address 0x401
    // Move result to BUS
    loadWord(0x010, (RACC << 11));          // MBR = 0x11
    loadWord(0x012, (WM   << 11) | 0x401);  // memory address 0x401 = 0x11


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
    // Branch not taken

    // If subtraction from previous compare < 0xF0, then goto 0x44
    loadWord(0x03E, (BRLT << 11) | 0x044);  // ACC = (0xE6) - (0xF0) = 0xF6     ZF=0, CF=1, OF=1, SF=1
    // Branch taken

    // Unreachable instructions
    loadWord(0x040, (WB   << 11) | 0x000);
    loadWord(0x042, (WACC << 11));

    
    // Loop construct
    // for(int i = 3, i > 0, i--);
    // Load initial value (3) to ACC
    loadWord(0x044, (WB   << 11) | 0x003);  // MBR = 0x03
    loadWord(0x046, (WACC << 11));          // ACC = 0x03

    // Load stopping condition (0) to BUS
    loadWord(0x048, (WB   << 11) | 0x000);  // MBR = 0x00

    // If ACC == 0, then exit loop
    loadWord(0x04A, (BRE  << 11) | 0x052);  // 1. ACC = (0x03) - (0x00) = 0x03  ZF=0, CF=0, OF=0, SF=0
                                            // 2. ACC = (0x02) - (0x00) = 0x02  ZF=0, CF=0, OF=0, SF=0
                                            // 3. ACC = (0x01) - (0x00) = 0x01  ZF=0, CF=0, OF=0, SF=0
                                            // 4. ACC = (0x00) - (0x00) = 0x00  ZF=1, CF=0, OF=0, SF=0
                                            // Then take branch
    
    // Subtract 1 from ACC
    // Load 1 to BUS
    loadWord(0x04C, (WB   << 11) | 0x001);  // MBR = 0x01

    // Perform the subtraction
    loadWord(0x04E, (SUB  << 11));          // 1. ACC = (0x03) - (0x01) = 0x02  ZF=0, CF=0, OF=0, SF=0
                                            // 2. ACC = (0x02) - (0x01) = 0x01  ZF=0, CF=0, OF=0, SF=0
                                            // 3. ACC = (0x01) - (0x01) = 0x00  ZF=1, CF=0, OF=0, SF=0

    // Loop back to loading of stopping condition (0x048)
    loadWord(0x050, (BR   << 11) | 0x048);


    // End of program
    loadWord(0x052, (EOP  << 11));
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
        #if PRINTCYCLE == 1
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
                MainMemory();                   // memory address MAR = MBR
                break;

            case RM:    // Read data from memory with the specified address, stores data to MBR
                OE = 1; IOM = 1; RW = 0;        // Memory read
                FETCH = 0; MEMORY = 1; IO = 0;  // Select MBR/MAR

                MAR = operand;                  // MAR <- operand
                if (MEMORY == 1) ADDR = MAR;    // Set ADDR bus to MAR/operand
                MainMemory();                   // Read result is at BUS
                if (MEMORY == 1) MBR = BUS;     // MBR = memory address MAR
                break;

            case RIO:   // Read data from IO memory with the specified address, stores data to IOBR
                OE = 1; IOM = 0; RW = 0;        // IO read
                FETCH = 0; MEMORY = 0; IO = 1;  // Select IOBR/IOAR 

                IOAR = operand;                 // IOAR <- operand
                if (IO == 1) ADDR = IOAR;       // Set ADDR bus to IOAR/operand
                IOMemory();                     // Read result is at BUS
                if (IO == 1) IOBR = BUS;        // IOBR = IOBuffer[IOAR]
                break;

            case WIO:   // Write data in IOBR to memory at address pointed to by IOAR
                OE = 1; IOM = 0; RW = 1;        // IO write
                FETCH = 0; MEMORY = 0; IO = 1;  // Select IOBR/IOAR

                IOAR = operand;                 // IOAR <- operand
                if (IO == 1) ADDR = IOAR;       // Set ADDR bus to IOAR/operand
                if (IO == 1) BUS = IOBR;        // Set data bus to IOBR
                IOMemory();                     // IOBuffer[IOAR] = IOBR
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

    #if PRINTCYCLE == 1
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
            printf("%-20s: 0x%02X\n\n", "IOBuffer[IOAR]", IOBuffer[IOAR]);
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

    #if PRINTCYCLE == 1
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

    #if PRINTCYCLE == 1
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
    if (IOM == 0) {
        if (RW == 0 && OE == 1) {           // IO read
            BUS = IOBuffer[ADDR];
        } else if (RW == 1 && OE == 1) {    // IO write
            IOBuffer[ADDR] = BUS;
        }
    }
}

// Helper function to 2 bytes of the same instruction at once
void loadWord(uint16_t addr, uint16_t word) {
    if (addr + 1 < MEMSIZE) {
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

#if PRINTCYCLE == 1
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
