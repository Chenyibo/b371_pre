#pragma once
#include <stdint.h>

#ifndef REGISTER_NUM
#define REGISTER_NUM 32
#endif

////////////////////////////////////////////////////////
/// Struct Definitions
////////////////////////////////////////////////////////
struct memory_stats_t{
    uint64_t lw_total;
    uint64_t sw_total;
};


struct instr_meta{
    uint32_t instr;
    int immediate; // sign extended
    int opcode;
    uint8_t reg_21_25;
    uint8_t reg_16_20;
    uint8_t reg_11_15;
    uint8_t  type; //
    int function;
    int jmp_offset;
};

struct ctrl_signals {
    // 1-bit signals
    int RegDst;
    int RegWrite;
    int ALUSrcA;

    int MemRead;
    int MemWrite;
    int MemtoReg;

    int IorD;
    int IRWrite;

    int PCWrite;
    int PCWriteCond;

    //2-bit signals
    int ALUOp;
    int ALUSrcB;
    int PCSource;

};

struct pipe_regs {
    int pc;
    int IR;
    int A;
    int B;
    int ALUOut;
    int MDR;
};

struct if_id_reg {
    int valid;
    uint32_t instr;
    int pc_plus4;
};

struct id_ex_reg {
    int valid;
    uint32_t instr;
    int pc_plus4;

    int rs;
    int rt;
    int rd;
    int rs_val;
    int rt_val;
    int imm;
    int opcode;
    int funct;
    int jump_index;

    int RegWrite;
    int MemRead;
    int MemWrite;
    int MemtoReg;
    int RegDst;
    int ALUSrc;
    int ALUOp;
    int Branch;
    int Jump;
    int is_halt;
};

struct ex_mem_reg {
    int valid;
    uint32_t instr;
    int alu_result;
    int store_data;
    int dest_reg;
    int RegWrite;
    int MemRead;
    int MemWrite;
    int MemtoReg;
    int is_halt;
};

struct mem_wb_reg {
    int valid;
    uint32_t instr;
    int mem_data;
    int alu_result;
    int dest_reg;
    int RegWrite;
    int MemtoReg;
    int is_halt;
};


////////////////////////////////////////////////////////
/// Helper Defines & Functions
////////////////////////////////////////////////////////


//Offsets and sizes
#define OPCODE_OFFSET 26
#define OPCODE_SIZE 6 // 6 bits to encode an opcode (26-31)
#define REGISTER_ID_SIZE 5 // 5 bits to encode an opcode
#define IMMEDIATE_OFFSET 0


//Instruction types
#define MEM_TYPE 0
#define R_TYPE 1
#define BRANCH_TYPE 2
#define JUMP_TYPE 3
#define I_TYPE 4
#define EOP_TYPE 6
#define JR_TYPE 7

// OPCODES
#define R_INST 0 // 000000
#define ADD 32    // 100000
#define ADDI 8    // 001000
#define LW 35     // 100011
#define SW 43     // 101011
#define BEQ  4    // 000100
#define J 2       // 000010
#define SLT 42    // 101010
#define EOP 63    // 111111


// FSM STATES
#define INSTR_FETCH 0
#define DECODE 1
#define MEM_ADDR_COMP 2
#define MEM_ACCESS_LD 3
#define WB_STEP 4
#define MEM_ACCESS_ST 5
#define EXEC 6
#define R_TYPE_COMPL 7
#define BRANCH_COMPL 8
#define JUMP_COMPL 9
#define EXIT_STATE 10
#define I_TYPE_EXEC 11
#define I_TYPE_COMPL 12



struct architectural_state {
    int state;
    uint64_t clock_cycle;
    struct ctrl_signals control;
    struct instr_meta IR_meta;
    struct pipe_regs curr_pipe_regs;
    struct pipe_regs next_pipe_regs;
    struct if_id_reg if_id;
    struct id_ex_reg id_ex;
    struct ex_mem_reg ex_mem;
    struct mem_wb_reg mem_wb;
    int pipeline_stop_fetch;
    int bits_for_cache_tag;
    struct memory_stats_t mem_stats;
    int registers[REGISTER_NUM];
    uint32_t *memory;
};

