
#include "mipssim.h"

int BREAK_POINT = 200000; // exit after so many cycles -- useful for debugging

// Global variables
char mem_init_path[1000];
char reg_init_path[1000];

uint32_t task_number = 0;
struct architectural_state arch_state;

uint8_t get_instruction_type(int opcode)
{
    switch (opcode)
    {
        /// opcodes are defined in definitions.h

    case R_INST:
        return R_TYPE;
    case ADD:
        return R_TYPE;
    case ADDI:
        return I_TYPE;
    case LW:
        return MEM_TYPE;
    case SW:
        return MEM_TYPE;
    case SLT:
        return R_TYPE;
    case EOP:
        return EOP_TYPE;
    case J:
        return JUMP_TYPE;
    case BEQ:
        return BRANCH_TYPE;
        ///@students task 1: fill in the rest

    default:
        assert(false && "missing opcode case");
    }
    assert(false);
}

int FSM(int state, struct instr_meta *IR_meta)
{
    //states defined in definitions.h
    struct ctrl_signals *control = &arch_state.control;
    //reset control signals
    memset(control, 0, (sizeof(struct ctrl_signals)));

    int opcode = IR_meta->opcode;
    int type = IR_meta->type;
    switch (state)
    {
    case INSTR_FETCH:
        control->MemRead = 1;
        control->ALUSrcA = 0;
        control->IorD = 0;
        control->IRWrite = 1;
        control->ALUSrcB = 1;
        control->ALUOp = 0;
        control->PCWrite = 1;
        control->PCSource = 0;
        state = DECODE;
        break;
    case DECODE:
        control->ALUSrcA = 0;
        control->ALUSrcB = 3;
        control->ALUOp = 0;
        if (type == R_TYPE)
            state = EXEC;
        else if (type == I_TYPE)
            state = I_TYPE_EXEC;
        else if (opcode == EOP)
            state = EXIT_STATE;
        else if (opcode == LW || opcode == SW)
            state = MEM_ADDR_COMP;
        else if (opcode == J)
            state = JUMP_COMPL;
        else if (opcode == BEQ)
            state = BRANCH_COMPL;
        else
            assert(false && "decode not yet implemented");
        break;
    case MEM_ADDR_COMP:
        control->ALUSrcA = 1;
        control->ALUSrcB = 2;
        control->ALUOp = 0;
        if (opcode == LW)
            state = MEM_ACCESS_LD;
        else
            state = MEM_ACCESS_ST;
        break;
    case MEM_ACCESS_LD:
        control->MemRead = 1;
        control->IorD = 1;
        state = WB_STEP;
        break;

    case WB_STEP:
        control->RegDst = 0;
        control->RegWrite = 1;
        control->MemtoReg = 1;
        state = INSTR_FETCH;
        break;

    case MEM_ACCESS_ST:
        control->MemWrite = 1;
        control->IorD = 1;
        state = INSTR_FETCH;
        break;

    case BRANCH_COMPL:
        control->ALUSrcA = 1;
        control->ALUSrcB = 0;
        control->ALUOp = 1;
        control->PCWriteCond = 1;
        control->PCSource = 1;
        state = INSTR_FETCH;
        break;
    case EXEC:
        control->ALUSrcA = 1;
        control->ALUSrcB = 0;
        control->ALUOp = 2;
        state = R_TYPE_COMPL;
        break;
    case R_TYPE_COMPL:
        control->RegDst = 1;
        control->RegWrite = 1;
        control->MemtoReg = 0;
        state = INSTR_FETCH;
        break;
    case JUMP_COMPL:
        control->PCWrite = 1;
        control->PCSource = 2;
        state = INSTR_FETCH;
        break;
    case I_TYPE_EXEC:
        control->ALUOp = 0;
        control->ALUSrcA = 1;
        control->ALUSrcB = 2;
        state = I_TYPE_COMPL;
        break;
    case I_TYPE_COMPL:
        control->RegDst = 0;
        control->RegWrite = 1;
        control->MemtoReg = 0;
        state = INSTR_FETCH;
        break;
    case EXIT_STATE:
        break;
    default:
        assert(false && "FSM state not yet implemented");
    }

    return state;
}

void instruction_fetch()
{
    ///@students task 5: to alter.
    if (arch_state.control.MemRead && !arch_state.control.IorD)
    {
        int address = arch_state.curr_pipe_regs.pc;
        arch_state.next_pipe_regs.IR = memory_read(address);
        arch_state.next_pipe_regs.pc = arch_state.curr_pipe_regs.pc + 4;
    }
}

void decode_and_read_RF()
{
    int read_register_1 = arch_state.IR_meta.reg_21_25;
    int read_register_2 = arch_state.IR_meta.reg_16_20;
    check_is_valid_reg_id(read_register_1);
    check_is_valid_reg_id(read_register_2);
    arch_state.next_pipe_regs.A = arch_state.registers[read_register_1];
    arch_state.next_pipe_regs.B = arch_state.registers[read_register_2];
}

void execute()
{
    ///@students task 5: extra instructions must be implemented here.

    // What do I need to check here
    // 4 Possible instructions: memory reference, R-Type, Branch, Jump
    // Memory Reference Case
    //    ALUOut = A + sign-extend (IR[15-0])
    //
    // R-Type Case
    //    ALUOut = A op B
    //
    // Branch Case:
    //    if (A == B) PC = ALUOut
    //
    // Jump Case:
    //    PC = PC [31-28] || (IR[25-0] << 2)

    struct ctrl_signals *control = &arch_state.control;
    struct instr_meta *IR_meta = &arch_state.IR_meta;
    struct pipe_regs *curr_pipe_regs = &arch_state.curr_pipe_regs;
    struct pipe_regs *next_pipe_regs = &arch_state.next_pipe_regs;

    int alu_opA = control->ALUSrcA == 1 ? curr_pipe_regs->A : curr_pipe_regs->pc;
    int alu_opB = 0;
    int immediate = IR_meta->immediate;
    int shifted_immediate = (immediate) << 2;

    // Determine value of ALUSrcB with Control signals
    switch (control->ALUSrcB)
    {
    case 0:
        // ALUSrcB = 00 => Second ALU input comes from B register
        alu_opB = curr_pipe_regs->B;
        break;
    case 1:
        // ALUSrcB = 01 => Second ALU input is 4 (constant)
        alu_opB = WORD_SIZE;
        break;
    case 2:
        // ALUSrcB = 10 => Second ALU input is sign-extended lower 16 bits of IR
        alu_opB = immediate;
        break;
    case 3:
        // ALUSrcB = 11 => Sign-extended, lower 16 bits of IR shifted left by 2 bits
        alu_opB = shifted_immediate;
        break;
    default:
        assert(false && "missing ALUSrcB control");
    }

    // Go into the ALU and perform the function
    switch (control->ALUOp)
    {
    case 0:
        // ALUOp = 00 => Add Operation
        next_pipe_regs->ALUOut = alu_opA + alu_opB;
        break;
    case 1:
        // ALUOp = 01 => Subtraction  operation
        next_pipe_regs->ALUOut = alu_opA - alu_opB;
        break;
    case 2:
        // ALUOp = 10 => operation determined by FUNCT field (R-Type)
        if (IR_meta->type == R_TYPE)
        {
            switch (IR_meta->function)
            {
            case ADD:
                next_pipe_regs->ALUOut = alu_opA + alu_opB;
                break;
            case SLT:
                next_pipe_regs->ALUOut = alu_opA < alu_opB ? 1 : 0;
                break;
            case LW:
                next_pipe_regs->ALUOut = alu_opB;
                break;
            case SW:
                next_pipe_regs->ALUOut = alu_opB;
                break;
                assert(false && "Unsupported R-Type instruction");
            }
        }
        break;
    default:
        assert(false && "ALUOp not fully defined");
    }

    // PC calculation
    switch (control->PCSource)
    {
    case 0:
        // PCSource = 00 => PC+4
        next_pipe_regs->pc = next_pipe_regs->ALUOut;
        break;
    case 1:
        // PCSource = 01 => ALUOut (branch target address determined by ALU)
        next_pipe_regs->pc = curr_pipe_regs->ALUOut;
        break;
    case 2:
        // PCSource = 10 => Jump address computed with PC[31-28] || (IR[15-0] << 2)
        next_pipe_regs->pc = get_piece_of_a_word(curr_pipe_regs->pc, 28, 4) | ((IR_meta->jmp_offset) << 2);
        break;
    default:
        assert(false && "PC Source not implemented");
    }
}

void memory_access()
{
    ///@students task 5: appropriate calls to functions defined in memory_hierarchy.c must be added
    int b = arch_state.curr_pipe_regs.B;


    if (arch_state.control.MemRead && arch_state.state != DECODE) {
        int address = arch_state.curr_pipe_regs.ALUOut;
        arch_state.next_pipe_regs.MDR = memory_read(address);
    }

    if (arch_state.control.MemWrite && arch_state.control.IorD) {
        memory_write(arch_state.curr_pipe_regs.ALUOut, b);
    }
}

void write_back()
{
    ///@students task 5: to alter.
    if (arch_state.control.RegWrite)
    {
        int write_data = arch_state.curr_pipe_regs.ALUOut;
        int write_reg_id = arch_state.IR_meta.reg_11_15;
        if (arch_state.control.MemtoReg)
            write_data = arch_state.curr_pipe_regs.MDR; // If the MemtoReg is asserted set source to MDR
        if (!arch_state.control.RegDst)
            write_reg_id = arch_state.IR_meta.reg_16_20; // If RegDst is deasserted then set source to reg_16_20
        check_is_valid_reg_id(write_reg_id);
        if (write_reg_id > 0)
        { // Zero is a reserved register in MIPs and is not accessible to writing.
            arch_state.registers[write_reg_id] = write_data;
        }
        else
            printf("Attempting to write reg_0. That is likely a mistake \n");
    }
}

void set_up_IR_meta(int IR, struct instr_meta *IR_meta)
{
    IR_meta->opcode = get_piece_of_a_word(IR, OPCODE_OFFSET, OPCODE_SIZE);
    IR_meta->immediate = get_sign_extended_imm_id(IR, IMMEDIATE_OFFSET);
    IR_meta->function = get_piece_of_a_word(IR, 0, 6);
    IR_meta->jmp_offset = get_piece_of_a_word(IR, 0, 26);
    IR_meta->reg_11_15 = (uint8_t)get_piece_of_a_word(IR, 11, REGISTER_ID_SIZE);
    IR_meta->reg_16_20 = (uint8_t)get_piece_of_a_word(IR, 16, REGISTER_ID_SIZE);
    IR_meta->reg_21_25 = (uint8_t)get_piece_of_a_word(IR, 21, REGISTER_ID_SIZE);
    IR_meta->type = get_instruction_type(IR_meta->opcode);

    switch (IR_meta->opcode)
    {
    case R_INST:
        if (IR_meta->function == ADD)
            printf("Executing ADD(%d), $%u = $%u + $%u (function: %u) \n",
                   IR_meta->opcode, IR_meta->reg_11_15, IR_meta->reg_21_25, IR_meta->reg_16_20, IR_meta->function);
        else if (IR_meta->function == SLT)
            printf("Executing SLT(%d), $%u = $%u < $%u ? 1 : 0 (function: %u) \n",
                   IR_meta->opcode, IR_meta->reg_11_15, IR_meta->reg_21_25, IR_meta->reg_16_20, IR_meta->function);
        else
            assert(false && "unknown R_INST");
        break;
    case EOP:
        printf("Executing EOP(%d) \n", IR_meta->opcode);
        break;
    case ADDI:
        printf("ADDI");
        printf("(%d),  $%u = $%u + #%u; \n",
               IR_meta->opcode, IR_meta->reg_16_20, IR_meta->reg_21_25, IR_meta->immediate);
        break;
    case SW:
        printf("SW");
        printf("(%d), mem[$%u + (%d)] = $%u; \n",
               IR_meta->opcode, IR_meta->reg_21_25, IR_meta->immediate, IR_meta->reg_16_20);
        break;
    case LW:
        printf("LW");
        printf("(%d), $%u = mem[$%u + (%d)];\n",
               IR_meta->opcode, IR_meta->reg_16_20, IR_meta->reg_21_25, IR_meta->immediate);
        break;
    case BEQ:
        printf("BEQ");
        printf("(%d), if $%u == $%u then pc = pc + ((%d) * 4); \n",
               IR_meta->opcode, IR_meta->reg_16_20, IR_meta->reg_21_25, IR_meta->immediate);
        break;
    case J:
        printf("JUMP");
        printf("(%d), jump  to instruction word jmp_offset %u \n",
               IR_meta->opcode, IR_meta->jmp_offset);
        break;
    default:
        assert(false && "unknown opcode");
    }
}

void assign_pipeline_registers_for_the_next_cycle()
{
    struct ctrl_signals *control = &arch_state.control;
    struct instr_meta *IR_meta = &arch_state.IR_meta;
    struct pipe_regs *curr_pipe_regs = &arch_state.curr_pipe_regs;
    struct pipe_regs *next_pipe_regs = &arch_state.next_pipe_regs;

    if (control->IRWrite)
    {
        curr_pipe_regs->IR = next_pipe_regs->IR;
        printf("PC %d: ", curr_pipe_regs->pc / 4);
        set_up_IR_meta(curr_pipe_regs->IR, IR_meta);
    }
    curr_pipe_regs->ALUOut = next_pipe_regs->ALUOut;
    curr_pipe_regs->A = next_pipe_regs->A;
    curr_pipe_regs->B = next_pipe_regs->B;
    if (control->PCWrite)
    {
        check_address_is_word_aligned(next_pipe_regs->pc);
        curr_pipe_regs->pc = next_pipe_regs->pc;
    }
    if (control->PCWriteCond && curr_pipe_regs->ALUOut == 0)
    {
        check_address_is_word_aligned(next_pipe_regs->pc);
        curr_pipe_regs->pc = next_pipe_regs->pc;
    }
}

int main(int argc, const char *argv[])
{
    /*--------------------------------------
    /------- Global Variable Init ----------
    /--------------------------------------*/
    parse_arguments(argc, argv);
    arch_state_init(&arch_state);
    ///@students WARNING: Do NOT change/move/remove main's code above this point!

    switch (task_number)
    {
    case (INSTRUCTION_TYPE):
        task_1();
        break;
    case (FINITE_STATE_MACHINE_DEC):
        task_2();
        break;
    case (FINITE_STATE_MACHINE):
        task_3();
        break;
    case (FINITE_STATE_MACHINE_EXT):
        task_4();
        break;
    case (FULL):
        task_5();
        break;
    default:
        assert(false && "No task given");
    }
}
