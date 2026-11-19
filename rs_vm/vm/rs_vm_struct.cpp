// Structure analysis module (L1 feature extract + L2 schema match +
// L3/L4 light assign). Does NOT rewrite code — that is L5/L6 in rs_vm_opt.
#include "rs_vm_struct.hpp"
#include "rs_vm_opcodes.h"
#include <stdio.h>
#include <string.h>

// ---- local IR facts (L1) ----
static int op_impure(uint8_t op) {
    switch (op) {
    case RSVM_OP_PRINT_S: case RSVM_OP_PRINT_I: case RSVM_OP_PRINT_STR:
    case RSVM_OP_PRINT_V: case RSVM_OP_PRINT_C: case RSVM_OP_PRINT_NL:
    case RSVM_OP_DELAY_MS: case RSVM_OP_PIN_MODE: case RSVM_OP_DIG_WR:
    case RSVM_OP_DIG_RD: case RSVM_OP_ADC_RD: case RSVM_OP_MILLIS:
    case RSVM_OP_GPIO_RD: case RSVM_OP_GPIO_WR: case RSVM_OP_ADC_RDP:
    case RSVM_OP_FREAD: case RSVM_OP_FWRITE:
    case RSVM_OP_SYS_CMD: case RSVM_OP_SYS_EXEC: case RSVM_OP_SYS_OPEN:
    case RSVM_OP_NATIVE: case RSVM_OP_CALL: case RSVM_OP_RET: case RSVM_OP_RET0:
    case RSVM_OP_JMP: case RSVM_OP_JZ: case RSVM_OP_JNZ:
    case RSVM_OP_TRAP_LOOP: case RSVM_OP_BREAK: case RSVM_OP_CONTINUE:
    case RSVM_OP_SWITCH_JT:
        return 1;
    default: return 0;
    }
}

static int span_pure(const uint8_t* c, uint16_t a, uint16_t b) {
    for (uint16_t p = a; p < b; ++p)
        if (op_impure(c[p])) return 0;
    return 1;
}

static int32_t rd_i32(const uint8_t* c, uint16_t p) {
    return (int32_t)c[p] | ((int32_t)c[p+1]<<8) | ((int32_t)c[p+2]<<16) | ((int32_t)c[p+3]<<24);
}

static int match_inc_body(const uint8_t* c, uint16_t body, uint16_t end,
                          uint8_t* slot_out, int32_t* delta_out) {
    if (body + 2 <= end && c[body] == RSVM_OP_INC_SLOT) {
        if (slot_out) *slot_out = c[body+1];
        if (delta_out) *delta_out = 1;
        return (body + 2 == end) ? 1 : 0;
    }
    if (body + 6 <= end && c[body] == RSVM_OP_ADD_IMM_SLOT) {
        if (slot_out) *slot_out = c[body+1];
        if (delta_out) *delta_out = rd_i32(c, (uint16_t)(body+2));
        return (body + 6 == end) ? 1 : 0;
    }
    // LOAD s; PUSHI d; ADD; STORE s
    if (body + 10 <= end && c[body] == RSVM_OP_LOAD && c[body+2] == RSVM_OP_PUSHI
        && c[body+7] == RSVM_OP_ADD && c[body+8] == RSVM_OP_STORE
        && c[body+1] == c[body+9]) {
        if (slot_out) *slot_out = c[body+1];
        if (delta_out) *delta_out = rd_i32(c, (uint16_t)(body+3));
        return (body + 10 == end) ? 1 : 0;
    }
    return 0;
}

static int match_phase_wrap_span(const uint8_t* c, uint16_t a, uint16_t b) {
    // look for PUSHI 2^k ; MOD or PUSHI 2^k-1 ; BAND
    for (uint16_t p = a; p + 6 <= b; ++p) {
        if (c[p] == RSVM_OP_PUSHI && (c[p+5] == RSVM_OP_MOD || c[p+5] == RSVM_OP_BAND)) {
            int32_t imm = rd_i32(c, (uint16_t)(p+1));
            if (c[p+5] == RSVM_OP_MOD) {
                uint32_t u = (uint32_t)imm;
                if (imm > 0 && (u & (u-1)) == 0) return 1;
            } else {
                // BAND mask = 2^k - 1
                uint32_t u = (uint32_t)(imm + 1);
                if (imm > 0 && (u & (u-1)) == 0) return 1;
            }
        }
    }
    return 0;
}

static int match_strength_shl(const uint8_t* c, uint16_t a, uint16_t b) {
    for (uint16_t p = a; p + 6 <= b; ++p) {
        if (c[p] != RSVM_OP_PUSHI) continue;
        uint8_t op = c[p+5];
        if (op == RSVM_OP_SHL) return 1;
        // also raw mul by power-of-two (pre-strength-reduce)
        if (op == RSVM_OP_MUL) {
            int32_t imm = rd_i32(c, (uint16_t)(p+1));
            uint32_t u = (uint32_t)imm;
            if (imm > 0 && (u & (u - 1)) == 0) return 1;
        }
    }
    return 0;
}

// Find TRAP body inside region span
static int find_trap_body(const uint8_t* c, uint16_t start, uint16_t end,
                          uint16_t* body_out, uint16_t* blen_out) {
    for (uint16_t p = start; p + 3 < end; ++p) {
        if (c[p] != RSVM_OP_TRAP_LOOP) continue;
        uint16_t blen = (uint16_t)c[p+1] | ((uint16_t)c[p+2] << 8);
        uint16_t body = (uint16_t)(p + 3);
        if ((uint32_t)body + blen <= end) {
            if (body_out) *body_out = body;
            if (blen_out) *blen_out = blen;
            return 1;
        }
    }
    return 0;
}

static void fill_base(rsvm_structure_hit_t* h, const char* kind, const char* form,
                      const char* purpose, const char* family, const char* big_o,
                      int conf, uint8_t pure, uint8_t pred, uint8_t prog, uint8_t conc,
                      uint16_t a, uint16_t b, uint8_t rk) {
    memset(h, 0, sizeof(*h));
    snprintf(h->kind, sizeof h->kind, "%s", kind);
    snprintf(h->general_form, sizeof h->general_form, "%s", form);
    snprintf(h->purpose, sizeof h->purpose, "%s", purpose);
    snprintf(h->family, sizeof h->family, "%s", family);
    snprintf(h->big_o, sizeof h->big_o, "%s", big_o);
    h->confidence = conf;
    h->rank = RSVM_RANK_ALGO;
    h->pure = pure;
    h->predictable = pred;
    h->progress = prog;
    h->concurrency = conc;
    h->region_start = a;
    h->region_end = b;
    h->region_kind = rk;
    h->parent_hit = -1;
    h->micro_schema = (h->rank == RSVM_RANK_MICRO) ? 1 : 0;
}

int rsvm_hit_rewrite_allowed(const rsvm_structure_hit_t* h) {
    if (!h) return 0;
    return h->pure && h->predictable == RSVM_PRED_PREDICTABLE
        && h->progress != RSVM_PROG_EXTERNAL;
}

// ---- axiom schema matchers (parametric rules) ----
static int rule_counted_inc(const rsvm_t* vm, uint16_t start, uint16_t end,
                            rsvm_structure_hit_t* out) {
    uint16_t body = 0, blen = 0;
    if (!find_trap_body(vm->code, start, end, &body, &blen)) return 0;
    uint8_t slot = 0; int32_t d = 0;
    if (!match_inc_body(vm->code, body, (uint16_t)(body + blen), &slot, &d)) return 0;
    if (!span_pure(vm->code, body, (uint16_t)(body + blen))) return 0;
    char form[RSVM_FORM_LEN];
    snprintf(form, sizeof form, "x := x + (%d)  x%d times → x += n*(%d)", (int)d, (int)slot, (int)d);
    fill_base(out, "counted_inc", form, "reduce", "affine_reduce", "O(n)→O(1)", 92, 1,
              RSVM_PRED_PREDICTABLE, RSVM_PROG_FINITE, RSVM_CONC_SEQUENTIAL,
              start, end, RSVM_REG_TRAP);
    return 92;
}

static int rule_phase_wrap(const rsvm_t* vm, uint16_t start, uint16_t end,
                           rsvm_structure_hit_t* out) {
    if (!match_phase_wrap_span(vm->code, start, end)) return 0;
    int pure = span_pure(vm->code, start, end);
    fill_base(out, "phase_wrap",
              "p := (p + d) mod 2^k  →  p &= (2^k - 1)",
              "map", "ring_index", "O(1)/step", pure ? 85 : 50, (uint8_t)pure,
              pure ? RSVM_PRED_PREDICTABLE : RSVM_PRED_PARTIAL, RSVM_PROG_FINITE, RSVM_CONC_SEQUENTIAL,
              start, end, RSVM_REG_NONE);
    out->rank = RSVM_RANK_MICRO;
    out->micro_schema = 1;
    return pure ? 85 : 50;
}

static int rule_strength_pow2(const rsvm_t* vm, uint16_t start, uint16_t end,
                              rsvm_structure_hit_t* out) {
    if (!match_strength_shl(vm->code, start, end)) return 0;
    fill_base(out, "strength_mul_pow2",
              "x * 2^k  →  x << k",
              "map", "strength_reduce", "O(1)", 90, 1,
              RSVM_PRED_PREDICTABLE, RSVM_PROG_FINITE, RSVM_CONC_SEQUENTIAL,
              start, end, RSVM_REG_NONE);
    out->rank = RSVM_RANK_MICRO;
    out->micro_schema = 1;
    return 90;
}

// Wait: blocking delay (impure — freezes progress for N ms)
static int rule_wait_delay(const rsvm_t* vm, uint16_t start, uint16_t end,
                           rsvm_structure_hit_t* out) {
    const uint8_t* c = vm->code;
    for (uint16_t p = start; p + 5 <= end; ++p) {
        if (c[p] != RSVM_OP_DELAY_MS) continue;
        int32_t ms = rd_i32(c, (uint16_t)(p + 1));
        char form[RSVM_FORM_LEN];
        snprintf(form, sizeof form,
                 "blocking wait %d ms then resume (no concurrent work)", (int)ms);
        fill_base(out, "wait_delay", form,
                  "wait", "delay_block", "O(1) wall-clock", 90, 0,
                  RSVM_PRED_PREDICTABLE, RSVM_PROG_EXTERNAL, RSVM_CONC_SEQUENTIAL,
                  start, end, RSVM_REG_NONE);
        out->rank = RSVM_RANK_FAMILY;
        return 90;
    }
    return 0;
}

static int rule_region_trap(const rsvm_t* vm, uint16_t start, uint16_t end,
                            rsvm_structure_hit_t* out) {
    uint16_t body = 0, blen = 0;
    if (!find_trap_body(vm->code, start, end, &body, &blen)) return 0;
    int pure = span_pure(vm->code, body, (uint16_t)(body + blen));
    fill_base(out, pure ? "trap_counted" : "trap_impure",
              pure ? "repeat body exactly n times (pure)" : "repeat body n times (side effects)",
              "repeat", "trap_loop", pure ? "O(n)" : "O(n)+IO", pure ? 75 : 60, (uint8_t)pure,
              pure ? RSVM_PRED_PREDICTABLE : RSVM_PRED_PARTIAL,
              pure ? RSVM_PROG_FINITE : RSVM_PROG_EXTERNAL, RSVM_CONC_SEQUENTIAL,
              start, end, RSVM_REG_TRAP);
    out->rank = RSVM_RANK_FAMILY; // R4 counted/trap loop under R5 repeat
    return pure ? 75 : 60;
}

// Region-kind → R4/R5 flow concepts (for / while / if / switch)
static int hit_from_region_kind(const rsvm_t* vm, const rsvm_region_t* r,
                                rsvm_structure_hit_t* out) {
    uint16_t a = r->start, b = r->end;
    if (b > vm->code_len) b = vm->code_len;
    uint8_t pure = (uint8_t)span_pure(vm->code, a, b);
    switch (r->kind) {
    case RSVM_REG_TRAP:
        fill_base(out, pure ? "trap_loop" : "trap_loop_impure",
                  pure ? "repeat body n times" : "repeat body n times (contains wait/io)",
                  "repeat", "trap_loop", pure ? "O(n)" : "O(n)+wait/io", 70, pure,
                  pure ? RSVM_PRED_PREDICTABLE : RSVM_PRED_PARTIAL,
                  pure ? RSVM_PROG_FINITE : RSVM_PROG_EXTERNAL, RSVM_CONC_SEQUENTIAL,
                  a, b, r->kind);
        out->rank = RSVM_RANK_FAMILY; return 70;
    case RSVM_REG_LOOP:
        fill_base(out, pure ? "do_n_classic" : "do_n_impure",
                  pure ? "repeat body n times (JMP countdown)" : "repeat body n times (wait/io in body)",
                  "repeat", "counted_loop", pure ? "O(n)" : "O(n)+wait/io", 70, pure,
                  pure ? RSVM_PRED_PREDICTABLE : RSVM_PRED_PARTIAL,
                  pure ? RSVM_PROG_FINITE : RSVM_PROG_EXTERNAL, RSVM_CONC_SEQUENTIAL,
                  a, b, r->kind);
        out->rank = RSVM_RANK_FAMILY; return 70;
    case RSVM_REG_WHILE:
        fill_base(out, "while_cond", "repeat until condition false",
                  "repeat", "cond_loop", "O(iterations)", 70, 1,
                  RSVM_PRED_PARTIAL, RSVM_PROG_DATA, RSVM_CONC_SEQUENTIAL,
                  a, b, r->kind);
        out->rank = RSVM_RANK_FAMILY; return 70;
    case RSVM_REG_FOR:
        fill_base(out, "for_affine", "repeat with induction variable",
                  "repeat", "counted_loop", "O(range)", 70, 1,
                  RSVM_PRED_PREDICTABLE, RSVM_PROG_FINITE, RSVM_CONC_SEQUENTIAL,
                  a, b, r->kind);
        out->rank = RSVM_RANK_FAMILY; return 70;
    case RSVM_REG_IF:
        fill_base(out, "if_branch", "compare; take one of two paths",
                  "decision", "binary_branch", "O(1)+body", 70, 1,
                  RSVM_PRED_PREDICTABLE, RSVM_PROG_FINITE, RSVM_CONC_SEQUENTIAL,
                  a, b, r->kind);
        out->rank = RSVM_RANK_FAMILY; return 70;
    case RSVM_REG_SWITCH:
        fill_base(out, "switch_chain", "compare value; select among cases",
                  "decision", "multi_branch", "O(cases)", 70, 1,
                  RSVM_PRED_PREDICTABLE, RSVM_PROG_FINITE, RSVM_CONC_SEQUENTIAL,
                  a, b, r->kind);
        out->rank = RSVM_RANK_FAMILY; return 70;
    case RSVM_REG_JT:
        fill_base(out, "switch_jt", "jump-table select among dense cases",
                  "decision", "multi_branch", "O(1)", 80, 1,
                  RSVM_PRED_PREDICTABLE, RSVM_PROG_FINITE, RSVM_CONC_SEQUENTIAL,
                  a, b, r->kind);
        out->rank = RSVM_RANK_FAMILY; return 80;
    case RSVM_REG_FN:
        fill_base(out, "fn_region", "callable unit with in/out info",
                  "info", "slot_bind", "O(body)", 60, 1,
                  RSVM_PRED_PARTIAL, RSVM_PROG_DATA, RSVM_CONC_SEQUENTIAL,
                  a, b, r->kind);
        out->rank = RSVM_RANK_REGION; return 60;
    default:
        return 0;
    }
}

static const rsvm_struct_rule_t RULES[] = {
    { "counted_inc",
      "x := x + c (n times) → x += n*c",
      "reduce", "affine_reduce", "O(n)→O(1)", 85, RSVM_RANK_ALGO,
      1, RSVM_PRED_PREDICTABLE, RSVM_PROG_FINITE, RSVM_CONC_SEQUENTIAL, rule_counted_inc },
    { "phase_wrap",
      "p mod 2^k → p & (2^k-1)",
      "map", "ring_index", "O(1)/step", 70, RSVM_RANK_MICRO,
      1, RSVM_PRED_PREDICTABLE, RSVM_PROG_FINITE, RSVM_CONC_SEQUENTIAL, rule_phase_wrap },
    { "strength_mul_pow2",
      "x * 2^k → x << k",
      "map", "strength_reduce", "O(1)", 80, RSVM_RANK_MICRO,
      1, RSVM_PRED_PREDICTABLE, RSVM_PROG_FINITE, RSVM_CONC_SEQUENTIAL, rule_strength_pow2 },
    { "trap_region",
      "repeat body n times (TRAP)",
      "repeat", "trap_loop", "O(n)", 50, RSVM_RANK_FAMILY,
      1, RSVM_PRED_PREDICTABLE, RSVM_PROG_FINITE, RSVM_CONC_SEQUENTIAL, rule_region_trap },
    { "wait_delay",
      "blocking delay_ms(N) — freeze then resume",
      "wait", "delay_block", "O(1) wall", 80, RSVM_RANK_FAMILY,
      0, RSVM_PRED_PREDICTABLE, RSVM_PROG_EXTERNAL, RSVM_CONC_SEQUENTIAL, rule_wait_delay },
};
static const int RULE_N = (int)(sizeof RULES / sizeof RULES[0]);

const rsvm_struct_rule_t* rsvm_struct_rules(void) { return RULES; }
int rsvm_struct_rule_count(void) { return RULE_N; }

// L1–L4 analysis entry
int rsvm_analyze_structures(const rsvm_t* vm, rsvm_structure_hit_t* out, int max_out) {
    if (!vm || !out || max_out <= 0) return 0;
    int n = 0;

    // Prefer region spans; also scan whole code once for global micro patterns
    if (vm->region_count > 0) {
        for (uint8_t i = 0; i < vm->region_count && n < max_out; ++i) {
            const rsvm_region_t* r = &vm->regions[i];
            uint16_t a = r->start, b = r->end;
            if (b > vm->code_len) b = vm->code_len;
            if (a >= b) continue;

            // Always emit R4/R5 flow concept from region kind (repeat|decision|info)
            rsvm_structure_hit_t flow;
            int flow_idx = -1;
            int flow_conf = hit_from_region_kind(vm, r, &flow);
            if (flow_conf > 0 && n < max_out) {
                flow_idx = n;
                out[n++] = flow;
            }
            // Specialized axiom rules (counted_inc, wait_delay, …) as extra hits
            for (int ri = 0; ri < RULE_N && n < max_out; ++ri) {
                rsvm_structure_hit_t h;
                int conf = RULES[ri].match(vm, a, b, &h);
                if (conf < RULES[ri].min_confidence) continue;
                // Skip generic trap_region if we already have flow trap/loop
                if (strcmp(RULES[ri].id, "trap_region") == 0 && flow_idx >= 0) continue;
                h.region_kind = r->kind;
                if (flow_idx >= 0) h.parent_hit = (int16_t)flow_idx;
                // de-dup kind within this region batch
                int dup = 0;
                for (int j = 0; j < n; ++j)
                    if (strcmp(out[j].kind, h.kind) == 0
                        && out[j].region_start == h.region_start) { dup = 1; break; }
                if (!dup) out[n++] = h;
            }
        }
    } else {
        // No regions: whole program as one span
        for (int ri = 0; ri < RULE_N && n < max_out; ++ri) {
            rsvm_structure_hit_t h;
            int conf = RULES[ri].match(vm, 0, vm->code_len, &h);
            if (conf >= RULES[ri].min_confidence)
                out[n++] = h;
        }
    }

    // Whole-code micro / wait schemas (not only inside regions)
    if (n < max_out) {
        rsvm_structure_hit_t h;
        int conf = rule_phase_wrap(vm, 0, vm->code_len, &h);
        if (conf >= 70) {
            int dup = 0;
            for (int i = 0; i < n; ++i)
                if (strcmp(out[i].kind, h.kind) == 0) { dup = 1; break; }
            if (!dup) out[n++] = h;
        }
    }
    if (n < max_out) {
        rsvm_structure_hit_t h;
        int conf = rule_strength_pow2(vm, 0, vm->code_len, &h);
        if (conf >= 80) {
            int dup = 0;
            for (int i = 0; i < n; ++i)
                if (strcmp(out[i].kind, h.kind) == 0) { dup = 1; break; }
            if (!dup) out[n++] = h;
        }
    }
    if (n < max_out) {
        rsvm_structure_hit_t h;
        int conf = rule_wait_delay(vm, 0, vm->code_len, &h);
        if (conf >= 80) {
            int dup = 0;
            for (int i = 0; i < n; ++i)
                if (strcmp(out[i].kind, h.kind) == 0) { dup = 1; break; }
            if (!dup) out[n++] = h;
        }
    }

    // Parent links: region-rank hits can parent algo hits that share span
    for (int i = 0; i < n; ++i) {
        if (out[i].rank > RSVM_RANK_REGION) {
            for (int j = 0; j < n; ++j) {
                if (i == j) continue;
                if (out[j].rank == RSVM_RANK_REGION
                    && out[j].region_start == out[i].region_start
                    && out[j].region_end == out[i].region_end) {
                    out[i].parent_hit = (int16_t)j;
                    break;
                }
            }
        }
    }
    return n;
}

int rsvm_structure_report(const rsvm_t* vm, char* buf, int buf_len) {
    if (!vm || !buf || buf_len < 8) return 0;
    rsvm_structure_hit_t hits[RSVM_STRUCT_MAX_HITS];
    int nh = rsvm_analyze_structures(vm, hits, RSVM_STRUCT_MAX_HITS);
    int used = 0;
    used += snprintf(buf + used, (size_t)(buf_len - used),
                     "[structure] hits=%d rules=%d regions=%u\n",
                     nh, RULE_N, (unsigned)vm->region_count);
    for (int i = 0; i < nh && used < buf_len - 80; ++i) {
        const rsvm_structure_hit_t* h = &hits[i];
        const char* pred = h->predictable==0?"pred": h->predictable==1?"part":"unpred";
        const char* prog = h->progress==0?"finite": h->progress==1?"data":"external";
        const char* conc = h->concurrency==0?"seq": h->concurrency==1?"async":"contend";
        used += snprintf(buf + used, (size_t)(buf_len - used),
            "  [%d] R%d %s  purpose=%s family=%s  %s  conf=%d\n"
            "       attrs: pure=%u %s %s %s  rewrite=%s  pc[%u..%u) parent=%d\n"
            "       form: %s\n",
            i, (int)h->rank, h->kind, h->purpose, h->family, h->big_o,
            h->confidence,
            (unsigned)h->pure, pred, prog, conc,
            rsvm_hit_rewrite_allowed(h) ? "yes" : "no",
            (unsigned)h->region_start, (unsigned)h->region_end, (int)h->parent_hit,
            h->general_form);
    }
    return used;
}
