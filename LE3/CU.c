// CPE 3202 COMPUTER ORGANIZATION AND ARCHITECTURE
// Group 2      MW 7:30AM - 10:30AM LB CEAC2 TC
// Estose, Jude Vicris; Sarcol, Joshua; Silmaro, Jame Paul Jr.      BS CpE - 3  2026/04/25
// Laboratory Exercise #3 The Buses

#include <stdlib.h>
#include <stdint.h>
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
static uint8_t dataMemory[MEMSIZE];     // Main memory
static uint8_t IOBuffer[IOSIZE];        // I/O

// Buses and Control signals
volatile uint16_t ADDR;             // Address Bus (11)
volatile uint8_t BUS;               // Data Bus (8)
volatile uint8_t CONTROL;           // Control Bus (5)
volatile uint8_t IOM;               // IO/M Control signal
volatile uint8_t RW;                // R/W Control signal
volatile uint8_t OE;                // OE Control signal

// Function prototypes
int CU(void);
void MainMemory(void);
void IOMemory(void);
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
    uint16_t PC = 0;        // Program Counter
    uint16_t IR = 0;        // Instruction Register
    uint16_t MAR = 0;       // Memory Address Register
    uint8_t MBR = 0;        // Memory Buffer Register
    uint16_t IOAR = 0;      // I/O Address Register
    uint8_t IOBR = 0;       // I/O Buffer Register

    // Local Control lines
    uint8_t INCREMENT = 0;  // Increment Control line
    uint8_t FETCH = 0;      // Fetch Control line
    uint8_t IO = 0;         // IO Control line
    uint8_t MEMORY = 0;     // Memory Control line

    uint8_t run = 1, isError = 0;
    do {
        #if PRINTCYCLE == 1
            uint16_t PC_before = PC;
        #endif

        // Fetch
        IOM = 1; RW = 0; OE = 1;        // Memory read
        FETCH = 1; IO = 0; MEMORY = 0;  // Local control signals
        
        // Upper byte
        ADDR = PC; MainMemory();        // Fetch upper byte
        if(FETCH == 1){
            IR = ((int) BUS) << 8;      // Load upper instruction byte
            
            INCREMENT = 1;              
            if(INCREMENT == 1) PC++;    // Point to lower byte
            ADDR = PC;
            INCREMENT = 0;
        }

        // Lower byte
        MainMemory();
        if(FETCH == 1){
            IR = IR | BUS;              // Load lower instruction byte

            INCREMENT = 1;
            if(INCREMENT == 1) PC++;    // Point to next instruction
            INCREMENT = 0;
        }
        FETCH = 0;

        // Decode and Execute
        uint8_t opcode = (IR >> 11) & 0x1F;     // Extract opcode (5 bits)
        uint16_t operand = IR & 0x7FF;          // Extract operand (11 bits)

        switch(opcode) {
            case WM:    // Write to memory (to what is already in MBR)
                MAR = operand;                  // Set MAR to operand address

                CONTROL = opcode;               // Current instruction to be executed
                IOM = 1; RW = 1; OE = 1;        // Memory write
                FETCH = 0; MEMORY = 1; IO = 0;  // Accessing memory

                if(MEMORY == 1) ADDR = MAR;     // Write MBR to memory
                if(MEMORY == 1) BUS = MBR;
                MainMemory();                   // dataMemory[MAR] = MBR;
                break;

            case RM:    // Read from memory
                MAR = operand;                  // Set MAR to operand address

                CONTROL = opcode;               // Current instruction to be executed
                IOM = 1; RW = 0; OE = 1;        // Memory read
                FETCH = 0; MEMORY = 1; IO = 0;  // Accessing memory

                if(MEMORY == 1) ADDR = MAR;     // Read memory to MBR
                MainMemory();
                if(MEMORY == 1) MBR = BUS;      // MBR = dataMemory[MAR];
                break;

            case BR:    // Branch to address
                MAR = operand;                  // Set MAR to operand address

                CONTROL = opcode;               // Current instruction to be executed
                IOM = 0; RW = 0; OE = 0;        // Disable external bus acess
                FETCH = 0; MEMORY = 0; IO = 0;

                PC = MAR;                       // PC <- MAR
                break;
            
            case RIO:   // Read from IO buffer
                IOAR = operand;                 // Set IOAR to operand address

                CONTROL = opcode;               // Current instruction to be executed
                IOM = 0; RW = 0; OE = 1;        // IO read
                FETCH = 0; MEMORY = 0; IO = 1;  // Accessing IO

                if(IO == 1) ADDR = IOAR;        // Read from IO buffer to IOBR
                IOMemory();
                if(IO == 1) IOBR = BUS;         // IOBR = IOBuffer[IOAR];
                break;

            case WIO:   // Write to IO buffer (to what is already in IOBR)
                IOAR = operand;                 // Set IOAR to operand address

                CONTROL = opcode;               // Current instruction to be executed
                IOM = 0; RW = 1; OE = 1;        // IO write
                FETCH = 0; MEMORY = 0; IO = 1;  // Accessing IO

                if(IO == 1) ADDR = IOAR;        // Write from IOBR to IO buffer
                if(IO == 1) BUS = IOBR;
                IOMemory();                     // IOBuffer[IOAR] = IOBR;
                break;

            case WB:    // Write data to MBR
                MBR = operand;                  // Write operand to MBR

                CONTROL = opcode;               // Current instruction to be executed
                IOM = 0; RW = 0; OE = 0;        // Disable external bus acess
                FETCH = 0; MEMORY = 0; IO = 0;
                break;

            case WIB:   // Write data to IOBR
                IOBR = operand;                 // Write operand to IOBR

                CONTROL = opcode;               // Current instruction to be executed
                IOM = 0; RW = 0; OE = 0;        // Disable external bus acess
                FETCH = 0; MEMORY = 0; IO = 0;
                break;

            case EOP:   // End of program
                CONTROL = opcode;               // Current instruction to be executed
                IOM = 0; RW = 0; OE = 0;        // Disable buses before halting
                FETCH = 0; MEMORY = 0; IO = 0;

                run = 0;
                break;

            default:    // Invalid opcode
                CONTROL = opcode;               // Current instruction to be executed
                IOM = 0; RW = 0; OE = 0;        // Disable buses on error
                FETCH = 0; MEMORY = 0; IO = 0;

                isError = 1;
                run = 0;
                break;
        }

        // Print current state (for debugging)
        #if PRINTCYCLE == 1
            printf("====================================\n");
            printf("%-5s: 0x%02X | %-6s: 0x%03X \n\n", "BUS", BUS, "ADDR", ADDR);
            printf("%-20s: 0x%03X\n", "PC", PC_before);
            printf("Fetching instruction...\n");
            printf("%-20s: 0x%04X\n", "IR", IR);
            printf("%-20s: 0x%02X\n", "Instruction Code", opcode);
            printf("%-20s: 0x%02X\n", "CONTROL", CONTROL);
            printf("%-20s: 0x%03X\n", "Operand", operand);

            switch (opcode) {
                case WM:
                    printf("%-20s: %s\n", "Instruction", "WM");
                    printf("Executing Write to Memory\n");
                    printf("%-20s: 0x%03X\n", "MAR", MAR);
                    printf("%-20s: 0x%02X\n", "MBR", MBR);
                    printf("%-20s: 0x%02X\n", "Memory[MAR]", dataMemory[MAR]);
                    break;

                case RM:
                    printf("%-20s: %s\n", "Instruction", "RM");
                    printf("Executing Read from Memory\n");
                    printf("%-20s: 0x%03X\n", "MAR", MAR);
                    printf("%-20s: 0x%02X\n", "MBR", MBR);
                    break;

                case BR:
                    printf("%-20s: %s\n", "Instruction", "BR");
                    printf("Executing Branch to Address\n");
                    printf("%-20s: 0x%03X\n", "MAR", MAR);
                    break;

                case RIO:
                    printf("%-20s: %s\n", "Instruction", "RIO");
                    printf("Executing Read from IO Buffer\n");
                    printf("%-20s: 0x%03X\n", "IOAR", IOAR);
                    printf("%-20s: 0x%02X\n", "IOBR", IOBR);
                    break;

                case WIO:
                    printf("%-20s: %s\n", "Instruction", "WIO");
                    printf("Executing Write to IO Buffer\n");
                    printf("%-20s: 0x%03X\n", "IOAR", IOAR);
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

void MainMemory(void){
    if(IOM == 1){
        if(RW == 0 && OE == 1)          // Memory read
            BUS = dataMemory[ADDR];
        else if (RW == 1 && OE == 1)    // Memory write
            dataMemory[ADDR] = BUS;
    }
}

void IOMemory(void){
    if(IOM == 0){
        if(RW == 0 && OE == 1)          // IO read
            BUS = IOBuffer[ADDR];
        else if (RW == 1 && OE == 1)    // IO write
            IOBuffer[ADDR] = BUS;
    }
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
