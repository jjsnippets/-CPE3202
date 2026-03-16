// CPE 3202 COMPUTER ORGANIZATION AND ARCHITECTURE
// Group 2      MW 7:30AM - 10:30AM LB CEAC2 TC
// Estose, Jude Vicris; Sarcol, Joshua; Silmaro, Jame Paul Jr.      BS CpE - 3  2026/03/16
// Laboratory Exercise #1: The ALU

#include <stdio.h>

// Opcodes
typedef enum operations {
    ADD = 0x01,
    SUB = 0x02,
    MUL = 0x03,
    AND = 0x04,
    OR = 0x05,
    NOT = 0x06,
    XOR = 0x07,
    SHR = 0x08,
    SHL = 0x09,
} OPERATIONS;

// Function Prototypes
int ALU(unsigned char operand1, unsigned char operand2, unsigned char control_signals);
unsigned char twosComp(unsigned char data);
unsigned char setFlags(unsigned int ACC);
void printBin(int data, unsigned char data_width);

// Global Variables
unsigned int ACC;               // Accumulator register
unsigned char CF, ZF, OF, SF;   // Carry, Zero, Overflow, Sign Flags

void main(){
    ALU(0x88, 0x85, ADD);
    ALU(0x03, 0x05, SUB);
    ALU(0xC0, 0x0A, MUL);
    ALU(0x46, 0x02, MUL);
    ALU(0x5A, 0x3C, AND);
    ALU(0x5A, 0x3C, OR);
    ALU(0x5A, 0x3C, NOT);
    ALU(0x5A, 0x3C, XOR);
    ALU(0x5A, 0x00, SHR);
    ALU(0x5A, 0x00, SHL);
}

// Main function of interest: ALU
int ALU(unsigned char operand1, unsigned char operand2, unsigned char control_signals){
    
    // Temporary operand registers
    unsigned char temp_op1, temp_op2;

    // initial values
    ACC = 0x0000;
    CF = 0, ZF = 0, OF = 0, SF = 0;

    // Print inputs
    printf("====================================\n");
    printf("OP1: ");
    printBin(operand1, 8);
    printf("\nOP2: ");
    printBin(operand2, 8);
    printf("\n");    

    switch (control_signals){
        case ADD:
        case SUB:
            printf("Operation: %s\n", (control_signals == ADD) ? "ADD" : "SUB");
            temp_op1 = operand1;
            temp_op2 = (control_signals == SUB) ? twosComp(operand2) : operand2;
            ACC = temp_op1 + temp_op2;
            break;
            
        case MUL: { // Booth's algorithm implementation
            unsigned char A = 0x00;
            unsigned char Q = operand2;
            unsigned char M = operand1;

            unsigned char A_7, A_0, Q_0, Q_n1 = 0x00;

            printf("Operation: MUL\n");
            printf("%8s %8s %4s %8s %s\n", "A", "Q", "Qn-1", "M", "n");
            printBin(A, 8);
            printBin(Q, 8);
            printf("%4d ", Q_n1);
            printBin(M, 8);
            printf("%d\n", 0);

            for (int i = 0; i < 8; i++){
                // Check the last bit of Q and Qn-1
                Q_0 = Q & 0x01;

                // Perform addition or subtraction based on Q_0 and Qn-1
                if (Q_0 == 1 && Q_n1 == 0) {
                    temp_op1 = twosComp(M);
                    A += temp_op1;
                } else if (Q_0 == 0 && Q_n1 == 1) {
                    A += M;
                }

                // Save bits before shifting
                A_7 = (A & 0x80) >> 7;
                A_0 = A & 0x01;
                Q_0 = Q & 0x01;

                // Right shift A and Q
                A = (A >> 1) | (A_7 << 7);  // Shift A right and fill MSB with A_7
                Q = (Q >> 1) | (A_0 << 7);  // Shift Q right and fill MSB with A_0
                Q_n1 = Q_0;                 // Update Qn-1 for the next iteration

                // Printing per iteration
                printBin(A, 8);
                printBin(Q, 8);
                printf("%4d ", Q_n1);
                printBin(M, 8);
                printf("%d\n", i + 1);
            }

            ACC = ((A << 8) | Q) & 0xFFFF; // Combine A and Q for the final result
            break;
        }
        case AND:
            printf("Operation: AND\n");
            ACC = operand1 & operand2;
            break;

        case OR:
            printf("Operation: OR\n");
            ACC = operand1 | operand2;
            break;  

        case NOT:
            printf("Operation: NOT\n");
            ACC = ~operand1 & 0x00FF; // NOT operation on operand1
            break;

        case XOR:
            printf("Operation: XOR\n");
            ACC = operand1 ^ operand2;
            break;

        case SHR:
            printf("Operation: SHR\n");
            ACC = operand1 >> 1;
            break;

        case SHL:
            printf("Operation: SHL\n");
            ACC = (operand1 << 1);
            break;

        default:
            printf("Invalid control signal\n");
            return -1;
    }
    
    setFlags(ACC);

    // Print results
    printf("ACC: ");
    printBin(ACC, 16);
    printf("\nCF: %d, ZF: %d, OF: %d, SF: %d\n\n", CF, ZF, OF, SF);

    return 0;
}

// Helper functions
unsigned char twosComp(unsigned char data){
    return ~data + 1;
}

unsigned char setFlags(unsigned int ACC){
    CF = (ACC > 0x00FF) ? 1 : 0;    // Carry: if ACC exceeds 8 bits
    ZF = (ACC == 0x0000) ? 1 : 0;   // Zero: if ACC is zero
    OF = (ACC > 0x007F) ? 1 : 0;    // Overflow: if ACC exceeds +127
    SF = (ACC & 0x0080) ? 1 : 0;    // Sign: if ACC is negative (8th bit is set)
    return 0;
}

void printBin(int data, unsigned char data_width){
    for (int i = data_width - 1; i >= 0; i--)
        printf("%d", (data >> i) & 1);
    printf(" ");
}
