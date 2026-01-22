
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
    case (PIPELINED_5_STAGE):
        task_6_pipeline();
        break;
    default:
        assert(false && "No task given");
    }
}

static inline void pipeline_decode(uint32_t instr, int *opcode, int *rs, int *rt, int *rd, int *funct, int *imm, int *jump_index)
{
    *opcode = get_piece_of_a_word(instr, OPCODE_OFFSET, OPCODE_SIZE);
    *rs = get_piece_of_a_word(instr, 21, REGISTER_ID_SIZE);
    *rt = get_piece_of_a_word(instr, 16, REGISTER_ID_SIZE);
    *rd = get_piece_of_a_word(instr, 11, REGISTER_ID_SIZE);
    *funct = get_piece_of_a_word(instr, 0, 6);
    *imm = get_sign_extended_imm_id(instr, IMMEDIATE_OFFSET);
    *jump_index = get_piece_of_a_word(instr, 0, 26);
}

static inline int pipeline_alu(int op, int a, int b)
{
    switch (op)
    {
    case 0:
        return a + b;
    case 1:
        return a - b;
    case 2:
        return a < b ? 1 : 0;
    default:
        assert(false && "unknown ALU op");
    }
}

static inline int pipeline_writes_reg(const struct id_ex_reg *r)
{
    return r->valid && r->RegWrite && !r->is_halt;
}

static inline int pipeline_reg_id_from_id_ex(const struct id_ex_reg *r)
{
    if (!pipeline_writes_reg(r))
        return 0;
    return r->RegDst ? r->rd : r->rt;
}

void task_6_pipeline()
{
    FILE *output = fopen("task_6.out", "w");
    fprintf(output, "cycle,pc,if_id_valid,if_id_instr,id_ex_valid,id_ex_instr,ex_mem_valid,ex_mem_instr,mem_wb_valid,mem_wb_instr,stall,flush\n");
    fflush(output);

    arch_state.pipeline_stop_fetch = 0;

    while (true)
    {
        int stall = 0;
        int flush = 0;
        int stop_fetch_next = arch_state.pipeline_stop_fetch;

        if (arch_state.mem_wb.valid && arch_state.mem_wb.RegWrite && arch_state.mem_wb.dest_reg != 0 && !arch_state.mem_wb.is_halt)
        {
            int write_data = arch_state.mem_wb.MemtoReg ? arch_state.mem_wb.mem_data : arch_state.mem_wb.alu_result;
            arch_state.registers[arch_state.mem_wb.dest_reg] = write_data;
        }

        struct mem_wb_reg next_mem_wb = {0};
        if (arch_state.ex_mem.valid)
        {
            next_mem_wb.valid = 1;
            next_mem_wb.instr = arch_state.ex_mem.instr;
            next_mem_wb.dest_reg = arch_state.ex_mem.dest_reg;
            next_mem_wb.RegWrite = arch_state.ex_mem.RegWrite;
            next_mem_wb.MemtoReg = arch_state.ex_mem.MemtoReg;
            next_mem_wb.alu_result = arch_state.ex_mem.alu_result;
            next_mem_wb.is_halt = arch_state.ex_mem.is_halt;

            if (arch_state.ex_mem.MemRead)
                next_mem_wb.mem_data = memory_read(arch_state.ex_mem.alu_result);
            if (arch_state.ex_mem.MemWrite)
                memory_write(arch_state.ex_mem.alu_result, arch_state.ex_mem.store_data);
        }

        struct ex_mem_reg next_ex_mem = {0};
        int redirect_pc = 0;
        int redirect_target = 0;

        if (arch_state.id_ex.valid)
        {
            int rs_val = arch_state.id_ex.rs_val;
            int rt_val = arch_state.id_ex.rt_val;

            if (arch_state.ex_mem.valid && arch_state.ex_mem.RegWrite && !arch_state.ex_mem.MemtoReg && arch_state.ex_mem.dest_reg != 0 && arch_state.ex_mem.dest_reg == arch_state.id_ex.rs)
                rs_val = arch_state.ex_mem.alu_result;
            else if (arch_state.mem_wb.valid && arch_state.mem_wb.RegWrite && arch_state.mem_wb.dest_reg != 0 && arch_state.mem_wb.dest_reg == arch_state.id_ex.rs)
                rs_val = arch_state.mem_wb.MemtoReg ? arch_state.mem_wb.mem_data : arch_state.mem_wb.alu_result;

            if (arch_state.ex_mem.valid && arch_state.ex_mem.RegWrite && !arch_state.ex_mem.MemtoReg && arch_state.ex_mem.dest_reg != 0 && arch_state.ex_mem.dest_reg == arch_state.id_ex.rt)
                rt_val = arch_state.ex_mem.alu_result;
            else if (arch_state.mem_wb.valid && arch_state.mem_wb.RegWrite && arch_state.mem_wb.dest_reg != 0 && arch_state.mem_wb.dest_reg == arch_state.id_ex.rt)
                rt_val = arch_state.mem_wb.MemtoReg ? arch_state.mem_wb.mem_data : arch_state.mem_wb.alu_result;

            int alu_b = arch_state.id_ex.ALUSrc ? arch_state.id_ex.imm : rt_val;
            int alu_result = pipeline_alu(arch_state.id_ex.ALUOp, rs_val, alu_b);
            int dest_reg = pipeline_reg_id_from_id_ex(&arch_state.id_ex);

            int branch_taken = arch_state.id_ex.Branch && (rs_val == rt_val);
            int branch_target = arch_state.id_ex.pc_plus4 + (arch_state.id_ex.imm << 2);
            int jump_taken = arch_state.id_ex.Jump;
            int jump_target = (arch_state.id_ex.pc_plus4 & 0xF0000000) | (arch_state.id_ex.jump_index << 2);

            if (branch_taken)
            {
                redirect_pc = 1;
                redirect_target = branch_target;
                flush = 1;
            }
            else if (jump_taken)
            {
                redirect_pc = 1;
                redirect_target = jump_target;
                flush = 1;
            }

            next_ex_mem.valid = 1;
            next_ex_mem.instr = arch_state.id_ex.instr;
            next_ex_mem.alu_result = alu_result;
            next_ex_mem.store_data = rt_val;
            next_ex_mem.dest_reg = dest_reg;
            next_ex_mem.RegWrite = arch_state.id_ex.RegWrite;
            next_ex_mem.MemRead = arch_state.id_ex.MemRead;
            next_ex_mem.MemWrite = arch_state.id_ex.MemWrite;
            next_ex_mem.MemtoReg = arch_state.id_ex.MemtoReg;
            next_ex_mem.is_halt = arch_state.id_ex.is_halt;
        }

        struct id_ex_reg next_id_ex = {0};
        if (arch_state.if_id.valid)
        {
            int opcode = 0, rs = 0, rt = 0, rd = 0, funct = 0, imm = 0, jump_index = 0;
            pipeline_decode(arch_state.if_id.instr, &opcode, &rs, &rt, &rd, &funct, &imm, &jump_index);

            int id_uses_rs = (opcode != J) && (opcode != EOP);
            int id_uses_rt = (opcode == R_INST) || (opcode == SW) || (opcode == BEQ);

            int load_dest = pipeline_reg_id_from_id_ex(&arch_state.id_ex);
            if (arch_state.id_ex.valid && arch_state.id_ex.MemRead && load_dest != 0)
            {
                if ((id_uses_rs && load_dest == rs) || (id_uses_rt && load_dest == rt))
                    stall = 1;
            }

            if (!stall && !flush)
            {
                next_id_ex.valid = 1;
                next_id_ex.instr = arch_state.if_id.instr;
                next_id_ex.pc_plus4 = arch_state.if_id.pc_plus4;
                next_id_ex.rs = rs;
                next_id_ex.rt = rt;
                next_id_ex.rd = rd;
                next_id_ex.rs_val = arch_state.registers[rs];
                next_id_ex.rt_val = arch_state.registers[rt];
                next_id_ex.imm = imm;
                next_id_ex.opcode = opcode;
                next_id_ex.funct = funct;
                next_id_ex.jump_index = jump_index;

                if (opcode == R_INST)
                {
                    next_id_ex.RegWrite = 1;
                    next_id_ex.RegDst = 1;
                    next_id_ex.ALUSrc = 0;
                    next_id_ex.MemtoReg = 0;
                    if (funct == ADD)
                        next_id_ex.ALUOp = 0;
                    else if (funct == SLT)
                        next_id_ex.ALUOp = 2;
                    else
                        assert(false && "unsupported R-type funct");
                }
                else if (opcode == ADDI)
                {
                    next_id_ex.RegWrite = 1;
                    next_id_ex.RegDst = 0;
                    next_id_ex.ALUSrc = 1;
                    next_id_ex.ALUOp = 0;
                }
                else if (opcode == LW)
                {
                    next_id_ex.RegWrite = 1;
                    next_id_ex.RegDst = 0;
                    next_id_ex.ALUSrc = 1;
                    next_id_ex.ALUOp = 0;
                    next_id_ex.MemRead = 1;
                    next_id_ex.MemtoReg = 1;
                }
                else if (opcode == SW)
                {
                    next_id_ex.RegWrite = 0;
                    next_id_ex.RegDst = 0;
                    next_id_ex.ALUSrc = 1;
                    next_id_ex.ALUOp = 0;
                    next_id_ex.MemWrite = 1;
                }
                else if (opcode == BEQ)
                {
                    next_id_ex.Branch = 1;
                    next_id_ex.ALUSrc = 0;
                    next_id_ex.ALUOp = 1;
                }
                else if (opcode == J)
                {
                    next_id_ex.Jump = 1;
                }
                else if (opcode == EOP)
                {
                    next_id_ex.is_halt = 1;
                    stop_fetch_next = 1;
                }
                else
                {
                    assert(false && "unsupported opcode");
                }
            }
        }

        struct if_id_reg next_if_id = arch_state.if_id;
        int next_pc = arch_state.curr_pipe_regs.pc;

        if (redirect_pc)
        {
            next_pc = redirect_target;
            next_if_id.valid = 0;
        }
        else if (!stall)
        {
            if (!stop_fetch_next)
            {
                uint32_t fetched = (uint32_t)memory_read(arch_state.curr_pipe_regs.pc);
                next_if_id.instr = fetched;
                next_if_id.pc_plus4 = arch_state.curr_pipe_regs.pc + 4;
                next_if_id.valid = 1;
                next_pc = arch_state.curr_pipe_regs.pc + 4;
            }
            else
            {
                next_if_id.valid = 0;
            }
        }

        if (stall)
            next_if_id = arch_state.if_id;

        if (flush)
        {
            next_if_id.valid = 0;
            next_id_ex.valid = 0;
        }

        fprintf(output, "%" PRIu64 ",%d,%d,%u,%d,%u,%d,%u,%d,%u,%d,%d\n",
                arch_state.clock_cycle,
                arch_state.curr_pipe_regs.pc,
                arch_state.if_id.valid, arch_state.if_id.instr,
                arch_state.id_ex.valid, arch_state.id_ex.instr,
                arch_state.ex_mem.valid, arch_state.ex_mem.instr,
                arch_state.mem_wb.valid, arch_state.mem_wb.instr,
                stall, flush);
        fflush(output);

        arch_state.mem_wb = next_mem_wb;
        arch_state.ex_mem = next_ex_mem;
        arch_state.id_ex = next_id_ex;
        arch_state.if_id = next_if_id;
        arch_state.curr_pipe_regs.pc = next_pc;
        arch_state.pipeline_stop_fetch = stop_fetch_next;

        arch_state.clock_cycle++;

        if (arch_state.pipeline_stop_fetch &&
            !arch_state.if_id.valid &&
            !arch_state.id_ex.valid &&
            !arch_state.ex_mem.valid &&
            !arch_state.mem_wb.valid)
        {
            printf("Exiting because the exit state was reached \n");
            break;
        }

        if (arch_state.clock_cycle == BREAK_POINT)
        {
            printf("Exiting because the break point (%u) was reached \n", BREAK_POINT);
            break;
        }
    }

    fclose(output);
}
