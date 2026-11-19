// Basis dimensions 0..31 (was 0..15). Extra dims carry more R5 verbs.
// 0 pure, 1 impure, 2 rewritable, 3 finite, 4 external, 5 sequential,
// 6 repeat, 7 decision, 8 wait, 9 info, 10 reduce, 11 map,
// 12 synth, 13 novel, 14 high_conf, 15 feasible,
// 16 order, 17 seek, 18 split, 19 join, 20 emit, 21 absorb,
// 22 invert, 23 bound, 24 filter, 25 generate, 26 compare, 27 sequence,
// 28 copy, 29 destroy, 30 name, 31 hidden_d

static float g_affinity[IKITAKU_AFF_N][IKITAKU_AFF_N];
static int g_aff_init = 0;
static int g_hebb_steps = 0;

static const char* AFF_KEYS[IKITAKU_AFF_N] = {
    "repeat", "decision", "wait", "info", "reduce", "map", "unknown", "io",
    "counted_inc", "phase_wrap", "strength_mul_pow2", "wait_delay",
    "trap_loop", "if_branch", "switch_jt", "for_affine",
    "while_cond", "repeat_x_wait", "repeat_x_decision", "decision_x_wait",
    "binary_branch", "delay_block", "affine_reduce", "ring_index",
    "order", "seek", "split", "join", "emit", "absorb",
    "invert", "bound", "filter", "generate", "compare", "sequence",
    "copy", "destroy", "name", "create",
    "one-loop", "two-loop", "accum-loop", "template",
    "path", "closed", "energy-loss", "center",
    "cause", "origin", "probe", "schema",
    "sin", "cos", "linear", "quad",
    "object", "motion", "living", "tool",
    "person", "place", "time", "event"
};

static int purpose_idx(const char* p) {
    if (!p || !p[0]) return -1;
    for (int i = 0; i < IKITAKU_AFF_N; ++i)
        if (strcmp(p, AFF_KEYS[i]) == 0) return i;
    // fuzzy: form_id may match
    return -1;
}

static int key_idx(const char* purpose, const char* form_id) {
    int a = purpose_idx(purpose);
    if (a >= 0) return a;
    if (form_id && form_id[0]) {
        for (int i = 0; i < IKITAKU_AFF_N; ++i)
            if (strcmp(form_id, AFF_KEYS[i]) == 0) return i;
    }
    return 7; // unknown bucket
}

static void aff_set(int a, int b, float v) {
    if (a < 0 || b < 0 || a >= IKITAKU_AFF_N || b >= IKITAKU_AFF_N) return;
    g_affinity[a][b] = g_affinity[b][a] = v;
}

static void affinity_ensure(void) {
    if (g_aff_init) return;
    memset(g_affinity, 0, sizeof g_affinity);
    for (int i = 0; i < IKITAKU_AFF_N; ++i) g_affinity[i][i] = 1.0f;
    // purpose priors
    aff_set(0, 2, 0.70f); // repeat↔wait
    aff_set(0, 1, 0.65f); // repeat↔decision
    aff_set(1, 2, 0.50f);
    aff_set(0, 4, 0.80f); // repeat↔reduce
    aff_set(3, 0, 0.40f); // info↔repeat
    aff_set(5, 10, 0.85f); // map↔strength_mul_pow2
    aff_set(5, 9, 0.75f);  // map↔phase_wrap
    aff_set(4, 8, 0.90f);  // reduce↔counted_inc
    aff_set(2, 11, 0.95f); // wait↔wait_delay
    aff_set(0, 12, 0.85f); // repeat↔trap_loop
    aff_set(1, 13, 0.90f); // decision↔if_branch
    aff_set(0, 17, 0.80f); // repeat↔repeat_x_wait
    aff_set(0, 18, 0.80f);
    aff_set(1, 19, 0.75f);
    // micro under map/reduce
    aff_set(10, 8, 0.35f);
    aff_set(9, 11, 0.20f);
    /* R5 object verbs */
    aff_set(24, 0, 0.55f); /* order↔repeat */
    aff_set(25, 12, 0.50f); /* seek */
    aff_set(26, 27, 0.70f); /* split↔join */
    aff_set(28, 29, 0.15f); /* emit antonym absorb — keep low */
    aff_set(30, 11, 0.80f); /* invert↔map */
    aff_set(32, 4, 0.60f);  /* filter↔reduce */
    aff_set(58, 0, 0.65f);  /* living↔repeat */
    aff_set(57, 0, 0.70f);  /* motion↔repeat */
    aff_set(56, 3, 0.85f);  /* object↔info */
    g_aff_init = 1;
}

float ikitaku_affinity(const char* Fa, const char* Fb) {
    affinity_ensure();
    int a = key_idx(Fa, Fa), b = key_idx(Fb, Fb);
    if (a < 0 || b < 0) return 0.1f;
    return g_affinity[a][b];
}

void ikitaku_affinity_observe(const ikitaku_session_t* s) {
    if (!s) return;
    affinity_ensure();
    if (g_hebb_steps >= IKITAKU_AFF_HEBB_CAP) return;
    g_hebb_steps++;
    int seen[IKITAKU_AFF_N];
    memset(seen, 0, sizeof seen);
    for (int i = 0; i < s->hypo_count; ++i) {
        int p = key_idx(s->hypos[i].purpose, s->hypos[i].form_id);
        if (p >= 0 && p < IKITAKU_AFF_N) seen[p] = 1;
        int f = purpose_idx(s->hypos[i].form_id);
        if (f >= 0) seen[f] = 1;
    }
    for (int i = 0; i < IKITAKU_AFF_N; ++i) {
        if (!seen[i]) continue;
        for (int j = i + 1; j < IKITAKU_AFF_N; ++j) {
            if (!seen[j]) continue;
            float* cell = &g_affinity[i][j];
            /* local cluster: already related — Hebb. far: do not pull
               antonyms/coincidences into synonym (mimicry is probe, not glue). */
            if (*cell < 0.20f) continue;
            float rate = (*cell >= 0.40f) ? 0.10f : 0.03f;
            *cell = (*cell) * (1.0f - rate) + rate * 1.0f;
            if (*cell > 1.0f) *cell = 1.0f;
            g_affinity[j][i] = *cell;
        }
    }
}

int ikitaku_affinity_save(const char* path) {
    affinity_ensure();
    if (!path) path = "/workspace/artifacts/rs_vm/ikitaku_affinity.mat";
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%d\n", IKITAKU_AFF_N);
    for (int i = 0; i < IKITAKU_AFF_N; ++i) {
        for (int j = 0; j < IKITAKU_AFF_N; ++j)
            fprintf(f, "%.4f%s", g_affinity[i][j], j == IKITAKU_AFF_N-1 ? "\n" : " ");
    }
    fclose(f);
    return 0;
}

int ikitaku_affinity_load(const char* path) {
    if (!path) path = "/workspace/artifacts/rs_vm/ikitaku_affinity.mat";
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    affinity_ensure();
    int n = 8;
    if (fscanf(f, "%d", &n) != 1) { rewind(f); n = 8; }
    if (n > IKITAKU_AFF_N) n = IKITAKU_AFF_N;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            if (fscanf(f, "%f", &g_affinity[i][j]) != 1) { fclose(f); return -1; }
    fclose(f);
    g_aff_init = 1;
    return 0;
}

void ikitaku_embed_hypo(const ikitaku_hypo_t* h, ikitaku_embed_t* out) {
    if (!out) return;
    memset(out, 0, sizeof *out);
    if (!h) return;
    snprintf(out->key, sizeof out->key, "%s", h->form_id[0] ? h->form_id : h->purpose);
    float* v = out->vec;
    // binary-ish features scaled like soft attention weights
    if (h->rewrite_ok) { v[0] = 1.0f; v[2] = 1.0f; }
    else { v[1] = 1.0f; }
    // progress inferred from note/purpose heuristically
    if (strcmp(h->purpose, "wait") == 0) v[4] = 1.0f;
    else v[3] = 0.7f;
    v[5] = 1.0f; // sequential default
    int pi = purpose_idx(h->purpose);
    if (pi == 0) v[6] = 1.0f;
    else if (pi == 1) v[7] = 1.0f;
    else if (pi == 2) v[8] = 1.0f;
    else if (pi == 3) v[9] = 1.0f;
    else if (pi == 4) v[10] = 1.0f;
    else if (pi == 5) v[11] = 1.0f;
    if (h->synthesized) v[12] = 1.0f;
    if (h->novel) v[13] = 1.0f;
    if (h->confidence >= 80) v[14] = 1.0f;
    else if (h->confidence >= 60) v[14] = 0.6f;
    else v[14] = 0.3f;
    if (h->feasible) v[15] = 1.0f;
    /* extra R5 dims 16..31 */
    if (pi == 24) v[16] = 1.0f;
    else if (pi == 25) v[17] = 1.0f;
    else if (pi == 26) v[18] = 1.0f;
    else if (pi == 27) v[19] = 1.0f;
    else if (pi == 28) v[20] = 1.0f;
    else if (pi == 29) v[21] = 1.0f;
    else if (pi == 30) v[22] = 1.0f;
    else if (pi == 31) v[23] = 1.0f;
    else if (pi == 32) v[24] = 1.0f;
    else if (pi == 33) v[25] = 1.0f;
    else if (pi == 34) v[26] = 1.0f;
    else if (pi == 35) v[27] = 1.0f;
    if (h->confidence < 40) v[31] = 0.6f; /* hidden_d-ish */
}

int ikitaku_cosine_sim(const ikitaku_embed_t* a, const ikitaku_embed_t* b) {
    if (!a || !b) return 0;
    double dot = 0, na = 0, nb = 0;
    for (int i = 0; i < IKITAKU_ASSOC_DIM; ++i) {
        dot += (double)a->vec[i] * (double)b->vec[i];
        na += (double)a->vec[i] * (double)a->vec[i];
        nb += (double)b->vec[i] * (double)b->vec[i];
    }
    if (na < 1e-9 || nb < 1e-9) return 0;
    double c = dot / (sqrt(na) * sqrt(nb));
    if (c < 0) c = 0;
    if (c > 1) c = 1;
    return (int)(c * 100.0 + 0.5);
}

int ikitaku_associate(const ikitaku_hypo_t* a, const ikitaku_hypo_t* b,
                      char* note_buf, int note_len) {
    if (!a || !b) return 0;
    int jac = ikitaku_hypo_similarity(a, b, NULL, 0);
    ikitaku_embed_t ea, eb;
    ikitaku_embed_hypo(a, &ea);
    ikitaku_embed_hypo(b, &eb);
    int cos = ikitaku_cosine_sim(&ea, &eb);
    float aff = ikitaku_affinity(a->purpose, b->purpose);
    int affi = (int)(aff * 100.0f + 0.5f);
    // weighted mix (LLM-like: content similarity + relational prior)
    int score = (jac * 40 + cos * 40 + affi * 20) / 100;
    // micro-schema sub-association graph: boost when both micro under map/reduce
    if (a->micro_schema && b->micro_schema) score = (score * 2 + 80) / 3;
    else if (a->micro_schema || b->micro_schema) score = (score * 3 + 50) / 4;
    if (score > 100) score = 100;
    if (note_buf && note_len > 0) {
        snprintf(note_buf, (size_t)note_len,
                 "assoc=%d (jac=%d cos=%d aff=%d) %s~%s",
                 score, jac, cos, affi, a->purpose, b->purpose);
    }
    return score;
}

int ikitaku_associate_hunt(const ikitaku_hypo_t* goal,
                           const ikitaku_hypo_t* avoid,
                           struct ikitaku_hunt_t* hunt,
                           char* note_buf, int note_len) {
    if (!goal || !avoid || !hunt) return 0;
    int score = ikitaku_associate(goal, avoid, note_buf, note_len);
    ikitaku_hunt_init(hunt,
                      goal->purpose[0] ? goal->purpose : "goal",
                      avoid->purpose[0] ? avoid->purpose : "avoid");
    ikitaku_hunt_arrive(hunt, "pos", goal->confidence);
    ikitaku_hunt_arrive(hunt, "neg", avoid->confidence);
    ikitaku_assoc_delta_t poles;
    ikitaku_assoc_poles(score, score, 50, &poles);
    if (poles.invert_recovers) hunt->invert_side = 1;
    if (note_buf && note_len > 0) {
        int used = (int)strlen(note_buf);
        if (used < note_len - 8)
            snprintf(note_buf + used, (size_t)(note_len - used),
                     " | hunt next=%d", hunt->next_probe);
    }
    return score;
}

// ===================== Multi-gap + associate-evaluate-consider =====================

int ikitaku_find_gaps(const rsvm_t* vm, const ikitaku_session_t* s,
                      ikitaku_gap_report_t* out) {
    if (!s || !out) return -1;
    memset(out, 0, sizeof *out);
    for (int i = 0; i < s->hypo_count && out->gap_n < IKITAKU_MAX_GAPS; ++i) {
        const ikitaku_hypo_t* h = &s->hypos[i];
        // weak confidence
        if (h->confidence < 50 || h->novel) {
            ikitaku_gap_t* g = &out->gaps[out->gap_n++];
            g->hypo_idx = i;
            snprintf(g->missing, sizeof g->missing, "strong_plugin_match");
            snprintf(g->why, sizeof g->why,
                     "no catalog plugin matched this span with conf>=50; IR shape unseen or too sparse");
            g->severity = 100 - h->confidence;
        }
        // wait without delay beacon is incomplete
        if (strcmp(h->purpose, "wait") == 0 && h->confidence < 80) {
            ikitaku_gap_t* g = &out->gaps[out->gap_n++];
            g->hypo_idx = i;
            snprintf(g->missing, sizeof g->missing, "delay_beacon");
            snprintf(g->why, sizeof g->why,
                     "purpose=wait but DELAY_MS not evidenced in span; wait subtype unknown");
            g->severity = 60;
        }
        // rewrite claimed but impure purpose wait/repeat
        if (h->rewrite_ok && (strcmp(h->purpose, "wait") == 0)) {
            ikitaku_gap_t* g = &out->gaps[out->gap_n++];
            g->hypo_idx = i;
            snprintf(g->missing, sizeof g->missing, "consistency_rewrite_vs_wait");
            snprintf(g->why, sizeof g->why,
                     "rewrite_ok on wait is contradictory: wall-clock progress cannot close to pure O(1)");
            g->severity = 90;
        }
        // micro without parent purpose association
        if (h->micro_schema && strcmp(h->purpose, "map") != 0 && strcmp(h->purpose, "reduce") != 0) {
            ikitaku_gap_t* g = &out->gaps[out->gap_n++];
            g->hypo_idx = i;
            snprintf(g->missing, sizeof g->missing, "micro_parent_F");
            snprintf(g->why, sizeof g->why,
                     "micro-schema lacks linked R5 parent (map/reduce); sub-graph floating");
            g->severity = 40;
        }
        // ambiguous
        if (h->ambiguous) {
            ikitaku_gap_t* g = &out->gaps[out->gap_n++];
            g->hypo_idx = i;
            snprintf(g->missing, sizeof g->missing, "disambiguation");
            snprintf(g->why, sizeof g->why,
                     "partial attribute overlap with peer forms; multiple readings remain open");
            g->severity = 55;
        }
    }
    // whole-program: no decision but has cmp? structure already handles
    if (vm && s->hypo_count == 0 && vm->code_len > 0 && out->gap_n < IKITAKU_MAX_GAPS) {
        ikitaku_gap_t* g = &out->gaps[out->gap_n++];
        g->hypo_idx = -1;
        snprintf(g->missing, sizeof g->missing, "any_structure_hit");
        snprintf(g->why, sizeof g->why,
                 "program produced zero structure hits; no region/micro beacon to anchor F");
        g->severity = 95;
    }
    int sum = 0;
    for (int i = 0; i < out->gap_n; ++i) sum += out->gaps[i].severity;
    out->joint_severity = out->gap_n ? sum / out->gap_n : 0;
    return out->gap_n;
}

int ikitaku_consider_gaps(const ikitaku_session_t* s,
                          const ikitaku_gap_report_t* gaps,
                          char* out_note, int note_len) {
    if (!gaps || !out_note || note_len < 8) return -1;
    if (gaps->gap_n == 0) {
        snprintf(out_note, (size_t)note_len, "consider: no gaps — information sufficient");
        return 0;
    }
    // Associate gaps pairwise: which missing-pieces interact
    int best_i = 0, best_sev = gaps->gaps[0].severity;
    for (int i = 1; i < gaps->gap_n; ++i)
        if (gaps->gaps[i].severity > best_sev) {
            best_sev = gaps->gaps[i].severity;
            best_i = i;
        }
    // Evaluate: if two gaps share hypo or purposes, joint close is better
    int related = 0;
    for (int i = 0; i < gaps->gap_n; ++i) {
        for (int j = i + 1; j < gaps->gap_n; ++j) {
            if (gaps->gaps[i].hypo_idx >= 0 &&
                gaps->gaps[i].hypo_idx == gaps->gaps[j].hypo_idx)
                related++;
            else if (s && gaps->gaps[i].hypo_idx >= 0 && gaps->gaps[j].hypo_idx >= 0) {
                int a = gaps->gaps[i].hypo_idx, b = gaps->gaps[j].hypo_idx;
                if (a < s->hypo_count && b < s->hypo_count) {
                    int as = ikitaku_associate(&s->hypos[a], &s->hypos[b], NULL, 0);
                    if (as >= 50) related++;
                }
            }
        }
    }
    snprintf(out_note, (size_t)note_len,
             "consider: %d gaps joint_sev=%d; priority close [%d] missing=%s sev=%d; "
             "related_pairs=%d → %s",
             gaps->gap_n, gaps->joint_severity, best_i,
             gaps->gaps[best_i].missing, best_sev, related,
             related > 0 ? "multi-var: close coupled gaps together"
                         : "single-var: attack highest severity first");
    return best_i;
}



// ===================== Pseudo-English debug =====================

int ikitaku_english_report(const ikitaku_session_t* s,
                           const ikitaku_cluster_report_t* cr,
                           const ikitaku_gap_report_t* gaps,
                           char* buf, int buf_len) {
    if (!buf || buf_len < 16) return 0;
    int u = 0;
    u += snprintf(buf + u, (size_t)(buf_len - u),
        "=== ikitaku in plain language ===\n");
    if (!s) return u;
    u += snprintf(buf + u, (size_t)(buf_len - u),
        "I formed %d hypotheses about this program", s->hypo_count);
    if (cr) u += snprintf(buf + u, (size_t)(buf_len - u),
        ", grouped into %d evidence clusters", cr->cluster_n);
    u += snprintf(buf + u, (size_t)(buf_len - u), ".\n");

    if (s->accepted >= 0 && s->accepted < s->hypo_count) {
        const ikitaku_hypo_t* a = &s->hypos[s->accepted];
        u += snprintf(buf + u, (size_t)(buf_len - u),
            "Best reading: this looks like **%s** (form %s), confidence %d%%. "
            "Rewrite is %s.\n",
            a->purpose, a->form_id, a->confidence,
            a->rewrite_ok ? "allowed" : "blocked");
        if (a->purpose_alt[0])
            u += snprintf(buf + u, (size_t)(buf_len - u),
                "Parallel reading: also like **%s** (conf %d) — two patterns at once.\n",
                a->purpose_alt, a->parallel_conf);
    } else {
        u += snprintf(buf + u, (size_t)(buf_len - u),
            "No reading was confident enough to accept.\n");
    }

    if (cr) {
        for (int i = 0; i < cr->cluster_n && u < buf_len - 80; ++i) {
            const ikitaku_cluster_t* c = &cr->clusters[i];
            u += snprintf(buf + u, (size_t)(buf_len - u),
                "Cluster %d gathers %d pieces about **%s** (similarity avg %d). ",
                c->id, c->member_n, c->purpose, c->sim_avg);
            if (c->dialectic_resolved)
                u += snprintf(buf + u, (size_t)(buf_len - u),
                    "A pure vs impure conflict was resolved toward the stricter claim. ");
            if (c->state == IKITAKU_CL_PROMOTED)
                u += snprintf(buf + u, (size_t)(buf_len - u),
                    "Enough evidence to propose R5 concept '%s'.", c->purpose);
            u += snprintf(buf + u, (size_t)(buf_len - u), "\n");
        }
    }

    if (gaps && gaps->gap_n > 0) {
        u += snprintf(buf + u, (size_t)(buf_len - u),
            "Information gaps (%d), joint severity %d:\n",
            gaps->gap_n, gaps->joint_severity);
        for (int i = 0; i < gaps->gap_n && u < buf_len - 60; ++i) {
            u += snprintf(buf + u, (size_t)(buf_len - u),
                "  - Missing **%s** (severity %d): %s\n",
                gaps->gaps[i].missing, gaps->gaps[i].severity,
                gaps->gaps[i].why[0] ? gaps->gaps[i].why : "(no why)");
        }
        if (gaps->consider_note[0])
            u += snprintf(buf + u, (size_t)(buf_len - u),
                "Next step: %s\n", gaps->consider_note);
    } else {
        u += snprintf(buf + u, (size_t)(buf_len - u),
            "No significant information gaps — the reading is informationally complete enough.\n");
    }
    return u;
}

// ===================== Data series pattern analysis =====================

int ikitaku_assoc_poles(int assoc_0_100, int shared_j,
                        int recombine_possible_conf,
                        ikitaku_assoc_delta_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (assoc_0_100 < 0) assoc_0_100 = 0;
    if (assoc_0_100 > 100) assoc_0_100 = 100;
    out->assoc = assoc_0_100;
    int to_syn = 100 - assoc_0_100;
    int to_ant = assoc_0_100;
    out->delta = to_syn < to_ant ? to_syn : to_ant;
    if (assoc_0_100 >= 70) out->pole = IKITAKU_POLE_NEAR;
    else if (assoc_0_100 <= 30) out->pole = IKITAKU_POLE_FAR;
    else out->pole = IKITAKU_POLE_MID;

    out->mid = 50;
    out->shared_avg = shared_j;
    /* Educated median: only sit in the middle if mid is closer to a
       usable pole than "both wrong". Fallacy of moderation = mid is
       farther from BOTH the synonym and the inverse than the raw score. */
    int mid_to_syn = 50;
    int mid_to_ant = 50;
    int raw_best = to_syn < to_ant ? to_syn : to_ant;
    int mid_best = mid_to_syn < mid_to_ant ? mid_to_syn : mid_to_ant;
    out->mid_is_fallacy = (out->pole != IKITAKU_POLE_MID && mid_best >= raw_best) ? 1 : 0;
    out->educated = out->mid_is_fallacy ? assoc_0_100 : 50;

    /* As wrong as possible → invert recovers (emit↔absorb, +x ↔ -x). */
    out->invert_recovers = (assoc_0_100 <= 15) ? 1 : 0;

    /* Recombine like desire: only if not-impossible. */
    out->recombine_ok = (recombine_possible_conf >= 35 &&
                         out->pole != IKITAKU_POLE_FAR) ? 1 : 0;

    const char* pname = out->pole==IKITAKU_POLE_NEAR ? "synonym"
                      : out->pole==IKITAKU_POLE_FAR ? "antonym" : "mid";
    snprintf(out->english, sizeof out->english,
             "assoc=%d pole=%s Δ=%d mid=%d shared=%d educated=%s invert=%s recombine=%s",
             out->assoc, pname, out->delta, out->mid, out->shared_avg,
             out->mid_is_fallacy ? "reject-moderation" : "ok-mid",
             out->invert_recovers ? "apply-inverse" : "no",
             out->recombine_ok ? "allowed" : "gated");
    return out->pole;
}


int ikitaku_form_layer_guess(const char* form) {
    if (!form || !form[0]) return 5;
    /* R6 strategies */
    if (!strcmp(form, "scientific") || !strcmp(form, "hegelian") ||
        !strcmp(form, "occam") || !strcmp(form, "theorize") ||
        !strcmp(form, "origin") || !strcmp(form, "bayesian"))
        return 6;
    /* R5 platonics / object verbs */
    static const char* r5[] = {
        "map","reduce","repeat","decision","wait","info","order","seek",
        "split","join","emit","absorb","invert","bound","filter","generate",
        "compare","sequence","copy","destroy","name","create","sequence",
        NULL
    };
    for (int i = 0; r5[i]; ++i)
        if (!strcmp(form, r5[i])) return 5;
    /* R4 families / surface-ish */
    if (strstr(form, "_x_") || strstr(form, "loop") || strstr(form, "branch") ||
        strstr(form, "linear") || strstr(form, "cycle") || strstr(form, "sort") ||
        strstr(form, "trap") || strstr(form, "delay") || strstr(form, "affine"))
        return 4;
    /* R3 genus merge names often name_x_name already caught; species-ish */
    if (strstr(form, "micro") || strstr(form, "slot") || strstr(form, "opcode"))
        return 2;
    return 4; /* default family band */
}

static int vert_weak_jaccard(const char* Fa, const char* Fb, int dist,
                             int* shared_out, char* tip_a, int ta_len,
                             char* tip_b, int tb_len) {
    /* Weak Jaccard: share abstract tip after layer_flow DOWN, diluted by |layer| dist.
       Same tip → high share; related affinity → mid; else low. */
    if (shared_out) *shared_out = 0;
    if (tip_a && ta_len > 0) tip_a[0] = 0;
    if (tip_b && tb_len > 0) tip_b[0] = 0;

    ikitaku_layer_drop_t fa, fb;
    memset(&fa, 0, sizeof fa);
    memset(&fb, 0, sizeof fb);
    int la = 5, lb = 5;
    ikitaku_layer_flow(Fa ? Fa : "map", 5, 2, IKITAKU_FLOW_DOWN, &fa);
    ikitaku_layer_flow(Fb ? Fb : "map", 5, 2, IKITAKU_FLOW_DOWN, &fb);
    const char* ta = fa.abstract_r5[0] ? fa.abstract_r5 :
                     (fa.broader[0] ? fa.broader : (Fa ? Fa : "map"));
    const char* tb = fb.abstract_r5[0] ? fb.abstract_r5 :
                     (fb.broader[0] ? fb.broader : (Fb ? Fb : "map"));
    if (tip_a && ta_len > 0) snprintf(tip_a, ta_len, "%s", ta);
    if (tip_b && tb_len > 0) snprintf(tip_b, tb_len, "%s", tb);

    int share = 0;
    if (ta[0] && tb[0] && !strcmp(ta, tb)) {
        share = 1;
        if (shared_out) *shared_out = 1;
    }
    float aff = ikitaku_affinity(Fa, Fb);
    float aff_tip = ikitaku_affinity(ta, tb);
    /* Jaccard-ish soft: intersection proxy / union proxy */
    float inter = share ? 0.55f : (0.25f * aff + 0.35f * aff_tip);
    float uni = 1.0f;
    float j = inter / uni;
    if (j > 1.0f) j = 1.0f;
    /* dilute by layer distance: each step halves vertical weight */
    float dil = 1.0f;
    for (int d = 0; d < dist && d < 6; ++d) dil *= 0.55f;
    j *= dil;
    /* floor for any cross-layer deliberate tactic */
    if (dist > 0 && j < 0.08f) j = 0.08f;
    return (int)(j * 100.0f + 0.5f);
}

int ikitaku_associate_axis(const char* form_a, int layer_a,
                           const char* form_b, int layer_b,
                           ikitaku_assoc_axis_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (layer_a < 0) layer_a = ikitaku_form_layer_guess(form_a);
    if (layer_b < 0) layer_b = ikitaku_form_layer_guess(form_b);
    out->layer_a = layer_a;
    out->layer_b = layer_b;
    out->layer_dist = layer_a > layer_b ? layer_a - layer_b : layer_b - layer_a;

    /* Horizontal component: same-layer affinity + string overlap proxy */
    float aff = ikitaku_affinity(form_a, form_b);
    int horiz = (int)(aff * 100.0f + 0.5f);
    if (form_a && form_b && !strcmp(form_a, form_b)) horiz = 100;
    /* weak same-band boost if layers equal */
    if (out->layer_dist == 0) {
        if (horiz < 15) horiz = 15; /* minimal same-layer presence */
    } else {
        /* horizontal is weak when layers differ */
        horiz = (int)(horiz * 0.35f);
    }
    out->horiz = horiz < 0 ? 0 : (horiz > 100 ? 100 : horiz);

    int shared = 0;
    int wj = vert_weak_jaccard(form_a, form_b, out->layer_dist, &shared,
                               out->tip_a, sizeof out->tip_a,
                               out->tip_b, sizeof out->tip_b);
    out->weak_jaccard = wj;
    out->shared_abstract = shared;
    out->vert = out->layer_dist > 0 ? wj : (wj / 3); /* vertical only counts cross-layer */

    if (out->layer_dist == 0) {
        out->kind = IKITAKU_ASSOC_HORIZ;
        out->tactic = -1;
        out->assoc = out->horiz;
    } else {
        out->kind = (out->horiz >= 25 && out->vert >= 20)
            ? IKITAKU_ASSOC_MIXED : IKITAKU_ASSOC_VERT;
        out->tactic = (layer_a > layer_b) ? IKITAKU_FLOW_DOWN : IKITAKU_FLOW_UP;
        /* combined: vertical is primary when dist>0, horiz secondary */
        out->assoc = (int)(0.55f * out->vert + 0.25f * out->horiz
                           + 0.20f * (shared ? 80 : 30));
        if (out->assoc > 100) out->assoc = 100;
    }

    snprintf(out->english, sizeof out->english,
             "assoc_axis kind=%s L%d↔L%d dist=%d horiz=%d vert=%d weak_jac=%d "
             "shared=%d tip=%s/%s tactic=%s score=%d",
             out->kind == IKITAKU_ASSOC_HORIZ ? "HORIZ" :
             out->kind == IKITAKU_ASSOC_VERT ? "VERT" : "MIXED",
             out->layer_a, out->layer_b, out->layer_dist,
             out->horiz, out->vert, out->weak_jaccard, out->shared_abstract,
             out->tip_a[0] ? out->tip_a : "-",
             out->tip_b[0] ? out->tip_b : "-",
             out->tactic == IKITAKU_FLOW_DOWN ? "OPEN/derive-down" :
             out->tactic == IKITAKU_FLOW_UP ? "CLOSE/anti-derive-up" : "same",
             out->assoc);
    return out->assoc;
}

int ikitaku_associate_vertical(const ikitaku_hypo_t* a, int layer_a,
                               const ikitaku_hypo_t* b, int layer_b,
                               ikitaku_assoc_axis_t* out) {
    if (!a || !b || !out) return -1;
    const char* fa = a->form_id[0] ? a->form_id : a->purpose;
    const char* fb = b->form_id[0] ? b->form_id : b->purpose;
    return ikitaku_associate_axis(fa, layer_a, fb, layer_b, out);
}

int ikitaku_assoc_bag_tactic(const char* outer_form, int outer_layer,
                             const char* inner_form, int inner_layer,
                             int open_not_close,
                             ikitaku_assoc_axis_t* out) {
    if (!out) return -1;
    /* open = derive into interior (focus down); close = wrap as unit (up) */
    if (outer_layer < 0) outer_layer = ikitaku_form_layer_guess(outer_form);
    if (inner_layer < 0) inner_layer = ikitaku_form_layer_guess(inner_form);
    /* For open: outer is macro, inner micro — force outer > inner if needed */
    if (open_not_close) {
        if (outer_layer <= inner_layer) outer_layer = inner_layer + 1;
        if (outer_layer > 7) outer_layer = 7;
    } else {
        /* close: inner becomes part of outer unit — outer should be higher or equal */
        if (outer_layer < inner_layer) outer_layer = inner_layer;
    }
    int score = ikitaku_associate_axis(outer_form, outer_layer,
                                       inner_form, inner_layer, out);
    out->tactic = open_not_close ? IKITAKU_FLOW_DOWN : IKITAKU_FLOW_UP;
    if (open_not_close)
        out->kind = IKITAKU_ASSOC_VERT;
    size_t el = strlen(out->english);
    if (el + 80 < sizeof out->english)
        snprintf(out->english + el, sizeof out->english - el,
                 " | bag_%s composition/extract",
                 open_not_close ? "OPEN(extract/derive)" : "CLOSE(build/anti-derive)");
    return score;
}
