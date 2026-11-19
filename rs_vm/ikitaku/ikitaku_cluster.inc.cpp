
int ikitaku_cluster_build(const ikitaku_session_t* s, int sim_join,
                          ikitaku_cluster_report_t* out) {
    if (!s || !out) return -1;
    memset(out, 0, sizeof *out);
    if (sim_join <= 0) sim_join = 40;
    int assigned[IKITAKU_MAX_HYPOS];
    for (int i = 0; i < IKITAKU_MAX_HYPOS; ++i) assigned[i] = -1;

    for (int i = 0; i < s->hypo_count; ++i) {
        if (assigned[i] >= 0) continue;
        if (out->cluster_n >= IKITAKU_MAX_CLUSTERS) break;
        ikitaku_cluster_t* c = &out->clusters[out->cluster_n];
        memset(c, 0, sizeof *c);
        c->id = out->cluster_n;
        c->member_idx[0] = i;
        c->member_n = 1;
        assigned[i] = c->id;
        snprintf(c->purpose, sizeof c->purpose, "%s", s->hypos[i].purpose);
        c->conf_low = c->conf_high = s->hypos[i].confidence;
        if (s->hypos[i].rewrite_ok) c->has_pure = 1;
        else c->has_impure = 1;
        if (s->hypos[i].synthesized) c->has_synth = 1;
        if (s->hypos[i].ambiguous) c->ambiguous = 1;

        // Grow by Jaccard / same purpose
        for (int j = i + 1; j < s->hypo_count && c->member_n < IKITAKU_CLUSTER_MEM; ++j) {
            if (assigned[j] >= 0) continue;
            char note[128];
            int sim = ikitaku_associate(&s->hypos[i], &s->hypos[j], note, 128);
            out->compare_ops++;
            int same_F = (s->hypos[i].purpose[0] &&
                          strcmp(s->hypos[i].purpose, s->hypos[j].purpose) == 0);
            if (sim >= sim_join || same_F) {
                c->member_idx[c->member_n++] = j;
                assigned[j] = c->id;
                if (s->hypos[j].confidence < c->conf_low)
                    c->conf_low = s->hypos[j].confidence;
                if (s->hypos[j].confidence > c->conf_high)
                    c->conf_high = s->hypos[j].confidence;
                if (s->hypos[j].rewrite_ok) c->has_pure = 1;
                else c->has_impure = 1;
                if (s->hypos[j].synthesized) c->has_synth = 1;
                if (s->hypos[j].ambiguous) c->ambiguous = 1;
            }
        }

        // Pairwise sim stats
        int smin = 100, smax = 0, ssum = 0, spairs = 0;
        for (int a = 0; a < c->member_n; ++a) {
            for (int b = a + 1; b < c->member_n; ++b) {
                int sim = ikitaku_associate(
                    &s->hypos[c->member_idx[a]],
                    &s->hypos[c->member_idx[b]], NULL, 0);
                out->compare_ops++;
                if (sim < smin) smin = sim;
                if (sim > smax) smax = sim;
                ssum += sim;
                spairs++;
            }
        }
        if (spairs == 0) { smin = smax = 100; ssum = 100; spairs = 1; }
        c->sim_min = smin;
        c->sim_max = smax;
        c->sim_avg = ssum / spairs;
        c->state = IKITAKU_CL_COMPARED;
        if (c->sim_avg >= 40 && c->sim_avg < 70) c->ambiguous = 1;

        snprintf(c->debug, sizeof c->debug,
                 "cluster#%d F=%s members=%d conf[%d..%d] sim(min/avg/max)=%d/%d/%d "
                 "pure=%u impure=%u synth=%u amb=%u",
                 c->id, c->purpose, c->member_n, c->conf_low, c->conf_high,
                 c->sim_min, c->sim_avg, c->sim_max,
                 (unsigned)c->has_pure, (unsigned)c->has_impure,
                 (unsigned)c->has_synth, (unsigned)c->ambiguous);
        out->cluster_n++;
    }
    snprintf(out->super_log + strlen(out->super_log),
             sizeof(out->super_log) - strlen(out->super_log),
             "[cluster] built %d clusters compare_ops=%d\n",
             out->cluster_n, out->compare_ops);
    ikitaku_kant_cluster_tag(out);
    return out->cluster_n;
}

int ikitaku_kant_cluster_tag(ikitaku_cluster_report_t* cr) {
    if (!cr) return -1;
    int n = 0;
    for (int i = 0; i < cr->cluster_n; ++i) {
        ikitaku_cluster_t* c = &cr->clusters[i];
        c->membership = c->sim_avg;
        c->antinomy = 0;
        const char* p = c->purpose;
        if (c->has_pure && c->has_impure && !c->dialectic_resolved) {
            snprintf(c->kant_class, sizeof c->kant_class, "quality");
            snprintf(c->kant_cat, sizeof c->kant_cat, "negation");
            c->antinomy = 1;
        } else if (p[0] && (!strcmp(p,"repeat") || !strcmp(p,"wait"))) {
            snprintf(c->kant_class, sizeof c->kant_class, "schema");
            snprintf(c->kant_cat, sizeof c->kant_cat, "movement");
        } else if (p[0] && (!strcmp(p,"cause") || !strcmp(p,"origin") ||
                            strstr(p,"x_"))) {
            snprintf(c->kant_class, sizeof c->kant_class, "relation");
            snprintf(c->kant_cat, sizeof c->kant_cat, "cause");
        } else if (c->conf_high - c->conf_low >= 30 || c->ambiguous) {
            snprintf(c->kant_class, sizeof c->kant_class, "modality");
            snprintf(c->kant_cat, sizeof c->kant_cat, "possible");
        } else if (c->member_n >= 3 && c->sim_avg >= 70) {
            snprintf(c->kant_class, sizeof c->kant_class, "quantity");
            snprintf(c->kant_cat, sizeof c->kant_cat, "totality");
        } else {
            snprintf(c->kant_class, sizeof c->kant_class, "quantity");
            snprintf(c->kant_cat, sizeof c->kant_cat, "unity");
        }
        n++;
    }
    /* two high-membership opposite classes → antinomy between clusters */
    for (int i = 0; i < cr->cluster_n; ++i) {
        for (int j = i + 1; j < cr->cluster_n; ++j) {
            ikitaku_cluster_t *a = &cr->clusters[i], *b = &cr->clusters[j];
            if (a->membership < 55 || b->membership < 55) continue;
            if (strcmp(a->kant_class, b->kant_class)==0) continue;
            if ((a->has_pure && b->has_impure) || (a->has_impure && b->has_pure)) {
                a->antinomy = b->antinomy = 1;
            }
        }
    }
    return n;
}

static void gap_parent(const char* name, char* out, int n) {
    if (!name || !out || n < 4) return;
    const char* x = strstr(name, "_x_");
    if (x && x != name) {
        int k = (int)(x - name);
        if (k >= n) k = n - 1;
        memcpy(out, name, (size_t)k);
        out[k] = 0;
        return;
    }
    if (name[0]=='[') {
        snprintf(out, (size_t)n, "map");
        return;
    }
    ikitaku_r5_verb_t v;
    if (ikitaku_r5_lookup(name, &v) == 0)
        snprintf(out, (size_t)n, "%s", name);
    else
        snprintf(out, (size_t)n, "map");
}

int ikitaku_cluster_gap_hint(const ikitaku_cluster_report_t* cr,
                             ikitaku_gap_hint_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!cr || cr->cluster_n < 2) {
        snprintf(out->hint, sizeof out->hint, "single");
        snprintf(out->english, sizeof out->english, "gap-hint: <2 clusters");
        return 0;
    }
    int sum = 0, np = 0, mx = -1, ia = 0, ib = 1;
    for (int i = 0; i < cr->cluster_n; ++i) {
        for (int j = i + 1; j < cr->cluster_n; ++j) {
            ikitaku_lie_t L;
            ikitaku_lie_bracket(cr->clusters[i].purpose,
                                cr->clusters[j].purpose, &L);
            sum += L.mag; np++;
            if (L.mag > mx) { mx = L.mag; ia = i; ib = j; }
        }
    }
    out->gap_mean = np ? sum / np : 0;
    out->gap_mag = mx < 0 ? 0 : mx;
    out->abnormal = (out->gap_mag >= 70) ||
                    (out->gap_mag >= 55 && out->gap_mag >= out->gap_mean + 15);
    snprintf(out->A, sizeof out->A, "%s", cr->clusters[ia].purpose);
    snprintf(out->B, sizeof out->B, "%s", cr->clusters[ib].purpose);
    out->ambiguous = cr->clusters[ia].ambiguous || cr->clusters[ib].ambiguous
                     || cr->clusters[ia].antinomy || cr->clusters[ib].antinomy;
    gap_parent(out->A, out->parent_a, (int)sizeof out->parent_a);
    gap_parent(out->B, out->parent_b, (int)sizeof out->parent_b);

    static const char* simp[][2] = {
        {"repeat","wait"}, {"small","big"}, {"split","join"},
        {"emit","absorb"}, {"copy","map"}, {"filter","map"}
    };
    int catalog = 0;
    int best_s = -1;
    ikitaku_analogy_t an;
    for (int k = 0; k < 6; ++k) {
        if ((!strcmp(out->A, simp[k][0]) && !strcmp(out->B, simp[k][1])) ||
            (!strcmp(out->A, simp[k][1]) && !strcmp(out->B, simp[k][0])))
            catalog = 1;
        ikitaku_analogy(out->A, out->B, simp[k][0], simp[k][1], &an);
        if (an.sim > best_s) best_s = an.sim;
    }
    ikitaku_analogy(out->A, out->parent_a, out->B, out->parent_b, &an);
    int climbed = strcmp(out->A, out->parent_a) != 0 ||
                  strcmp(out->B, out->parent_b) != 0;
    if (climbed && an.sim > best_s) best_s = an.sim;
    out->analog_sim = best_s < 0 ? 0 : best_s;

    if (catalog) {
        snprintf(out->hint, sizeof out->hint, "hold");
    } else if (out->abnormal && out->analog_sim >= 55) {
        out->novel = 1;
        snprintf(out->hint, sizeof out->hint, "novel-species");
    } else if (out->abnormal && out->analog_sim < 35) {
        out->nonsense = 1;
        snprintf(out->hint, sizeof out->hint, "useless-gap");
    } else if (out->ambiguous) {
        snprintf(out->hint, sizeof out->hint, "peel-parent");
    } else {
        snprintf(out->hint, sizeof out->hint, "hold");
    }
    {
        ikitaku_metaphor_plan_t mp;
        memset(&mp, 0, sizeof mp);
        ikitaku_metaphor_plan(out->A, out->B, out->novel ? 1 : 0, &mp);
        snprintf(out->strategy, sizeof out->strategy, "%s", mp.strategy);
        snprintf(out->origin_lead, sizeof out->origin_lead, "%s", mp.origin_lead);
        out->meta_jaccard = mp.jaccard;
        if (mp.analog_sim > out->analog_sim) out->analog_sim = mp.analog_sim;
        if (!strcmp(out->hint, "peel-parent") && mp.origin_lead[0])
            snprintf(out->hint, sizeof out->hint, "%s", mp.origin_lead);
    }
    snprintf(out->english, sizeof out->english,
             "gap %s:%s mag=%d mean=%d abn=%d analog=%d → %s parent %s/%s "
             "strat=%s origin=%s j=%d",
             out->A, out->B, out->gap_mag, out->gap_mean, out->abnormal,
             out->analog_sim, out->hint, out->parent_a, out->parent_b,
             out->strategy[0] ? out->strategy : "-",
             out->origin_lead[0] ? out->origin_lead : "-",
             out->meta_jaccard);
    return out->novel ? 1 : (out->nonsense ? -1 : 0);
}

int ikitaku_cluster_dialectic(ikitaku_session_t* s, ikitaku_cluster_report_t* cr,
                              const ikitaku_config_t* cfg) {
    if (!s || !cr) return 0;
    int resolved = 0;
    for (int ci = 0; ci < cr->cluster_n; ++ci) {
        ikitaku_cluster_t* c = &cr->clusters[ci];
        if (!c->has_pure || !c->has_impure) continue;
        // Find one pure and one impure member
        int pi = -1, ii = -1;
        for (int m = 0; m < c->member_n; ++m) {
            int idx = c->member_idx[m];
            if (s->hypos[idx].rewrite_ok && pi < 0) pi = idx;
            if (!s->hypos[idx].rewrite_ok && ii < 0) ii = idx;
        }
        if (pi < 0 || ii < 0) continue;
        ikitaku_hypo_t syn;
        ikitaku_hunt_t hunt;
        memset(&hunt, 0, sizeof hunt);
        if (ikitaku_dialectic_hunt(&s->hypos[pi], &s->hypos[ii], &syn, &hunt)) {
            cr->dialectic_ops++;
            c->dialectic_resolved = 1;
            c->state = IKITAKU_CL_DIALECTIC;
            int dlen = (int)strlen(c->debug);
            snprintf(c->debug + dlen, sizeof(c->debug) - (size_t)dlen,
                     " | DIALECTIC+HUNT pure[%d] vs impure[%d] → %s conf=%d next=%d",
                     pi, ii, syn.form_id, syn.confidence, hunt.next_probe);
            if (s->hypo_count < IKITAKU_MAX_HYPOS) {
                syn.parent_a = (int16_t)pi;
                syn.parent_b = (int16_t)ii;
                push_hypo(s, &syn);
            }
            resolved++;
            snprintf(cr->super_log + strlen(cr->super_log),
                     sizeof(cr->super_log) - strlen(cr->super_log),
                     "[dialectic] cluster#%d resolved pure/impure → %s\n",
                     c->id, syn.form_id);
        }
        if (c->ambiguous) {
            c->state = IKITAKU_CL_AMBIGUOUS;
            for (int m = 0; m < c->member_n; ++m) {
                int idx = c->member_idx[m];
                if (s->hypos[idx].confidence < (cfg ? cfg->min_accept_conf : 70)
                    && !s->hypos[idx].ambiguous) {
                    ikitaku_mark_ambiguous(s, idx, "cluster partial Jaccard");
                    cr->ambiguous_marks++;
                }
            }
        }
    }
    return resolved;
}

int ikitaku_cluster_promote(const ikitaku_session_t* s,
                            ikitaku_cluster_report_t* cr,
                            int min_members,
                            ikitaku_promotion_t* promos, int max_promos) {
    if (!s || !cr || !promos || max_promos <= 0) return 0;
    if (min_members < 2) min_members = 2;
    int n = 0;
    for (int ci = 0; ci < cr->cluster_n && n < max_promos; ++ci) {
        ikitaku_cluster_t* c = &cr->clusters[ci];
        if (c->member_n < min_members) continue;
        if (c->purpose[0] == 0 || strcmp(c->purpose, "unknown") == 0) continue;
        ikitaku_promotion_t* p = &promos[n];
        memset(p, 0, sizeof *p);
        snprintf(p->form_id, sizeof p->form_id, "promo_c%d_%s", c->id, c->purpose);
        snprintf(p->purpose, sizeof p->purpose, "%s", c->purpose);
        snprintf(p->note, sizeof p->note,
                 "cluster#%d members=%d sim_avg=%d state=%u conf[%d..%d]",
                 c->id, c->member_n, c->sim_avg, (unsigned)c->state,
                 c->conf_low, c->conf_high);
        p->conf_low = c->conf_low;
        p->conf_high = c->conf_high;
        p->evidence_n = c->member_n > 8 ? 8 : c->member_n;
        for (int m = 0; m < p->evidence_n; ++m)
            p->evidence_idx[m] = c->member_idx[m];
        c->state = IKITAKU_CL_PROMOTED;
        cr->promote_ops++;
        int dlen = (int)strlen(c->debug);
        snprintf(c->debug + dlen, sizeof(c->debug) - (size_t)dlen,
                 " | PROMOTED → %s conf[%d..%d]", p->form_id, p->conf_low, p->conf_high);
        snprintf(cr->super_log + strlen(cr->super_log),
                 sizeof(cr->super_log) - strlen(cr->super_log),
                 "[promote] cluster#%d → %s evidence=%d conf[%d..%d]\n",
                 c->id, p->form_id, p->evidence_n, p->conf_low, p->conf_high);
        ikitaku_promotion_save(p);
        n++;
    }
    return n;
}

int ikitaku_cluster_report(const ikitaku_cluster_report_t* cr,
                           char* buf, int buf_len) {
    if (!cr || !buf || buf_len < 8) return 0;
    int u = 0;
    u += snprintf(buf + u, (size_t)(buf_len - u),
        "=== evidence clusters n=%d compare=%d dialectic=%d synth=%d promote=%d amb=%d ===\n",
        cr->cluster_n, cr->compare_ops, cr->dialectic_ops, cr->synth_ops,
        cr->promote_ops, cr->ambiguous_marks);
    for (int i = 0; i < cr->cluster_n && u < buf_len - 80; ++i) {
        const ikitaku_cluster_t* c = &cr->clusters[i];
        u += snprintf(buf + u, (size_t)(buf_len - u),
            "  C%d F=%-10s mem=%d sim=%d/%d/%d conf[%d..%d] st=%u "
            "pure=%u imp=%u syn=%u amb=%u dial=%d\n"
            "       members:",
            c->id, c->purpose, c->member_n, c->sim_min, c->sim_avg, c->sim_max,
            c->conf_low, c->conf_high, (unsigned)c->state,
            (unsigned)c->has_pure, (unsigned)c->has_impure,
            (unsigned)c->has_synth, (unsigned)c->ambiguous,
            c->dialectic_resolved);
        for (int m = 0; m < c->member_n && u < buf_len - 20; ++m)
            u += snprintf(buf + u, (size_t)(buf_len - u), " %d", c->member_idx[m]);
        u += snprintf(buf + u, (size_t)(buf_len - u), "\n       dbg: %s\n", c->debug);
    }
    if (cr->super_log[0] && u < buf_len - 40)
        u += snprintf(buf + u, (size_t)(buf_len - u), "--- cluster super_log ---\n%s",
                      cr->super_log);
    return u;
}

// ===================== Meta-cognitive superloop =====================

int ikitaku_analyze_ex(const rsvm_t* vm, const ikitaku_config_t* cfg,
                       ikitaku_session_t* out,
                       ikitaku_cluster_report_t* clusters,
                       ikitaku_metacog_t* meta) {
    if (!vm || !out) return -1;
    ikitaku_config_t local;
    if (!cfg) { ikitaku_config_default(&local); cfg = &local; }

    ikitaku_metacog_t meta_local;
    if (!meta) meta = &meta_local;
    memset(meta, 0, sizeof *meta);
    meta->max_depth = 3;

    ikitaku_cluster_report_t cr_local;
    if (!clusters) clusters = &cr_local;
    memset(clusters, 0, sizeof *clusters);

    // PHASE OBSERVE — existing analyze body without final accept
    // Reuse ikitaku_analyze then post-process clusters (simpler interoperability)
    meta->phase = IKITAKU_PHASE_OBSERVE;
    int nh = ikitaku_analyze(vm, cfg, out);
    meta->phase_hypos[IKITAKU_PHASE_OBSERVE] = out->hypo_count;
    // Parallel multi-form: e.g. impure repeat also reads as wait-carrying loop
    for (int i = 0; i < out->hypo_count; ++i) {
        if (strcmp(out->hypos[i].purpose, "repeat") == 0 && !out->hypos[i].rewrite_ok) {
            for (int j = 0; j < out->hypo_count; ++j) {
                if (strcmp(out->hypos[j].purpose, "wait") == 0) {
                    snprintf(out->hypos[i].purpose_alt, sizeof out->hypos[i].purpose_alt, "wait");
                    out->hypos[i].parallel_conf = out->hypos[j].confidence;
                    break;
                }
            }
        }
        if (strcmp(out->hypos[i].purpose, "decision") == 0) {
            for (int j = 0; j < out->hypo_count; ++j) {
                if (strcmp(out->hypos[j].purpose, "repeat") == 0 &&
                    out->hypos[j].span_start <= out->hypos[i].span_start &&
                    out->hypos[j].span_end >= out->hypos[i].span_end) {
                    snprintf(out->hypos[i].purpose_alt, sizeof out->hypos[i].purpose_alt, "repeat");
                    out->hypos[i].parallel_conf = 60;
                    break;
                }
            }
        }
    }

    meta->phase_cost[IKITAKU_PHASE_OBSERVE] = out->cost_used;
    snprintf(meta->metacog_note, sizeof meta->metacog_note,
             "observe: %d hypos cost=%d", out->hypo_count, out->cost_used);

    // PHASE CLUSTER (subloop under superloop)
    meta->phase = IKITAKU_PHASE_CLUSTER;
    meta->depth = 1;
    int cn = ikitaku_cluster_build(out, 40, clusters);
    meta->phase_hypos[IKITAKU_PHASE_CLUSTER] = cn;
    snprintf(meta->metacog_note, sizeof meta->metacog_note,
             "cluster: %d groups from %d hypos", cn, out->hypo_count);

    // PHASE DIALECTIC inside clusters
    meta->phase = IKITAKU_PHASE_DIALECTIC;
    meta->depth = 2;
    int before = out->hypo_count;
    int rd = ikitaku_cluster_dialectic(out, clusters, cfg);
    meta->phase_hypos[IKITAKU_PHASE_DIALECTIC] = out->hypo_count - before;
    snprintf(meta->metacog_note, sizeof meta->metacog_note,
             "dialectic: %d cluster resolutions (+%d hypos)",
             rd, out->hypo_count - before);

    // PHASE PROMOTE
    meta->phase = IKITAKU_PHASE_PROMOTE;
    meta->depth = 1;
    ikitaku_promotion_t promos[IKITAKU_MAX_CLUSTERS];
    int pn = ikitaku_cluster_promote(out, clusters, 2, promos, IKITAKU_MAX_CLUSTERS);
    meta->phase_hypos[IKITAKU_PHASE_PROMOTE] = pn;
    snprintf(meta->metacog_note, sizeof meta->metacog_note,
             "promote: %d R5 candidates from clusters", pn);

    // PHASE ACCEPT — recompute best after cluster dialectic additions
    // ASSOCIATE-EVALUATE-CONSIDER over information gaps (multi-var)
    {
        ikitaku_gap_report_t gaps;
        ikitaku_find_gaps(vm, out, &gaps);
        char cnote[192];
        ikitaku_consider_gaps(out, &gaps, cnote, sizeof cnote);
        logf(out, "[consider] %s\n", cnote);
        snprintf(meta->metacog_note, sizeof meta->metacog_note, "%s", cnote);
    }

    meta->phase = IKITAKU_PHASE_ACCEPT;
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
        snprintf(meta->metacog_note, sizeof meta->metacog_note,
                 "accept: [%d] %s score=%d (superloop done depth=%d)",
                 best, out->hypos[best].form_id, out->hypos[best].score, meta->depth);
    else
        snprintf(meta->metacog_note, sizeof meta->metacog_note,
                 "accept: none (budget/cost or all ambiguous)");

    logf(out, "[metacog] %s\n", meta->metacog_note);
    ikitaku_affinity_observe(out);
    ikitaku_affinity_save(NULL);
    {
        ikitaku_r6_t r6;
        const char* g = (best >= 0) ? out->hypos[best].purpose : "unknown";
        ikitaku_r6_compose(g, 1, 1, 60, 60, 1, &r6);
        logf(out, "[r6] %s\n", r6.english);
    }
    return out->hypo_count;
}

int ikitaku_discriminant(const ikitaku_session_t* s, ikitaku_disc_t* out) {
    if (!s || !out) return -1;
    memset(out, 0, sizeof *out);
    for (int i = 0; i < s->hypo_count; ++i)
        out->primary[i] = -1;
    for (int i = 0; i < s->hypo_count && out->n < IKITAKU_DISC_MAX; ++i) {
        const char* p = s->hypos[i].purpose;
        if (!p[0]) continue;
        int found = -1;
        for (int c = 0; c < out->n; ++c)
            if (strcmp(out->cls[c].label, p) == 0) { found = c; break; }
        if (found < 0) {
            found = out->n++;
            snprintf(out->cls[found].label, sizeof out->cls[found].label, "%s", p);
        }
        ikitaku_embed_t e;
        ikitaku_embed_hypo(&s->hypos[i], &e);
        int dim = IKITAKU_DISC_DIM < IKITAKU_ASSOC_DIM ? IKITAKU_DISC_DIM : IKITAKU_ASSOC_DIM;
        for (int d = 0; d < dim; ++d)
            out->cls[found].mean[d] += e.vec[d];
        out->cls[found].n++;
    }
    for (int c = 0; c < out->n; ++c)
        if (out->cls[c].n)
            for (int d = 0; d < IKITAKU_DISC_DIM; ++d)
                out->cls[c].mean[d] /= (float)out->cls[c].n;
    for (int i = 0; i < s->hypo_count; ++i) {
        ikitaku_embed_t e;
        ikitaku_embed_hypo(&s->hypos[i], &e);
        double wsum = 0;
        double raw[IKITAKU_DISC_MAX];
        int best = -1; double bestw = -1;
        for (int c = 0; c < out->n; ++c) {
            double dist = 0;
            for (int d = 0; d < IKITAKU_DISC_DIM; ++d) {
                double df = e.vec[d] - out->cls[c].mean[d];
                dist += df * df;
            }
            out->cls[c].within += (float)dist;
            raw[c] = 1.0 / (1.0 + dist);
            wsum += raw[c];
            if (raw[c] > bestw) { bestw = raw[c]; best = c; }
        }
        for (int c = 0; c < out->n; ++c)
            out->membership[i][c] = wsum > 0 ? (int)(raw[c] / wsum * 100.0 + 0.5) : 0;
        out->primary[i] = best;
    }
    for (int c = 0; c < out->n; ++c)
        if (out->cls[c].n) out->cls[c].within /= (float)out->cls[c].n;
    snprintf(out->english, sizeof out->english,
             "Discriminant: %d purpose-classes; fuzzy memberships (not hard labels).",
             out->n);
    return out->n;
}



// ===================== Association model (LLM-like) =====================
