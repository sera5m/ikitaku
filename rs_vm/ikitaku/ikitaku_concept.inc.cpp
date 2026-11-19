int ikitaku_peel_transforms(const ikitaku_series_t* s,
                            ikitaku_stack_report_t* io) {
    if (!s || !io || s->n < 4) return -1;
    int n = s->n;
    double dxs[] = { 0.05, 0.06, 0.08, 0.10, 0.12, 0.13, 0.15, 0.20, 0.26, 0.30 };
    int ndx = 10;
    double x0s[] = { 0.0, 0.05 };
    struct { const char* outer; const char* inner; int outer_sin; } pairs[] = {
        { "sin", "tan", 1 },
        { "sin", "sin", 1 },
        { "sin", "x",   1 },
        { "tan", "sin", 0 },
        { "tan", "x",   0 },
    };
    int best_c = io->best >= 0 ? io->tries[io->best].conf : 0;
    int shallow = 0;
    if (io->best >= 0) {
        int w = 0;
        for (const char* p = io->tries[io->best].formula; *p; ++p)
            if (*p == '(') w++;
        if (w <= 1 && best_c >= 80 && ikitaku_occam_margin() >= 5)
            shallow = 1; /* calm Occam: don't invent extra wrap */
    }
    int best_i = -1;
    char best_form[64] = {0};
    char best_note[96] = {0};
    int best_inv = 0;

    for (int p = 0; p < 5; ++p) {
        if (shallow && strcmp(pairs[p].inner, "x") != 0) continue;
        ikitaku_series_t inner;
        memset(&inner, 0, sizeof inner);
        inner.kind = IKITAKU_DATA_F32;
        inner.n = n;
        int domain_ok = 1;
        for (int i = 0; i < n; ++i) {
            double v = s->values[i];
            if (pairs[p].outer_sin) {
                if (v > 1.0) v = 1.0;
                if (v < -1.0) v = -1.0;
                inner.values[i] = asin(v);
            } else {
                inner.values[i] = atan(v);
            }
        }
        double (*ifn)(double) = fn_sin;
        if (strcmp(pairs[p].inner, "tan") == 0) ifn = fn_tan;
        else if (strcmp(pairs[p].inner, "x") == 0) ifn = fn_id;
        int ic = 0;
        double bdx = 0.1, bx0 = 0.0;
        for (int d = 0; d < ndx; ++d)
            for (int z = 0; z < 2; ++z) {
                int c = corr_series_fn(&inner, ifn, dxs[d], x0s[z]);
                if (c > ic) { ic = c; bdx = dxs[d]; bx0 = x0s[z]; }
            }
        /* Occam: extra wrap must beat current by 8 unless inner is identity */
        int need = best_c + (strcmp(pairs[p].inner, "x") == 0 ? 0 : ikitaku_occam_margin());
        if (ic > best_c && ic >= need) {
            best_c = ic;
            best_i = p;
            snprintf(best_form, sizeof best_form, "%s(%s(x)) dx=%.2f",
                     pairs[p].outer, pairs[p].inner, bdx);
            snprintf(best_note, sizeof best_note,
                     "peeled %s → inner looks like %s (inv-corr %d)",
                     pairs[p].outer, pairs[p].inner, ic);
            best_inv = 1;
        }
        (void)domain_ok; (void)bx0;
        ikitaku_think_cost_add_ops(n * 4);
    }

    if (best_i >= 0 && io->n < IKITAKU_MAX_STACK) {
        ikitaku_stack_try_t* tr = &io->tries[io->n++];
        snprintf(tr->formula, sizeof tr->formula, "%s", best_form);
        tr->conf = best_c;
        tr->inverse_ok = best_inv;
        snprintf(tr->note, sizeof tr->note, "%s", best_note);
        io->best = io->n - 1;
        io->right_track = 1;
        snprintf(io->english, sizeof io->english,
                 "Peel: inverted outer wrap. %s", best_note);
    }
    return best_i >= 0 ? 1 : 0;
}

int ikitaku_layered_reason(const ikitaku_series_t* s, ikitaku_stack_report_t* out) {
    if (!s || !out) return -1;
    ikitaku_fit_report_t fits;
    ikitaku_series_fit(s, &fits);
    ikitaku_stack_complete(s, &fits, out);
    ikitaku_peel_transforms(s, out);
    return out->best;
}



int ikitaku_concept_chain_init(const char* F, const char* bound_transform,
                               ikitaku_concept_chain_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    const char* f0 = (F && F[0]) ? F : "map";
    // order 0 platonic F
    out->slot[0].order = 0;
    snprintf(out->slot[0].concept, 32, "%s", f0);
    out->slot[0].conf = 80;
    {
        ikitaku_r5_verb_t rv;
        int hit = ikitaku_r5_lookup(f0, &rv);
        out->slot[0].platonic_depth = (hit == 0 && rv.locked && !rv.r6) ? 8 : 0;
    }
    out->slot[0].var_depth = 0;
    // order 1 specialized f = F + typical method
    out->slot[1].order = 1;
    snprintf(out->slot[1].concept, 32, "%s_instance", f0);
    out->slot[1].conf = 60;
    // order 2 f' = transform-as-variable
    out->slot[2].order = 2;
    snprintf(out->slot[2].concept, 32, "wrap");
    if (bound_transform && bound_transform[0])
        snprintf(out->slot[2].bound, 32, "%s", bound_transform);
    out->slot[2].conf = bound_transform && bound_transform[0] ? 70 : 30;
    // order 3 f'' = inner variable / data
    out->slot[3].order = 3;
    snprintf(out->slot[3].concept, 32, "x");
    snprintf(out->slot[3].bound, 32, "x");
    out->slot[3].conf = 50;
    out->slot[2].var_depth = 1;
    out->slot[3].var_depth = 1;
    out->n = 4;
    out->max_drop = IKITAKU_CHAIN_LEN - 1;
    out->form_depth = 0;
    out->integrate_bound = IKITAKU_DEPTH_MAX;
    out->integrate_depth = IKITAKU_DEPTH_MAX;
    out->dropped = (bound_transform && bound_transform[0]) ? 1 : 0;
    for (int i = 0; i < out->n; ++i) {
        if (out->slot[i].conf < 0) out->slot[i].conf = 0;
        if (out->slot[i].conf > 100) out->slot[i].conf = 100;
        out->slot[i].confusion = 100 - out->slot[i].conf;
    }
    {
        int mn = 100, sum = 0, cx = 0;
        for (int i = 0; i < out->n; ++i) {
            if (out->slot[i].conf < mn) mn = out->slot[i].conf;
            sum += out->slot[i].conf;
            if (out->slot[i].confusion > cx) cx = out->slot[i].confusion;
        }
        int avg = out->n ? sum / out->n : 0;
        out->macro_conf = out->n ? (2 * mn + avg) / 3 : 0;
        out->macro_confusion = cx;
    }
    ikitaku_layer_sure_eval(out);
    snprintf(out->english, sizeof out->english,
             "Chain F=%s → f=%s → f'=%s%s → f''=x  (concepts as variables at f')",
             out->slot[0].concept, out->slot[1].concept, out->slot[2].concept,
             out->slot[2].bound[0] ? out->slot[2].bound : "(unbound)");
    return out->n;
}

int ikitaku_concept_drop(ikitaku_concept_chain_t* ch, int order, const char* as_var) {
    if (!ch || !as_var || order < 0 || order >= IKITAKU_CHAIN_LEN) return -1;
    while (ch->n <= order && ch->n < IKITAKU_CHAIN_LEN) {
        int k = ch->n;
        ch->slot[k].order = k;
        snprintf(ch->slot[k].concept, 32, "f^(%d)", k);
        ch->slot[k].var_depth = 1;
        ch->n++;
    }
    snprintf(ch->slot[order].bound, 32, "%s", as_var);
    ch->slot[order].var_depth = 1;
    if (order == 0) {
        ch->slot[0].platonic_depth = 0;
        ch->form_depth = 1;
    }
    ch->dropped = 1;
    snprintf(ch->english, sizeof ch->english,
             "Dropped order-%d concept '%s' to variable '%s' — reason at f' not only f.",
             order, ch->slot[order].concept, as_var);
    return 0;
}

int ikitaku_concept_feed_stack(const ikitaku_concept_chain_t* ch,
                               char names[][32], int max_names) {
    if (!ch || !names || max_names <= 0) return 0;
    int n = 0;
    // Compose bound slots from inside out: f''(x) wrapped by f' then F
    const char* inner = "x";
    const char* wrap = NULL;
    const char* outer = NULL;
    for (int i = 0; i < ch->n; ++i) {
        if (ch->slot[i].order == 3 && ch->slot[i].bound[0]) inner = ch->slot[i].bound;
        if (ch->slot[i].order == 2 && ch->slot[i].bound[0]) wrap = ch->slot[i].bound;
        if (ch->slot[i].order <= 1 && ch->slot[i].bound[0]) outer = ch->slot[i].bound;
        if (ch->slot[i].order == 0 && !outer) outer = ch->slot[i].concept;
    }
    if (n < max_names) snprintf(names[n++], 32, "%s", inner);
    if (wrap && n < max_names) snprintf(names[n++], 32, "%s(%s)", wrap, inner);
    if (outer && wrap && n < max_names)
        snprintf(names[n++], 32, "%s(%s(%s))", outer, wrap, inner);
    else if (outer && n < max_names)
        snprintf(names[n++], 32, "%s(%s)", outer, inner);
    return n;
}

int ikitaku_layered_reason_ex(const ikitaku_series_t* s,
                              ikitaku_concept_chain_t* chain,
                              ikitaku_stack_report_t* out) {
    if (!s || !out) return -1;
    ikitaku_fit_report_t fits;
    ikitaku_series_fit(s, &fits);
    ikitaku_stack_complete(s, &fits, out);
    ikitaku_peel_transforms(s, out);

    // Feed concepts back: if chain binds wrap=tan, outer=sin, prefer that family
    if (chain) {
        char names[6][32];
        int nn = ikitaku_concept_feed_stack(chain, names, 6);
        // If a fed name matches a stack try, boost its conf slightly (feedback)
        for (int i = 0; i < nn; ++i) {
            for (int t = 0; t < out->n; ++t) {
                if (strstr(out->tries[t].formula, names[i])
                    || strstr(names[i], "sin(tan")) {
                    if (strstr(out->tries[t].formula, "sin(tan")) {
                        out->tries[t].conf += 0; // already scored
                    }
                }
            }
        }
        // Drop partial-best kind into f' if unbound
        if (!chain->slot[2].bound[0] && fits.n && fits.best >= 0) {
            if (fits.fits[fits.best].kind == IKITAKU_FIT_SIN)
                ikitaku_concept_drop(chain, 2, "sin");
            else if (fits.fits[fits.best].kind == IKITAKU_FIT_TAN)
                ikitaku_concept_drop(chain, 2, "tan");
        }
        // If right-track and f' bound to sin, drop inner candidate tan as f''
        if (out->right_track && strcmp(chain->slot[2].bound, "sin") == 0)
            ikitaku_concept_drop(chain, 3, "tan");
        char fed[96] = {0};
        for (int i = 0; i < nn && i < 3; ++i) {
            char bit[40];
            snprintf(bit, sizeof bit, "%s%s", i ? " ; " : "", names[i]);
            strncat(fed, bit, sizeof(fed) - strlen(fed) - 1);
        }
        snprintf(chain->english, sizeof chain->english,
                 "Feedback: concepts re-entered as vars [%s]. dropped=%u",
                 fed, (unsigned)chain->dropped);
    }
    return out->best;
}



int ikitaku_F_as_form(ikitaku_concept_chain_t* ch, const char* form_name) {
    if (!ch) return -1;
    uint8_t d = ch->form_depth;
    if (d < IKITAKU_DEPTH_MAX) d++;
    ch->form_depth = d;
    ch->slot[0].platonic_depth = 0;
    ch->slot[0].var_depth = d;
    if (form_name && form_name[0])
        snprintf(ch->slot[0].bound, 32, "%s", form_name);
    ikitaku_layer_sure_eval(ch);
    snprintf(ch->english, sizeof ch->english,
             "F demoted to FORM '%s' form_depth=%u — axioms may sit lower.",
             ch->slot[0].bound[0] ? ch->slot[0].bound : ch->slot[0].concept,
             (unsigned)ch->form_depth);
    return 0;
}

int ikitaku_dropdown_loop(ikitaku_concept_chain_t* ch,
                          const char** candidates, int n_cand,
                          uint8_t depth) {
    if (!ch) return -1;
    if (depth < 1) depth = 1;
    if (depth > IKITAKU_DEPTH_MAX) depth = IKITAKU_DEPTH_MAX;
    int start = ch->n;
    int added = 0;
    uint8_t remain = depth;
    while (remain > 0 && ch->n < IKITAKU_CHAIN_LEN) {
        int extra = added;
        int order = ch->n;
        ch->slot[order].order = order;
        snprintf(ch->slot[order].concept, 32, "f^(%d)", order);
        ch->slot[order].var_depth = remain;
        ch->slot[order].platonic_depth = 0;
        ch->slot[order].conf = 20;
        if (candidates && extra < n_cand && candidates[extra]) {
            snprintf(ch->slot[order].bound, 32, "%s", candidates[extra]);
            ch->slot[order].conf = 45;
            ch->dropped = 1;
        }
        ch->n++;
        added++;
        remain--;
        if (ch->integrate_depth > 0) ch->integrate_depth--;
    }
    ikitaku_layer_sure_eval(ch);
    snprintf(ch->english, sizeof ch->english,
             "Drop-down: chain %d->%d used=%d remain=%u form_depth=%u bound=%u",
             start, ch->n, added, (unsigned)remain,
             (unsigned)ch->form_depth, (unsigned)ch->integrate_bound);
    return added;
}

int ikitaku_integrate(ikitaku_concept_chain_t* ch, uint8_t depth,
                      const char** candidates, int n_cand) {
    if (!ch) return -1;
    if (depth > IKITAKU_DEPTH_MAX) depth = IKITAKU_DEPTH_MAX;
    if (ch->integrate_bound == 0) ch->integrate_bound = IKITAKU_DEPTH_MAX;
    if (depth > ch->integrate_bound) depth = ch->integrate_bound;
    ch->integrate_depth = depth;
    int total = 0;
    uint8_t d = depth;
    int idx = 0;
    while (d > 0 && ch->n < IKITAKU_CHAIN_LEN) {
        const char* cand = (candidates && idx < n_cand) ? candidates[idx] : NULL;
        const char* one[1] = { cand };
        int n = ikitaku_dropdown_loop(ch, cand ? one : NULL, cand ? 1 : 0, 1);
        if (n < 1) break;
        total += n;
        idx++;
        d--;
    }
    snprintf(ch->english, sizeof ch->english,
             "integrate depth=%u added=%d chain_n=%d remain=%u (bound %u)",
             (unsigned)depth, total, ch->n,
             (unsigned)ch->integrate_depth, (unsigned)ch->integrate_bound);
    return total;
}



int ikitaku_layer_sure_eval(ikitaku_concept_chain_t* ch) {
    if (!ch) return -1;
    for (int i = 0; i < ch->n; ++i) {
        int sure = 50;
        /* default: higher orders we opened in desperation are less sure */
        if (i == 0) sure = ch->form_depth ? 45 : 75;
        else if (i == 1) sure = 60;
        else if (i <= 3) sure = 55;
        else sure = 35 - (i - 4) * 4;
        if (sure < 10) sure = 10;

        if (ch->slot[i].bound[0]) sure += 15;
        else sure -= 10;

        if (ch->slot[i].var_depth && !ch->slot[i].bound[0])
            sure -= 8; /* open slot: we do not know if this order even exists */

        /* off-by-one: same bind as neighbor → layer assignment shaky */
        if (i > 0 && ch->slot[i].bound[0] && ch->slot[i-1].bound[0]
            && strcmp(ch->slot[i].bound, ch->slot[i-1].bound) == 0)
            sure -= 20;
        if (i + 1 < ch->n && ch->slot[i].bound[0] && ch->slot[i+1].bound[0]
            && strcmp(ch->slot[i].bound, ch->slot[i+1].bound) == 0)
            sure -= 20;

        /* wrap-like names more at home near order 2-5 */
        if (ch->slot[i].bound[0]) {
            int wrapish = (strcmp(ch->slot[i].bound, "sin") == 0
                        || strcmp(ch->slot[i].bound, "tan") == 0
                        || strcmp(ch->slot[i].bound, "wrap") == 0);
            if (wrapish && i >= 2 && i <= 5) sure += 10;
            if (wrapish && i == 0) sure -= 15;
        }

        if (ch->slot[i].platonic_depth && i != 0) sure -= 25;
        if (sure < 0) sure = 0;
        if (sure > 100) sure = 100;
        ch->slot[i].layer_sure = sure;
        /* this-layer interval ~ ±5 around point, clamp */
        int half = (sure >= 70) ? 4 : (sure >= 45 ? 6 : 8);
        int lo = sure - half; if (lo < 0) lo = 0;
        int hi = sure + half; if (hi > 100) hi = 100;
        ch->slot[i].sure_lo = lo;
        ch->slot[i].sure_hi = hi;
        /* mass leftover split to neighbors (off-by-one) */
        int rest = 100 - sure;
        int above = (i == 0) ? rest / 4 : rest / 2;
        int below = rest - above;
        if (i + 1 >= ch->n) { above += below / 2; below = below / 2; }
        int aw = 5;
        ch->slot[i].above_lo = above > aw ? above - aw : 0;
        ch->slot[i].above_hi = above + aw > 100 ? 100 : above + aw;
        ch->slot[i].below_lo = below > aw ? below - aw : 0;
        ch->slot[i].below_hi = below + aw > 100 ? 100 : below + aw;
        /* meta-confidence: calibrated from this-layer interval + bind */
        ch->slot[i].meta_lo = sure - 8;
        if (ch->slot[i].meta_lo < 0) ch->slot[i].meta_lo = 0;
        ch->slot[i].meta_hi = sure + 8;
        if (ch->slot[i].meta_hi > 100) ch->slot[i].meta_hi = 100;
        if (ch->slot[i].bound[0]) {
            ch->slot[i].meta_lo += 4;
            if (ch->slot[i].meta_lo > ch->slot[i].meta_hi)
                ch->slot[i].meta_lo = ch->slot[i].sure_lo;
        }
    }
    return 0;
}

int ikitaku_layer_sure_report(const ikitaku_concept_chain_t* ch,
                              char* buf, int buf_len) {
    if (!ch || !buf || buf_len < 8) return 0;
    int u = 0;
    u += snprintf(buf + u, (size_t)(buf_len - u),
        "order  f^n   concept        this-layer     one-above      one-below      meta-conf\n");
    for (int i = 0; i < ch->n && u < buf_len - 60; ++i) {
        const char* item = ch->slot[i].bound[0] ? ch->slot[i].bound : ch->slot[i].concept;
        char meta[24];
        if (ch->slot[i].meta_lo < 0)
            snprintf(meta, sizeof meta, "STUB later");
        else
            snprintf(meta, sizeof meta, "[%d-%d]", ch->slot[i].meta_lo, ch->slot[i].meta_hi);
        u += snprintf(buf + u, (size_t)(buf_len - u),
            "  %d    f^%-2d  %-14s [%d-%d]         [%d-%d]         [%d-%d]         %s\n",
            i, i, item,
            ch->slot[i].sure_lo, ch->slot[i].sure_hi,
            ch->slot[i].above_lo, ch->slot[i].above_hi,
            ch->slot[i].below_lo, ch->slot[i].below_hi,
            meta);
    }
    return u;
}


int ikitaku_r5_load(ikitaku_r5_map_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    FILE* f = fopen(IKITAKU_R5_CAT, "r");
    if (!f) return 0;
    char line[256];
    while (out->n < IKITAKU_R5_MAX && fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') continue;
        char verb[24], form[80], note[96];
        int locked = 0, r6 = 0;
        verb[0]=form[0]=note[0]=0;
        /* verb|form|locked|r6|note */
        if (sscanf(line, "%23[^|]|%79[^|]|%d|%d|%95[^\n]",
                   verb, form, &locked, &r6, note) < 4)
            continue;
        ikitaku_r5_verb_t* v = &out->v[out->n++];
        snprintf(v->verb, sizeof v->verb, "%s", verb);
        snprintf(v->form, sizeof v->form, "%s", form);
        snprintf(v->note, sizeof v->note, "%s", note);
        v->locked = locked ? 1 : 0;
        v->r6 = r6 ? 1 : 0;
        if (v->locked) out->n_locked++;
        else out->n_novel++;
    }
    fclose(f);
    return out->n;
}

int ikitaku_r5_lookup(const char* verb, ikitaku_r5_verb_t* out) {
    if (!verb || !verb[0]) return -1;
    ikitaku_r5_map_t m;
    if (ikitaku_r5_load(&m) <= 0) return -1;
    /* generate aliases create */
    const char* key = verb;
    if (strcmp(verb, "create") == 0) key = "generate";
    for (int i = 0; i < m.n; ++i) {
        if (strcmp(m.v[i].verb, verb) == 0 || strcmp(m.v[i].verb, key) == 0) {
            if (out) *out = m.v[i];
            return 0;
        }
    }
    return -1;
}

int ikitaku_r5_is_platonic(const char* verb) {
    ikitaku_r5_verb_t v;
    if (ikitaku_r5_lookup(verb, &v) != 0) return 0;
    return (v.locked && !v.r6) ? 1 : 0;
}

int ikitaku_r5_propose(const char* verb, const char* form, const char* note) {
    if (!verb || !verb[0]) return -1;
    ikitaku_r5_verb_t exist;
    if (ikitaku_r5_lookup(verb, &exist) == 0) return 1; /* already known */
    FILE* f = fopen(IKITAKU_R5_CAT, "a");
    if (!f) return -1;
    fprintf(f, "%s|%s|0|0|%s\n",
            verb,
            (form && form[0]) ? form : "novel L5 candidate",
            (note && note[0]) ? note : "proposed, not locked");
    fclose(f);
    return 0;
}

int ikitaku_r5_object_list(char names[][24], int maxn) {
    ikitaku_r5_map_t m;
    if (ikitaku_r5_load(&m) < 0) return 0;
    int n = 0;
    for (int i = 0; i < m.n && n < maxn; ++i) {
        if (!m.v[i].locked || m.v[i].r6) continue;
        if (names) snprintf(names[n], 24, "%s", m.v[i].verb);
        n++;
    }
    return n;
}

static void r5_mark(ikitaku_r5_id_report_t* r, const char* verb,
                    int conf, const char* ev) {
    for (int i = 0; i < r->n; ++i) {
        if (strcmp(r->item[i].verb, verb) != 0) continue;
        if (conf > r->item[i].conf) {
            r->item[i].hit = 1;
            r->item[i].conf = conf;
            snprintf(r->item[i].evidence, sizeof r->item[i].evidence, "%s", ev);
        }
        return;
    }
}

int ikitaku_r5_identify(const rsvm_t* vm,
                        const ikitaku_session_t* sess,
                        const ikitaku_series_t* series,
                        ikitaku_r5_id_report_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    char names[24][24];
    int nv = ikitaku_r5_object_list(names, 24);
    for (int i = 0; i < nv && out->n < 24; ++i) {
        snprintf(out->item[out->n].verb, 24, "%s", names[i]);
        out->n++;
    }
    if (sess) {
        for (int i = 0; i < sess->hypo_count; ++i) {
            const char* p = sess->hypos[i].purpose;
            int c = sess->hypos[i].confidence;
            if (p[0]) r5_mark(out, p, c, sess->hypos[i].form_id);
            if (strcmp(p, "decision") == 0)
                r5_mark(out, "compare", c > 10 ? c - 10 : c, "decision uses compare");
        }
    }
    if (vm && vm->code_len) {
        int pr = 0, del = 0, cmp = 0, mov = 0, arith = 0, rd = 0, ldi = 0, arr = 0;
        for (uint16_t i = 0; i < vm->code_len; ++i) {
            uint8_t op = vm->code[i];
            if (op == 0x02 || op == 0x03 || op == 0x04 || op == 0x05) pr++;
            if (op == 0x50) del++;
            if (op >= 0x30 && op <= 0x35) cmp++;
            if (op == 0x11) mov++;
            if (op >= 0x20 && op <= 0x25) arith++;
            if (op == 0x53 || op == 0x54 || op == 0x56 || op == 0x58) rd++;
            if (op == 0x10 || op == 0x14) ldi++;
            if (op >= 0xC2 && op <= 0xCC) arr++;
        }
        if (pr)  r5_mark(out, "emit", pr > 3 ? 80 : 65, "print/host out");
        if (arr) {
            r5_mark(out, "info", 75, "indexed bag (array)");
            r5_mark(out, "join", 55, "arr pack/push");
            r5_mark(out, "split", 60, "index-partition of bag");
        }
        if (del) r5_mark(out, "wait", 85, "DELAY_MS");
        if (cmp) r5_mark(out, "compare", 75, "cmp opcode");
        if (mov) r5_mark(out, "copy", 60, "MOV");
        if (arith) r5_mark(out, "map", 70, "arith op");
        if (rd) r5_mark(out, "absorb", 70, "gpio/adc read");
        if (ldi) r5_mark(out, "name", 55, "LDI bind");
        if (ldi) r5_mark(out, "info", 50, "slot exists");
        if (vm->code_len > 4) r5_mark(out, "sequence", 50, "ops in order");
    }
    if (series && series->n >= 3) {
        ikitaku_schema_t sc;
        ikitaku_schema_extract(series, &sc);
        if (strcmp(sc.purpose, "map-math") == 0)
            r5_mark(out, "map", sc.conf, sc.schema);
        if (strcmp(sc.purpose, "map-table") == 0)
            r5_mark(out, "map", sc.conf, sc.schema);
        if (strcmp(sc.purpose, "generate") == 0)
            r5_mark(out, "generate", sc.conf, sc.schema);
        if (strcmp(sc.purpose, "info") == 0)
            r5_mark(out, "info", sc.conf, sc.schema);
    }
    out->n_hit = 0;
    char hits[160] = {0};
    int u = 0;
    for (int i = 0; i < out->n; ++i) if (out->item[i].hit) {
        out->n_hit++;
        u += snprintf(hits + u, sizeof hits - (size_t)u, "%s%s",
                      u ? "," : "", out->item[i].verb);
    }
    snprintf(out->english, sizeof out->english,
             "L5 identify: %d/%d verbs  [%s]", out->n_hit, out->n, hits);
    return out->n_hit;
}

int ikitaku_curiosity(const rsvm_t* vm) {
    if (!vm) return 0;
    int c = (int)vm->code_len / 70;
    int jz = 0, cmp = 0;
    for (uint16_t i = 0; i < vm->code_len; ++i) {
        uint8_t op = vm->code[i];
        if (op == 0x41 || op == 0x42) jz++;
        if (op >= 0x30 && op <= 0x35) cmp++;
    }
    c += jz / 4 + cmp / 6;
    if (c > 5) c = 5;
    if (c < 0) c = 0;
    return c;
}

static int watch_inv(const int32_t* a, int n) {
    int c = 0;
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (a[i] < a[j]) c++;
    return c;
}

int ikitaku_watch_run(rsvm_t* vm, ikitaku_watch_t* out) {
    if (!vm || !out) return -1;
    memset(out, 0, sizeof *out);
    out->curiosity = ikitaku_curiosity(vm);
    out->mental_ok = 1;
    for (uint16_t i = 0; i < vm->code_len; ++i) {
        uint8_t op = vm->code[i];
        if (op == 0x50 || op == 0x53 || op == 0x54 || op == 0x56 || op == 0x58)
            out->mental_ok = 0;
    }
    int n = vm->slot_count;
    if (n > IKITAKU_WATCH_SLOTS) n = IKITAKU_WATCH_SLOTS;
    out->nslot = n;
    for (int i = 0; i < n; ++i)
        out->before[i] = vm->slots[i].v;
    rsvm_status_t st = rsvm_run(vm);
    out->ran = (st == RSVM_OK || st == 0) ? 1 : 1;
    for (int i = 0; i < n; ++i)
        out->after[i] = vm->slots[i].v;

    int32_t chb[IKITAKU_WATCH_SLOTS], cha[IKITAKU_WATCH_SLOTS];
    int nc = 0;
    for (int i = 0; i < n && nc < IKITAKU_WATCH_SLOTS; ++i) {
        if (out->before[i] == out->after[i]) continue;
        chb[nc] = out->before[i];
        cha[nc] = out->after[i];
        nc++;
    }
    if (nc >= 3) {
        int32_t sb[IKITAKU_WATCH_SLOTS], sa[IKITAKU_WATCH_SLOTS];
        for (int i = 0; i < nc; ++i) { sb[i] = chb[i]; sa[i] = cha[i]; }
        for (int i = 0; i < nc; ++i)
            for (int j = i + 1; j < nc; ++j) {
                if (sb[j] < sb[i]) { int32_t t = sb[i]; sb[i] = sb[j]; sb[j] = t; }
                if (sa[j] < sa[i]) { int32_t t = sa[i]; sa[i] = sa[j]; sa[j] = t; }
            }
        int perm = 1;
        for (int i = 0; i < nc; ++i) if (sb[i] != sa[i]) perm = 0;
        out->inversions_before = watch_inv(chb, nc);
        out->inversions_after = watch_inv(cha, nc);
        if (perm && out->inversions_after + 1 <= out->inversions_before)
            out->lift_order = 1;
    }
    /* R4 name families: h0,h1,h2… same prefix letter+digit */
    if (!out->lift_order && n >= 3) {
        for (char pref = 'a'; pref <= 'z' && !out->lift_order; ++pref) {
            int32_t fb[16], fa[16]; int fn = 0;
            for (int i = 0; i < n && fn < 16; ++i) {
                const char* nm = vm->names[i];
                if (nm[0] == pref && nm[1] >= '0' && nm[1] <= '9') {
                    fb[fn] = out->before[i];
                    fa[fn] = out->after[i];
                    fn++;
                }
            }
            if (fn < 3) continue;
            int ib = watch_inv(fb, fn), ia = watch_inv(fa, fn);
            int desc = 1, asc = 1;
            for (int k = 1; k < fn; ++k) {
                if (fa[k] > fa[k-1]) desc = 0;
                if (fa[k] < fa[k-1]) asc = 0;
            }
            if (ia + 1 <= ib || desc || asc) {
                out->lift_order = 1;
                out->inversions_before = ib;
                out->inversions_after = ia;
            }
        }
    }
    /* Heap arrays: a bag of values with index axis — not a scalar slot. */
    if (!out->lift_order) {
        for (int s = 0; s < n; ++s) {
            if (vm->slots[s].ty != 6) continue; /* RSVM_TY_ARR */
            int32_t off = vm->slots[s].v;
            if (off < 0) continue;
            uint8_t* meta_p = vm->heap + off + 4; /* hdr pack: u16+u8+u8 */
            int32_t alen = meta_p[1] ? (int32_t)meta_p[1] : (int32_t)vm->slots[s].aux;
            if (alen < 3) continue;
            if (alen > 32) alen = 32;
            size_t meta = 1 + 4;
            rsvm_val_t* el = (rsvm_val_t*)(vm->heap + off + 4 + meta);
            int32_t fa[32];
            int asc = 1, desc = 1;
            for (int k = 0; k < alen; ++k) {
                fa[k] = el[k].v;
                if (k && fa[k] < fa[k-1]) asc = 0;
                if (k && fa[k] > fa[k-1]) desc = 0;
            }
            out->inversions_after = watch_inv(fa, alen);
            if (asc || desc) {
                out->lift_order = 1;
                out->lift_seek = 1;
                break;
            }
        }
    }
    if (out->lift_order) out->lift_seek = 1;

    snprintf(out->r4, sizeof out->r4, "%s",
             out->lift_order ? "order-key" :
             out->lift_seek ? "seek-slot" :
             out->lift_bound ? "bound-cap" : "compare-branch");
    snprintf(out->english, sizeof out->english,
             "watch curi=%d mental=%d changed=%d inv %d→%d  lift order=%d seek=%d bound=%d r4=%s",
             out->curiosity, out->mental_ok, nc,
             out->inversions_before, out->inversions_after,
             out->lift_order, out->lift_seek, out->lift_bound, out->r4);
    return 0;
}

int ikitaku_watch_apply(const ikitaku_watch_t* w, ikitaku_r5_id_report_t* id) {
    if (!w || !id) return -1;
    auto mark = [&](const char* verb, int conf, const char* ev) {
        for (int i = 0; i < id->n; ++i) {
            if (strcmp(id->item[i].verb, verb) != 0) continue;
            if (conf > id->item[i].conf) {
                id->item[i].hit = 1;
                id->item[i].conf = conf;
                snprintf(id->item[i].evidence, sizeof id->item[i].evidence, "%s", ev);
            }
            return;
        }
    };
    if (w->lift_order) mark("order", 70 + w->curiosity * 4, "trace: inversions dropped");
    if (w->lift_seek)  mark("seek",  55 + w->curiosity * 3, "trace: witness min/max");
    if (w->lift_bound) mark("bound", 50 + w->curiosity * 2, "trace: cap held");
    id->n_hit = 0;
    char hits[160] = {0}; int u = 0;
    for (int i = 0; i < id->n; ++i) if (id->item[i].hit) {
        id->n_hit++;
        u += snprintf(hits + u, sizeof hits - (size_t)u, "%s%s",
                      u ? "," : "", id->item[i].verb);
    }
    snprintf(id->english, sizeof id->english,
             "L5+watch: %d/%d [%s] r4=%s curi=%d",
             id->n_hit, id->n, hits, w->r4, w->curiosity);
    return id->n_hit;
}

int ikitaku_r4_load(ikitaku_r4_map_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    FILE* f = fopen(IKITAKU_R4_CAT, "r");
    if (!f) return 0;
    char line[256];
    while (out->n < IKITAKU_R4_MAX && fgets(line, sizeof line, f)) {
        if (line[0]=='#' || line[0]=='\n') continue;
        ikitaku_r4_fam_t* e = &out->f[out->n];
        int locked = 1;
        e->parent[0]=e->family[0]=e->form[0]=e->note[0]=0;
        if (sscanf(line, "%23[^|]|%31[^|]|%79[^|]|%d|%63[^\n]",
                   e->parent, e->family, e->form, &locked, e->note) < 4)
            continue;
        e->locked = locked ? 1 : 0;
        out->n++;
    }
    fclose(f);
    return out->n;
}

int ikitaku_r4_under(const char* parent, char fams[][32], int maxn) {
    ikitaku_r4_map_t m;
    if (ikitaku_r4_load(&m) < 0 || !parent) return 0;
    int n = 0;
    for (int i = 0; i < m.n && n < maxn; ++i) {
        if (strcmp(m.f[i].parent, parent) != 0) continue;
        if (fams) snprintf(fams[n], 32, "%s", m.f[i].family);
        n++;
    }
    return n;
}

int ikitaku_r4_propose(const char* parent, const char* family, const char* form) {
    if (!parent || !family) return -1;
    ikitaku_r4_map_t m;
    ikitaku_r4_load(&m);
    for (int i = 0; i < m.n; ++i)
        if (strcmp(m.f[i].parent, parent)==0 && strcmp(m.f[i].family, family)==0)
            return 1;
    FILE* f = fopen(IKITAKU_R4_CAT, "a");
    if (!f) return -1;
    fprintf(f, "%s|%s|%s|0|proposed R4\n", parent, family,
            (form && form[0]) ? form : "novel family");
    fclose(f);
    return 0;
}

static void r4_set(ikitaku_r5_id_t* it, const char* fam) {
    if (!it->hit) return;
    if (it->r4[0] && strcmp(it->r4, fam)==0) return;
    if (!it->r4[0]) snprintf(it->r4, sizeof it->r4, "%s", fam);
}

int ikitaku_r4_attach(ikitaku_r5_id_report_t* id, const ikitaku_watch_t* w) {
    if (!id) return -1;
    const char* wr4 = (w && w->r4[0]) ? w->r4 : "";
    for (int i = 0; i < id->n; ++i) {
        ikitaku_r5_id_t* it = &id->item[i];
        if (!it->hit) continue;
        if (strcmp(it->verb, "map")==0) {
            if (strstr(it->evidence, "period") || strstr(it->evidence, "table"))
                r4_set(it, "map-table");
            else if (strstr(it->evidence, "x^") || strstr(it->evidence, "sin"))
                r4_set(it, "map-math");
            else r4_set(it, "map-pointwise");
        } else if (strcmp(it->verb, "reduce")==0)
            r4_set(it, "affine-reduce");
        else if (strcmp(it->verb, "repeat")==0) {
            if (strstr(wr4, "time") || strstr(it->evidence, "wait"))
                r4_set(it, "time-axis");
            else if (strstr(it->evidence, "counted"))
                r4_set(it, "counted");
            else r4_set(it, "iterator-axis");
        } else if (strcmp(it->verb, "decision")==0)
            r4_set(it, "binary-cmp");
        else if (strcmp(it->verb, "compare")==0)
            r4_set(it, "lt-sign");
        else if (strcmp(it->verb, "order")==0)
            r4_set(it, strstr(wr4, "order") ? "key-sort" : "compare-swap");
        else if (strcmp(it->verb, "split")==0)
            r4_set(it, "partition");
        else if (strcmp(it->verb, "join")==0)
            r4_set(it, strstr(it->evidence, "push") ? "concat" : "concat");
        else if (strcmp(it->verb, "seek")==0)
            r4_set(it, "witness-minmax");
        else if (strcmp(it->verb, "wait")==0)
            r4_set(it, "delay-block");
        else if (strcmp(it->verb, "emit")==0)
            r4_set(it, "print");
        else if (strcmp(it->verb, "copy")==0)
            r4_set(it, "mov");
        else if (strcmp(it->verb, "sequence")==0)
            r4_set(it, "then");
        else if (strcmp(it->verb, "name")==0)
            r4_set(it, "bind-slot");
        else if (strcmp(it->verb, "info")==0) {
            if (strstr(it->evidence, "array") || strstr(it->evidence, "bag") ||
                strstr(it->evidence, "indexed"))
                r4_set(it, "array");
            else
                r4_set(it, "scalar");
        }
        else if (strcmp(it->verb, "bound")==0)
            r4_set(it, "clamp");
        else if (strcmp(it->verb, "absorb")==0)
            r4_set(it, "sensor");
        else {
            /* self-discovery: hit R5 with no family → propose under that parent */
            char fams[8][32];
            if (ikitaku_r4_under(it->verb, fams, 8) == 0)
                ikitaku_r4_propose(it->verb, "untyped", "no R4 family matched");
        }
    }
    return 0;
}

int ikitaku_discovery_propose(const ikitaku_discovery_t* d) {
    if (!d || !d->r5[0] || !d->form[0]) return -1;
    char dir[256], path[320], cmd[360];
    const char* r4 = d->r4[0] ? d->r4 : "unknown";
    snprintf(dir, sizeof dir, "%s/%s/%s", IKITAKU_DISCOVERY_ROOT, d->r5, r4);
    snprintf(cmd, sizeof cmd, "mkdir -p '%s'", dir);
    if (system(cmd) != 0) return -1;
    snprintf(path, sizeof path, "%s/%s.form", dir, d->form);
    FILE* exist = fopen(path, "r");
    if (exist) { fclose(exist); return 1; }
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "r5=%s\nr4=%s\nform=%s\nalias=%s\nevidence=%s\nconf=%d\nlocked=%u\n",
            d->r5, r4, d->form,
            d->alias[0] ? d->alias : "-",
            d->evidence[0] ? d->evidence : "-",
            d->conf, d->locked);
    fclose(f);
    return 0;
}

int ikitaku_discovery_load(const char* r5, const char* r4,
                           ikitaku_discovery_t* out, int maxn) {
    if (!r5 || !out || maxn < 1) return 0;
    char cmd[400], path[256];
    const char* fr4 = (r4 && r4[0]) ? r4 : "*";
    snprintf(path, sizeof path, "%s/%s/%s", IKITAKU_DISCOVERY_ROOT, r5, fr4);
    snprintf(cmd, sizeof cmd, "ls -1 '%s'/*.form 2>/dev/null", path);
    FILE* p = popen(cmd, "r");
    if (!p) return 0;
    int n = 0;
    char line[320];
    while (n < maxn && fgets(line, sizeof line, p)) {
        char* nl = strchr(line, '\n'); if (nl) *nl = 0;
        FILE* f = fopen(line, "r");
        if (!f) continue;
        memset(&out[n], 0, sizeof out[n]);
        char k[32], v[96];
        while (fgets(line, sizeof line, f)) {
            if (sscanf(line, "%31[^=]=%95[^\n]", k, v) != 2) continue;
            if (!strcmp(k,"r5")) snprintf(out[n].r5, sizeof out[n].r5, "%s", v);
            else if (!strcmp(k,"r4")) snprintf(out[n].r4, sizeof out[n].r4, "%s", v);
            else if (!strcmp(k,"form")) snprintf(out[n].form, sizeof out[n].form, "%s", v);
            else if (!strcmp(k,"alias")) snprintf(out[n].alias, sizeof out[n].alias, "%s", v);
            else if (!strcmp(k,"evidence")) snprintf(out[n].evidence, sizeof out[n].evidence, "%s", v);
            else if (!strcmp(k,"conf")) out[n].conf = atoi(v);
            else if (!strcmp(k,"locked")) out[n].locked = (uint8_t)atoi(v);
        }
        fclose(f);
        if (out[n].form[0]) n++;
    }
    pclose(p);
    return n;
}

static int id_has(const ikitaku_r5_id_report_t* id, const char* v) {
    for (int i = 0; i < id->n; ++i)
        if (id->item[i].hit && strcmp(id->item[i].verb, v)==0) return 1;
    return 0;
}

int ikitaku_discovery_consider(const ikitaku_r5_id_report_t* id,
                               const ikitaku_watch_t* w,
                               ikitaku_discovery_t* out) {
    if (!id || !out) return -1;
    memset(out, 0, sizeof *out);
    int bag   = id_has(id, "info");
    int ord   = id_has(id, "order") || (w && w->lift_order);
    int split = id_has(id, "split");
    int join  = id_has(id, "join");
    int rep   = id_has(id, "repeat");
    int dec   = id_has(id, "decision");
    if (bag && ord && split && join && rep && dec) {
        snprintf(out->r5, sizeof out->r5, "order");
        snprintf(out->r4, sizeof out->r4, "key-sort");
        snprintf(out->form, sizeof out->form, "partition-then-order");
        snprintf(out->alias, sizeof out->alias, "qsort");
        snprintf(out->evidence, sizeof out->evidence,
                 "array + split + join + repeat(decision) + monotone bag");
        out->conf = 72;
        out->locked = 0;
        ikitaku_discovery_propose(out);
        return 1;
    }
    if (bag && ord && rep && dec) {
        snprintf(out->r5, sizeof out->r5, "order");
        snprintf(out->r4, sizeof out->r4, "key-sort");
        snprintf(out->form, sizeof out->form, "repeat-decision-order");
        snprintf(out->alias, sizeof out->alias, "-");
        snprintf(out->evidence, sizeof out->evidence,
                 "array + repeat(decision) + monotone bag");
        out->conf = 64;
        ikitaku_discovery_propose(out);
        return 1;
    }
    return 0;
}

static int origin_freq(const ikitaku_session_t* s, const char* p) {
    int n = 0;
    for (int i = 0; i < s->hypo_count; ++i)
        if (strcmp(s->hypos[i].purpose, p) == 0) n++;
    return n;
}

static int origin_has_synth(const ikitaku_session_t* s, const char* a, const char* b) {
    for (int i = 0; i < s->hypo_count; ++i) {
        if (!s->hypos[i].synthesized) continue;
        const char* f = s->hypos[i].form_id;
        const char* p = s->hypos[i].purpose;
        if ((strstr(f, a) && strstr(f, b)) ||
            (strstr(p, a) && strstr(p, b)) ||
            strcmp(p, "repeat_x_decision") == 0)
            return 1;
    }
    return 0;
}

int ikitaku_origin_trace(const ikitaku_session_t* s,
                         const ikitaku_watch_t* w,
                         const ikitaku_r5_id_report_t* id,
                         ikitaku_origin_t* out) {
    if (!s || !out) return -1;
    memset(out, 0, sizeof *out);
    if (s->hypo_count < 1) {
        snprintf(out->english, sizeof out->english, "origin: no events yet");
        return 0;
    }

    char purposes[16][32];
    int pc = 0;
    for (int i = 0; i < s->hypo_count && pc < 16; ++i) {
        const char* p = s->hypos[i].purpose;
        if (!p[0]) continue;
        int seen = 0;
        for (int k = 0; k < pc; ++k) if (strcmp(purposes[k], p)==0) seen = 1;
        if (!seen) snprintf(purposes[pc++], 32, "%s", p);
    }

    char C[32]; C[0] = 0;
    int c_unexp = 0;
    if (w && w->lift_order) {
        snprintf(C, sizeof C, "order");
        c_unexp = 1; /* C appeared as a trace event, not the accept */
    }
    if (!C[0] && id) {
        for (int i = 0; i < id->n; ++i)
            if (id->item[i].hit && strcmp(id->item[i].verb, "order")==0)
                snprintf(C, sizeof C, "order");
    }
    if (!C[0]) {
        /* last accepted purpose */
        for (int i = s->hypo_count - 1; i >= 0; --i)
            if (s->hypos[i].confidence >= 80) {
                snprintf(C, sizeof C, "%s", s->hypos[i].purpose);
                break;
            }
    }
    if (!C[0] && pc) snprintf(C, sizeof C, "%s", purposes[pc-1]);

    /* B = most frequent purpose that is not C */
    char B[32]; B[0] = 0;
    int bf = 0;
    for (int i = 0; i < pc; ++i) {
        if (strcmp(purposes[i], C)==0) continue;
        int f = origin_freq(s, purposes[i]);
        if (f > bf) { bf = f; snprintf(B, sizeof B, "%s", purposes[i]); }
    }
    /* prefer decision as B when C is order */
    if (strcmp(C, "order")==0) {
        int df = origin_freq(s, "decision");
        if (df >= 2) { snprintf(B, sizeof B, "decision"); bf = df; }
    }

    char A[32]; A[0] = 0;
    int reversed = 0;
    if (bf >= 3 && B[0]) {
        reversed = 1;
        int af = 0;
        for (int i = 0; i < pc; ++i) {
            if (strcmp(purposes[i], B)==0 || strcmp(purposes[i], C)==0) continue;
            int f = origin_freq(s, purposes[i]);
            if (f > af) { af = f; snprintf(A, sizeof A, "%s", purposes[i]); }
        }
        if (!A[0] && origin_freq(s, "repeat") > 0)
            snprintf(A, sizeof A, "repeat");
    }

    if (!B[0]) {
        snprintf(out->english, sizeof out->english,
                 "origin: saw C=%s but no frequent B to invert", C);
        return 0;
    }

    ikitaku_cause_link_t* L = &out->link[out->n++];
    snprintf(L->b, sizeof L->b, "%s", B);
    snprintf(L->c, sizeof L->c, "%s", C);
    if (A[0]) snprintf(L->a, sizeof L->a, "%s", A);
    L->b_freq = bf;
    L->c_unexpected = c_unexp;
    L->reversed = reversed;
    if (reversed) out->reversed_n++;

    /* associate-consider intervals from freq + affinity-ish */
    int bc = 50 + (bf > 8 ? 20 : bf * 2);
    if (c_unexp) bc += 8;
    if (bc > 88) bc = 88;
    L->bc_lo = bc - 12; if (L->bc_lo < 20) L->bc_lo = 20;
    L->bc_hi = bc + 10; if (L->bc_hi > 95) L->bc_hi = 95;
    if (A[0]) {
        int af = origin_freq(s, A);
        int ab = 45 + (af > 6 ? 15 : af * 2);
        L->ab_lo = ab - 12; if (L->ab_lo < 15) L->ab_lo = 15;
        L->ab_hi = ab + 10; if (L->ab_hi > 90) L->ab_hi = 90;
    }

    L->tested = 1;
    L->test_ok = origin_has_synth(s, B[0]?B:"x", A[0]?A:"y") ||
                 origin_has_synth(s, "decision", "repeat") ||
                 (c_unexp && bf >= 3);
    if (L->test_ok) {
        if (A[0])
            snprintf(L->rule, sizeof L->rule, "%s → %s → %s", A, B, C);
        else
            snprintf(L->rule, sizeof L->rule, "%s → %s", B, C);
    } else {
        snprintf(L->rule, sizeof L->rule, "%s co-occurs with %s (rule not locked)", B, C);
    }
    if (c_unexp)
        snprintf(L->why, sizeof L->why,
                 "C=%s showed up on the trace; B=%s was already frequent so invert for A",
                 C, B);
    else
        snprintf(L->why, sizeof L->why,
                 "C=%s was the standing accept; B=%s is the prior event", C, B);

    out->confusion = (c_unexp ? 40 : 12) + (bf > 4 ? 15 : bf * 2);
    if (out->confusion > 100) out->confusion = 100;
    out->fired = out->confusion >= IKITAKU_ORIGIN_THRESH ? 1 : 0;

    /* similar-pattern memory tightens the interval */
    {
        FILE* pf = fopen(IKITAKU_ORIGIN_PAT, "r");
        if (pf) {
            char line[160];
            while (fgets(line, sizeof line, pf)) {
                if (L->rule[0] && strstr(line, L->rule)) {
                    out->pattern_hits++;
                    L->bc_lo += 6; if (L->bc_lo > 80) L->bc_lo = 80;
                    L->bc_hi += 4; if (L->bc_hi > 96) L->bc_hi = 96;
                    L->test_ok = 1;
                }
            }
            fclose(pf);
        }
        if (L->test_ok && L->rule[0]) {
            system("mkdir -p '/home/workdir/artifacts/rs_vm/ikitaku/discovery/origin'");
            FILE* wpat = fopen(IKITAKU_ORIGIN_PAT, "a");
            if (wpat) {
                fprintf(wpat, "%s\n", L->rule);
                fclose(wpat);
            }
        }
    }

    snprintf(out->english, sizeof out->english,
             "origin R6: %s  bc[%d-%d] confu=%d %s%s hits=%d test=%s",
             L->rule, L->bc_lo, L->bc_hi, out->confusion,
             L->c_unexpected ? "C-unexpected " : "",
             L->reversed ? "reversed-B " : "",
             out->pattern_hits,
             L->test_ok ? "hold" : "open");
    return out->n;
}

int ikitaku_complexity_est(const rsvm_t* vm, const ikitaku_session_t* s,
                           ikitaku_probe_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    int dec = 0, rep = 0, cmp = 0;
    if (s) {
        for (int i = 0; i < s->hypo_count; ++i) {
            if (strcmp(s->hypos[i].purpose, "decision")==0) dec++;
            if (strcmp(s->hypos[i].purpose, "repeat")==0) rep++;
        }
    }
    if (vm) {
        for (uint16_t i = 0; i < vm->code_len; ++i)
            if (vm->code[i] >= 0x30 && vm->code[i] <= 0x35) cmp++;
    }
    int surface = dec + rep + (vm ? (int)vm->code_len / 40 : 0);
    /* boxes look thinner than they are — add prior before peeking */
    int hidden = 0;
    if (dec >= 3 && rep >= 1) hidden = 18 + dec;
    else if (dec >= 2) hidden = 10;
    if (cmp >= 8 && surface < 8) hidden += 12;
    out->surface = surface;
    out->hidden_prior = hidden;
    out->complexity = surface + hidden;
    if (out->complexity > 80) out->complexity = 80;
    out->box = hidden >= 12 ? 1 : 0;
    snprintf(out->english, sizeof out->english,
             "complexity surface=%d hidden=+%d total=%d box=%d",
             surface, hidden, out->complexity, out->box);
    return out->complexity;
}

static int probe_period(const int32_t* a, int n) {
    if (n < 4) return 0;
    for (int p = 2; p <= n / 2 && p <= 8; ++p) {
        int ok = 1, hits = 0;
        for (int i = p; i < n; ++i) {
            if (a[i] != a[i - p]) { ok = 0; break; }
            hits++;
        }
        if (ok && hits >= p) return p;
    }
    return 0;
}

int ikitaku_probe_box(rsvm_t* vm, const ikitaku_session_t* s,
                      int confusion_in, ikitaku_probe_t* out) {
    if (!vm || !out) return -1;
    ikitaku_complexity_est(vm, s, out);
    out->confusion_in = confusion_in;
    out->depth = 1;
    int budget = 120 + out->complexity * 10;
    if (budget > 1600) budget = 1600;

    rsvm_t tmp = *vm;
    tmp.pc = 0;
    tmp.running = true;
    tmp.last_status = RSVM_OK;
    tmp.steps = 0;
    tmp.host.print_i32 = 0;
    tmp.host.print_str = 0;
    tmp.host.print_char = 0;

    typedef struct { int32_t seq[16][32]; int slen[16]; } probe_log_t;
    probe_log_t log;
    memset(&log, 0, sizeof log);
    tmp.host.user = &log;
    tmp.host.on_slot = [](uint8_t s, int32_t v, void* u) {
        probe_log_t* L = (probe_log_t*)u;
        if (s >= 16) return;
        if (L->slen[s] < 32) L->seq[s][L->slen[s]++] = v;
    };

    for (int t = 0; t < budget && tmp.running && tmp.last_status == RSVM_OK; ++t) {
        rsvm_status_t st = rsvm_step(&tmp);
        out->budget_used++;
        if (st != RSVM_OK && st != 0) break;
        if (tmp.pc >= tmp.code_len) break;
    }
    out->trials = 1;
    int32_t (*seq)[32] = log.seq;
    int* slen = log.slen;

    int best = -1, bestn = 0;
    for (int i = 0; i < 16; ++i)
        if (slen[i] > bestn) { bestn = slen[i]; best = i; }

    if (best >= 0 && bestn >= 3) {
        int inc = 1;
        for (int k = 1; k < bestn; ++k)
            if (seq[best][k] != seq[best][k-1] + 1) inc = 0;
        int per = probe_period(seq[best], bestn);
        if (inc) {
            snprintf(out->rule, sizeof out->rule, "slot%d increment", best);
            out->period = 0;
        } else if (per) {
            out->period = per;
            snprintf(out->rule, sizeof out->rule, "slot%d cycle-%d", best, per);
        } else {
            snprintf(out->rule, sizeof out->rule, "slot%d opaque-trace", best);
        }
    } else {
        snprintf(out->rule, sizeof out->rule, "no internal trace");
    }

    /* recurse once if still opaque and we budgeted for a box */
    if (out->box && strstr(out->rule, "opaque") && out->depth < 2) {
        out->depth = 2;
        out->trials++;
        int per = (best >= 0) ? probe_period(seq[best], bestn) : 0;
        if (per) {
            out->period = per;
            snprintf(out->rule, sizeof out->rule, "slot%d cycle-%d", best, per);
        }
    }

    int drop = 0;
    if (strstr(out->rule, "cycle") || strstr(out->rule, "increment"))
        drop = 25 + (out->period ? 10 : 0);
    else if (out->box)
        drop = -8; /* peek confirmed it was hiding something */
    out->confusion_out = confusion_in - drop;
    if (out->confusion_out < 0) out->confusion_out = 0;
    if (out->confusion_out > 100) out->confusion_out = 100;

    snprintf(out->english, sizeof out->english,
             "probe box=%d cx=%d used=%d d=%d rule=%s confu %d→%d",
             out->box, out->complexity, out->budget_used, out->depth,
             out->rule, out->confusion_in, out->confusion_out);
    return out->box;
}

int ikitaku_behavior_tick(rsvm_t* vm, const ikitaku_session_t* s,
                          ikitaku_behavior_t* out) {
    if (!vm || !s || !out) return -1;
    memset(out, 0, sizeof *out);
    ikitaku_r5_identify(vm, s, NULL, &out->id);
    ikitaku_complexity_est(vm, s, &out->probe);
    out->confusion = out->probe.hidden_prior + out->probe.surface;
    if (out->confusion > 80) out->confusion = 80;
    if (out->probe.box || out->confusion >= 20)
        ikitaku_probe_box(vm, s, out->confusion, &out->probe);
    if (out->probe.budget_used > 0)
        out->confusion = out->probe.confusion_out;
    ikitaku_watch_run(vm, &out->watch);
    ikitaku_watch_apply(&out->watch, &out->id);
    ikitaku_r4_attach(&out->id, &out->watch);
    ikitaku_discovery_consider(&out->id, &out->watch, &out->discovery);

    int acc = 0;
    for (int i = 0; i < s->hypo_count; ++i)
        if (s->hypos[i].confidence > acc) acc = s->hypos[i].confidence;
    int c_hit = 0;
    if (out->watch.lift_order) c_hit = 1;
    for (int i = 0; i < out->id.n; ++i)
        if (out->id.item[i].hit &&
            (strcmp(out->id.item[i].verb, "order")==0 ||
             strcmp(out->id.item[i].verb, "split")==0 ||
             strcmp(out->id.item[i].verb, "bound")==0))
            c_hit = 1;

    /* keep probe confusion; C can bump it back up */
    if (c_hit) out->confusion += 28 + out->watch.curiosity * 3;
    if (out->watch.mental_ok == 0) out->confusion += 8;
    (void)acc;
    if (out->confusion > 100) out->confusion = 100;
    if (out->confusion < 0) out->confusion = 0;

    if (out->confusion >= IKITAKU_ORIGIN_THRESH) {
        out->origin_fired = 1;
        ikitaku_origin_trace(s, &out->watch, &out->id, &out->origin);
        if (out->origin.confusion < out->confusion)
            out->origin.confusion = out->confusion;
        out->origin.fired = 1;
    } else {
        snprintf(out->origin.english, sizeof out->origin.english,
                 "origin idle (confusion %d < %d)",
                 out->confusion, IKITAKU_ORIGIN_THRESH);
    }

    snprintf(out->english, sizeof out->english,
             "behavior: confusion=%d origin=%s C=%s abc=%s probe=%s cx=%d",
             out->confusion,
             out->origin_fired ? "FIRE" : "idle",
             out->watch.lift_order ? "order" : (c_hit ? "event" : "-"),
             out->origin.n ? out->origin.link[0].rule : "-",
             out->probe.rule[0] ? out->probe.rule : "-",
             out->probe.complexity);
    return out->origin_fired;
}

static int cause_first(const ikitaku_session_t* s, const char* p) {
    for (int i = 0; i < s->hypo_count; ++i)
        if (strcmp(s->hypos[i].purpose, p)==0) return i;
    return -1;
}

static int cause_span_fwd(const ikitaku_session_t* s, const char* a, const char* b) {
    int fwd = 0, bwd = 0, n = 0;
    for (int i = 0; i < s->hypo_count; ++i) {
        if (strcmp(s->hypos[i].purpose, a)!=0) continue;
        for (int j = 0; j < s->hypo_count; ++j) {
            if (strcmp(s->hypos[j].purpose, b)!=0) continue;
            n++;
            if (s->hypos[i].span_end <= s->hypos[j].span_start) fwd++;
            else if (s->hypos[j].span_end <= s->hypos[i].span_start) bwd++;
        }
    }
    if (!n) return 0;
    return (fwd * 100 / n) - (bwd * 100 / n);
}

int ikitaku_cause_audit(const ikitaku_session_t* s,
                        const ikitaku_cause_link_t* link,
                        const ikitaku_cluster_report_t* cr,
                        ikitaku_cause_audit_t* out) {
    if (!s || !link || !out) return -1;
    memset(out, 0, sizeof *out);
    const char* A = link->a[0] ? link->a : "";
    const char* B = link->b[0] ? link->b : "";
    if (!B[0]) {
        snprintf(out->species, sizeof out->species, "cum-hoc");
        snprintf(out->english, sizeof out->english, "audit: no B to test");
        return 0;
    }

    int ia = A[0] ? cause_first(s, A) : -1;
    int ib = cause_first(s, B);
    if (ia >= 0 && ib >= 0) {
        char note[64];
        out->assoc_ab = ikitaku_associate(&s->hypos[ia], &s->hypos[ib], note, sizeof note);
        out->assoc_ba = ikitaku_associate(&s->hypos[ib], &s->hypos[ia], note, sizeof note);
    }
    out->commutator = cause_span_fwd(s, A[0]?A:B, B);
    if (ia >= 0 && ib >= 0 && A[0]) {
        int wrap = 0, nw = 0;
        for (int i = 0; i < s->hypo_count; ++i) {
            if (strcmp(s->hypos[i].purpose, A)!=0) continue;
            for (int j = 0; j < s->hypo_count; ++j) {
                if (strcmp(s->hypos[j].purpose, B)!=0) continue;
                nw++;
                if (s->hypos[i].span_start <= s->hypos[j].span_start &&
                    s->hypos[i].span_end   >= s->hypos[j].span_end)
                    wrap++;
            }
        }
        if (nw && wrap * 2 >= nw) out->commutator += 30;
    }
    /* cluster unity: same promoted purpose */
    if (cr) {
        int ca = -1, cb = -1;
        for (int i = 0; i < cr->cluster_n; ++i) {
            if (A[0] && strcmp(cr->clusters[i].purpose, A)==0) ca = i;
            if (strcmp(cr->clusters[i].purpose, B)==0) cb = i;
        }
        out->cluster_unity = (ca >= 0 && ca == cb) ? 1 : 0;
    }

    /* common cause D: frequent purpose that is neither A nor B nor C */
    int best_d = 0;
    for (int i = 0; i < s->hypo_count; ++i) {
        const char* d = s->hypos[i].purpose;
        if (!d[0] || strcmp(d,B)==0) continue;
        if (A[0] && strcmp(d,A)==0) continue;
        if (link->c[0] && strcmp(d, link->c)==0) continue;
        int f = 0;
        for (int k = 0; k < s->hypo_count; ++k)
            if (strcmp(s->hypos[k].purpose, d)==0) f++;
        if (f > best_d && f >= 4) {
            best_d = f;
            snprintf(out->hidden_d, sizeof out->hidden_d, "%s", d);
        }
    }

    int after_only = out->commutator > 15;
    int same_time = out->commutator > -15 && out->commutator < 15;
    int weak = (out->assoc_ab < 40 && out->assoc_ba < 40);

    if (out->hidden_d[0] && best_d >= 6 && out->cluster_unity == 0 &&
        out->assoc_ab < 50)
        snprintf(out->species, sizeof out->species, "common-cause");
    else if (same_time && !link->tested)
        snprintf(out->species, sizeof out->species, "cum-hoc");
    else if (after_only && (weak || !link->test_ok))
        snprintf(out->species, sizeof out->species, "post-hoc");
    else if (link->test_ok && after_only && out->assoc_ab >= 40)
        snprintf(out->species, sizeof out->species, "directed-chain");
    else if (same_time)
        snprintf(out->species, sizeof out->species, "cum-hoc");
    else
        snprintf(out->species, sizeof out->species, "post-hoc");

    out->stand = strcmp(out->species, "directed-chain")==0 ? 1 : 0;

    snprintf(out->english, sizeof out->english,
             "audit R4=%s stand=%d [A,B]=%d assoc %d/%d unity=%d D=%s",
             out->species, out->stand, out->commutator,
             out->assoc_ab, out->assoc_ba, out->cluster_unity,
             out->hidden_d[0] ? out->hidden_d : "-");
    return out->stand;
}

int ikitaku_atom_sheet(char keys[][24], char roles[][32], int maxn) {
    if (!keys || maxn <= 0) return 0;
    FILE* f = fopen(IKITAKU_ATOM_CAT, "r");
    if (!f) return 0;
    int n = 0;
    char line[192];
    while (fgets(line, sizeof line, f) && n < maxn) {
        if (line[0]=='#' || line[0]=='\n') continue;
        char *k = strtok(line, "|");
        char *r = strtok(NULL, "|");
        if (!k) continue;
        while (*k==' ') k++;
        snprintf(keys[n], 24, "%s", k);
        if (roles && r) {
            char *nl = strchr(r, '\n'); if (nl) *nl=0;
            snprintf(roles[n], 32, "%s", r);
        }
        n++;
    }
    fclose(f);
    return n;
}

static int subject_parse_facts(ikitaku_subject_t* s, const char* blob) {
    s->n = 0;
    if (!blob) return 0;
    const char* p = blob;
    while (*p && s->n < IKITAKU_SUBJ_ATTR) {
        const char* semi = strchr(p, ';');
        const char* end = semi ? semi : p + strlen(p);
        char piece[64];
        int n = (int)(end - p);
        if (n > 63) n = 63;
        memcpy(piece, p, n); piece[n] = 0;
        char* eq = strchr(piece, '=');
        if (eq) {
            *eq = 0;
            snprintf(s->fact[s->n].key, 24, "%s", piece);
            snprintf(s->fact[s->n].val, 96, "%s", eq+1);
            s->n++;
        }
        if (!semi) break;
        p = semi + 1;
    }
    return s->n;
}


static char g_subj_cat_buf[256];
static int g_subj_override = 0;

void ikitaku_subj_set_path(const char* path) {
    if (!path || !path[0]) { g_subj_override = 0; return; }
    snprintf(g_subj_cat_buf, sizeof g_subj_cat_buf, "%s", path);
    g_subj_override = 1;
}
const char* ikitaku_subj_path(void) {
    return g_subj_override ? g_subj_cat_buf : IKITAKU_SUBJ_CAT;
}

int ikitaku_subject_load(ikitaku_subject_map_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    FILE* f = fopen(ikitaku_subj_path(), "r");
    if (!f) return 0;
    char line[512];
    while (fgets(line, sizeof line, f) && out->n < IKITAKU_SUBJ_MAX) {
        if (line[0]=='#' || line[0]=='\n') continue;
        char name[32]={0}, r5[24]={0}, r4[32]={0}, facts[400]={0};
        int lo=50, hi=70;
        /* name|r5|r4|facts|lo|hi — facts may contain no extra pipes */
        char* pipe[8]; int np=0;
        pipe[np++] = line;
        for (char* c=line; *c && np<8; ++c) {
            if (*c=='|') { *c=0; pipe[np++]=c+1; }
        }
        if (np < 3) continue;
        snprintf(name,32,"%s",pipe[0]);
        snprintf(r5,24,"%s",pipe[1]);
        snprintf(r4,32,"%s",pipe[2]);
        if (np >= 6) {
            snprintf(facts,400,"%s",pipe[3]);
            lo=atoi(pipe[np-2]); hi=atoi(pipe[np-1]);
        } else if (np == 5) {
            snprintf(facts,400,"%s",pipe[3]);
            lo=atoi(pipe[4]); hi=lo+20;
        } else if (np == 4) {
            snprintf(facts,400,"%s",pipe[3]);
        }
        char* nl = strchr(facts,'\n'); if (nl) *nl=0;
        ikitaku_subject_t* s = &out->s[out->n++];
        snprintf(s->name,32,"%s",name);
        snprintf(s->r5,24,"%s",r5);
        snprintf(s->r4,32,"%s",r4);
        s->conf_lo=lo; s->conf_hi=hi;
        subject_parse_facts(s, facts);
    }
    fclose(f);
    return out->n;
}

int ikitaku_subject_get(const char* name, ikitaku_subject_t* out) {
    if (!name || !out) return -1;
    ikitaku_subject_map_t m;
    ikitaku_subject_load(&m);
    for (int i=0;i<m.n;i++) {
        if (strcmp(m.s[i].name, name)==0) { *out = m.s[i]; return 0; }
    }
    return -1;
}

int ikitaku_subject_teach(const char* name, const char* r5, const char* r4,
                          const char* attr, const char* val, int conf) {
    if (!name || !attr) return -1;
    ikitaku_subject_map_t m;
    ikitaku_subject_load(&m);
    int slot = -1;
    for (int i = 0; i < m.n; i++)
        if (strcmp(m.s[i].name, name)==0) slot = i;
    if (slot < 0) {
        if (m.n >= IKITAKU_SUBJ_MAX) return -1;
        slot = m.n++;
        memset(&m.s[slot], 0, sizeof m.s[slot]);
        snprintf(m.s[slot].name, 32, "%s", name);
        snprintf(m.s[slot].r5, 24, "%s", r5 && r5[0] ? r5 : "info");
        snprintf(m.s[slot].r4, 32, "%s", r4 && r4[0] ? r4 : "atom");
        m.s[slot].conf_lo = conf > 15 ? conf - 15 : conf;
        m.s[slot].conf_hi = conf < 85 ? conf + 15 : conf;
    }
    ikitaku_subject_t* s = &m.s[slot];
    int found = 0;
    for (int i = 0; i < s->n; i++) {
        if (strcmp(s->fact[i].key, attr)==0) {
            snprintf(s->fact[i].val, 96, "%s", val ? val : "1");
            found = 1;
            break;
        }
    }
    if (!found && s->n < IKITAKU_SUBJ_ATTR) {
        snprintf(s->fact[s->n].key, 24, "%s", attr);
        snprintf(s->fact[s->n].val, 96, "%s", val ? val : "1");
        s->n++;
    }
    FILE* f = fopen(ikitaku_subj_path(), "w");
    if (!f) return -1;
    fprintf(f, "# subject|r5|r4|attr=val;...|conf_lo|conf_hi\n");
    for (int i = 0; i < m.n; i++) {
        fprintf(f, "%s|%s|%s|", m.s[i].name, m.s[i].r5, m.s[i].r4);
        for (int k = 0; k < m.s[i].n; k++)
            fprintf(f, "%s%s=%s", k ? ";" : "", m.s[i].fact[k].key, m.s[i].fact[k].val);
        fprintf(f, "|%d|%d\n", m.s[i].conf_lo, m.s[i].conf_hi);
    }
    fclose(f);
    return 0;
}

int ikitaku_teach_delta(const char* parent, const char* child,
                        const char* attr, const char* val, int conf,
                        char* english, int elen) {
    if (!parent || !child || !attr) return -1;
    ikitaku_subject_map_t m;
    ikitaku_subject_load(&m);
    int ip = -1, ic = -1;
    for (int i = 0; i < m.n; i++) {
        if (strcmp(m.s[i].name, parent)==0) ip = i;
        if (strcmp(m.s[i].name, child)==0) ic = i;
    }
    if (ip < 0) {
        if (english && elen > 0)
            snprintf(english, (size_t)elen,
                     "need-context: parent '%s' missing — encyclopedia hole", parent);
        return -2;
    }
    ikitaku_subject_t* p = &m.s[ip];
    int inherited = p->n;
    if (ic < 0) {
        if (m.n >= IKITAKU_SUBJ_MAX) return -1;
        ic = m.n++;
        m.s[ic] = *p;
        snprintf(m.s[ic].name, 32, "%s", child);
        m.s[ic].conf_lo = conf > 15 ? conf - 15 : conf;
        m.s[ic].conf_hi = conf < 85 ? conf + 15 : conf;
    }
    ikitaku_subject_t* c = &m.s[ic];
    int found = 0, same = 0;
    for (int i = 0; i < c->n; i++) {
        if (strcmp(c->fact[i].key, attr)==0) {
            same = val && strcmp(c->fact[i].val, val)==0;
            snprintf(c->fact[i].val, 32, "%s", val ? val : "1");
            found = 1;
            break;
        }
    }
    if (!found && c->n < IKITAKU_SUBJ_ATTR) {
        snprintf(c->fact[c->n].key, 24, "%s", attr);
        snprintf(c->fact[c->n].val, 32, "%s", val ? val : "1");
        c->n++;
    }
    FILE* f = fopen(ikitaku_subj_path(), "w");
    if (!f) return -1;
    fprintf(f, "# subject|r5|r4|attr=val;...|conf_lo|conf_hi\n");
    for (int i = 0; i < m.n; i++) {
        fprintf(f, "%s|%s|%s|", m.s[i].name, m.s[i].r5, m.s[i].r4);
        for (int k = 0; k < m.s[i].n; k++)
            fprintf(f, "%s%s=%s", k ? ";" : "", m.s[i].fact[k].key, m.s[i].fact[k].val);
        fprintf(f, "|%d|%d\n", m.s[i].conf_lo, m.s[i].conf_hi);
    }
    fclose(f);
    if (english && elen > 0) {
        if (same)
            snprintf(english, (size_t)elen,
                     "delta 0: '%s' already %s=%s (inherited %d from '%s')",
                     child, attr, val ? val : "1", inherited, parent);
        else
            snprintf(english, (size_t)elen,
                     "delta 1 attr %s=%s on '%s' (inherited %d from '%s')",
                     attr, val ? val : "1", child, inherited, parent);
    }
    return same ? 0 : 1;
}

int ikitaku_alias_resolve(const char* word, ikitaku_alias_t* out) {
    if (!word || !out) return -1;
    memset(out, 0, sizeof *out);
    FILE* f = fopen(IKITAKU_ALIAS_CAT, "r");
    if (!f) return -1;
    char line[192];
    while (fgets(line, sizeof line, f)) {
        if (line[0]=='#' || line[0]=='\n') continue;
        char w[32]={0}, sub[32]={0}, r5[24]={0}, r4[32]={0};
        int conf=50;
        if (sscanf(line, "%31[^|]|%31[^|]|%23[^|]|%31[^|]|%d",
                   w, sub, r5, r4, &conf) < 4) continue;
        if (strcmp(w, word)==0) {
            snprintf(out->word,32,"%s",w);
            snprintf(out->subject,32,"%s",sub);
            snprintf(out->r5,24,"%s",r5);
            snprintf(out->r4,32,"%s",r4);
            out->conf = conf;
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return -1;
}

int ikitaku_alias_bind(const char* word, const char* subject, int conf) {
    if (!word || !subject) return -1;
    ikitaku_subject_t s;
    if (ikitaku_subject_get(subject, &s) != 0) return -2;
    ikitaku_alias_t exist;
    if (ikitaku_alias_resolve(word, &exist)==0) return 1;
    FILE* f = fopen(IKITAKU_ALIAS_CAT, "a");
    if (!f) return -1;
    fprintf(f, "%s|%s|%s|%s|%d\n", word, subject, s.r5, s.r4, conf);
    fclose(f);
    return 0;
}
