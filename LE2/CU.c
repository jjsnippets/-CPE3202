// CPE 3202 COMPUTER ORGANIZATION AND ARCHITECTURE
// Group 2      MW 7:30AM - 10:30AM LB CEAC2 TC
// Estose, Jude Vicris; Sarcol, Joshua; Silmaro, Jame Paul Jr.      BS CpE - 3  2026/04/08
// Laboratory Exercise #2 The Control Unit (CU)

#include <stdlib.h>
#include <stdio.h>

#define MEMSIZE 0x0800
#define IOSIZE 0x0020
#define PRINTCYCLE 1

// Opcodes
typedef enum inst_opcode {
    WM  = 0b00001,      // Write to memory
    RM  = 0b00010,      // Read from memory
    BR  = 0b00011,      // Branch
    RIO = 0b00100,      // Read from IO buffer
    WIO = 0b00101,      // Write from IO buffer
    WB  = 0b00110,      // Write data to MBR
    WIB = 0b00111,      // Write data to IOBR
    EOP = 0b11111,      // End of program
} INST_OPCODE;

// Main memory and I/O buffer
static unsigned char dataMemory[MEMSIZE];
static unsigned char IOBuffer[IOSIZE];

// Function prototypes
int CU(void);
void initMemory(void);

int main(void) {
    initMemory();

    if (CU() == 1) {
        printf("Program run successfully.\n");
    } else {
        printf("Program execution failed.\n");
    }

    return 0;
}

int CU(void){
    unsigned short int PC = 0;      // Program Counter
    unsigned short int IR = 0;      // Instruction Register
    unsigned short int MAR = 0;     // Memory Address Register
    unsigned char MBR = 0;          // Memory Buffer Register
    unsigned short int IOAR = 0;    // I/O Address Register
    unsigned char IOBR = 0;         // I/O Buffer Register

    unsigned char run = 1, isError = 0;
    do {
        #if PRINTCYCLE == 1
            unsigned short int PC_before = PC;
        #endif

        // Fetch
        IR = ((unsigned short int)dataMemory[PC] << 8) | (unsigned short int)dataMemory[PC + 1];
        PC += 2;   // Increment PC to point to next instruction

        // Decode and Execute
        unsigned char opcode = (IR >> 11) & 0x1F;       // Extract opcode (5 bits)
        unsigned short int operand = IR & 0x7FF;        // Extract operand (11 bits)

        switch(opcode) {
            case WM:    // Write to memory (to what is already in MBR)
                MAR = operand;          // Set MAR to operand address
                dataMemory[MAR] = MBR;  // Write MBR to memory
                break;

            case RM:    // Read from memory
                MAR = operand;          // Set MAR to operand address
                MBR = dataMemory[MAR];  // Read memory to MBR
                break;

            case BR:    // Branch to address
                MAR = operand;          // Set MAR to operand address
                PC = MAR;               // PC <- MAR
                break;
            
            case RIO:   // Read from IO buffer
                IOAR = operand;         // Set IOAR to operand address
                IOBR = IOBuffer[IOAR];  // Read from IO buffer to IOBR
                break;

            case WIO:   // Write to IO buffer (to what is already in IOBR)
                IOAR = operand;         // Set IOAR to operand address
                IOBuffer[IOAR] = IOBR;  // Write from IOBR to IO buffer
                break;

            case WB:    // Write data to MBR
                MBR = operand;          // Write operand to MBR
                break;

            case WIB:   // Write data to IOBR
                IOBR = operand;         // Write operand to IOBR
                break;

            case EOP:   // End of program
                run = 0;
                break;

            default:    // Invalid opcode
                isError = 1;
                run = 0;
                break;
        }

        // Print current state (for debugging)
        #if PRINTCYCLE == 1
            printf("====================================\n");
            printf("%-20s: 0x%03X\n", "PC", PC_before);
            printf("Fetching instruction...\n");
            printf("%-20s: 0x%04X\n", "IR", IR);
            printf("%-20s: 0x%02X\n", "Instruction Code", opcode);
            printf("%-20s: 0x%03X\n", "Operand", operand);

            switch (opcode) {
                case WM:
                    printf("%-20s: %s\n", "Instruction", "WM");
                    printf("Executing Write to Memory\n");
                    printf("%-20s: 0x%02X\n", "MBR", MBR);
                    printf("%-20s: 0x%02X\n", "Memory[MAR]", dataMemory[MAR]);
                    break;

                case RM:
                    printf("%-20s: %s\n", "Instruction", "RM");
                    printf("Executing Read from Memory\n");
                    printf("%-20s: 0x%02X\n", "MBR", MBR);
                    break;

                case BR:
                    printf("%-20s: %s\n", "Instruction", "BR");
                    printf("Executing Branch to Address\n");
                    break;

                case RIO:
                    printf("%-20s: %s\n", "Instruction", "RIO");
                    printf("Executing Read from IO Buffer\n");
                    printf("%-20s: 0x%02X\n", "IOBR", IOBR);
                    break;

                case WIO:
                    printf("%-20s: %s\n", "Instruction", "WIO");
                    printf("Executing Write to IO Buffer\n");
                    printf("%-20s: 0x%02X\n", "IOBR", IOBR);
                    printf("%-20s: 0x%02X\n", "IOBuffer[IOAR]", IOBuffer[IOAR]);
                    break;

                case WB:
                    printf("%-20s: %s\n", "Instruction", "WB");
                    printf("Executing Write to MBR\n");
                    printf("%-20s: 0x%02X\n", "MBR", MBR);
                    break;

                case WIB:
                    printf("%-20s: %s\n", "Instruction", "WIB");
                    printf("Executing Write to IOBR\n");
                    printf("%-20s: 0x%02X\n", "IOBR", IOBR);
                    break;

                case EOP:
                    printf("End of Program\n");
                    break;

                default:
                    printf("Invalid opcode encountered.\n");
                    break;
            }
        #endif

    } while (run);

    return (isError) ? 0 : 1;
}

void initMemory(void){
    #if PRINTCYCLE == 1
        printf("Initializing Main Memory...\n\n");
    #endif

    // Initialize memory with some instructions (for testing)
    dataMemory[0x000] = 0x30;       // WB 0xFF
    dataMemory[0x001] = 0xFF;
    dataMemory[0x002] = 0x0C;       // WM 0x400
    dataMemory[0x003] = 0x00;
    dataMemory[0x004] = 0x14;       // RM 0x400
    dataMemory[0x005] = 0x00;
    dataMemory[0x006] = 0x19;       // BR 0x12A
    dataMemory[0x007] = 0x2A;

    dataMemory[0x12A] = 0x38;       // WIB 0x05
    dataMemory[0x12B] = 0x05;
    dataMemory[0x12C] = 0x28;       // WIO 0x0A
    dataMemory[0x12D] = 0x0A;
    dataMemory[0x12E] = 0x20;       // RIO 0x0A
    dataMemory[0x12F] = 0x0A;
    dataMemory[0x130] = 0xF8;       // EOP
    dataMemory[0x131] = 0x00;
} 




