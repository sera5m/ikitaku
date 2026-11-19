// ikitaku — cost-budgeted multi-hypothesis + dialectic + F⊗F synthesis
#include "ikitaku.hpp"
#include "../vm/rs_vm_opcodes.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

static int g_occam_margin = 8; /* extra wrap must beat best by this */

int ikitaku_occam_margin(void) { return g_occam_margin; }

void ikitaku_occam_from_desperation(int level) {
    /* calm: strict Occam; thrown: allow deeper stacks */
    if (level <= 1) g_occam_margin = 8;
    else if (level == 2) g_occam_margin = 5;
    else if (level == 3) g_occam_margin = 3;
    else if (level == 4) g_occam_margin = 1;
    else g_occam_margin = 0;
}

void ikitaku_config_default(ikitaku_config_t* c) {
    if (!c) return;
    c->attention_desire = 24;
    c->allow_infeasible = 1;
    c->novelty_focus_boost = 2;
    c->min_accept_conf = 70;
    c->min_novel_conf = 50;
    c->cost_simple = 1;
    c->cost_stretch = 2;
    c->cost_novel = 4;
    c->cost_synthesize = 3;
    c->cost_dialectic = 2;
    c->exceptional_threshold = 25;
    c->enable_learn = 1;
    c->enable_synthesis = 1;
}

static void logf(ikitaku_session_t* s, const char* fmt, ...) {
    if (!s) return;
    int used = (int)strlen(s->log);
    if (used >= IKITAKU_MAX_LOG - 2) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s->log + used, (size_t)(IKITAKU_MAX_LOG - used), fmt, ap);
    va_end(ap);
}

static int budget_left(const ikitaku_session_t* s, const ikitaku_config_t* cfg) {
    return cfg->attention_desire - s->cost_used;
}

static int pay(ikitaku_session_t* s, const ikitaku_config_t* cfg, int cost, int exceptional) {
    if (exceptional && cost > budget_left(s, cfg) && budget_left(s, cfg) > 0) {
        s->cost_used += budget_left(s, cfg);
        return 1;
    }
    if (cost > budget_left(s, cfg)) return 0;
    s->cost_used += cost;
    return 1;
}

static int score_of(int conf, int cost) {
    return conf - cost * 3;
}

static int span_has_op(const rsvm_t* vm, uint16_t a, uint16_t b, uint8_t op) {
    if (b > vm->code_len) b = vm->code_len;
    for (uint16_t p = a; p < b; ++p)
        if (vm->code[p] == op) return 1;
    return 0;
}

static int span_has_delay(const rsvm_t* vm, uint16_t a, uint16_t b) {
    return span_has_op(vm, a, b, RSVM_OP_DELAY_MS);
}
static int span_has_print(const rsvm_t* vm, uint16_t a, uint16_t b) {
    return span_has_op(vm, a, b, RSVM_OP_PRINT_S)
        || span_has_op(vm, a, b, RSVM_OP_PRINT_I)
        || span_has_op(vm, a, b, RSVM_OP_PRINT_STR)
        || span_has_op(vm, a, b, RSVM_OP_PRINT_V)
        || span_has_op(vm, a, b, RSVM_OP_PRINT_C)
        || span_has_op(vm, a, b, RSVM_OP_PRINT_NL);
}
static int span_has_cmp(const rsvm_t* vm, uint16_t a, uint16_t b) {
    return span_has_op(vm, a, b, RSVM_OP_LT) || span_has_op(vm, a, b, RSVM_OP_GT)
        || span_has_op(vm, a, b, RSVM_OP_LE) || span_has_op(vm, a, b, RSVM_OP_GE)
        || span_has_op(vm, a, b, RSVM_OP_EQ) || span_has_op(vm, a, b, RSVM_OP_NE);
}
static int span_has_trap(const rsvm_t* vm, uint16_t a, uint16_t b) {
    return span_has_op(vm, a, b, RSVM_OP_TRAP_LOOP);
}
static int span_has_inc(const rsvm_t* vm, uint16_t a, uint16_t b) {
    return span_has_op(vm, a, b, RSVM_OP_INC_SLOT)
        || span_has_op(vm, a, b, RSVM_OP_ADD_IMM_SLOT);
}

static void push_hypo(ikitaku_session_t* s, const ikitaku_hypo_t* h) {
    if (!s || !h || s->hypo_count >= IKITAKU_MAX_HYPOS) return;
    s->hypos[s->hypo_count++] = *h;
}

int ikitaku_learned_load(ikitaku_learned_t* out, int max_out) {

#include "ikitaku_core.inc.cpp"
#include "ikitaku_cluster.inc.cpp"
#include "ikitaku_assoc.inc.cpp"
#include "ikitaku_series.inc.cpp"
#include "ikitaku_meta.inc.cpp"
#include "ikitaku_concept.inc.cpp"
#include "ikitaku_mcog.inc.cpp"
#include "ikitaku_life.inc.cpp"
#include "ikitaku_rank.inc.cpp"
