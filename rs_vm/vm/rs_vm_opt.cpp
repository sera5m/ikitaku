// rs_vm_opt — multi-pass optimizer + pure-math loop hacking (f(x) / feed chains)
//
// When @enable_algo_rewrite is on, trapdoor bodies that are pure arithmetic
// (no I/O, no bytes, no host calls) are candidates for closed-form reduction:
//   x := x + c        →  x += n*c
//   x := a*x + b      →  affine closed form (a^n)
//   s := s + t; t++   →  dimension-reduced feed chain
//
#include "rs_vm_opt.hpp"
#include "rs_vm_opcodes.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    int32_t slot;
    int32_t scale;
    int32_t bias;
    int32_t aux;
    int     conf;
} rsvm_match_t;

typedef struct {
    const char* name;
    const char* from_big_o;
    const char* to_big_o;
    int         min_confidence;
    int (*match)(const rsvm_t* vm, uint16_t trap_pc, uint16_t body, uint16_t blen, rsvm_match_t* out);
    int (*apply)(rsvm_t* vm, uint16_t trap_pc, uint16_t body, uint16_t blen, const rsvm_match_t* m);
} rsvm_algo_rule_t;

static int op_is_impure(uint8_t op) {
    switch (op) {
    case RSVM_OP_PRINT_S: case RSVM_OP_PRINT_I: case RSVM_OP_PRINT_STR:
    case RSVM_OP_PRINT_V: case RSVM_OP_PRINT_C: case RSVM_OP_PRINT_NL:
    case RSVM_OP_DELAY_MS: case RSVM_OP_PIN_MODE: case RSVM_OP_DIG_WR:
    case RSVM_OP_DIG_RD: case RSVM_OP_ADC_RD: case RSVM_OP_MILLIS:
    case RSVM_OP_GPIO_RD: case RSVM_OP_GPIO_WR: case RSVM_OP_ADC_RDP:
    case RSVM_OP_FREAD: case RSVM_OP_FWRITE:
    case RSVM_OP_SYS_CMD: case RSVM_OP_SYS_EXEC: case RSVM_OP_SYS_OPEN:
    case RSVM_OP_SYS_LS: case RSVM_OP_SYS_CD: case RSVM_OP_SYS_PWD:
    case RSVM_OP_MW_TEXT: case RSVM_OP_UART_SEND: case RSVM_OP_POOL_OP:
    case RSVM_OP_SYS_ENV: case RSVM_OP_SYS_GREP: case RSVM_OP_SYS_CAT:
    case RSVM_OP_SYS_ECHO: case RSVM_OP_SYS_HEAD: case RSVM_OP_SYS_WC:
    case RSVM_OP_SYSCONF_GET: case RSVM_OP_SYSCONF_SET:
    case RSVM_OP_NATIVE: case RSVM_OP_CALL: case RSVM_OP_RET: case RSVM_OP_RET0:
    case RSVM_OP_NEW: case RSVM_OP_FIELD_LD: case RSVM_OP_FIELD_ST:
    case RSVM_OP_ARR_NEW: case RSVM_OP_ARR_LD: case RSVM_OP_ARR_ST:
    case RSVM_OP_ARR_NEWD: case RSVM_OP_ARR_LDN: case RSVM_OP_ARR_STN:
    case RSVM_OP_ARR_LEN: case RSVM_OP_ARR_PUSH: case RSVM_OP_ARR_PACK:
    case RSVM_OP_ARR_SWAP: case RSVM_OP_ARR_POP:
    case RSVM_OP_STR_LIT: case RSVM_OP_STR_CHAR: case RSVM_OP_STR_LEN:
    case RSVM_OP_JMP: case RSVM_OP_JZ: case RSVM_OP_JNZ:
    case RSVM_OP_TRAP_LOOP: case RSVM_OP_BREAK: case RSVM_OP_CONTINUE:
    case RSVM_OP_SWITCH_JT:
        return 1;
    default:
        return 0;
    }
}

static int body_is_pure_math(const uint8_t* c, uint16_t body, uint16_t blen) {
    uint16_t end = (uint16_t)(body + blen);
    uint16_t p = body;
    while (p < end) {
        uint8_t op = c[p++];
        if (op_is_impure(op)) return 0;
        switch (op) {
        case RSVM_OP_PUSHI:
            if (p + 4 > end) return 0; p = (uint16_t)(p + 4); break;
        case RSVM_OP_ADD_IMM_SLOT:
            if (p >= end) return 0; p++;
            if (p + 4 > end) return 0; p = (uint16_t)(p + 4); break;
        case RSVM_OP_LDI:
            if (p >= end) return 0; p++;
            if (p + 4 > end) return 0; p = (uint16_t)(p + 4); break;
        case RSVM_OP_LOAD: case RSVM_OP_STORE: case RSVM_OP_INC_SLOT:
            if (p >= end) return 0; p++; break;
        case RSVM_OP_MOV:
            if (p + 2 > end) return 0; p = (uint16_t)(p + 2); break;
        case RSVM_OP_LOAD_ADD_STORE:
            if (p + 3 > end) return 0; p = (uint16_t)(p + 3); break;
        case RSVM_OP_LOAD_PUSHI:
            if (p >= end) return 0; p++;
            if (p + 4 > end) return 0; p = (uint16_t)(p + 4); break;
        case RSVM_OP_PACK2:
            if (p + 2 > end) return 0; p = (uint16_t)(p + 2); break;
        case RSVM_OP_PUSH_TY:
            if (p + 6 > end) return 0; p = (uint16_t)(p + 6); break;
        default:
            break;
        }
    }
    return 1;
}

static int32_t rd_i32_at(const uint8_t* c, uint16_t p) {
    return (int32_t)c[p] | ((int32_t)c[p+1]<<8) | ((int32_t)c[p+2]<<16) | ((int32_t)c[p+3]<<24);
}

static void wr_i32_at(uint8_t* c, uint16_t p, int32_t v) {
    c[p] = (uint8_t)(v & 0xFF);
    c[p+1] = (uint8_t)((v >> 8) & 0xFF);
    c[p+2] = (uint8_t)((v >> 16) & 0xFF);
    c[p+3] = (uint8_t)((v >> 24) & 0xFF);
}

static int is_inc_slot_at(const uint8_t* c, uint16_t pc, uint16_t end, uint8_t* slot_out) {
    if (pc + 2 > end) return 0;
    if (c[pc] == RSVM_OP_INC_SLOT) {
        if (slot_out) *slot_out = c[pc + 1];
        return 2;
    }
    if (pc + 10 <= end && c[pc] == RSVM_OP_LOAD && c[pc+2] == RSVM_OP_PUSHI
        && c[pc+7] == RSVM_OP_ADD && c[pc+8] == RSVM_OP_STORE
        && c[pc+1] == c[pc+9] && rd_i32_at(c, (uint16_t)(pc+3)) == 1) {
        if (slot_out) *slot_out = c[pc+1];
        return 10;
    }
    return 0;
}

static int is_add_imm_slot_at(const uint8_t* c, uint16_t pc, uint16_t end,
                             uint8_t* slot_out, int32_t* imm_out) {
    if (pc + 6 <= end && c[pc] == RSVM_OP_ADD_IMM_SLOT) {
        if (slot_out) *slot_out = c[pc+1];
        if (imm_out) *imm_out = rd_i32_at(c, (uint16_t)(pc+2));
        return 6;
    }
    if (pc + 10 <= end && c[pc] == RSVM_OP_LOAD && c[pc+2] == RSVM_OP_PUSHI
        && c[pc+7] == RSVM_OP_ADD && c[pc+8] == RSVM_OP_STORE
        && c[pc+1] == c[pc+9]) {
        if (slot_out) *slot_out = c[pc+1];
        if (imm_out) *imm_out = rd_i32_at(c, (uint16_t)(pc+3));
        return 10;
    }
    return 0;
}

static int is_affine_slot_at(const uint8_t* c, uint16_t pc, uint16_t end,
                            uint8_t* slot_out, int32_t* a_out, int32_t* b_out) {
    if (pc + 16 <= end
        && c[pc] == RSVM_OP_LOAD && c[pc+2] == RSVM_OP_PUSHI
        && c[pc+7] == RSVM_OP_MUL && c[pc+8] == RSVM_OP_PUSHI
        && c[pc+13] == RSVM_OP_ADD && c[pc+14] == RSVM_OP_STORE
        && c[pc+1] == c[pc+15]) {
        if (slot_out) *slot_out = c[pc+1];
        if (a_out) *a_out = rd_i32_at(c, (uint16_t)(pc+3));
        if (b_out) *b_out = rd_i32_at(c, (uint16_t)(pc+9));
        return 16;
    }
    if (pc + 10 <= end
        && c[pc] == RSVM_OP_LOAD && c[pc+2] == RSVM_OP_PUSHI
        && c[pc+7] == RSVM_OP_MUL && c[pc+8] == RSVM_OP_STORE
        && c[pc+1] == c[pc+9]) {
        if (slot_out) *slot_out = c[pc+1];
        if (a_out) *a_out = rd_i32_at(c, (uint16_t)(pc+3));
        if (b_out) *b_out = 0;
        return 10;
    }
    return 0;
}

static int is_feed_acc_at(const uint8_t* c, uint16_t pc, uint16_t end,
                         uint8_t* s_out, uint8_t* t_out) {
    if (pc + 9 <= end
        && c[pc] == RSVM_OP_LOAD && c[pc+2] == RSVM_OP_LOAD
        && c[pc+4] == RSVM_OP_ADD && c[pc+5] == RSVM_OP_STORE
        && c[pc+1] == c[pc+6]
        && c[pc+7] == RSVM_OP_INC_SLOT
        && c[pc+3] == c[pc+8]) {
        if (s_out) *s_out = c[pc+1];
        if (t_out) *t_out = c[pc+3];
        return 9;
    }
    if (pc + 6 <= end
        && c[pc] == RSVM_OP_LOAD_ADD_STORE
        && c[pc+1] == c[pc+2]
        && c[pc+4] == RSVM_OP_INC_SLOT
        && c[pc+3] == c[pc+5]) {
        if (s_out) *s_out = c[pc+1];
        if (t_out) *t_out = c[pc+3];
        return 6;
    }
    return 0;
}

static int splice_code(rsvm_t* vm, uint16_t start, uint16_t end,
                       const uint8_t* repl, uint16_t new_span) {
    if (end > vm->code_len || start > end) return 0;
    uint16_t old_span = (uint16_t)(end - start);
    if (new_span > old_span) {
        if ((uint32_t)vm->code_len + (new_span - old_span) > RSVM_MAX_CODE) return 0;
        memmove(vm->code + start + new_span, vm->code + end, vm->code_len - end);
        vm->code_len = (uint16_t)(vm->code_len + (new_span - old_span));
    } else {
        uint16_t tail = (uint16_t)(vm->code_len - end);
        memmove(vm->code + start + new_span, vm->code + end, tail);
        vm->code_len = (uint16_t)(start + new_span + tail);
    }
    if (repl && new_span) memcpy(vm->code + start, repl, new_span);
    for (uint8_t i = 0; i < vm->region_count; ++i) {
        rsvm_region_t* r = &vm->regions[i];
        if (r->start >= end) {
            int32_t d = (int32_t)new_span - (int32_t)old_span;
            r->start = (uint16_t)((int32_t)r->start + d);
            r->end   = (uint16_t)((int32_t)r->end + d);
        } else if (r->end > start && r->start < end) {
            r->start = start;
            r->end = (uint16_t)(start + new_span);
            r->kind = RSVM_REG_LOOP;
        }
    }
    return 1;
}

static int try_const_count(const rsvm_t* vm, uint16_t trap_pc, int32_t* n_out) {
    if (trap_pc >= 5 && vm->code[trap_pc - 5] == RSVM_OP_PUSHI) {
        *n_out = rd_i32_at(vm->code, (uint16_t)(trap_pc - 4));
        return 1;
    }
    return 0;
}

static int match_pure_inc(const rsvm_t* vm, uint16_t trap_pc, uint16_t body, uint16_t blen, rsvm_match_t* out) {
    (void)trap_pc;
    if (!body_is_pure_math(vm->code, body, blen)) return 0;
    uint8_t slot = 0;
    int used = is_inc_slot_at(vm->code, body, (uint16_t)(body + blen), &slot);
    if (used && (uint16_t)used == blen) {
        out->slot = slot; out->scale = 1; out->bias = 1; out->aux = 0; out->conf = 92;
        return 92;
    }
    int32_t imm = 0;
    used = is_add_imm_slot_at(vm->code, body, (uint16_t)(body + blen), &slot, &imm);
    if (used && (uint16_t)used == blen) {
        out->slot = slot; out->scale = 1; out->bias = imm; out->aux = 0; out->conf = 90;
        return 90;
    }
    return 0;
}

static int apply_pure_inc(rsvm_t* vm, uint16_t trap_pc, uint16_t body, uint16_t blen, const rsvm_match_t* m) {
    (void)body;
    uint16_t end = (uint16_t)(trap_pc + 1 + 2 + blen);
    int32_t n = 0;
    if (try_const_count(vm, trap_pc, &n) && n >= 0) {
        int64_t add = (int64_t)n * (int64_t)m->bias;
        if (add <= 2147483647ll && add >= -2147483648ll) {
            uint16_t start = (uint16_t)(trap_pc - 5);
            uint8_t repl[6];
            repl[0] = RSVM_OP_ADD_IMM_SLOT;
            repl[1] = (uint8_t)m->slot;
            wr_i32_at(repl, 2, (int32_t)add);
            return splice_code(vm, start, end, repl, 6);
        }
    }
    uint8_t repl[14];
    int ri = 0;
    repl[ri++] = RSVM_OP_PUSHI;
    wr_i32_at(repl, (uint16_t)ri, m->bias); ri += 4;
    repl[ri++] = RSVM_OP_MUL;
    repl[ri++] = RSVM_OP_LOAD;
    repl[ri++] = (uint8_t)m->slot;
    repl[ri++] = RSVM_OP_ADD;
    repl[ri++] = RSVM_OP_STORE;
    repl[ri++] = (uint8_t)m->slot;
    return splice_code(vm, trap_pc, end, repl, (uint16_t)ri);
}

static int match_affine(const rsvm_t* vm, uint16_t trap_pc, uint16_t body, uint16_t blen, rsvm_match_t* out) {
    (void)trap_pc;
    if (!body_is_pure_math(vm->code, body, blen)) return 0;
    uint8_t slot = 0;
    int32_t a = 1, b = 0;
    int used = is_affine_slot_at(vm->code, body, (uint16_t)(body + blen), &slot, &a, &b);
    if (!used || (uint16_t)used != blen) return 0;
    out->slot = slot; out->scale = a; out->bias = b; out->aux = 0;
    out->conf = (a == 1) ? 80 : 75;
    return out->conf;
}

static int32_t ipow_checked(int32_t base, int32_t exp) {
    if (exp < 0) return 0;
    int64_t r = 1, b = base;
    while (exp) {
        if (exp & 1) {
            r *= b;
            if (r > 2147483647ll || r < -2147483648ll) return 0;
        }
        b *= b;
        if (b > 2147483647ll || b < -2147483648ll) {
            if (exp > 1) return 0;
        }
        exp >>= 1;
    }
    return (int32_t)r;
}

static int apply_affine(rsvm_t* vm, uint16_t trap_pc, uint16_t body, uint16_t blen, const rsvm_match_t* m) {
    if (m->scale == 1)
        return apply_pure_inc(vm, trap_pc, body, blen, m);
    uint16_t end = (uint16_t)(trap_pc + 1 + 2 + blen);
    int32_t n = 0;
    if (!try_const_count(vm, trap_pc, &n) || n < 0 || n > 40)
        return 0;
    int32_t a = m->scale, b = m->bias;
    if (a == 0) {
        uint16_t start = (uint16_t)(trap_pc - 5);
        uint8_t repl[6];
        repl[0] = RSVM_OP_LDI;
        repl[1] = (uint8_t)m->slot;
        wr_i32_at(repl, 2, (n >= 1) ? b : 0);
        return splice_code(vm, start, end, repl, 6);
    }
    int32_t an = ipow_checked(a, n);
    if (an == 0 && !(a == 0 || n == 0)) return 0;
    int64_t g = (int64_t)b * (int64_t)(an - 1) / (a - 1);
    if (g > 2147483647ll || g < -2147483648ll) return 0;
    uint16_t start = (uint16_t)(trap_pc - 5);
    uint8_t repl[16];
    int ri = 0;
    repl[ri++] = RSVM_OP_LOAD; repl[ri++] = (uint8_t)m->slot;
    repl[ri++] = RSVM_OP_PUSHI; wr_i32_at(repl, (uint16_t)ri, an); ri += 4;
    repl[ri++] = RSVM_OP_MUL;
    repl[ri++] = RSVM_OP_PUSHI; wr_i32_at(repl, (uint16_t)ri, (int32_t)g); ri += 4;
    repl[ri++] = RSVM_OP_ADD;
    repl[ri++] = RSVM_OP_STORE; repl[ri++] = (uint8_t)m->slot;
    return splice_code(vm, start, end, repl, (uint16_t)ri);
}

static int match_feed_chain(const rsvm_t* vm, uint16_t trap_pc, uint16_t body, uint16_t blen, rsvm_match_t* out) {
    (void)trap_pc;
    if (!body_is_pure_math(vm->code, body, blen)) return 0;
    uint8_t s = 0, t = 0;
    int used = is_feed_acc_at(vm->code, body, (uint16_t)(body + blen), &s, &t);
    if (!used || (uint16_t)used != blen) return 0;
    out->slot = s; out->aux = t; out->scale = 1; out->bias = 1; out->conf = 88;
    return 88;
}

static int apply_feed_chain(rsvm_t* vm, uint16_t trap_pc, uint16_t body, uint16_t blen, const rsvm_match_t* m) {
    (void)body;
    uint16_t end = (uint16_t)(trap_pc + 1 + 2 + blen);
    int32_t n = 0;
    if (!try_const_count(vm, trap_pc, &n) || n < 0) return 0;
    int64_t tri = (int64_t)n * (int64_t)(n - 1) / 2;
    if (tri > 2147483647ll || tri < -2147483648ll) return 0;
    uint16_t start = (uint16_t)(trap_pc - 5);
    uint8_t repl[32];
    int ri = 0;
    repl[ri++] = RSVM_OP_LOAD; repl[ri++] = (uint8_t)m->aux;
    repl[ri++] = RSVM_OP_PUSHI; wr_i32_at(repl, (uint16_t)ri, n); ri += 4;
    repl[ri++] = RSVM_OP_MUL;
    repl[ri++] = RSVM_OP_PUSHI; wr_i32_at(repl, (uint16_t)ri, (int32_t)tri); ri += 4;
    repl[ri++] = RSVM_OP_ADD;
    repl[ri++] = RSVM_OP_LOAD; repl[ri++] = (uint8_t)m->slot;
    repl[ri++] = RSVM_OP_ADD;
    repl[ri++] = RSVM_OP_STORE; repl[ri++] = (uint8_t)m->slot;
    repl[ri++] = RSVM_OP_ADD_IMM_SLOT; repl[ri++] = (uint8_t)m->aux;
    wr_i32_at(repl, (uint16_t)ri, n); ri += 4;
    return splice_code(vm, start, end, repl, (uint16_t)ri);
}

static const rsvm_algo_rule_t RULES[] = {
    { "pure_counted_inc", "O(n)", "O(1)", 85, match_pure_inc, apply_pure_inc },
    { "affine_fx",        "O(n)", "O(1)", 70, match_affine,   apply_affine },
    { "feed_chain_sum",   "O(n)", "O(1)", 80, match_feed_chain, apply_feed_chain },
};
static const int RULE_COUNT = 3;

const char* rsvm_estimate_big_o(const rsvm_t* vm, uint16_t body_start, uint16_t body_end) {
    if (!vm || body_end <= body_start) return "O(1)";
    uint16_t blen = (uint16_t)(body_end - body_start);
    if (!body_is_pure_math(vm->code, body_start, blen)) return "O(n)+IO";
    int traps = 0;
    for (uint16_t p = body_start; p < body_end; ++p)
        if (vm->code[p] == RSVM_OP_TRAP_LOOP) traps++;
    if (traps >= 2) return "O(n^2)";
    if (traps == 1) return "O(n*m)";
    uint8_t slot;
    if (is_inc_slot_at(vm->code, body_start, body_end, &slot) == (int)blen)
        return "O(n) pure-inc";
    int32_t a, b;
    if (is_affine_slot_at(vm->code, body_start, body_end, &slot, &a, &b) == (int)blen)
        return "O(n) affine";
    uint8_t s, t;
    if (is_feed_acc_at(vm->code, body_start, body_end, &s, &t) == (int)blen)
        return "O(n) feed";
    return "O(n)";
}

static void log_msg(rsvm_t* vm, const char* msg) {
    if (!vm || !msg) return;
    size_t cur = strlen(vm->opt_log);
    size_t room = sizeof(vm->opt_log) - cur - 1;
    if (room < 8) return;
    snprintf(vm->opt_log + cur, room, "%s", msg);
}

static int pass_superop_residue(rsvm_t* vm) { (void)vm; return 0; }

static int is_pow2(int32_t v, int* shift_out) {
    if (v <= 0) return 0;
    uint32_t u = (uint32_t)v;
    if ((u & (u - 1)) != 0) return 0;
    int s = 0;
    while (u > 1) { u >>= 1; s++; }
    if (shift_out) *shift_out = s;
    return 1;
}

static int pass_const_fold(rsvm_t* vm) {
    if (!vm || vm->code_len < 6) return 0;
    int changed = 0;
    uint8_t* c = vm->code;
    for (uint16_t p = 0; p + 11 <= vm->code_len; ++p) {
        if (c[p] == RSVM_OP_PUSHI && c[p+5] == RSVM_OP_PUSHI && c[p+10] == RSVM_OP_ADD) {
            int32_t a = rd_i32_at(c, (uint16_t)(p+1));
            int32_t b = rd_i32_at(c, (uint16_t)(p+6));
            c[p] = RSVM_OP_PUSHI;
            wr_i32_at(c, (uint16_t)(p+1), a + b);
            for (int k = 5; k < 11; ++k) c[p+k] = RSVM_OP_NOP;
            changed++;
            p = (uint16_t)(p + 4);
        }
    }
    // Strength reduction: … ; PUSHI 2^k ; MUL  →  PUSHI k ; SHL
    // Pattern is 5-byte PUSHI + 1-byte MUL. Value on stack stays.
    for (uint16_t p = 0; p + 6 <= vm->code_len; ++p) {
        if (c[p] == RSVM_OP_PUSHI && c[p+5] == RSVM_OP_MUL) {
            int32_t imm = rd_i32_at(c, (uint16_t)(p+1));
            int sh = 0;
            if (is_pow2(imm, &sh) && sh > 0 && sh < 31) {
                wr_i32_at(c, (uint16_t)(p+1), sh);
                c[p+5] = RSVM_OP_SHL;
                changed++;
                if (vm->superoptimize) {
                    char buf[80];
                    snprintf(buf, sizeof buf, "[super] strength *%d → <<%d\n", (int)imm, sh);
                    log_msg(vm, buf);
                }
            }
        }
        // PUSHI 2^k ; DIV → PUSHI k ; SHR (unsigned-ish; signed ok for non-neg)
        if (c[p] == RSVM_OP_PUSHI && c[p+5] == RSVM_OP_DIV) {
            int32_t imm = rd_i32_at(c, (uint16_t)(p+1));
            int sh = 0;
            if (is_pow2(imm, &sh) && sh > 0 && sh < 31) {
                wr_i32_at(c, (uint16_t)(p+1), sh);
                c[p+5] = RSVM_OP_SHR;
                changed++;
                if (vm->superoptimize) {
                    char buf[80];
                    snprintf(buf, sizeof buf, "[super] strength /%d → >>%d\n", (int)imm, sh);
                    log_msg(vm, buf);
                }
            }
        }
        // PUSHI 2^k ; MOD → PUSHI (2^k-1) ; BAND  (trig phase wrap, ring buffers)
        if (c[p] == RSVM_OP_PUSHI && c[p+5] == RSVM_OP_MOD) {
            int32_t imm = rd_i32_at(c, (uint16_t)(p+1));
            int sh = 0;
            if (is_pow2(imm, &sh) && sh > 0 && sh < 31) {
                wr_i32_at(c, (uint16_t)(p+1), imm - 1);
                c[p+5] = RSVM_OP_BAND;
                changed++;
                if (vm->superoptimize) {
                    char buf[96];
                    snprintf(buf, sizeof buf, "[super] strength %% %d → & %d (wrap/trig)\n",
                             (int)imm, (int)(imm - 1));
                    log_msg(vm, buf);
                }
            }
        }
    }
    return changed;
}

// Superoptimize: chain-deposit (STORE s; LOAD s → drop both when pure),
// and log readiness for ISQRT / trig wrap when patterns appear.
static int pass_superoptimize(rsvm_t* vm) {
    if (!vm || !vm->superoptimize) return 0;
    int changed = 0;
    uint8_t* c = vm->code;
    // STORE s ; LOAD s  →  DUP was better but we can NOP the pair if stack top
    // is the value just stored (common after assignment before read).
    // Encoding: STORE u8 ; LOAD u8  with same slot → replace with NOP NOP NOP
    // only when nothing between them.
    for (uint16_t p = 0; p + 4 <= vm->code_len; ++p) {
        if (c[p] == RSVM_OP_STORE && c[p+2] == RSVM_OP_LOAD && c[p+1] == c[p+3]) {
            // Keep STORE (side effect on slot), turn LOAD into re-push via LOAD —
            // better: STORE; LOAD same = value still needed: leave as-is OR
            // replace LOAD with DUP if we had left value on stack — STORE pops.
            // So true elim needs producer not to STORE. Skip unsafe cases.
            continue;
        }
        // LOAD s ; STORE s  (noop) → NOP
        if (c[p] == RSVM_OP_LOAD && c[p+2] == RSVM_OP_STORE && c[p+1] == c[p+3]) {
            c[p] = RSVM_OP_NOP; c[p+1] = RSVM_OP_NOP;
            c[p+2] = RSVM_OP_NOP; c[p+3] = RSVM_OP_NOP;
            changed++;
            log_msg(vm, "[super] elim LOAD/STORE same-slot noop\n");
        }
    }
    // Count pure trap regions for debug
    int pure_traps = 0;
    for (uint16_t p = 0; p + 3 < vm->code_len; ++p) {
        if (c[p] != RSVM_OP_TRAP_LOOP) continue;
        uint16_t blen = (uint16_t)c[p+1] | ((uint16_t)c[p+2] << 8);
        uint16_t body = (uint16_t)(p + 3);
        if ((uint32_t)body + blen > vm->code_len) continue;
        if (body_is_pure_math(c, body, blen)) pure_traps++;
    }
    if (pure_traps) {
        char buf[96];
        snprintf(buf, sizeof buf, "[super] pure_math trap regions=%d (eligible for f^n)\n", pure_traps);
        log_msg(vm, buf);
    }
    return changed;
}

static int pass_pack2(rsvm_t* vm) {
    if (!vm || !vm->packed_instructions) return 0;
    uint8_t* c = vm->code;
    int changed = 0;
    for (uint16_t p = 0; p + 2 < vm->code_len; ++p) {
        uint8_t a = c[p], b = c[p+1];
        int oka = 0, okb = 0;
        switch (a) {
        case RSVM_OP_POP: case RSVM_OP_DUP: case RSVM_OP_ADD: case RSVM_OP_SUB:
        case RSVM_OP_MUL: case RSVM_OP_NEG: case RSVM_OP_NOT: case RSVM_OP_NOP: oka = 1; break;
        default: break;
        }
        switch (b) {
        case RSVM_OP_POP: case RSVM_OP_DUP: case RSVM_OP_ADD: case RSVM_OP_SUB:
        case RSVM_OP_MUL: case RSVM_OP_NEG: case RSVM_OP_NOT: case RSVM_OP_NOP: okb = 1; break;
        default: break;
        }
        if (oka && okb && c[p+2] == RSVM_OP_NOP) {
            c[p] = RSVM_OP_PACK2; c[p+1] = a; c[p+2] = b;
            changed++;
            p = (uint16_t)(p + 2);
        }
    }
    return changed;
}

static int pass_algo_rewrite(rsvm_t* vm) {
    if (!vm || !vm->enable_algo_rewrite) return 0;
    int changed = 0;
    for (uint16_t p = 0; p + 3 < vm->code_len; ++p) {
        if (vm->code[p] != RSVM_OP_TRAP_LOOP) continue;
        uint16_t blen = (uint16_t)vm->code[p+1] | ((uint16_t)vm->code[p+2] << 8);
        uint16_t body = (uint16_t)(p + 3);
        if ((uint32_t)body + blen > vm->code_len) continue;

        int pure = body_is_pure_math(vm->code, body, blen);
        const char* big_o = rsvm_estimate_big_o(vm, body, (uint16_t)(body + blen));
        if (!pure) {
            char buf[96];
            snprintf(buf, sizeof buf, "[algo] skip impure body big_o=%s\n", big_o);
            log_msg(vm, buf);
            continue;
        }

        for (int r = 0; r < RULE_COUNT; ++r) {
            rsvm_match_t m;
            memset(&m, 0, sizeof m);
            int conf = RULES[r].match(vm, p, body, blen, &m);
            if (conf < RULES[r].min_confidence) {
                if (conf > 0) {
                    char buf[160];
                    snprintf(buf, sizeof buf, "[algo] reject %s conf=%d<%d (%s)\n",
                             RULES[r].name, conf, RULES[r].min_confidence, big_o);
                    log_msg(vm, buf);
                }
                continue;
            }
            char buf[192];
            snprintf(buf, sizeof buf,
                     "[algo] suggest %s: %s → %s conf=%d slot=%d scale=%d bias=%d — trying\n",
                     RULES[r].name, RULES[r].from_big_o, RULES[r].to_big_o,
                     conf, (int)m.slot, (int)m.scale, (int)m.bias);
            log_msg(vm, buf);

            uint16_t old_len = vm->code_len;
            uint8_t* snap = (uint8_t*)malloc(old_len);
            if (!snap) continue;
            memcpy(snap, vm->code, old_len);

            int ok = RULES[r].apply(vm, p, body, blen, &m);
            if (!ok) {
                memcpy(vm->code, snap, old_len);
                vm->code_len = old_len;
                log_msg(vm, "[algo] trial failed — reverted\n");
                free(snap);
                continue;
            }
            free(snap);
            snprintf(buf, sizeof buf, "[algo] applied %s (conf=%d) dim-reduce ok\n",
                     RULES[r].name, conf);
            log_msg(vm, buf);
            changed++;
            p = 0;
            break;
        }
    }
    return changed;
}

void rsvm_optimize(rsvm_t* vm) {
    if (!vm) return;
    vm->opt_log[0] = 0;
    int max = vm->opt_passes;
    if (max > 4) max = 4;
    // Superoptimize implies at least pass 2 (strength) + 4 (algo) when enabled alone
    if (vm->superoptimize && max < 2) max = 2;
    vm->opt_applied = 0;
    for (int pass = 1; pass <= max; ++pass) {
        int ch = 0;
        if (pass == 1) ch = pass_superop_residue(vm);
        else if (pass == 2) ch = pass_const_fold(vm);
        else if (pass == 3) ch = pass_pack2(vm);
        else if (pass == 4) {
            ch = pass_algo_rewrite(vm);
            if (vm->superoptimize) ch += pass_superoptimize(vm);
        }
        vm->opt_applied = (uint8_t)pass;
        if (vm->enable_algo_rewrite || vm->packed_instructions || vm->superoptimize) {
            char buf[64];
            snprintf(buf, sizeof buf, "[opt] pass %d changes=%d\n", pass, ch);
            log_msg(vm, buf);
        }
        if (ch && pass < max && pass >= 2) {
            pass_superop_residue(vm);
            pass_const_fold(vm);
        }
    }
    // Final superoptimize sweep after all passes
    if (vm->superoptimize && max >= 2)
        pass_superoptimize(vm);
}
