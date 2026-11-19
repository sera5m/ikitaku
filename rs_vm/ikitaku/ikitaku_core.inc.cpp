    if (!out || max_out <= 0) return 0;
    FILE* f = fopen("/home/workdir/artifacts/rs_vm/ikitaku_learned.cat", "r");
    if (!f) f = fopen(IKITAKU_LEARNED_PATH, "r");
    if (!f) return 0;
    int n = 0;
    char line[256];
    while (n < max_out && fgets(line, sizeof line, f)) {
        char* p1 = strchr(line, '|'); if (!p1) continue;
        *p1 = 0; char* p2 = strchr(p1 + 1, '|'); if (!p2) continue;
        *p2 = 0; char* p3 = strchr(p2 + 1, '|'); if (!p3) continue;
        *p3 = 0;
        snprintf(out[n].form_id, sizeof out[n].form_id, "%s", line);
        snprintf(out[n].purpose, sizeof out[n].purpose, "%s", p1 + 1);
        out[n].times_seen = atoi(p2 + 1);
        char* nl = strchr(p3 + 1, '\n'); if (nl) *nl = 0;
        snprintf(out[n].note, sizeof out[n].note, "%s", p3 + 1);
        n++;
    }
    fclose(f);
    return n;
}

int ikitaku_learned_save_append(const ikitaku_learned_t* e) {
    if (!e) return -1;
    FILE* f = fopen("/home/workdir/artifacts/rs_vm/ikitaku_learned.cat", "a");
    if (!f) return -1;
    fprintf(f, "%s|%s|%d|%s\n", e->form_id, e->purpose, e->times_seen, e->note);
    fclose(f);
    return 0;
}

int ikitaku_dialectic(const ikitaku_hypo_t* thesis,
                      const ikitaku_hypo_t* antithesis,
                      ikitaku_hypo_t* synthesis_out) {
    if (!thesis || !antithesis || !synthesis_out) return 0;
    memset(synthesis_out, 0, sizeof *synthesis_out);
    synthesis_out->method = IKITAKU_THINK_DIALECTIC;
    synthesis_out->parent_a = synthesis_out->parent_b = -1;

    if (strcmp(thesis->purpose, antithesis->purpose) == 0) {
        const ikitaku_hypo_t* pure_side = thesis->rewrite_ok ? thesis : antithesis;
        const ikitaku_hypo_t* impure_side = thesis->rewrite_ok ? antithesis : thesis;
        if (pure_side->rewrite_ok && !impure_side->rewrite_ok) {
            *synthesis_out = *impure_side;
            synthesis_out->method = IKITAKU_THINK_DIALECTIC;
            snprintf(synthesis_out->form_id, sizeof synthesis_out->form_id,
                     "%s_resolved", impure_side->form_id);
            snprintf(synthesis_out->note, sizeof synthesis_out->note,
                     "dialectic: pure-rewrite negated by impure evidence -> keep impure");
            synthesis_out->confidence = (thesis->confidence + antithesis->confidence) / 2 + 5;
            if (synthesis_out->confidence > 95) synthesis_out->confidence = 95;
            synthesis_out->feasible = 1;
            synthesis_out->rewrite_ok = 0;
            return 1;
        }
    }
    if ((strcmp(thesis->purpose, "decision") == 0 && strcmp(antithesis->purpose, "wait") == 0)
        || (strcmp(thesis->purpose, "wait") == 0 && strcmp(antithesis->purpose, "decision") == 0)) {
        snprintf(synthesis_out->form_id, sizeof synthesis_out->form_id, "decision_then_wait");
        snprintf(synthesis_out->purpose, sizeof synthesis_out->purpose, "repeat");
        snprintf(synthesis_out->note, sizeof synthesis_out->note,
                 "dialectic: branch + wait under repeat (sequential compose)");
        synthesis_out->confidence = 72;
        synthesis_out->feasible = 1;
        synthesis_out->rewrite_ok = 0;
        synthesis_out->span_start = thesis->span_start < antithesis->span_start
            ? thesis->span_start : antithesis->span_start;
        synthesis_out->span_end = thesis->span_end > antithesis->span_end
            ? thesis->span_end : antithesis->span_end;
        return 1;
    }
    if ((strcmp(thesis->purpose, "seek") == 0 && strcmp(antithesis->purpose, "join") == 0)
        || (strcmp(thesis->purpose, "join") == 0 && strcmp(antithesis->purpose, "seek") == 0)
        || (strstr(thesis->form_id, "own") && strstr(antithesis->form_id, "joint"))
        || (strstr(thesis->form_id, "joint") && strstr(antithesis->form_id, "own"))) {
        snprintf(synthesis_out->form_id, sizeof synthesis_out->form_id, "goal_conflict");
        snprintf(synthesis_out->purpose, sizeof synthesis_out->purpose, "decision");
        snprintf(synthesis_out->note, sizeof synthesis_out->note,
                 "dialectic: two goals both useful; R7 right-task is not singular; need goal-priority");
        synthesis_out->confidence = 80;
        synthesis_out->feasible = 0;
        synthesis_out->rewrite_ok = 0;
        synthesis_out->ambiguous = 1;
        return 1;
    }
    
    /* Structured hypos: copy form_id/span/rewrite from parents so synthesis is non-empty */
    {
        ikitaku_hypo_t syn;
        memset(&syn, 0, sizeof syn);
        if (ikitaku_synthesize_F(thesis->purpose, antithesis->purpose,
                                 NULL, thesis->span_start, thesis->span_end, &syn)
            && syn.confidence >= 50
            && strcmp(syn.form_id, "synth_rejected") != 0) {
            *synthesis_out = syn;
            synthesis_out->method = IKITAKU_THINK_DIALECTIC;
            synthesis_out->synthesized = 1;
            synthesis_out->span_start = thesis->span_start < antithesis->span_start
                ? thesis->span_start : antithesis->span_start;
            synthesis_out->span_end = thesis->span_end > antithesis->span_end
                ? thesis->span_end : antithesis->span_end;
            if (thesis->form_id[0] && antithesis->form_id[0]) {
                char note2[IKITAKU_NOTE_LEN];
                snprintf(note2, sizeof note2,
                         "dialectic structured: %s⊗%s via %s (forms %s/%s)",
                         thesis->purpose, antithesis->purpose, syn.form_id,
                         thesis->form_id, antithesis->form_id);
                snprintf(synthesis_out->note, sizeof synthesis_out->note, "%s", note2);
            }
            synthesis_out->confidence = (syn.confidence
                + (thesis->confidence + antithesis->confidence) / 2) / 2;
            if (synthesis_out->confidence > 95) synthesis_out->confidence = 95;
            if (synthesis_out->confidence < 50) synthesis_out->confidence = 50;
            return 1;
        }
    }
    /* Minimal structured synthesis even without rule: keep higher-conf purpose, tag form */
    {
        const ikitaku_hypo_t* win = thesis->confidence >= antithesis->confidence
            ? thesis : antithesis;
        *synthesis_out = *win;
        synthesis_out->method = IKITAKU_THINK_DIALECTIC;
        synthesis_out->synthesized = 1;
        snprintf(synthesis_out->form_id, sizeof synthesis_out->form_id,
                 "%s_x_%s", thesis->purpose, antithesis->purpose);
        snprintf(synthesis_out->note, sizeof synthesis_out->note,
                 "dialectic fallback structured: prefer conf=%d purpose=%s",
                 win->confidence, win->purpose);
        synthesis_out->confidence = (thesis->confidence + antithesis->confidence) / 2;
        if (synthesis_out->confidence < 40) synthesis_out->confidence = 40;
        synthesis_out->feasible = 1;
        return 1;
    }

    return 0;
}

int ikitaku_dialectic_hunt(const ikitaku_hypo_t* thesis,
                           const ikitaku_hypo_t* antithesis,
                           ikitaku_hypo_t* synthesis_out,
                           struct ikitaku_hunt_t* hunt) {
    int ok = ikitaku_dialectic(thesis, antithesis, synthesis_out);
    if (!hunt || !thesis || !antithesis) return ok;
    ikitaku_hunt_init(hunt,
                      thesis->purpose[0] ? thesis->purpose : thesis->form_id,
                      antithesis->purpose[0] ? antithesis->purpose : antithesis->form_id);
    ikitaku_hunt_arrive(hunt, "pos", thesis->confidence);
    ikitaku_hunt_arrive(hunt, "neg", antithesis->confidence);
    if (ok && synthesis_out) {
        int nlen = (int)strlen(synthesis_out->note);
        snprintf(synthesis_out->note + nlen,
                 sizeof(synthesis_out->note) - (size_t)nlen,
                 " | hunt next=%d %s", hunt->next_probe,
                 hunt->invert_side > 0 ? "invert-neg" :
                 hunt->invert_side < 0 ? "invert-pos" : "mid");
        if (hunt->solved && synthesis_out->confidence < 90)
            synthesis_out->confidence += 4;
    }
    return ok;
}

int ikitaku_synthesize_F(const char* Fa, const char* Fb,
                         const rsvm_t* vm, uint16_t start, uint16_t end,
                         ikitaku_hypo_t* out) {
    if (!Fa || !Fb || !out) return 0;
    memset(out, 0, sizeof *out);
    out->method = IKITAKU_THINK_SYNTHESIS;
    out->synthesized = 1;
    out->span_start = start;
    out->span_end = end;
    out->parent_a = out->parent_b = -1;
    out->structure_hit = -1;

    const char *A = Fa, *B = Fb;
    if (strcmp(A, B) > 0) { const char* t = A; A = B; B = t; }

    if (strcmp(A, "repeat") == 0 && strcmp(B, "wait") == 0) {
        int has = vm ? span_has_delay(vm, start, end) : 1;
        snprintf(out->form_id, sizeof out->form_id, "repeat_x_wait");
        snprintf(out->purpose, sizeof out->purpose, "repeat");
        snprintf(out->note, sizeof out->note,
                 "synthesis FxF: repeat x wait -> timed_loop; rewrite=no");
        out->confidence = has ? 88 : 55;
        out->feasible = has ? 1 : 0;
        out->rewrite_ok = 0;
        return 1;
    }
    if (strcmp(A, "decision") == 0 && strcmp(B, "repeat") == 0) {
        int has = vm ? span_has_cmp(vm, start, end) : 1;
        snprintf(out->form_id, sizeof out->form_id, "repeat_x_decision");
        snprintf(out->purpose, sizeof out->purpose, "repeat");
        snprintf(out->note, sizeof out->note,
                 "synthesis: repeat x decision -> loop with branched body");
        out->confidence = has ? 85 : 50;
        out->feasible = has ? 1 : 0;
        out->rewrite_ok = 0;
        return 1;
    }
    if (strcmp(A, "decision") == 0 && strcmp(B, "wait") == 0) {
        snprintf(out->form_id, sizeof out->form_id, "decision_x_wait");
        snprintf(out->purpose, sizeof out->purpose, "decision");
        snprintf(out->note, sizeof out->note,
                 "synthesis: decision x wait -> branch then block");
        out->confidence = 70;
        out->feasible = 1;
        out->rewrite_ok = 0;
        return 1;
    }
    if (strcmp(A, "info") == 0 && strcmp(B, "repeat") == 0) {
        snprintf(out->form_id, sizeof out->form_id, "repeat_x_info");
        snprintf(out->purpose, sizeof out->purpose, "reduce");
        snprintf(out->note, sizeof out->note,
                 "synthesis: repeat x info -> reduction candidate");
        out->confidence = 75;
        out->feasible = 1;
        out->rewrite_ok = 1;
        return 1;
    }
    /* Generic pairs — thicken beyond sparse specials */
    if (strcmp(A, "map") == 0 && strcmp(B, "order") == 0) {
        snprintf(out->form_id, sizeof out->form_id, "map_x_order");
        snprintf(out->purpose, sizeof out->purpose, "order");
        snprintf(out->note, sizeof out->note,
                 "synthesis: map x order -> rearrange by key after transform");
        out->confidence = 78;
        out->feasible = 1;
        out->rewrite_ok = 1;
        return 1;
    }
    if (strcmp(A, "map") == 0 && strcmp(B, "repeat") == 0) {
        snprintf(out->form_id, sizeof out->form_id, "map_x_repeat");
        snprintf(out->purpose, sizeof out->purpose, "map");
        snprintf(out->note, sizeof out->note,
                 "synthesis: map x repeat -> elementwise along axis");
        out->confidence = 80;
        out->feasible = 1;
        out->rewrite_ok = 1;
        return 1;
    }
    if (strcmp(A, "map") == 0 && strcmp(B, "filter") == 0) {
        snprintf(out->form_id, sizeof out->form_id, "filter_x_map");
        snprintf(out->purpose, sizeof out->purpose, "filter");
        snprintf(out->note, sizeof out->note,
                 "synthesis: filter x map -> select then transform");
        out->confidence = 76;
        out->feasible = 1;
        out->rewrite_ok = 1;
        return 1;
    }
    if (strcmp(A, "join") == 0 && strcmp(B, "map") == 0) {
        snprintf(out->form_id, sizeof out->form_id, "join_x_map");
        snprintf(out->purpose, sizeof out->purpose, "join");
        snprintf(out->note, sizeof out->note,
                 "synthesis: join x map -> zip/concat then transform");
        out->confidence = 74;
        out->feasible = 1;
        out->rewrite_ok = 1;
        return 1;
    }
    if (strcmp(A, "seek") == 0 && strcmp(B, "map") == 0) {
        snprintf(out->form_id, sizeof out->form_id, "map_x_seek");
        snprintf(out->purpose, sizeof out->purpose, "seek");
        snprintf(out->note, sizeof out->note,
                 "synthesis: seek x map -> find then transform witness");
        out->confidence = 72;
        out->feasible = 1;
        out->rewrite_ok = 1;
        return 1;
    }
    if (strcmp(A, "bound") == 0 && strcmp(B, "map") == 0) {
        snprintf(out->form_id, sizeof out->form_id, "bound_x_map");
        snprintf(out->purpose, sizeof out->purpose, "bound");
        snprintf(out->note, sizeof out->note,
                 "synthesis: bound x map -> constrained transform");
        out->confidence = 70;
        out->feasible = 1;
        out->rewrite_ok = 1;
        return 1;
    }
    if (strcmp(A, "invert") == 0 && strcmp(B, "map") == 0) {
        snprintf(out->form_id, sizeof out->form_id, "invert_x_map");
        snprintf(out->purpose, sizeof out->purpose, "invert");
        snprintf(out->note, sizeof out->note,
                 "synthesis: invert x map -> undo then re-map");
        out->confidence = 73;
        out->feasible = 1;
        out->rewrite_ok = 1;
        return 1;
    }
    if (strcmp(A, "order") == 0 && strcmp(B, "repeat") == 0) {
        snprintf(out->form_id, sizeof out->form_id, "order_x_repeat");
        snprintf(out->purpose, sizeof out->purpose, "order");
        snprintf(out->note, sizeof out->note,
                 "synthesis: order x repeat -> sort-like multi-pass");
        out->confidence = 77;
        out->feasible = 1;
        out->rewrite_ok = 0;
        return 1;
    }
    if (strcmp(A, "sequence") == 0 && strcmp(B, "map") == 0) {
        snprintf(out->form_id, sizeof out->form_id, "map_x_sequence");
        snprintf(out->purpose, sizeof out->purpose, "sequence");
        snprintf(out->note, sizeof out->note,
                 "synthesis: sequence x map -> staged transforms");
        out->confidence = 75;
        out->feasible = 1;
        out->rewrite_ok = 1;
        return 1;
    }
    if (strcmp(A, "theorize") == 0 && strcmp(B, "map") == 0) {
        snprintf(out->form_id, sizeof out->form_id, "map_x_theorize");
        snprintf(out->purpose, sizeof out->purpose, "theorize");
        snprintf(out->note, sizeof out->note,
                 "synthesis: theorize x map -> claim about a transform");
        out->confidence = 68;
        out->feasible = 1;
        out->rewrite_ok = 1;
        return 1;
    }
    /* Generic antiderivative merge: always produce a form (F_merge style) */
    {
        snprintf(out->form_id, sizeof out->form_id, "%s_x_%s", A, B);
        snprintf(out->purpose, sizeof out->purpose, "%s", A);
        snprintf(out->note, sizeof out->note,
                 "synthesis generic: %s ⊗ %s -> composite (antiderivative merge)",
                 A, B);
        out->confidence = 58;
        out->feasible = 1;
        out->rewrite_ok = 1;
        out->novel = 1;
        return 1;
    }

}

int ikitaku_specialize(const rsvm_t* vm, const char* purpose_F,
                       uint16_t start, uint16_t end, ikitaku_hypo_t* out) {
    if (!vm || !purpose_F || !out) return 0;
    memset(out, 0, sizeof *out);
    snprintf(out->purpose, sizeof out->purpose, "%s", purpose_F);
    out->span_start = start;
    out->span_end = end;
    out->structure_hit = -1;
    out->method = IKITAKU_THINK_DEDUCTIVE;
    out->feasible = 1;
    out->parent_a = out->parent_b = -1;

    int delay = span_has_delay(vm, start, end);
    int pr = span_has_print(vm, start, end);
    int cmp = span_has_cmp(vm, start, end);
    int trap = span_has_trap(vm, start, end);
    int inc = span_has_inc(vm, start, end);

    if (strcmp(purpose_F, "wait") == 0) {
        if (delay) {
            snprintf(out->form_id, sizeof out->form_id, "wait_delay");
            snprintf(out->note, sizeof out->note,
                     "context: DELAY_MS -> f=delay_block + c=ms; pure=0 external");
            out->confidence = 92;
            out->rewrite_ok = 0;
            return 1;
        }
        snprintf(out->form_id, sizeof out->form_id, "wait_unknown");
        snprintf(out->note, sizeof out->note, "F=wait, no delay beacon");
        out->confidence = 35;
        out->feasible = 0;
        return 1;
    }
    if (strcmp(purpose_F, "repeat") == 0) {
        if (delay && trap) {
            snprintf(out->form_id, sizeof out->form_id, "repeat_trap_timed");
            snprintf(out->note, sizeof out->note,
                     "context: TRAP+DELAY -> timed trap; rewrite forbidden");
            out->confidence = 90;
            out->rewrite_ok = 0;
            return 1;
        }
        if (inc && !delay && !pr) {
            snprintf(out->form_id, sizeof out->form_id, "repeat_counted_inc");
            snprintf(out->note, sizeof out->note,
                     "context: pure INC -> counted_inc; rewrite ok");
            out->confidence = 93;
            out->rewrite_ok = 1;
            return 1;
        }
        if (cmp) {
            snprintf(out->form_id, sizeof out->form_id, "repeat_with_branch");
            snprintf(out->note, sizeof out->note,
                     "context: cmp in body -> loop x decision");
            out->confidence = 82;
            out->rewrite_ok = 0;
            return 1;
        }
        if (pr || delay) {
            snprintf(out->form_id, sizeof out->form_id, "repeat_impure");
            snprintf(out->note, sizeof out->note, "context: IO/wait -> impure loop");
            out->confidence = 80;
            out->rewrite_ok = 0;
            return 1;
        }
        snprintf(out->form_id, sizeof out->form_id, "repeat_generic");
        snprintf(out->note, sizeof out->note, "context: weak beacons");
        out->confidence = 60;
        out->rewrite_ok = 0;
        return 1;
    }
    if (strcmp(purpose_F, "decision") == 0) {
        if (cmp) {
            snprintf(out->form_id, sizeof out->form_id, "decision_binary_cmp");
            snprintf(out->note, sizeof out->note,
                     "context: cmp ops -> binary_branch + c=bodies");
            out->confidence = 88;
            out->rewrite_ok = 1;
            return 1;
        }
        snprintf(out->form_id, sizeof out->form_id, "decision_generic");
        snprintf(out->note, sizeof out->note, "F=decision without cmp");
        out->confidence = 45;
        out->feasible = 0;
        return 1;
    }
    if (strcmp(purpose_F, "reduce") == 0 && inc) {
        snprintf(out->form_id, sizeof out->form_id, "reduce_counted_inc");
        snprintf(out->note, sizeof out->note, "context: INC -> O(1) candidate");
        out->confidence = 92;
        out->rewrite_ok = !delay && !pr;
        return 1;
    }
    snprintf(out->form_id, sizeof out->form_id, "specialize_fail");
    snprintf(out->note, sizeof out->note, "no context rule for F=%s", purpose_F);
    out->confidence = 15;
    out->novel = 1;
    out->feasible = 0;
    return 0;
}

static ikitaku_hypo_t hypo_from_hit(const rsvm_structure_hit_t* hit, int idx,
                                    ikitaku_think_t method, int cost) {
    ikitaku_hypo_t h;
    memset(&h, 0, sizeof h);
    snprintf(h.form_id, sizeof h.form_id, "%s", hit->kind);
    snprintf(h.purpose, sizeof h.purpose, "%s", hit->purpose);
    snprintf(h.note, sizeof h.note, "%s", hit->general_form);
    h.confidence = hit->confidence;
    h.feasible = 1;
    h.cost = cost;
    h.score = score_of(hit->confidence, cost);
    h.method = (uint8_t)method;
    h.rewrite_ok = (uint8_t)rsvm_hit_rewrite_allowed(hit);
    h.span_start = hit->region_start;
    h.span_end = hit->region_end;
    h.structure_hit = (int16_t)idx;
    h.parent_a = h.parent_b = -1;
    h.micro_schema = hit->micro_schema;
    return h;
}

int ikitaku_analyze(const rsvm_t* vm, const ikitaku_config_t* cfg,
                    ikitaku_session_t* out) {
    if (!vm || !out) return -1;
    ikitaku_config_t local;
    if (!cfg) { ikitaku_config_default(&local); cfg = &local; }
    memset(out, 0, sizeof *out);
    out->accepted = -1;

    logf(out, "[ikitaku] budget=%d synth=%d learn=%d\n",
         cfg->attention_desire, cfg->enable_synthesis, cfg->enable_learn);

    ikitaku_learned_t learned[IKITAKU_LEARNED_MAX];
    int nlearn = ikitaku_learned_load(learned, IKITAKU_LEARNED_MAX);
    if (nlearn > 0) logf(out, "[ikitaku] loaded %d learned stubs\n", nlearn);

    rsvm_structure_hit_t hits[RSVM_STRUCT_MAX_HITS];
    int nh = rsvm_analyze_structures(vm, hits, RSVM_STRUCT_MAX_HITS);
    logf(out, "[ikitaku] structure hits=%d\n", nh);

    for (int i = 0; i < nh; ++i) {
        if (!pay(out, cfg, cfg->cost_simple, 0)) {
            logf(out, "  budget exhausted at hit %d\n", i);
            break;
        }
        ikitaku_think_t m = hits[i].confidence >= cfg->min_accept_conf
            ? IKITAKU_THINK_DEDUCTIVE : IKITAKU_THINK_ABDUCTIVE;
        ikitaku_hypo_t h = hypo_from_hit(&hits[i], i, m, cfg->cost_simple);
        if (hits[i].confidence < cfg->min_novel_conf) h.novel = 1;
        push_hypo(out, &h);
        logf(out, "  +[%d] %s F=%s conf=%d score=%d\n",
             out->hypo_count - 1, h.form_id, h.purpose, h.confidence, h.score);
    }

    {
        int d_n = out->hypo_count; // snapshot — no dialectic on dialectic products
        for (int i = 0; i < d_n; ++i) {
            for (int j = i + 1; j < d_n; ++j) {
                if (budget_left(out, cfg) < cfg->cost_dialectic) break;
                if (strcmp(out->hypos[i].purpose, out->hypos[j].purpose) != 0) continue;
                if (out->hypos[i].rewrite_ok == out->hypos[j].rewrite_ok) continue;
                if (!pay(out, cfg, cfg->cost_dialectic, 0)) break;
                ikitaku_hypo_t syn;
                if (ikitaku_dialectic(&out->hypos[i], &out->hypos[j], &syn)) {
                    syn.cost = cfg->cost_dialectic;
                    syn.score = score_of(syn.confidence, syn.cost);
                    syn.parent_a = (int16_t)i;
                    syn.parent_b = (int16_t)j;
                    push_hypo(out, &syn);
                    logf(out, "  dialectic [%d]x[%d] -> %s conf=%d\n",
                         i, j, syn.form_id, syn.confidence);
                }
            }
        }
    }

    if (cfg->enable_synthesis) {
        int hn = out->hypo_count;
        for (int i = 0; i < hn; ++i) {
            for (int j = i + 1; j < hn; ++j) {
                if (budget_left(out, cfg) < cfg->cost_synthesize) break;
                if (strcmp(out->hypos[i].purpose, out->hypos[j].purpose) == 0) continue;
                if (out->hypos[i].purpose[0] == 0 || out->hypos[j].purpose[0] == 0) continue;
                if (strcmp(out->hypos[i].purpose, "unknown") == 0) continue;
                if (strcmp(out->hypos[j].purpose, "unknown") == 0) continue;
                int nest = (out->hypos[i].span_start <= out->hypos[j].span_start
                            && out->hypos[i].span_end >= out->hypos[j].span_end)
                        || (out->hypos[j].span_start <= out->hypos[i].span_start
                            && out->hypos[j].span_end >= out->hypos[i].span_end);
                if (!nest) continue;
                if (!pay(out, cfg, cfg->cost_synthesize, 0)) break;
                uint16_t a = out->hypos[i].span_start < out->hypos[j].span_start
                    ? out->hypos[i].span_start : out->hypos[j].span_start;
                uint16_t b = out->hypos[i].span_end > out->hypos[j].span_end
                    ? out->hypos[i].span_end : out->hypos[j].span_end;
                ikitaku_hypo_t syn;
                if (ikitaku_synthesize_F(out->hypos[i].purpose, out->hypos[j].purpose,
                                         vm, a, b, &syn)) {
                    syn.cost = cfg->cost_synthesize;
                    syn.score = score_of(syn.confidence, syn.cost);
                    syn.parent_a = (int16_t)i;
                    syn.parent_b = (int16_t)j;
                    push_hypo(out, &syn);
                    out->synth_count++;
                    logf(out, "  synth %s x %s -> %s conf=%d\n",
                         out->hypos[i].purpose, out->hypos[j].purpose,
                         syn.form_id, syn.confidence);
                }
            }
        }
    }

    int base_n = out->hypo_count;
    for (int i = 0; i < base_n && budget_left(out, cfg) >= cfg->cost_simple; ++i) {
        if (out->hypos[i].confidence < cfg->min_accept_conf) continue;
        if (strcmp(out->hypos[i].purpose, "unknown") == 0) continue;
        if (!pay(out, cfg, cfg->cost_simple, 0)) break;
        ikitaku_hypo_t sp;
        if (ikitaku_specialize(vm, out->hypos[i].purpose,
                               out->hypos[i].span_start, out->hypos[i].span_end, &sp)) {
            if (strcmp(sp.form_id, out->hypos[i].form_id) != 0) {
                sp.cost = cfg->cost_simple;
                sp.score = score_of(sp.confidence, sp.cost);
                push_hypo(out, &sp);
                logf(out, "  specialize F=%s -> %s conf=%d rew=%u\n",
                     out->hypos[i].purpose, sp.form_id, sp.confidence,
                     (unsigned)sp.rewrite_ok);
            }
        }
    }

    if (nh == 0 && vm->code_len > 0) {
        int cost = cfg->cost_novel * cfg->novelty_focus_boost;
        if (pay(out, cfg, cost, 1)) {
            ikitaku_hypo_t h;
            memset(&h, 0, sizeof h);
            snprintf(h.form_id, sizeof h.form_id, "novel_unknown_program");
            snprintf(h.purpose, sizeof h.purpose, "unknown");
            snprintf(h.note, sizeof h.note, "NOVEL: no plugin matched; learn stub");
            h.confidence = 20;
            h.feasible = 0;
            h.novel = 1;
            h.cost = cost;
            h.score = score_of(20, cost);
            h.method = IKITAKU_THINK_ABDUCTIVE;
            h.span_end = vm->code_len;
            h.parent_a = h.parent_b = -1;
            push_hypo(out, &h);
            out->novel_count++;
            logf(out, "  NOVEL program cost=%d\n", cost);
            if (cfg->enable_learn) {
                ikitaku_learned_t e;
                memset(&e, 0, sizeof e);
                snprintf(e.form_id, sizeof e.form_id, "novel_sparse_arith");
                snprintf(e.purpose, sizeof e.purpose, "info");
                snprintf(e.note, sizeof e.note, "auto: sparse program no regions");
                e.times_seen = 1;
                if (ikitaku_learned_save_append(&e) == 0) {
                    out->learned_added++;
                    logf(out, "  learned stub saved\n");
                }
            }
        }
    }

    // Attribute comparison: partial overlap → mark ambiguous (glass vs ceramic)
    for (int i = 0; i < out->hypo_count; ++i) {
        for (int j = i + 1; j < out->hypo_count; ++j) {
            char note[128];
            int sim = ikitaku_hypo_similarity(&out->hypos[i], &out->hypos[j], note, 128);
            if (sim >= 40 && sim < 70) {
                logf(out, "  compare [%d]~[%d] %s\n", i, j, note);
                // mark the lower-conf one ambiguous if not already strong accept
                int lo = out->hypos[i].confidence <= out->hypos[j].confidence ? i : j;
                if (out->hypos[lo].confidence < cfg->min_accept_conf && !out->hypos[lo].ambiguous) {
                    ikitaku_mark_ambiguous(out, lo, "partial attr overlap vs peer");
                }
            }
        }
    }

    // Promotion: enough L4-similar → propose R5 cluster
    {
        ikitaku_promotion_t promo;
        if (ikitaku_try_promote(out, 2, &promo)) {
            logf(out, "  PROMOTE candidate %s R5=%s conf[%d..%d] evidence=%d\n",
                 promo.form_id, promo.purpose, promo.conf_low, promo.conf_high,
                 promo.evidence_n);
            if (cfg->enable_learn)
                ikitaku_promotion_save(&promo);
        }
    }

    int best = -1, best_s = -9999;
    for (int i = 0; i < out->hypo_count; ++i) {
        if (!out->hypos[i].feasible) continue;
        if (out->hypos[i].confidence < cfg->min_accept_conf) continue;
        if (out->hypos[i].score > best_s) {
            best_s = out->hypos[i].score;
            best = i;
        }
    }
    out->accepted = best;
    if (best >= 0)
        logf(out, "[ikitaku] ACCEPT [%d] %s score=%d conf=%d cost=%d/%d\n",
             best, out->hypos[best].form_id, out->hypos[best].score,
             out->hypos[best].confidence, out->cost_used, cfg->attention_desire);
    else
        logf(out, "[ikitaku] no accept; cost=%d/%d\n",
             out->cost_used, cfg->attention_desire);
    return out->hypo_count;
}

int ikitaku_report(const ikitaku_session_t* s, char* buf, int buf_len) {
    if (!s || !buf || buf_len < 8) return 0;
    int u = 0;
    u += snprintf(buf + u, (size_t)(buf_len - u),
        "=== ikitaku === hypos=%d accepted=%d novel=%d synth=%d learned+%d cost=%d\n",
        s->hypo_count, s->accepted, s->novel_count, s->synth_count,
        s->learned_added, s->cost_used);
    for (int i = 0; i < s->hypo_count && u < buf_len - 100; ++i) {
        const ikitaku_hypo_t* h = &s->hypos[i];
        u += snprintf(buf + u, (size_t)(buf_len - u),
            "  [%d]%s%s%s %s F=%s conf=%d score=%d cost=%d meth=%u rew=%u\n"
            "       %s\n",
            i,
            h->novel ? " NOVEL" : "",
            h->feasible ? "" : " stretch",
            h->synthesized ? " SYNTH" : "",
            h->form_id, h->purpose, h->confidence, h->score, h->cost,
            (unsigned)h->method, (unsigned)h->rewrite_ok,
            h->note);
    }
    if (s->log[0] && u < buf_len - 50)
        u += snprintf(buf + u, (size_t)(buf_len - u), "--- log ---\n%s", s->log);
    return u;
}


// ---- attribute / pattern comparison ----
// Discrete attrs drawn from structure hits: pure, predictable, progress, purpose, family

static int attr_bits_hit(const rsvm_structure_hit_t* h, char tags[][24], int max_tags) {
    int n = 0;
    if (!h || max_tags <= 0) return 0;
#define ADD(s) do { if (n < max_tags) { snprintf(tags[n++], 24, "%s", s); } } while(0)
    ADD(h->pure ? "pure" : "impure");
    if (h->predictable == RSVM_PRED_PREDICTABLE) ADD("pred");
    else if (h->predictable == RSVM_PRED_PARTIAL) ADD("part");
    else ADD("unpred");
    if (h->progress == RSVM_PROG_FINITE) ADD("finite");
    else if (h->progress == RSVM_PROG_DATA) ADD("data");
    else ADD("external");
    if (h->purpose[0]) ADD(h->purpose);
    if (h->family[0]) ADD(h->family);
#undef ADD
    return n;
}

static int attr_bits_hypo(const ikitaku_hypo_t* h, char tags[][24], int max_tags) {
    int n = 0;
    if (!h || max_tags <= 0) return 0;
#define ADD(s) do { if (n < max_tags) { snprintf(tags[n++], 24, "%s", s); } } while(0)
    ADD(h->rewrite_ok ? "rewritable" : "no_rewrite");
    ADD(h->feasible ? "feasible" : "stretch");
    if (h->purpose[0]) ADD(h->purpose);
    if (h->form_id[0]) ADD(h->form_id);
    if (h->novel) ADD("novel");
    if (h->synthesized) ADD("synth");
    if (h->ambiguous) ADD("ambiguous");
#undef ADD
    return n;
}

static int jaccard_tags(char a[][24], int na, char b[][24], int nb,
                        char* shared, int shared_cap,
                        char* only_a, int oa_cap,
                        char* only_b, int ob_cap) {
    int inter = 0, uni = 0;
    int used_s = 0, used_oa = 0, used_ob = 0;
    if (shared && shared_cap) shared[0] = 0;
    if (only_a && oa_cap) only_a[0] = 0;
    if (only_b && ob_cap) only_b[0] = 0;
    for (int i = 0; i < na; ++i) {
        int found = 0;
        for (int j = 0; j < nb; ++j)
            if (strcmp(a[i], b[j]) == 0) { found = 1; break; }
        if (found) {
            inter++;
            if (shared && used_s < shared_cap - 12) {
                used_s += snprintf(shared + used_s, (size_t)(shared_cap - used_s),
                                   "%s%s", used_s ? "," : "", a[i]);
            }
        } else if (only_a && used_oa < oa_cap - 12) {
            used_oa += snprintf(only_a + used_oa, (size_t)(oa_cap - used_oa),
                                "%s%s", used_oa ? "," : "", a[i]);
        }
    }
    for (int j = 0; j < nb; ++j) {
        int found = 0;
        for (int i = 0; i < na; ++i)
            if (strcmp(a[i], b[j]) == 0) { found = 1; break; }
        if (!found && only_b && used_ob < ob_cap - 12) {
            used_ob += snprintf(only_b + used_ob, (size_t)(ob_cap - used_ob),
                                "%s%s", used_ob ? "," : "", b[j]);
        }
    }
    uni = na + nb - inter;
    if (uni <= 0) return 0;
    return (inter * 100) / uni;
}

int ikitaku_attr_similarity(const rsvm_structure_hit_t* a,
                            const rsvm_structure_hit_t* b,
                            char* note_buf, int note_len) {
    if (!a || !b) return 0;
    char ta[12][24], tb[12][24];
    int na = attr_bits_hit(a, ta, 12);
    int nb = attr_bits_hit(b, tb, 12);
    char sh[64], oa[64], ob[64];
    int sim = jaccard_tags(ta, na, tb, nb, sh, 64, oa, 64, ob, 64);
    if (note_buf && note_len > 0) {
        snprintf(note_buf, (size_t)note_len,
                 "sim=%d shared[%s] only_a[%s] only_b[%s] — %s",
                 sim, sh, oa, ob,
                 sim >= 70 ? "same family candidate" :
                 sim >= 40 ? "AMBIGUOUS partial overlap — defer" :
                 "distinct");
    }
    return sim;
}

int ikitaku_hypo_similarity(const ikitaku_hypo_t* a,
                            const ikitaku_hypo_t* b,
                            char* note_buf, int note_len) {
    if (!a || !b) return 0;
    char ta[12][24], tb[12][24];
    int na = attr_bits_hypo(a, ta, 12);
    int nb = attr_bits_hypo(b, tb, 12);
    char sh[64], oa[64], ob[64];
    int sim = jaccard_tags(ta, na, tb, nb, sh, 64, oa, 64, ob, 64);
    // boost if same purpose
    if (a->purpose[0] && strcmp(a->purpose, b->purpose) == 0)
        sim = sim + (100 - sim) / 3;
    if (note_buf && note_len > 0) {
        snprintf(note_buf, (size_t)note_len,
                 "sim=%d shared[%s] only_a[%s] only_b[%s] — %s",
                 sim, sh, oa, ob,
                 sim >= 70 ? "aligned" :
                 sim >= 40 ? "AMBIGUOUS — save for later" : "distant");
    }
    return sim;
}

int ikitaku_forms_commute(const char* Fa, const char* Fb) {
    if (!Fa || !Fb) return 0;
    // synthesis is order-independent for known pairs (A⊗B == B⊗A)
    const char* pairs[][2] = {
        {"repeat", "wait"}, {"decision", "repeat"}, {"decision", "wait"},
        {"info", "repeat"}, {0, 0}
    };
    for (int i = 0; pairs[i][0]; ++i) {
        if ((strcmp(Fa, pairs[i][0]) == 0 && strcmp(Fb, pairs[i][1]) == 0)
            || (strcmp(Fa, pairs[i][1]) == 0 && strcmp(Fb, pairs[i][0]) == 0))
            return 1;
    }
    return 0;
}

int ikitaku_mark_ambiguous(ikitaku_session_t* s, int hypo_i, const char* reason) {
    if (!s || hypo_i < 0 || hypo_i >= s->hypo_count) return -1;
    s->hypos[hypo_i].ambiguous = 1;
    s->hypos[hypo_i].feasible = 0; // not accepted until resolved
    if (reason && reason[0]) {
        char tmp[IKITAKU_NOTE_LEN];
        snprintf(tmp, sizeof tmp, "AMBIGUOUS: %s | %s", reason, s->hypos[hypo_i].note);
        snprintf(s->hypos[hypo_i].note, sizeof s->hypos[hypo_i].note, "%s", tmp);
    }
    logf(s, "  mark ambiguous [%d] %s\n", hypo_i, reason ? reason : "");
    // persist deferred
    ikitaku_learned_t e;
    memset(&e, 0, sizeof e);
    snprintf(e.form_id, sizeof e.form_id, "amb_%s", s->hypos[hypo_i].form_id);
    snprintf(e.purpose, sizeof e.purpose, "%s", s->hypos[hypo_i].purpose);
    snprintf(e.note, sizeof e.note, "%s", s->hypos[hypo_i].note);
    e.times_seen = 1;
    ikitaku_learned_save_append(&e);
    return 0;
}

int ikitaku_try_promote(const ikitaku_session_t* s, int min_similar,
                        ikitaku_promotion_t* out) {
    if (!s || !out || min_similar < 2) return 0;
    memset(out, 0, sizeof *out);
    // Find purpose with most feasible hypos; if enough, propose R5
    for (int i = 0; i < s->hypo_count; ++i) {
        if (!s->hypos[i].feasible && !s->hypos[i].synthesized) continue;
        if (s->hypos[i].purpose[0] == 0 || strcmp(s->hypos[i].purpose, "unknown") == 0)
            continue;
        int idxs[8], n = 0;
        idxs[n++] = i;
        for (int j = i + 1; j < s->hypo_count && n < 8; ++j) {
            char note[96];
            int sim = ikitaku_hypo_similarity(&s->hypos[i], &s->hypos[j], note, 96);
            if (sim >= 40 && strcmp(s->hypos[i].purpose, s->hypos[j].purpose) == 0)
                idxs[n++] = j;
            else if (sim >= 40 && sim < 70) {
                // partial — would be ambiguous; still count as weak evidence toward shared R5
                if (n < 8) idxs[n++] = j;
            }
        }
        if (n >= min_similar) {
            snprintf(out->form_id, sizeof out->form_id, "promo_%s_cluster",
                     s->hypos[i].purpose);
            snprintf(out->purpose, sizeof out->purpose, "%s", s->hypos[i].purpose);
            snprintf(out->note, sizeof out->note,
                     "L4-similar cluster → R5=%s; evidence=%d hypos",
                     s->hypos[i].purpose, n);
            int clo = 100, chi = 0;
            for (int k = 0; k < n; ++k) {
                out->evidence_idx[k] = idxs[k];
                int c = s->hypos[idxs[k]].confidence;
                if (c < clo) clo = c;
                if (c > chi) chi = c;
            }
            out->evidence_n = n;
            out->conf_low = clo;
            out->conf_high = chi;
            return 1;
        }
    }
    return 0;
}

int ikitaku_promotion_save(const ikitaku_promotion_t* p) {
    if (!p) return -1;
    FILE* f = fopen("/home/workdir/artifacts/rs_vm/ikitaku_promotions.cat", "a");
    if (!f) return -1;
    fprintf(f, "%s|%s|%d-%d|n=%d|%s\n",
            p->form_id, p->purpose, p->conf_low, p->conf_high,
            p->evidence_n, p->note);
    fclose(f);
    return 0;
}



// ===================== Evidence clustering =====================

static void anti_purpose(const char* p, char* out, int n) {
    if (!p) p = "map";
    struct { const char* a; const char* b; } flip[] = {
        {"join","split"}, {"split","join"},
        {"emit","absorb"}, {"absorb","emit"},
        {"map","order"}, {"order","map"},
        {"repeat","bound"}, {"bound","repeat"},
        {"occam","origin"}, {"origin","occam"},
        {"collapse","cascade"}, {"cascade","collapse"},
        {"iterative","waterfall"}, {"waterfall","iterative"},
        {"hegelian","occam"}, {"scientific","hegelian"},
        {"theorize","bound"}, {"create","destroy"},
        {"destroy","create"}, {"copy","destroy"},
        {0,0}
    };
    for (int i = 0; flip[i].a; ++i)
        if (!strcmp(p, flip[i].a)) {
            snprintf(out, (size_t)n, "%s", flip[i].b);
            return;
        }
    snprintf(out, (size_t)n, "invert");
}

int ikitaku_anti_synthesis(const ikitaku_hypo_t* syn, ikitaku_hypo_t* anti_out) {
    if (!syn || !anti_out) return -1;
    memset(anti_out, 0, sizeof *anti_out);
    *anti_out = *syn;
    anti_out->synthesized = 0;
    anti_out->rewrite_ok = syn->rewrite_ok ? 0 : 1;
    anti_purpose(syn->purpose[0] ? syn->purpose : syn->form_id,
                 anti_out->purpose, sizeof anti_out->purpose);
    snprintf(anti_out->form_id, sizeof anti_out->form_id, "anti_%s",
             syn->form_id[0] ? syn->form_id : syn->purpose);
    anti_out->confidence = 100 - syn->confidence;
    if (anti_out->confidence < 20) anti_out->confidence = 20;
    if (anti_out->confidence > 80) anti_out->confidence = 80;
    snprintf(anti_out->note, sizeof anti_out->note,
             "anti-synthesis of %s/%s (test the product of reasoning)",
             syn->purpose, syn->form_id);
    return 0;
}

int ikitaku_dialectic_recurse(const ikitaku_hypo_t* thesis,
                              const ikitaku_hypo_t* antithesis,
                              int max_depth,
                              ikitaku_hegel_rec_t* out) {
    if (!thesis || !antithesis || !out) return -1;
    memset(out, 0, sizeof *out);
    if (max_depth < 1) max_depth = 1;
    if (max_depth > IKITAKU_HEGEL_REC_MAX) max_depth = IKITAKU_HEGEL_REC_MAX;
    out->conf_in = thesis->confidence;

    ikitaku_hypo_t T = *thesis, A = *antithesis, S;
    memset(&S, 0, sizeof S);
    for (int d = 0; d < max_depth; ++d) {
        int ok = ikitaku_dialectic(&T, &A, &S);
        if (!ok) {
            /* generic compose if collide table missed */
            ok = ikitaku_synthesize_F(
                T.purpose[0] ? T.purpose : T.form_id,
                A.purpose[0] ? A.purpose : A.form_id,
                NULL, 0, 0, &S) > 0;
        }
        if (!ok || !S.form_id[0]) {
            if (d == 0) S = T;
            out->step[out->n++] = S;
            break;
        }
        S.synthesized = 1;
        out->step[out->n++] = S;
        out->depth_used = d + 1;

        ikitaku_hypo_t anti;
        ikitaku_anti_synthesis(&S, &anti);
        ikitaku_hypo_t S2;
        memset(&S2, 0, sizeof S2);
        int ok2 = ikitaku_dialectic(&S, &anti, &S2);
        if (!ok2)
            ok2 = ikitaku_synthesize_F(S.purpose, anti.purpose, NULL, 0, 0, &S2) > 0;

        /* Did anti-S beat S? */
        if (ok2 && S2.confidence > S.confidence + 5) {
            out->flipped++;
            T = S2;
            A = anti;
        } else {
            out->held = 1;
            /* next round: synthesis as thesis, original antithesis as check */
            T = S;
            A = anti;
            /* if S held firmly, stop — product survived critique */
            if (S.confidence >= 70 && (!ok2 || S2.confidence <= S.confidence))
                break;
        }
    }
    if (out->n > 0) out->conf_out = out->step[out->n - 1].confidence;
    else out->conf_out = out->conf_in;
    snprintf(out->english, sizeof out->english,
             "hegel-rec depth=%d steps=%d held=%d flipped=%d conf %d→%d last=%s/%s",
             out->depth_used, out->n, out->held, out->flipped,
             out->conf_in, out->conf_out,
             out->n ? out->step[out->n-1].purpose : "-",
             out->n ? out->step[out->n-1].form_id : "-");
    return out->held || out->n > 0;
}

static void xform_push(ikitaku_reason_pipe_t* o, const char* verb, const char* on, int conf) {
    if (!o || o->n >= IKITAKU_XFORM_MAX) return;
    ikitaku_xform_step_t* s = &o->step[o->n++];
    snprintf(s->verb, sizeof s->verb, "%s", verb);
    snprintf(s->on, sizeof s->on, "%s", on ? on : "-");
    s->conf = conf;
}

int ikitaku_reason_pipeline(const ikitaku_hypo_t* a, const ikitaku_hypo_t* b,
                            int kind, int max_hegel_depth,
                            ikitaku_reason_pipe_t* out) {
    if (!a || !out) return -1;
    memset(out, 0, sizeof *out);
    out->kind = (uint8_t)kind;
    out->exempted = 0; /* law: no skip because this is "reasoning" */

    const char* pa = a->purpose[0] ? a->purpose : a->form_id;
    const char* pb = (b && b->purpose[0]) ? b->purpose : (b ? b->form_id : "");

    /* 1. compare — relation, not a decision */
    int same = (pb[0] && !strcmp(pa, pb)) ? 1 : 0;
    int dconf = 0;
    if (b) {
        dconf = a->confidence - b->confidence;
        if (dconf < 0) dconf = -dconf;
    }
    int rel = same ? 90 : (b ? (100 - dconf) : 50);
    if (rel < 0) rel = 0;
    if (rel > 100) rel = 100;
    xform_push(out, "compare", pb[0] ? pb : pa, rel);

    /* 2. split if two sides, else skip */
    if (b && !same)
        xform_push(out, "split", "T|A", (a->confidence + b->confidence) / 2);

    /* 3. join or invert */
    if (b) {
        ikitaku_hypo_t S;
        memset(&S, 0, sizeof S);
        int joined = ikitaku_dialectic(a, b, &S);
        if (!joined)
            joined = ikitaku_synthesize_F(pa, pb, NULL, 0, 0, &S) > 0;
        if (joined && S.form_id[0]) {
            xform_push(out, "join", S.form_id, S.confidence);
            out->product = S;
        } else {
            xform_push(out, "invert", pa, a->confidence);
            out->product = *a;
        }
    } else {
        out->product = *a;
        xform_push(out, "bound", pa, a->confidence);
    }

    /* 4. bound — clamp claim by residual-ish conf */
    int cap = out->product.confidence;
    if (cap > 95) cap = 95;
    out->product.confidence = cap;
    xform_push(out, "bound", out->product.form_id[0] ? out->product.form_id : pa, cap);

    /* 5. repeat Hegel on the product if two sides exist (method OR data — same) */
    if (b && max_hegel_depth > 0) {
        xform_push(out, "repeat", "S-vs-antiS", cap);
        ikitaku_dialectic_recurse(a, b, max_hegel_depth, &out->hegel);
        if (out->hegel.n > 0)
            out->product = out->hegel.step[out->hegel.n - 1];
    }

    const char* kn =
        kind == IKITAKU_BAG_METHOD ? "METHOD" :
        kind == IKITAKU_BAG_SYNTHESIS ? "SYNTHESIS" : "DATA";
    snprintf(out->english, sizeof out->english,
             "reason-pipe kind=%s exempt=0 verbs=", kn);
    for (int i = 0; i < out->n; ++i) {
        size_t L = strlen(out->english);
        if (L + 20 >= sizeof out->english) break;
        snprintf(out->english + L, sizeof out->english - L, "%s%s",
                 i ? "→" : "", out->step[i].verb);
    }
    {
        size_t L = strlen(out->english);
        snprintf(out->english + L, sizeof out->english - L,
                 " product=%s/%s conf=%d hegel_held=%d",
                 out->product.purpose, out->product.form_id,
                 out->product.confidence, out->hegel.held);
    }
    return out->n;
}
