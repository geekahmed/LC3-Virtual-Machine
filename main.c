#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/termios.h>


uint16_t mem_read(uint16_t address);
void mem_write(uint16_t address, uint16_t val);
uint16_t sign_extend(uint16_t x, int bit_count);
uint16_t swap16(uint16_t x);
void update_flag(uint16_t reg);
void read_image_file(FILE* file);
int read_image(const char* image_path);
uint16_t check_key();

struct termios original_tio;
void disable_input_buffering(){
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering(){
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

void handle_interrupt(int signal){
    restore_input_buffering();
    printf("\n");
    exit(-2);
}

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

uint16_t registers[R_COUNT];

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

// Traps
enum{
    TRAP_GETC = 0x20,  // Get character from keyboard, not echoed onto the terminal
    TRAP_OUT = 0x21,   // Output a character
    TRAP_PUTS = 0x22,  // Output a word string
    TRAP_IN = 0x23,    // Get character from keyboard, echoed onto the terminal
    TRAP_PUTSP = 0x24, // Output a byte string
    TRAP_HALT = 0x25   // Halt the program
};

// Memory Mapped Registers
enum{
    MR_KBSR = 0xFE00, /* Keyboard status */
    MR_KBDR = 0xFE02  /* Keyboard data */
};
int main(int argc, const char* argv[]){
    // Load Arguments
    if (argc < 2){
        printf("LC3 [Image-File1] ...\n");
        exit(2);
    }
    for (int i = 1; i < argc; ++i) {
        if (!read_image(argv[i])){
            printf("Failed to load image: %s\n", argv[i]);
            exit(1);
        }
    }

    signal(SIGINT, handle_interrupt);
    disable_input_buffering();

    enum {PC_START = 0x3000};
    registers[R_PC] = PC_START;

    int running = 1;
    while (running){
        // Fetch
        uint16_t instruction = mem_read(registers[R_PC]++);
        uint16_t opcode = instruction >> 12;

        switch (opcode) {
            case OP_ADD:
                {
                    uint16_t DR = (instruction >> 9) & 0x7;
                    uint16_t SR1 = (instruction >> 6) & 0x7;
                    uint16_t imm_flag = (instruction >> 5) & 0x1;
                    if (imm_flag){
                        uint16_t imm_val = sign_extend(instruction & 0x1F, 5);
                        registers[DR] = registers[SR1] + imm_val;
                    } else {
                        uint16_t SR2 = instruction & 0x7;
                        registers[DR] = registers[SR1] + registers[SR2];
                    }
                    update_flag(DR);
                    break;
                }

            case OP_AND:
                {
                    uint16_t DR = (instruction >> 9) & 0x7;
                    uint16_t SR1 = (instruction >> 6) & 0x7;
                    uint16_t imm_flag = (instruction >> 5) & 0x1;
                    if (imm_flag){
                        uint16_t imm_val = sign_extend(instruction & 0x1F, 5);
                        registers[DR] = registers[SR1] & imm_val;
                    } else {
                        uint16_t SR2 = instruction & 0x7;
                        registers[DR] = registers[SR1] & registers[SR2];
                    }
                    update_flag(DR);
                    break;
                }

            case OP_NOT:
                {
                    uint16_t DR = (instruction >> 9) & 0x7;
                    uint16_t SR = (instruction >> 6) & 0x7;
                    registers[DR] = ~registers[SR];
                    update_flag(DR);
                    break;
                }

            case OP_BR:
                {
                    uint16_t pc_offset = sign_extend(instruction & 0x1FF, 9);
                    uint16_t cond_flag = (instruction >> 9) & 0x7;
                    if (cond_flag & registers[R_COND]){
                        registers[R_PC] += pc_offset;
                    }
                    break;
                }

            case OP_JMP:
                {
                    uint16_t r1 = (instruction >> 6) & 0x7 ;
                    registers[R_PC] = registers[r1];
                    break;
                }

            case OP_JSR:
                {
                    uint16_t long_flag = (instruction >> 11) & 1;
                    registers[R_R7] = registers[R_PC];
                    if (long_flag){
                        uint16_t long_pc_offset = sign_extend(instruction & 0x7FF, 11);
                        registers[R_PC] += long_pc_offset;  /* JSR */
                    }
                    else{
                        uint16_t r1 = (instruction >> 6) & 0x7;
                        registers[R_PC] = registers[r1]; /* JSRR */
                    }
                    break;
                }
            case OP_LD:
                {
                    uint16_t r0 = (instruction >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instruction & 0x1FF, 9);
                    registers[r0] = mem_read(registers[R_PC] + pc_offset);
                    update_flag(r0);
                }
                break;
            case OP_LDI:
                {
                    uint16_t DR = (instruction >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instruction & 0x1FF, 9);
                    registers[DR] = mem_read(mem_read(registers[R_PC] + pc_offset));
                    update_flag(DR);
                    break;
                }

            case OP_LDR:
                {
                    uint16_t r0 = (instruction >> 9) & 0x7;
                    uint16_t r1 = (instruction >> 6) & 0x7;
                    uint16_t offset = sign_extend(instruction & 0x3F, 6);
                    registers[r0] = mem_read(registers[r1] + offset);
                    update_flag(r0);
                }
                break;
            case OP_LEA:
                {
                    uint16_t r0 = (instruction >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instruction & 0x1FF, 9);
                    registers[r0] = registers[R_PC] + pc_offset;
                    update_flag(r0);
                }
                break;
            case OP_ST:
                {
                    uint16_t r0 = (instruction >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instruction & 0x1FF, 9);
                    mem_write(registers[R_PC] + pc_offset, registers[r0]);
                }
                break;
            case OP_STI:
                {
                    uint16_t SR = (instruction >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instruction & 0x1FF, 9);
                    mem_write(mem_read(registers[R_PC] + pc_offset), registers[SR]);
                    break;
                }

            case OP_STR:
                {
                    uint16_t r0 = (instruction >> 9) & 0x7;
                    uint16_t r1 = (instruction >> 6) & 0x7;
                    uint16_t offset = sign_extend(instruction & 0x3F, 6);
                    mem_write(registers[r1] + offset, registers[r0]);
                }
                break;
            case OP_TRAP:
                switch (instruction & 0xFF)
                {
                    case TRAP_GETC:
                        {
                          registers[R_R0] = (uint16_t) getchar();
                        }
                        break;
                    case TRAP_OUT:
                        {
                            putc((char)registers[R_R0], stdout);
                            fflush(stdout);
                        }
                        break;
                    case TRAP_PUTS:
                        {
                            uint16_t* ch = memory + registers[R_R0];
                            while (*ch){
                                putc((char)*ch, stdout);
                                ++ch;
                            }
                            fflush(stdout);
                        }
                        break;
                    case TRAP_IN:
                        {
                            printf("Enter a character: ");
                            char ch = getchar();
                            putc(ch, stdout);
                            registers[R_R0] = (uint16_t)ch;
                        }
                        break;
                    case TRAP_PUTSP:
                        {
                            uint16_t* ch = memory + registers[R_R0];
                            while (*ch)
                            {
                                char char1 = (*ch) & 0xFF;
                                putc(char1, stdout);
                                char char2 = (*ch) >> 8;
                                if (char2) putc(char2, stdout);
                                ++ch;
                            }
                            fflush(stdout);
                        }
                        break;
                    case TRAP_HALT:
                        {
                            puts("HALT");
                            fflush(stdout);
                            running = 0;
                        }
                        break;
                }
                break;
            case OP_RES:
            case OP_RTI: abort();
            default:
                break;
        }
        }

    restore_input_buffering();
}

uint16_t sign_extend(uint16_t x, int bit_count){
    if ((x >> (bit_count - 1)) & 1){
        x |= (0xFFFF << bit_count);
    }
    return x;
}
void update_flag(uint16_t reg){
    if (registers[reg] == 0){
        registers[R_COND] = FL_ZRO;
    } else if (registers[reg] >> 15){ // Negative Value
        registers[R_COND] = FL_NEG;
    } else {
        registers[R_COND] = FL_POS;
    }
}

void read_image_file(FILE* file){
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);

    uint16_t  max_read = UINT16_MAX - origin;
    uint16_t *p = memory + origin;

    size_t read = fread(p, sizeof(uint16_t), max_read, file);
    while (read-- > 0){
        *p = swap16(*p);
        ++p;
    }
}

int read_image(const char* image_path){
    FILE* file = fopen(image_path, "rb");
    if (!file) return 0;
    read_image_file(file);
    fclose(file);
    return 1;
}
uint16_t swap16(uint16_t x){
    return (x << 8) | (x >> 8);
}
uint16_t mem_read(uint16_t address){
    if (address == MR_KBSR){
        if (check_key()){
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        }
        else{
            memory[MR_KBSR] = 0;
        }
    }
    return memory[address];
}
void mem_write(uint16_t address, uint16_t val){
    memory[address] = val;
}
uint16_t check_key(){
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}