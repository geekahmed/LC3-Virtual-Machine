#include <bits/stdint-uintn.h>
#include <stdint.h>

// 65536 memory locations
uint16_t memory[UINT16_MAX];

// Registers
/*
 * 8 general purpose registers (R0-R7)
 * 1 program counter (PC) register
 * 1 condition flags (COND) register
*/

enum {
    R_R0=0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC,
    R_COND,
    R_COUNT
};

uint16_t registers[R_COND];

// Instruction Set
// 16 bit instruction with 4 bit to the opcode
enum {
    OP_BR = 0, // Branch
    OP_ADD,    // Add
    OP_LD,     // Load
    OP_ST,     // Store,
    OP_JSR,    // Jump Register
    OP_AND,    // Bitwise And
    OP_LDR,    // Load Register
    OP_STR,    // Store Register
    OP_RTI,    // Unused
    OP_NOT,    // Bitwise Not
    OP_LDI,    // Load Indirect
    OP_STI,    // Store Indirect
    OP_JMP,    // Jump
    OP_RES,    // Reserved (Unused)
    OP_LEA,    // Load Effective Address
    OP_TRAP,   // Execute Trap
};

// Condition Flags
// 3 Condition Flags only.
enum {
    FL_POS = 1 << 0, // Positive
    FL_ZRO = 1 << 1, // Zero
    FL_NEG = 1 << 2, // Negative
};
int main(){
    return 0;
}