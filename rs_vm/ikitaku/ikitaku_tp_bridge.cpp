/* Project Toplethos 1.2 three-address ops onto the September rsvm code[]
   so ikitaku_analyze (span_has_op / region walks) still runs.
   Mapping is lossy on purpose: the reasoner only needs the verbs. */
#include "ikitaku_tp_bridge.hpp"
#include <string.h>

static int emit(rsvm_t* vm, uint8_t op) {
    if (vm->code_len >= RSVM_MAX_CODE) return -1;
    vm->code[vm->code_len++] = op;
    return 0;
}

static int emit_region(rsvm_t* vm, uint16_t start, uint8_t kind) {
    if (vm->region_count >= RSVM_MAX_REGIONS) return 0;
    rsvm_region_t* r = &vm->regions[vm->region_count++];
    r->start = start;
    r->end = vm->code_len;
    r->kind = kind;
    r->flags = 0;
    return 0;
}

int ikitaku_tp_fill(rsvm_t* vm, const iki_tp_insn_t* ins, int n) {
    if (!vm || !ins || n < 0) return -1;
    memset(vm, 0, sizeof *vm);
    vm->step_limit = RSVM_MAX_STEPS;

    uint16_t loop_start = 0;
    int in_loop = 0;

    for (int i = 0; i < n; ++i) {
        const iki_tp_insn_t* in = &ins[i];
        uint8_t op = RSVM_OP_NOP;
        switch (in->op) {
        case IKI_TP_Imm:        op = RSVM_OP_LDI; break;
        case IKI_TP_Mov:        op = RSVM_OP_MOV; break;
        case IKI_TP_Add:        op = RSVM_OP_ADD; break;
        case IKI_TP_Sub:        op = RSVM_OP_SUB; break;
        case IKI_TP_Mul:        op = RSVM_OP_MUL; break;
        case IKI_TP_Div:        op = RSVM_OP_DIV; break;
        case IKI_TP_Rem:        op = RSVM_OP_MOD; break;
        case IKI_TP_And:        op = RSVM_OP_BAND; break;
        case IKI_TP_Or:         op = RSVM_OP_BOR; break;
        case IKI_TP_Xor:        op = RSVM_OP_BXOR; break;
        case IKI_TP_Shl:        op = RSVM_OP_SHL; break;
        case IKI_TP_Shr:        op = RSVM_OP_SHR; break;
        case IKI_TP_Eq:         op = RSVM_OP_EQ; break;
        case IKI_TP_Ne:         op = RSVM_OP_NE; break;
        case IKI_TP_Lt:         op = RSVM_OP_LT; break;
        case IKI_TP_Le:         op = RSVM_OP_LE; break;
        case IKI_TP_Gt:         op = RSVM_OP_GT; break;
        case IKI_TP_Ge:         op = RSVM_OP_GE; break;
        case IKI_TP_Not:        op = RSVM_OP_NOT; break;
        case IKI_TP_Neg:        op = RSVM_OP_NEG; break;
        case IKI_TP_BitNot:     op = RSVM_OP_BNOT; break;
        case IKI_TP_Jmp:
            op = RSVM_OP_JMP;
            if (in->imm < (uint64_t)vm->code_len) {
                /* back edge: counted loop in the old image */
                if (emit(vm, RSVM_OP_TRAP_LOOP) < 0) return -1;
                emit_region(vm, (uint16_t)in->imm, RSVM_REG_TRAP);
                in_loop = 0;
            }
            break;
        case IKI_TP_Br:
        case IKI_TP_BrRange:
            op = RSVM_OP_JZ;
            if (!in_loop) { loop_start = vm->code_len; in_loop = 1; }
            break;
        case IKI_TP_Label:      op = RSVM_OP_NOP; break;
        case IKI_TP_Ret:        op = RSVM_OP_RET0; break;
        case IKI_TP_Call:       op = RSVM_OP_CALL; break;
        case IKI_TP_Load:       op = RSVM_OP_LOAD; break;
        case IKI_TP_Store:      op = RSVM_OP_STORE; break;
        case IKI_TP_Addr:       op = RSVM_OP_LOAD; break;
        case IKI_TP_Print:      op = RSVM_OP_PRINT_V; break;
        case IKI_TP_Trap:       op = RSVM_OP_HALT; break;
        case IKI_TP_Clock:
        case IKI_TP_Watch:      op = RSVM_OP_DELAY_MS; break;
        case IKI_TP_LoadBmp:
        case IKI_TP_LoadBytes:  op = RSVM_OP_FREAD; break;
        case IKI_TP_SaveBmp:    op = RSVM_OP_FWRITE; break;
        case IKI_TP_Fork:
        case IKI_TP_Start:      op = RSVM_OP_NOP; break;
        case IKI_TP_Lock:
        case IKI_TP_Unlock:     op = RSVM_OP_PROP_CHK; break;
        case IKI_TP_Pow:        op = RSVM_OP_MUL; break;
        case IKI_TP_FloorDiv:   op = RSVM_OP_DIV; break;
        case IKI_TP_Cast:       op = RSVM_OP_CONV; break;
        case IKI_TP_State:      op = RSVM_OP_LOAD; break;
        case IKI_TP_SlotAdd:    op = RSVM_OP_ADD_IMM_SLOT; break;
        case IKI_TP_StaticInit: op = RSVM_OP_NOP; break;
        case IKI_TP_Threads:
        case IKI_TP_Freemem:    op = RSVM_OP_PUSHI; break;
        default:                op = RSVM_OP_NOP; break;
        }
        if (emit(vm, op) < 0) return -1;
    }
    if (in_loop) emit_region(vm, loop_start, RSVM_REG_LOOP);
    vm->entry = 0;
    return (int)vm->code_len;
}

#ifdef IKITAKU_HAVE_TP
int ikitaku_tp_fill_program(rsvm_t* vm, const tp::Program& prog) {
    iki_tp_insn_t buf[RSVM_MAX_CODE];
    int n = 0;
    for (const auto& fn : prog.fns) {
        for (const auto& in : fn.code) {
            if (n >= RSVM_MAX_CODE) break;
            buf[n].op = (uint16_t)in.op;
            buf[n].dst = in.dst;
            buf[n].lhs = in.lhs;
            buf[n].rhs = in.rhs;
            buf[n].imm = in.imm;
            buf[n].line = in.line;
            ++n;
        }
    }
    return ikitaku_tp_fill(vm, buf, n);
}
#endif
