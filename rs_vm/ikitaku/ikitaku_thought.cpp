/* Manual thought input. Numbers → series. Control words → instruction image.
   Both feed the existing analyze path. */
#include "ikitaku_thought.hpp"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int has_word(const char* s, const char* w) {
    const char* p = s;
    size_t n = strlen(w);
    while (*p) {
        if (strncmp(p, w, n) == 0 && (p == s || !isalnum((unsigned char)p[-1]))
            && !isalnum((unsigned char)p[n]))
            return 1;
        ++p;
    }
    return 0;
}

static int parse_nums(const char* s, double* out, int maxn) {
    int n = 0;
    const char* p = s;
    while (*p && n < maxn) {
        while (*p && !isdigit((unsigned char)*p) && *p != '-' && *p != '.') ++p;
        if (!*p) break;
        char* end = NULL;
        double v = strtod(p, &end);
        if (end == p) { ++p; continue; }
        out[n++] = v;
        p = end;
    }
    return n;
}

static int build_image(const ikitaku_thought_t* t, rsvm_t* vm) {
    iki_tp_insn_t ins[32];
    int n = 0;
    if (t->has_loop) {
        ins[n].op = IKI_TP_Br; ins[n].imm = 0; ++n;
        ins[n].op = IKI_TP_Add; ++n;
        ins[n].op = IKI_TP_Jmp; ins[n].imm = 0; ++n;
    }
    if (t->has_if || t->has_cmp) {
        ins[n].op = IKI_TP_Lt; ++n;
        ins[n].op = IKI_TP_Br; ++n;
    }
    if (t->has_wait) { ins[n].op = IKI_TP_Clock; ++n; }
    if (t->has_print) { ins[n].op = IKI_TP_Print; ++n; }
    if (t->has_sort) {
        ins[n].op = IKI_TP_Lt; ++n;
        ins[n].op = IKI_TP_Store; ++n;
        ins[n].op = IKI_TP_Jmp; ins[n].imm = 0; ++n;
    }
    if (n == 0) {
        ins[n].op = IKI_TP_Imm; ins[n].imm = 0; ++n;
        ins[n].op = IKI_TP_Ret; ++n;
    }
    return ikitaku_tp_fill(vm, ins, n);
}

int ikitaku_thought_in(const char* text, const ikitaku_config_t* cfg,
                       ikitaku_thought_t* out) {
    if (!text || !out || !text[0]) return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->text, sizeof out->text, "%s", text);

    ikitaku_config_t local;
    if (!cfg) { ikitaku_config_default(&local); cfg = &local; }

    out->n_nums = parse_nums(text, out->nums, IKITAKU_THOUGHT_NUMS);
    out->has_loop  = has_word(text, "loop") || has_word(text, "do") || has_word(text, "while");
    out->has_if    = has_word(text, "if") || has_word(text, "else") || has_word(text, "switch");
    out->has_wait  = has_word(text, "wait") || has_word(text, "delay") || has_word(text, "ms");
    out->has_print = has_word(text, "print") || has_word(text, "emit");
    out->has_sort  = has_word(text, "sort") || has_word(text, "qsort") || has_word(text, "partition");
    out->has_cmp   = has_word(text, "compare") || has_word(text, "gt") || has_word(text, "lt");

    if (out->n_nums >= 2) {
        ikitaku_series_t ser;
        memset(&ser, 0, sizeof ser);
        ser.kind = IKITAKU_DATA_F32;
        ser.n = out->n_nums;
        for (int i = 0; i < out->n_nums && i < IKITAKU_SERIES_MAX; ++i)
            ser.values[i] = out->nums[i];
        if (ikitaku_series_analyze(&ser, &out->series) == 0) {
            out->used_series = 1;
            snprintf(out->english, sizeof out->english, "%s", out->series.english);
        }
    }

    rsvm_t vm;
    if (build_image(out, &vm) > 0) {
        out->used_image = 1;
        ikitaku_analyze(&vm, cfg, &out->session);
        if (out->session.accepted >= 0 && out->session.accepted < out->session.hypo_count) {
            const ikitaku_hypo_t* h = &out->session.hypos[out->session.accepted];
            if (!out->english[0])
                snprintf(out->english, sizeof out->english, "%s | %s (conf %d)",
                         h->purpose, h->form_id, h->confidence);
        }
    }
    if (!out->english[0])
        snprintf(out->english, sizeof out->english, "remainder (no series, thin image)");
    return 0;
}

int ikitaku_thought_dump(const ikitaku_thought_t* t, char* buf, int buflen) {
    if (!t || !buf || buflen < 8) return -1;
    int n = snprintf(buf, (size_t)buflen,
        "thought: %s\nnums: %d  series: %d  image: %d\nenglish: %s\n",
        t->text, t->n_nums, t->used_series, t->used_image, t->english);
    if (n < 0 || n >= buflen) return -1;
    if (t->used_series && n + 80 < buflen) {
        n += snprintf(buf + n, (size_t)(buflen - n),
            "pattern: %s  conf %d  slope %.3f\n",
            t->series.form_r5, t->series.conf_primary, t->series.slope_est);
    }
    if (t->session.hypo_count > 0 && n + 64 < buflen) {
        n += snprintf(buf + n, (size_t)(buflen - n),
            "hypos: %d  accepted: %d  cost: %d\n",
            t->session.hypo_count, t->session.accepted, t->session.cost_used);
    }
    if (t->session.log[0] && n + 16 < buflen)
        snprintf(buf + n, (size_t)(buflen - n), "log:\n%s", t->session.log);
    return 0;
}
