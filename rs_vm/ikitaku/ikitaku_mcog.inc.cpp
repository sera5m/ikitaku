
static void mcog_add(ikitaku_mcog_t* m, uint8_t jtype, uint8_t mk,
                     const char* event, const char* claim, int prior) {
    if (!m || m->n >= IKITAKU_MCOG_MAX) return;
    ikitaku_theory_t* t = &m->th[m->n++];
    memset(t, 0, sizeof *t);
    t->jtype = jtype;
    t->mk = mk;
    snprintf(t->event, sizeof t->event, "%s", event ? event : "");
    snprintf(t->claim, sizeof t->claim, "%s", claim ? claim : "");
    t->prior = prior;
}

static void mcog_score(ikitaku_theory_t* t, int held, const char* outcome) {
    if (!t) return;
    t->scored = 1;
    t->held = held ? 1 : 0;
    snprintf(t->outcome, sizeof t->outcome, "%s", outcome ? outcome : "");
}

int ikitaku_lgg_purposes(const ikitaku_session_t* s, char* out, int olen) {
    if (!out || olen < 8) return -1;
    if (!s || s->hypo_count < 1) {
        snprintf(out, (size_t)olen, "F = ?");
        return 0;
    }
    /* LGG sketch: intersection of purpose tokens */
    int has_dec = 0, has_rep = 0, has_wait = 0, has_map = 0;
    for (int i = 0; i < s->hypo_count; ++i) {
        const char* p = s->hypos[i].purpose;
        if (strcmp(p, "decision") == 0) has_dec = 1;
        else if (strcmp(p, "repeat") == 0) has_rep = 1;
        else if (strcmp(p, "wait") == 0) has_wait = 1;
        else if (strcmp(p, "map") == 0) has_map = 1;
    }
    if (has_rep && has_dec && has_wait)
        snprintf(out, (size_t)olen, "LGG F = repeat(decision) + wait");
    else if (has_rep && has_dec)
        snprintf(out, (size_t)olen, "LGG F = repeat(decision)");
    else if (has_rep && has_wait)
        snprintf(out, (size_t)olen, "LGG F = repeat + wait");
    else if (has_map && has_dec)
        snprintf(out, (size_t)olen, "LGG F = map + decision");
    else if (has_rep)
        snprintf(out, (size_t)olen, "LGG F = repeat");
    else if (has_dec)
        snprintf(out, (size_t)olen, "LGG F = decision");
    else if (has_map)
        snprintf(out, (size_t)olen, "LGG F = map");
    else
        snprintf(out, (size_t)olen, "LGG F = unknown");
    return 0;
}

int ikitaku_mcog_run(const ikitaku_session_t* s,
                     const ikitaku_cluster_report_t* cr,
                     const ikitaku_fit_report_t* fits,
                     int desperation_level,
                     ikitaku_mcog_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->calibration = -1;

    char lgg[80];
    ikitaku_lgg_purposes(s, lgg, sizeof lgg);
    snprintf(out->decl, sizeof out->decl, "%s", lgg);
    snprintf(out->proc, sizeof out->proc, "analyze → cluster → kant-tag → try-pattern");
    ikitaku_try_pat_t tp;
    memset(&tp, 0, sizeof tp);
    if (cr && cr->cluster_n > 0)
        ikitaku_kant_try_pattern(cr, &tp);
    if (tp.try_pattern[0]) {
        snprintf(out->try_pattern, sizeof out->try_pattern, "%s", tp.try_pattern);
        snprintf(out->kant_class, sizeof out->kant_class, "%s", tp.kant_class);
        snprintf(out->cond, sizeof out->cond, "try=%s (%s/%s mem=%d ant=%d)",
                 tp.try_pattern, tp.kant_class, tp.kant_cat,
                 tp.membership, tp.antinomy);
    } else {
        snprintf(out->cond, sizeof out->cond,
                 "Occam +%d; wait blocks rewrite; nonsense if dep>=4 and weak fit",
                 ikitaku_occam_margin());
    }

    int hy = s ? s->hypo_count : 0;
    int cl = cr ? cr->cluster_n : 0;
    /* EOL theory: event = "this program will yield a stable general form" */
    mcog_add(out, IKITAKU_J_EOL, IKITAKU_MK_DECL,
             "session will find a stable general form",
             hy >= 4 ? "theorize: form exists (enough hypos)" :
                       "theorize: form may be sparse",
             hy >= 4 ? 70 : 40);

    const char* top = (s && s->accepted >= 0) ? s->hypos[s->accepted].purpose : "none";
    int tconf = (s && s->accepted >= 0) ? s->hypos[s->accepted].confidence : 0;
    /* JOL: theorize the accepted form will remain the reading */
    char ev[80], clm[96];
    snprintf(ev, sizeof ev, "accepted purpose '%s' remains the reading", top);
    snprintf(clm, sizeof clm, "theorize: %s holds at conf %d", top, tconf);
    mcog_add(out, IKITAKU_J_JOL, IKITAKU_MK_PROC, ev, clm, tconf);

    /* FOK: can't name sort-species etc. */
    int named = (s && s->accepted >= 0 && tconf >= 70);
    mcog_add(out, IKITAKU_J_FOK, IKITAKU_MK_DECL,
             "a more specific species (sort/quadratic-in-code) is recognizable",
             named ? "theorize: I would recognize a tighter species if shown"
                   : "theorize: I do not even have a handle yet",
             named ? 55 : 25);

    /* Retro: did clusters + accept agree with LGG? */
    int lgg_ok = (s && s->accepted >= 0);
    mcog_add(out, IKITAKU_J_RETRO, IKITAKU_MK_COND,
             "accept matches LGG skeleton",
             lgg_ok ? "theorize: accept is an instance of the LGG"
                    : "theorize: no accept — theory of form is open",
             lgg_ok ? 65 : 30);
    if (tp.try_pattern[0])
        mcog_add(out, IKITAKU_J_JOL, IKITAKU_MK_COND,
                 "kant-fuzzy cluster implies a try-pattern",
                 tp.try_pattern, tp.conf);

    /* Score theories against what actually happened this session (events). */
    mcog_score(&out->th[0], hy >= 3 && cl >= 1,
               hy >= 3 ? "event: hypos+cluster appeared" : "event: sparse");
    mcog_score(&out->th[1], s && s->accepted >= 0 && tconf >= 70,
               s && s->accepted >= 0 ? "event: accept stood" : "event: no accept");
    int fit_best = (fits && fits->n && fits->best >= 0) ? fits->fits[fits->best].conf : -1;
    mcog_score(&out->th[2], fit_best >= 80 || named,
               fit_best >= 80 ? "event: a tight numeric form was recognized"
                              : "event: species still unnamed");
    mcog_score(&out->th[3], lgg_ok,
               lgg_ok ? "event: accept exists under LGG" : "event: open");
    if (out->n > 4)
        mcog_score(&out->th[4], tp.membership >= 55,
                   tp.membership >= 55 ? "event: cluster category stood"
                                       : "event: category weak");

    out->n_scored = 0; out->n_held = 0;
    for (int i = 0; i < out->n; ++i) {
        if (out->th[i].scored) {
            out->n_scored++;
            if (out->th[i].held) out->n_held++;
        }
    }
    if (out->n_scored)
        out->calibration = (out->n_held * 100) / out->n_scored;

    snprintf(out->monitor, sizeof out->monitor,
             "monitor: %d/%d theories held (cal %d). dep=%d",
             out->n_held, out->n_scored, out->calibration, desperation_level);
    if (desperation_level >= 4)
        snprintf(out->control, sizeof out->control,
                 "control: loosen Occam, allow nonsense call, do not promote");
    else if (s && s->accepted >= 0 && tconf >= 85)
        snprintf(out->control, sizeof out->control,
                 "control: keep Occam, stand on accept, cache LGG");
    else
        snprintf(out->control, sizeof out->control,
                 "control: keep probing; do not treat correlation as identity");
    if (tp.try_pattern[0] && (int)strlen(out->control) + 24 < (int)sizeof out->control)
        snprintf(out->control + strlen(out->control),
                 sizeof(out->control) - strlen(out->control),
                 " lead=%s", tp.try_pattern);
    if (cr && cr->cluster_n >= 2) {
        ikitaku_gap_hint_t gh;
        ikitaku_cluster_gap_hint(cr, &gh);
        snprintf(out->gap_hint, sizeof out->gap_hint, "%s", gh.hint);
        if ((int)strlen(out->control) + 28 < (int)sizeof out->control)
            snprintf(out->control + strlen(out->control),
                     sizeof(out->control) - strlen(out->control),
                     " gap=%s", gh.hint);
        if (gh.nonsense)
            snprintf(out->control, sizeof out->control,
                     "control: useless TAS gap — peel parent, simpler analogy");
        else if (gh.novel)
            snprintf(out->control, sizeof out->control,
                     "control: novel-species (Gentner held) — don't collapse");
    }

    snprintf(out->english, sizeof out->english,
             "M2 metacog. Decl: %s. %s %s Theories are about events, not guesses. cal=%d.",
             out->decl, out->monitor, out->control, out->calibration);
    return out->n;
}

int ikitaku_time_transform(const ikitaku_session_t* s, ikitaku_time_xform_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (s) {
        for (int i = 0; i < s->hypo_count; ++i) {
            if (strcmp(s->hypos[i].purpose, "wait") == 0) out->has_wait = 1;
            if (strstr(s->hypos[i].form_id, "wait")
                || strstr(s->hypos[i].form_id, "delay"))
                out->has_progress = 1;
            if (!s->hypos[i].rewrite_ok) out->impure = 1;
        }
    }
    if (out->has_wait || out->has_progress) {
        snprintf(out->forward, sizeof out->forward,
                 "T: present + Δt lifts every time-affected var (counters, waits, IO). "
                 "A wait loop is T applied n times.");
        snprintf(out->inverse, sizeof out->inverse,
                 "T^{-1}: counterfactual — what if X instead of Y in the past "
                 "(peel Δt, bind a different prior).");
        snprintf(out->english, sizeof out->english,
                 "Time is a transform, not a decoration. Forward T on wait/progress; "
                 "inverse is alternate history. impure=%d.", out->impure);
    } else {
        snprintf(out->forward, sizeof out->forward,
                 "No wait/progress seen — T is identity on this span (pure math snapshot).");
        snprintf(out->inverse, sizeof out->inverse,
                 "T^{-1} = identity; no past branch to reopen.");
        snprintf(out->english, sizeof out->english,
                 "This reading has no time axis. Algebra can drop T.");
    }
    return 0;
}

int ikitaku_taxon(const ikitaku_session_t* s, ikitaku_taxon_report_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!s) return 0;
    for (int i = 0; i < s->hypo_count && out->n < IKITAKU_TAXON_MAX; ++i) {
        const char* g = s->hypos[i].purpose[0] ? s->hypos[i].purpose : "unknown";
        int found = -1;
        for (int k = 0; k < out->n; ++k)
            if (strcmp(out->item[k].genus, g) == 0) { found = k; break; }
        if (found < 0) {
            found = out->n++;
            snprintf(out->item[found].genus, sizeof out->item[found].genus, "%s", g);
            snprintf(out->item[found].species, sizeof out->item[found].species,
                     "%s", s->hypos[i].form_id);
            out->item[found].conf = s->hypos[i].confidence;
            snprintf(out->item[found].note, sizeof out->item[found].note,
                     "genus R5=%s  species=%s", g, s->hypos[i].form_id);
        } else if (s->hypos[i].confidence > out->item[found].conf) {
            snprintf(out->item[found].species, sizeof out->item[found].species,
                     "%s", s->hypos[i].form_id);
            out->item[found].conf = s->hypos[i].confidence;
        }
    }
    snprintf(out->english, sizeof out->english,
             "Taxon: %d genera. Species is the named form under the platonic genus.",
             out->n);
    return out->n;
}

int ikitaku_ontology(const ikitaku_session_t* s, ikitaku_onto_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!s) return 0;
    for (int i = 0; i < s->hypo_count && out->n < IKITAKU_ONTO_MAX; ++i) {
        const ikitaku_hypo_t* h = &s->hypos[i];
        ikitaku_onto_card_t* c = &out->card[out->n++];
        snprintf(c->r5, sizeof c->r5, "%s", h->purpose[0] ? h->purpose : "unknown");
        snprintf(c->r3, sizeof c->r3, "%s", h->form_id[0] ? h->form_id : "?");
        c->conf = h->confidence;
        snprintf(c->r1, sizeof c->r1, "span %u-%u",
                 (unsigned)h->span_start, (unsigned)h->span_end);
        snprintf(c->r0, sizeof c->r0, "IR ops in span");
        const char* axis = "none";
        const char* fam = "advance-axis";
        const char* micro = "body-step";
        if (strcmp(h->purpose, "wait") == 0
            || strstr(h->form_id, "wait") || strstr(h->form_id, "delay")
            || strstr(h->form_id, "timed")) {
            axis = "time"; fam = "time-axis"; micro = "delay-then-resume";
        } else if (strstr(h->form_id, "counted") || strstr(h->form_id, "inc")) {
            axis = "iterator"; fam = "iterator-axis"; micro = "add-const-to-slot";
        } else if (strcmp(h->purpose, "decision") == 0
                   || strstr(h->form_id, "if") || strstr(h->form_id, "cmp")) {
            axis = "compare"; fam = "compare-branch"; micro = "cmp-then-select";
        } else if (strcmp(h->purpose, "map") == 0) {
            axis = "value"; fam = "pointwise-map"; micro = "apply-fn-to-slot";
        } else if (strcmp(h->purpose, "reduce") == 0) {
            axis = "iterator"; fam = "affine-reduce"; micro = "fold-inc";
        } else if (strcmp(h->purpose, "repeat") == 0) {
            axis = "iterator"; fam = "advance-axis"; micro = "loop-body";
        }
        snprintf(c->axis, sizeof c->axis, "%s", axis);
        snprintf(c->r4, sizeof c->r4, "%s", fam);
        snprintf(c->r2, sizeof c->r2, "%s", micro);
    }
    snprintf(out->english, sizeof out->english,
             "Ontology %d cards. R5→R4 axis-family→R3 species→R2 micro→R1 span→R0 IR.",
             out->n);
    return out->n;
}

int ikitaku_extract_card(const ikitaku_onto_card_t* c, ikitaku_extract_t* out) {
    if (!c || !out) return -1;
    memset(out, 0, sizeof *out);
    int r3_says_r5 = c->r3[0] && c->r5[0] && strstr(c->r3, c->r5) != NULL;
    int thin = (strcmp(c->r3, "trap_loop") == 0
             || strcmp(c->r3, "trap_loop_impure") == 0
             || strcmp(c->r3, "do_n_impure") == 0
             || strcmp(c->r3, "repeat_impure") == 0);
    out->tautology = (r3_says_r5 && thin) || (strcmp(c->r5, "repeat") == 0 && thin);
    /* Mimicry test: looks specialized but axis/micro don't support the purpose */
    int fancy = strstr(c->r3, "_x_") || strstr(c->r3, "timed") || strstr(c->r3, "counted");
    out->mimicry = fancy && (strcmp(c->axis, "none") == 0);
    if (!out->mimicry && strcmp(c->r5, "wait") == 0 && strcmp(c->axis, "time") != 0)
        out->mimicry = 1;
    if (!out->mimicry && strcmp(c->r5, "reduce") == 0 && strcmp(c->axis, "iterator") != 0)
        out->mimicry = 1;
    if (!out->mimicry &&
        (strcmp(c->r5, "sort") == 0 || strstr(c->r3, "sort")
         || strstr(c->r3, "bubble") || strstr(c->r3, "merge"))) {
        if (strcmp(c->axis, "compare") != 0 || !strstr(c->r2, "swap"))
            out->mimicry = 1;
    }
    out->instance_ok = !out->tautology && !out->mimicry && c->r5[0] && c->axis[0]
                    && strcmp(c->axis, "none") != 0;
    /* Regression: strip detail until a stable pattern */
    if (out->tautology)
        snprintf(out->stripped, sizeof out->stripped, "repeat(axis=?)");
    else if (out->instance_ok)
        snprintf(out->stripped, sizeof out->stripped, "%s(axis=%s)", c->r5, c->axis);
    else
        snprintf(out->stripped, sizeof out->stripped, "%s", c->r5[0] ? c->r5 : "unknown");
    snprintf(out->note, sizeof out->note,
             "%s taut=%d mimic=%d inst=%d → %s",
             c->r3, out->tautology, out->mimicry, out->instance_ok, out->stripped);
    return 0;
}

int ikitaku_extract_session(const ikitaku_session_t* s, char* english, int elen) {
    if (!english || elen < 8) return -1;
    ikitaku_onto_t O;
    ikitaku_ontology(s, &O);
    int taut = 0, mim = 0, inst = 0;
    char best[48] = "unknown";
    for (int i = 0; i < O.n; ++i) {
        ikitaku_extract_t e;
        ikitaku_extract_card(&O.card[i], &e);
        if (e.tautology) taut++;
        if (e.mimicry) mim++;
        if (e.instance_ok) {
            inst++;
            snprintf(best, sizeof best, "%s", e.stripped);
        }
    }
    snprintf(english, (size_t)elen,
             "extract: tautology=%d mimicry=%d (test-only flag) instances=%d  "
             "stripped pattern: %s  [mimicry is not a reasoning method]",
             taut, mim, inst, best);
    return inst;
}

int ikitaku_hunt_init(ikitaku_hunt_t* h, const char* goal, const char* avoid) {
    if (!h) return -1;
    memset(h, 0, sizeof *h);
    snprintf(h->goal, sizeof h->goal, "%s", goal ? goal : "closer-to-want");
    snprintf(h->avoid, sizeof h->avoid, "%s", avoid ? avoid : "that-pole");
    h->pos_score = -1;
    h->neg_score = -1;
    h->next_probe = 50;
    snprintf(h->english, sizeof h->english,
             "hunt goal='%s' avoid='%s' waiting both poles",
             h->goal, h->avoid);
    return 0;
}

int ikitaku_hunt_arrive(ikitaku_hunt_t* h, const char* side, int score) {
    if (!h || !side) return -1;
    if (score < 0) score = 0;
    if (score > 100) score = 100;
    if (h->n >= IKITAKU_HUNT_MAX) return -1;
    ikitaku_hunt_slot_t* s = &h->slot[h->n++];
    snprintf(s->side, sizeof s->side, "%s", side);
    s->score = score;
    s->ready = 1;
    s->from50 = score > 50 ? score - 50 : 50 - score;
    if (strcmp(side, "pos") == 0 || strcmp(side, "goal") == 0)
        h->pos_score = score;
    if (strcmp(side, "neg") == 0 || strcmp(side, "avoid") == 0)
        h->neg_score = score;
    return ikitaku_hunt_tick(h);
}

int ikitaku_hunt_tick(ikitaku_hunt_t* h) {
    if (!h) return -1;
    if (h->pos_score < 0 && h->neg_score < 0) {
        snprintf(h->english, sizeof h->english, "hunt: no poles arrived");
        return 0;
    }
    if (h->pos_score >= 0 && h->neg_score < 0) {
        snprintf(h->english, sizeof h->english,
                 "pos %d arrived. mutex wait for negative.", h->pos_score);
        return 0;
    }
    if (h->neg_score >= 0 && h->pos_score < 0) {
        snprintf(h->english, sizeof h->english,
                 "neg %d arrived. mutex wait for positive.", h->neg_score);
        return 0;
    }
    int p = h->pos_score, n = h->neg_score;
    int p50 = p > 50 ? p - 50 : 50 - p;
    int n50 = n > 50 ? n - 50 : 50 - n;
    /* bias: signed pull from 50. more extreme miss gets inverted. */
    h->bias = (p - 50) - (n - 50);
    if (n50 > p50 && n <= 50) {
        h->invert_side = 1;
        h->next_probe = 100 - n; /* inversion of the negative */
    } else if (p50 > n50 && p <= 50) {
        h->invert_side = -1;
        h->next_probe = 100 - p;
    } else {
        h->invert_side = 0;
        h->next_probe = (p + (100 - n)) / 2; /* binary sit toward goal */
        if (h->next_probe < 0) h->next_probe = 0;
        if (h->next_probe > 100) h->next_probe = 100;
    }
    h->solved = (p >= 70 && n <= 30) ? 1 : 0;
    snprintf(h->english, sizeof h->english,
             "pos=%d neg=%d |Δ50| p=%d n=%d bias=%+d %s next=%d %s",
             p, n, p50, n50, h->bias,
             h->invert_side > 0 ? "invert-neg" :
             h->invert_side < 0 ? "invert-pos" : "mid-sit",
             h->next_probe,
             h->solved ? "HOLD" : "keep-hunting");
    return h->solved;
}

int ikitaku_critique(const char* claim, int has_run_evidence, int definitional,
                     int both_poles_ok, int opaque,
                     ikitaku_critique_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->judgment = definitional ? IKITAKU_K_ANALYTIC : IKITAKU_K_SYNTHETIC;
    out->source = has_run_evidence ? IKITAKU_K_POSTERIORI : IKITAKU_K_PRIORI;
    out->antinomy = both_poles_ok ? 1 : 0;
    out->thing_in_self = opaque ? 1 : 0;
    /* synthetic a priori = new universal without this run's senses */
    out->axiom = (out->judgment == IKITAKU_K_SYNTHETIC &&
                  out->source == IKITAKU_K_PRIORI) ? 1 : 0;
    out->presupposition_open = (out->judgment == IKITAKU_K_SYNTHETIC) ? 1 : 0;
    out->hidden_d = opaque ? 1 : 0;
    out->conf = definitional ? 90 : (has_run_evidence ? 70 : 55);
    snprintf(out->action, sizeof out->action, "%s",
             opaque ? "extract" : "keep");
    if (out->axiom)
        snprintf(out->axiom_name, sizeof out->axiom_name, "synthetic-a-priori");
    else if (out->judgment == IKITAKU_K_ANALYTIC)
        snprintf(out->axiom_name, sizeof out->axiom_name, "analytic-definition");
    else
        snprintf(out->axiom_name, sizeof out->axiom_name, "empirical-reading");
    const char* j = out->judgment == IKITAKU_K_ANALYTIC ? "analytic" : "synthetic";
    const char* s = out->source == IKITAKU_K_PRIORI ? "a-priori" : "a-posteriori";
    snprintf(out->english, sizeof out->english,
             "critique '%s' → %s %s axiom=%s open=%d hidden_d=%d act=%s",
             claim ? claim : "?", j, s, out->axiom_name,
             out->presupposition_open, out->hidden_d, out->action);
    return out->axiom;
}

int ikitaku_critique_challenge(const ikitaku_critique_t* prior,
                               const char* claim,
                               int new_evidence_conf,
                               int residual,
                               int weakly_dependent,
                               ikitaku_critique_t* out) {
    if (!out) return -1;
    if (prior) *out = *prior;
    else memset(out, 0, sizeof *out);
    out->source = IKITAKU_K_POSTERIORI; /* challenge is from appearance */
    if (weakly_dependent || residual >= 20)
        out->hidden_d = 1;
    if (out->thing_in_self)
        out->hidden_d = 1;
    /* Analytic definitions stay closed unless residual is huge. */
    if (out->judgment == IKITAKU_K_ANALYTIC && residual < 40) {
        snprintf(out->action, sizeof out->action, "keep");
        out->presupposition_open = 0;
        out->conf = 88;
    } else if (residual >= 25 || (new_evidence_conf >= 60 && out->axiom)) {
        /* New data vs old axiom: drop axiom status, open the presupposition. */
        out->axiom = 0;
        out->presupposition_open = 1;
        out->conf = new_evidence_conf > 0 ? (100 - residual) : out->conf / 2;
        if (out->conf < 15) out->conf = 15;
        if (out->thing_in_self || residual >= 40)
            snprintf(out->action, sizeof out->action, "extract");
        else
            snprintf(out->action, sizeof out->action, "ask");
        snprintf(out->axiom_name, sizeof out->axiom_name, "challenged-priori");
    } else {
        snprintf(out->action, sizeof out->action, "keep");
        out->conf = (out->conf + new_evidence_conf) / 2;
    }
    snprintf(out->english, sizeof out->english,
             "challenge '%s' hidden_d=%d open=%d residual=%d ev=%d act=%s conf=%d %s",
             claim ? claim : "?", out->hidden_d, out->presupposition_open,
             residual, new_evidence_conf, out->action, out->conf,
             out->axiom_name);
    if (strcmp(out->action, "ask") == 0) {
        ikitaku_context_ask_t ask;
        ikitaku_need_context(claim ? claim : "challenged-claim", NULL, &ask);
    }
    return out->presupposition_open;
}

int ikitaku_place_report(int used_r5, int used_r4, int used_hunt,
                         int used_critique, int xp_known, int residual,
                         ikitaku_place_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->self_is_evaluator = 1;
    out->world_is_superset = 1;
    out->depth = 0;
    char* p = out->layers_used;
    int cap = (int)sizeof out->layers_used;
    int n = 0;
    if (used_r5) { out->depth++; n += snprintf(p+n, cap-n, "R5 "); }
    if (used_r4) { out->depth++; n += snprintf(p+n, cap-n, "R4 "); }
    if (used_hunt) { out->depth++; n += snprintf(p+n, cap-n, "hunt "); }
    if (used_critique) { out->depth++; n += snprintf(p+n, cap-n, "kant "); }
    if (out->depth < 1) out->depth = 1;
    if (out->depth > 5) out->depth = 5;
    out->incomplete_priori = (xp_known < 2) ? 1 : 0;
    out->hidden_d = (residual >= 20) ? 1 : 0;
    if (residual >= 25) out->incomplete_priori = 1;
    snprintf(out->english, sizeof out->english,
             "place: I am structured data evaluating a given bag (depth=%d [%s]). "
             "reality is a larger unused superset. incomplete_priori=%d hidden_d=%d xp=%d",
             out->depth, out->layers_used[0] ? out->layers_used : "R0",
             out->incomplete_priori, out->hidden_d, xp_known);
    return out->depth;
}

int ikitaku_mechanism_priori(ikitaku_mechanism_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->all_data_structured = 1;
    out->self_is_mechanism = 1;
    out->code_is_mechanism = 1;
    out->acts_on_itself = 1;
    snprintf(out->r5_cycle, sizeof out->r5_cycle, "info+order+repeat");
    snprintf(out->english, sizeof out->english,
             "mechanism-priori: every bag is structured data. "
             "this evaluator and the code it sees are mechanisms of data "
             "that act on data (cycle %s). it is a self, not a person.",
             out->r5_cycle);
    return 1;
}

static int strat_recompute_score(ikitaku_strategy_t* s) {
    if (!s) return 0;
    if (!s->novel || s->locked) {
        s->score = s->conf;
        return s->score;
    }
    /* Novel score: base 40 + win-rate*30 + avg_drop*0.4, trials damp */
    int score = 40;
    if (s->trials > 0) {
        int win_pct = (100 * s->wins) / s->trials;
        int avg_drop = s->drop_sum / s->trials;
        if (avg_drop > 50) avg_drop = 50;
        score = 35 + (win_pct * 35) / 100 + (avg_drop * 40) / 50;
        /* few trials → confidence penalty */
        if (s->trials < 3) score = (score * (50 + s->trials * 15)) / 100;
    } else {
        score = 42; /* untested novel */
    }
    if (score < 5) score = 5;
    if (score > 95) score = 95;
    s->score = score;
    s->conf = score; /* novel conf tracks score */
    return score;
}

int ikitaku_strategy_load(ikitaku_strategy_map_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    FILE* f = fopen(IKITAKU_STRAT_CAT, "r");
    if (!f) return 0;
    char line[240];
    while (fgets(line, sizeof line, f) && out->n < IKITAKU_STRAT_MAX) {
        if (line[0]=='#' || line[0]=='\n') continue;
        ikitaku_strategy_t* s = &out->st[out->n];
        memset(s, 0, sizeof *s);
        int macro=0, locked=0, conf=50, novel=0, trials=0, wins=0, drop_sum=0;
        int nfield = sscanf(line, "%23[^|]|%79[^|]|%d|%d|%d|%d|%d|%d|%d",
                            s->name, s->form, &macro, &locked, &conf,
                            &novel, &trials, &wins, &drop_sum);
        if (nfield < 2) continue;
        s->macro = (uint8_t)macro;
        s->locked = (uint8_t)locked;
        s->conf = conf;
        s->novel = (uint8_t)(nfield >= 6 ? novel : (locked ? 0 : 1));
        /* basial known-good: locked=1 → never novel */
        if (s->locked) s->novel = 0;
        s->trials = trials;
        s->wins = wins;
        s->drop_sum = drop_sum;
        strat_recompute_score(s);
        out->n++;
    }
    fclose(f);
    return out->n;
}

int ikitaku_strategy_save(const ikitaku_strategy_map_t* m) {
    if (!m) return -1;
    FILE* f = fopen(IKITAKU_STRAT_CAT, "w");
    if (!f) return -1;
    fprintf(f, "# name|form|macro|locked|conf|novel|trials|wins|drop_sum\n");
    for (int i = 0; i < m->n; ++i) {
        const ikitaku_strategy_t* s = &m->st[i];
        fprintf(f, "%s|%s|%d|%d|%d|%d|%d|%d|%d\n",
                s->name, s->form[0] ? s->form : "-",
                (int)s->macro, (int)s->locked, s->conf,
                (int)s->novel, s->trials, s->wins, s->drop_sum);
    }
    fclose(f);
    return m->n;
}

int ikitaku_strategy_propose(const char* name, const char* form, int macro) {
    if (!name || !name[0]) return -1;
    ikitaku_strategy_map_t m;
    ikitaku_strategy_load(&m);
    for (int i = 0; i < m.n; i++)
        if (strcmp(m.st[i].name, name) == 0) return 1; /* exists */
    if (m.n >= IKITAKU_STRAT_MAX) return -2;
    ikitaku_strategy_t* s = &m.st[m.n++];
    memset(s, 0, sizeof *s);
    snprintf(s->name, sizeof s->name, "%s", name);
    snprintf(s->form, sizeof s->form, "%s",
             (form && form[0]) ? form : "novel R6 macro");
    s->macro = macro ? 1 : 0;
    s->locked = 0;
    s->novel = 1;          /* developed — subject to scoring */
    s->conf = 45;
    s->trials = 0;
    s->wins = 0;
    s->drop_sum = 0;
    strat_recompute_score(s);
    ikitaku_strategy_save(&m);
    ikitaku_r5_propose(name, form ? form : "novel R6", "proposed metacog novel");
    return 0;
}

int ikitaku_strategy_score(const char* name, int residual_in, int residual_out,
                           ikitaku_strategy_t* out_opt) {
    if (!name || !name[0]) return -1;
    ikitaku_strategy_map_t m;
    if (ikitaku_strategy_load(&m) < 0) return -1;
    int idx = -1;
    for (int i = 0; i < m.n; ++i)
        if (!strcmp(m.st[i].name, name)) { idx = i; break; }
    if (idx < 0) return -1;
    ikitaku_strategy_t* s = &m.st[idx];
    /* Basial known-good: do not train */
    if (s->locked || !s->novel) {
        if (out_opt) *out_opt = *s;
        return s->conf; /* fixed */
    }
    if (residual_in < 0) residual_in = 0;
    if (residual_out < 0) residual_out = 0;
    int drop = residual_in - residual_out;
    if (drop < 0) drop = 0;
    s->trials++;
    if (drop > 0) s->wins++;
    s->drop_sum += drop;
    int sc = strat_recompute_score(s);
    ikitaku_strategy_save(&m);
    if (out_opt) *out_opt = *s;
    return sc;
}

int ikitaku_strategy_rank_novel(ikitaku_strategy_t* out, int max_out) {
    if (!out || max_out <= 0) return 0;
    ikitaku_strategy_map_t m;
    ikitaku_strategy_load(&m);
    int n = 0;
    for (int i = 0; i < m.n && n < max_out; ++i) {
        if (!m.st[i].novel || m.st[i].locked) continue;
        out[n++] = m.st[i];
    }
    /* sort by score desc */
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (out[j].score > out[i].score) {
                ikitaku_strategy_t tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
    return n;
}

int ikitaku_strategy_pick_novel(int min_score, ikitaku_strategy_t* out) {
    if (!out) return -1;
    ikitaku_strategy_t rank[IKITAKU_STRAT_MAX];
    int n = ikitaku_strategy_rank_novel(rank, IKITAKU_STRAT_MAX);
    if (n <= 0) return -1;
    if (rank[0].score < min_score) return -1;
    *out = rank[0];
    return rank[0].score;
}

int ikitaku_strategy_pick(const char* objective, int revisit_ok,
                          ikitaku_strategy_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    ikitaku_strategy_map_t m;
    ikitaku_strategy_load(&m);
    const char* want = revisit_ok ? "iterative" : "waterfall";
    for (int i=0;i<m.n;i++) {
        if (strcmp(m.st[i].name, want)==0) {
            *out = m.st[i];
            return 0;
        }
    }
    snprintf(out->name, sizeof out->name, "%s", want);
    snprintf(out->form, sizeof out->form, "%s",
             revisit_ok ? "try-score-revise loop" : "lock stage then next");
    out->macro = 1;
    out->conf = 40;
    (void)objective;
    return 1;
}

int ikitaku_think_about_thinking(const ikitaku_l6_t* l6,
                                 const ikitaku_strategy_map_t* st,
                                 ikitaku_thinking_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->thinking_is_concept = 1;
    out->thinking_is_self = 1;
    ikitaku_strategy_map_t local;
    if (!st) {
        ikitaku_strategy_load(&local);
        st = &local;
    }
    for (int i=0;i<st->n && out->n<8;i++)
        snprintf(out->methods[out->n++], 24, "%s", st->st[i].name);
    if (l6 && l6->chosen >= 0 && out->n < 8) {
        const char* mn[] = {"plugin","associate","dialectic","synth","series","novel"};
        int c = l6->chosen;
        if (c >= 0 && c < 6) {
            snprintf(out->methods[out->n], 24, "l6:%s", mn[c]);
            out->n++;
        }
    }
    ikitaku_subject_teach("thinking", "theorize", "method",
                          "kind", "analyze-methods-as-data", 70);
    /* Methods are bags: same pipeline as data (no reasoning exemption). */
    if (out->n >= 2) {
        ikitaku_hypo_t ma, mb;
        ikitaku_hypo_fill(&ma, out->methods[0], out->methods[0], 70, 1);
        ikitaku_hypo_fill(&mb, out->methods[1], out->methods[1], 65, 0);
        ikitaku_reason_pipe_t pipe;
        memset(&pipe, 0, sizeof pipe);
        ikitaku_reason_pipeline(&ma, &mb, IKITAKU_BAG_METHOD, 2, &pipe);
        ikitaku_reason_shift_t sh;
        memset(&sh, 0, sizeof sh);
        ikitaku_reason_shift_hypo(&pipe.product, IKITAKU_BAG_METHOD, &sh);
        snprintf(out->english, sizeof out->english,
                 "thinking=%d methods as data. pipe exempt=%d shift DOWN R%d UP R%d %s",
                 out->n, pipe.exempted, sh.down.to_layer, sh.up.to_layer,
                 pipe.english);
    } else {
        snprintf(out->english, sizeof out->english,
                 "thinking is this process as a concept (%d methods as attrs). "
                 "self-on-ladder: a self, not a person. macros sit on micros.",
                 out->n);
    }
    return out->n;
}

int ikitaku_method_lower(const char* name, ikitaku_method_form_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->name, sizeof out->name, "%s", name ? name : "?");
    const char* n = out->name;
    if (!strcmp(n,"scientific")) {
        snprintf(out->formula, sizeof out->formula, "try∘score∘revise");
        snprintf(out->vars[0],24,"try"); snprintf(out->vars[1],24,"score");
        snprintf(out->vars[2],24,"revise"); out->n_vars = 3;
    } else if (!strcmp(n,"hegelian")) {
        snprintf(out->formula, sizeof out->formula, "thesis∘antithesis∘synthesis");
        snprintf(out->vars[0],24,"thesis"); snprintf(out->vars[1],24,"antithesis");
        snprintf(out->vars[2],24,"synthesis"); out->n_vars = 3;
    } else if (!strcmp(n,"kant")) {
        snprintf(out->formula, sizeof out->formula, "axiom∘critique∘challenge");
        snprintf(out->vars[0],24,"axiom"); snprintf(out->vars[1],24,"critique");
        snprintf(out->vars[2],24,"challenge"); out->n_vars = 3;
    } else if (!strcmp(n,"occam")) {
        snprintf(out->formula, sizeof out->formula, "prefer∘shallow");
        snprintf(out->vars[0],24,"prefer"); snprintf(out->vars[1],24,"shallow");
        out->n_vars = 2;
    } else if (!strcmp(n,"origin")) {
        snprintf(out->formula, sizeof out->formula, "B∘C∘invert-A");
        snprintf(out->vars[0],24,"B"); snprintf(out->vars[1],24,"C");
        snprintf(out->vars[2],24,"invert-A"); out->n_vars = 3;
    } else if (!strcmp(n,"iterative")) {
        snprintf(out->formula, sizeof out->formula, "loop∘(try∘score∘revise)");
        snprintf(out->vars[0],24,"loop"); snprintf(out->vars[1],24,"try");
        snprintf(out->vars[2],24,"score"); out->n_vars = 3;
    } else if (!strcmp(n,"waterfall")) {
        snprintf(out->formula, sizeof out->formula, "lock∘next∘no-rewind");
        snprintf(out->vars[0],24,"lock"); snprintf(out->vars[1],24,"next");
        out->n_vars = 2;
    } else if (!strcmp(n,"bayesian")) {
        snprintf(out->formula, sizeof out->formula, "prior∘likelihood∘posterior");
        snprintf(out->vars[0],24,"prior"); snprintf(out->vars[1],24,"likelihood");
        snprintf(out->vars[2],24,"posterior"); out->n_vars = 3;
    } else {
        snprintf(out->formula, sizeof out->formula, "%s∘x", n);
        snprintf(out->vars[0],24,"%s", n); snprintf(out->vars[1],24,"x");
        out->n_vars = 2;
    }
    return out->n_vars;
}

int ikitaku_method_hegel_pick(int residual, int math_conf, int rule_conf,
                              int desperation, ikitaku_method_pick_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    const char* names[] = {
        "scientific","hegelian","kant","occam","origin","bayesian","iterative"
    };
    ikitaku_thought_plan_t plan;
    ikitaku_thought_plan(desperation, math_conf, rule_conf, &plan);
    for (int i = 0; i < 7 && out->n < IKITAKU_METH_MAX; ++i) {
        ikitaku_method_form_t* m = &out->m[out->n];
        ikitaku_method_lower(names[i], m);
        m->score = 40;
        for (int p = 0; p < plan.n; ++p)
            if (!strcmp(plan.form[p].name, names[i]))
                m->score = plan.form[p].weight;
        if (!strcmp(names[i],"iterative") && residual >= 25) m->score += 12;
        if (!strcmp(names[i],"occam") && residual < 15 && math_conf >= 80)
            m->score += 10;
        if (!strcmp(names[i],"hegelian") && math_conf >= 50 && rule_conf >= 50)
            m->score += 8;
        if (m->score > 100) m->score = 100;
        out->n++;
    }
    for (out->cycles = 0; out->cycles < 3; ++out->cycles) {
        int a = 0, b = 1;
        for (int i = 1; i < out->n; ++i)
            if (out->m[i].score > out->m[a].score) { b = a; a = i; }
            else if (i != a && out->m[i].score > out->m[b].score) b = i;
        if (b == a && out->n > 1) b = a ? 0 : 1;
        ikitaku_hypo_t t, an, s;
        memset(&t,0,sizeof t); memset(&an,0,sizeof an);
        snprintf(t.purpose, sizeof t.purpose, "%s", out->m[a].name);
        snprintf(an.purpose, sizeof an.purpose, "%s", out->m[b].name);
        t.confidence = out->m[a].score; an.confidence = out->m[b].score;
        ikitaku_critique_t cr;
        ikitaku_kant_tas(&t, &an, &s, &cr);
        int diff = t.confidence - an.confidence;
        if (diff < 0) diff = -diff;
        int synth = (t.confidence + an.confidence) / 2 + (100 - diff) / 10;
        if (synth > out->m[b].score && out->n < IKITAKU_METH_MAX && s.synthesized) {
            ikitaku_method_form_t* m = &out->m[out->n++];
            snprintf(m->name, sizeof m->name, "%s", s.form_id[0]?s.form_id:"synth");
            snprintf(m->formula, sizeof m->formula, "%s∘%s",
                     out->m[a].formula, out->m[b].formula);
            snprintf(m->vars[0],24,"%s", out->m[a].vars[0]);
            snprintf(m->vars[1],24,"%s", out->m[b].vars[0]);
            m->n_vars = 2;
            m->score = synth > 100 ? 100 : synth;
        } else {
            int bump = (out->m[a].score - out->m[b].score) / 5;
            if (bump < 0) bump = 0;
            out->m[b].score += bump;
        }
    }
    out->chosen = 0;
    for (int i = 1; i < out->n; ++i)
        if (out->m[i].score > out->m[out->chosen].score) out->chosen = i;
    if (residual >= 40 || desperation >= 5) {
        ikitaku_approach_t ap;
        memset(&ap, 0, sizeof ap);
        ikitaku_approach_shift(out->m[out->chosen].name, residual, desperation,
                               residual >= 60, 0, &ap);
        if (ap.next_lead[0]) {
            int hit = -1;
            for (int i = 0; i < out->n; ++i)
                if (!strcmp(out->m[i].name, ap.next_lead) ||
                    strstr(ap.next_lead, out->m[i].name)) {
                    hit = i; break;
                }
            if (hit >= 0) out->chosen = hit;
            else if (out->n < IKITAKU_METH_MAX) {
                ikitaku_method_form_t* m = &out->m[out->n++];
                memset(m, 0, sizeof *m);
                snprintf(m->name, sizeof m->name, "%s", ap.next_lead);
                snprintf(m->formula, sizeof m->formula, "approach-shift");
                m->score = 70 + (ap.novelty_seek ? 10 : 0);
                out->chosen = out->n - 1;
            }
        }
    }
    snprintf(out->english, sizeof out->english,
             "method-tool pick %s formula=%s score=%d cycles=%d n=%d",
             out->m[out->chosen].name, out->m[out->chosen].formula,
             out->m[out->chosen].score, out->cycles, out->n);
    return out->chosen;
}

int ikitaku_kant_try_pattern(const ikitaku_cluster_report_t* cr,
                             ikitaku_try_pat_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!cr || cr->cluster_n < 1) {
        snprintf(out->try_pattern, sizeof out->try_pattern, "scientific");
        snprintf(out->formula, sizeof out->formula, "try∘score∘revise");
        snprintf(out->english, sizeof out->english, "try-pattern default scientific (no cluster)");
        return 0;
    }
    int best = 0;
    for (int i = 1; i < cr->cluster_n; ++i)
        if (cr->clusters[i].membership > cr->clusters[best].membership)
            best = i;
    const ikitaku_cluster_t* c = &cr->clusters[best];
    snprintf(out->kant_class, sizeof out->kant_class, "%s",
             c->kant_class[0] ? c->kant_class : "quantity");
    snprintf(out->kant_cat, sizeof out->kant_cat, "%s",
             c->kant_cat[0] ? c->kant_cat : "unity");
    out->membership = c->membership;
    out->antinomy = c->antinomy;
    out->n_tagged = cr->cluster_n;
    const char* pat = "scientific";
    const char* form = "try∘score∘revise";
    if (c->antinomy || !strcmp(out->kant_class, "quality")) {
        pat = "hegelian"; form = "thesis∘antithesis∘synthesis";
    } else if (!strcmp(out->kant_class, "relation")) {
        pat = "origin"; form = "B∘C∘invert-A";
    } else if (!strcmp(out->kant_class, "schema")) {
        pat = "iterative"; form = "loop∘(try∘score∘revise)";
    } else if (!strcmp(out->kant_class, "modality")) {
        pat = "bayesian"; form = "prior∘likelihood∘posterior";
    } else if (!strcmp(out->kant_cat, "totality")) {
        pat = "iterative"; form = "loop∘(try∘score∘revise)";
    } else if (!strcmp(out->kant_cat, "unity") && c->membership >= 80) {
        pat = "occam"; form = "prefer∘shallow";
    }
    /* two strong different classes → synthesize try-pattern */
    int other = -1;
    for (int i = 0; i < cr->cluster_n; ++i) {
        if (i == best) continue;
        if (cr->clusters[i].membership >= 55 &&
            strcmp(cr->clusters[i].kant_class, out->kant_class) != 0) {
            other = i; break;
        }
    }
    if (other >= 0 && c->antinomy) {
        snprintf(out->try_pattern, sizeof out->try_pattern, "hegelian_x_%s", pat);
        snprintf(out->formula, sizeof out->formula,
                 "thesis∘antithesis∘synthesis∘%s", form);
    } else {
        snprintf(out->try_pattern, sizeof out->try_pattern, "%s", pat);
        snprintf(out->formula, sizeof out->formula, "%s", form);
    }
    out->conf = c->membership;
    if (out->conf > 100) out->conf = 100;
    snprintf(out->english, sizeof out->english,
             "try-pattern %s formula=%s from %s/%s mem=%d ant=%d n=%d",
             out->try_pattern, out->formula, out->kant_class, out->kant_cat,
             out->membership, out->antinomy, out->n_tagged);
    return out->conf;
}

int ikitaku_method_hegel_pick_kant(const ikitaku_cluster_report_t* cr,
                                   int residual, int math_conf, int rule_conf,
                                   int desperation, ikitaku_method_pick_t* out) {
    ikitaku_method_hegel_pick(residual, math_conf, rule_conf, desperation, out);
    if (!out) return -1;
    ikitaku_try_pat_t tp;
    ikitaku_kant_try_pattern(cr, &tp);
    const char* want = tp.try_pattern;
    /* boost the method that matches the Kant try-pattern */
    for (int i = 0; i < out->n; ++i) {
        int hit = !strcmp(out->m[i].name, want);
        int part = !hit && want[0] && strstr(want, out->m[i].name);
        if (hit) {
            out->m[i].score += 25;
            if (out->m[i].score > 100) out->m[i].score = 100;
        } else if (part) {
            out->m[i].score += 10;
            if (out->m[i].score > 100) out->m[i].score = 100;
        }
    }
    int want_i = -1;
    for (int i = 0; i < out->n; ++i)
        if (!strcmp(out->m[i].name, want)) { want_i = i; break; }
    out->chosen = 0;
    for (int i = 1; i < out->n; ++i)
        if (out->m[i].score > out->m[out->chosen].score) out->chosen = i;
    /* category stood: run that experiment even if occam is calmer */
    if (want_i >= 0 && tp.membership >= 70 && out->m[want_i].score >= 40)
        out->chosen = want_i;
    snprintf(out->english, sizeof out->english,
             "kant-try %s → pick %s formula=%s score=%d",
             want, out->m[out->chosen].name, out->m[out->chosen].formula,
             out->m[out->chosen].score);
    return out->chosen;
}

int ikitaku_self_card(ikitaku_self_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->is_concept = 1;
    out->is_person = 0;
    snprintf(out->r5, sizeof out->r5, "theorize");
    snprintf(out->formula, sizeof out->formula,
             "bag∘method∘(try∘score∘revise)∘watch");
    ikitaku_subject_teach("iki", "theorize", "analyze",
                          "kind", "process-as-object", 80);
    ikitaku_subject_teach("iki", "theorize", "analyze",
                          "self", "concept-on-ladder", 75);
    snprintf(out->english, sizeof out->english,
             "self-card: iki is ordered data that evaluates ordered data. "
             "concept=1 person=0 r5=%s", out->r5);
    return 1;
}

int ikitaku_self_reflect(int residual, int n_done, int solved,
                         const ikitaku_method_pick_t* methods,
                         ikitaku_self_t* out) {
    if (!out) return -1;
    ikitaku_self_card(out);
    ikitaku_method_pick_t pk;
    memset(&pk, 0, sizeof pk);
    if (methods) out->last = *methods;
    else {
        ikitaku_method_hegel_pick(residual, 55, 55,
                                  residual >= 25 ? 3 : 1, &pk);
        out->last = pk;
    }
    /* own run as a bag: residual/done/solved are the attrs */
    int own = residual;
    if (solved) own = own > 8 ? own - 8 : 0;
    if (n_done > 0) own = own > 5 ? own - 5 : 0;
    ikitaku_method_hegel_pick(own, 60, 55, 2, &pk);
    /* concept expansion: leftover method name not in last set */
    const char* neu = pk.m[pk.chosen].name;
    int known = 0;
    for (int i = 0; i < out->last.n; ++i)
        if (!strcmp(out->last.m[i].name, neu)) { known = 1; break; }
    if (!known && strchr(neu, '_') ) {
        out->expansions++;
        snprintf(out->formula, sizeof out->formula, "%s", pk.m[pk.chosen].formula);
        ikitaku_r3_rec_t rec;
        ikitaku_F_merge_lower("theorize", "analyze", &rec);
        ikitaku_goal_create("theorize", neu, NULL);
    }
    out->last = pk;
    snprintf(out->english, sizeof out->english,
             "self-reflect pick=%s formula=%s expand=%d residual %d→%d done=%d",
             pk.m[pk.chosen].name, pk.m[pk.chosen].formula,
             out->expansions, residual, own, n_done);
    return out->expansions;
}

int ikitaku_method_have(const char* parent, const char* family) {
    if (!parent || !family) return 0;
    char fams[16][32];
    int n = ikitaku_r4_under(parent, fams, 16);
    for (int i = 0; i < n; ++i)
        if (strcmp(fams[i], family) == 0) return 1;
    ikitaku_subject_t sub;
    if (ikitaku_subject_get(family, &sub) == 0) return 1;
    if (ikitaku_subject_get(parent, &sub) == 0) {
        for (int i = 0; i < sub.n; ++i)
            if (strstr(sub.fact[i].val, family)) return 1;
    }
    return 0;
}

int ikitaku_solver_audit(int needs_boolean, int needs_nested, int needs_xor,
                         int has_data, int cascade_ok,
                         ikitaku_solver_gap_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->want_solver = 1;
    out->tier = 6; /* default: cognition insufficient, method named */
    snprintf(out->question, sizeof out->question, "unknown-question");


    int have_bool = ikitaku_method_have("decision", "boolean-tt");
    int have_charge = ikitaku_method_have("invert", "charge-disunity");
    int have_nest = ikitaku_method_have("emit", "nested-quote");
    int have_xor  = ikitaku_method_have("compare", "xor-lock");

    if (!has_data) {
        out->kind = IKITAKU_GAP_DATA;
        snprintf(out->missing, sizeof out->missing, "observations");
    } else if (needs_nested && !have_nest) {
        out->kind = IKITAKU_GAP_METHOD;
        snprintf(out->missing, sizeof out->missing, "emit|nested-quote");
    } else if (needs_boolean && !have_bool && !have_charge) {
        out->kind = IKITAKU_GAP_METHOD;
        snprintf(out->missing, sizeof out->missing, "decision|boolean-tt");
    } else if (needs_boolean && !have_bool) {
        out->kind = IKITAKU_GAP_METHOD;
        snprintf(out->missing, sizeof out->missing, "decision|boolean-tt");
    } else if (needs_xor && !have_xor) {
        out->kind = IKITAKU_GAP_METHOD;
        snprintf(out->missing, sizeof out->missing, "compare|xor-lock");
    } else if (cascade_ok) {
        out->kind = IKITAKU_GAP_NONE;
        out->has_solver = 1;
        out->understanding = 1;
        out->gap_charge = ikitaku_charge_compose(out->understanding, out->want_solver);
        out->tier = 6;
        snprintf(out->iterate, sizeof out->iterate, "apply existing solver");

        snprintf(out->english, sizeof out->english,
                 "solver-audit: HAS solver. understanding=+1 (unity with having a solver). gap_charge=%d",
                 out->gap_charge);
        return 0;
    } else {
        /* tools exist but this attempt failed — still a method-use gap */
        out->kind = IKITAKU_GAP_METHOD;
        snprintf(out->missing, sizeof out->missing, "solver-application");
        out->tier = 7; /* methods exist, still cannot determine the failure */
    }

    out->has_solver = 0;
    out->understanding = -1; /* disunity with has-solver */
    out->gap_charge = ikitaku_charge_compose(out->understanding, out->want_solver);
    snprintf(out->iterate, sizeof out->iterate,
             "name missing method '%s'; ask; do not hang; do not fake a solution",
             out->missing);
    snprintf(out->english, sizeof out->english,
             "solver-audit: NO method to complete this. missing=%s. "
             "understanding-state is disunity (-1) with unity-of-having-a-solver (+1); "
             "gap_charge=%d kind=%d tier=R%d",
             out->missing, out->gap_charge, out->kind, out->tier);

    return out->kind;
}

int ikitaku_solver_from_remainder(const ikitaku_remainder_t* r,
                                  ikitaku_solver_gap_t* out) {
    if (!r || !out) return -1;
    memset(out, 0, sizeof *out);
    out->want_solver = 1;
    snprintf(out->question, sizeof out->question, "isolate-one-target");
    if (!r->irreducible) {
        out->kind = IKITAKU_GAP_NONE;
        out->has_solver = 1;
        out->understanding = 1;
        out->tier = 6;
        out->gap_charge = ikitaku_charge_compose(1, 1);
        snprintf(out->english, sizeof out->english,
                 "from-remainder: isolable. %s", r->english);
        return 0;
    }
    /* Method may have held. Unique pick did not. Not a missing-method hole. */
    out->kind = IKITAKU_GAP_REMAINDER;
    out->has_solver = r->dissolved ? 1 : 0;
    out->understanding = r->weird ? 1 : -1; /* unity with method, not with unique answer */
    out->gap_charge = ikitaku_charge_compose(out->understanding, out->want_solver);
    out->tier = 9; /* leftover named; nothing to iterate into */
    snprintf(out->missing, sizeof out->missing, "isolable-pick");
    snprintf(out->iterate, sizeof out->iterate,
             "stop. leftover worlds. do not invent a pick. do not ask for another method.");
    snprintf(out->english, sizeof out->english,
             "from-remainder: IRREDUCIBLE%s. %s",
             r->weird ? " WEIRD" : "", r->english);
    return out->kind;
}

int ikitaku_solver_iterate(ikitaku_solver_gap_t* g,
                           ikitaku_reason_chain_t* chain) {
    if (!g) return -1;
    if (chain) memset(chain, 0, sizeof *chain);
    if (g->kind == IKITAKU_GAP_NONE && g->has_solver) {
        if (chain) {
            chain_add(chain, "solver present — understanding=+1 unity with having a solver");
            chain_add(chain, "gap_charge=%d (unity)", g->gap_charge);
            snprintf(chain->conclusion, sizeof chain->conclusion,
                     "has-solver unity; run cascade");
        }
        return 0;
    }
    if (g->kind == IKITAKU_GAP_REMAINDER) {
        if (chain) {
            chain_add(chain, "tested: a method ran");
            if (g->has_solver)
                chain_add(chain, "method held (has_solver=1) — this is not a missing-method hole");
            chain_add(chain, "leftover: pick is not unique. missing=%s", g->missing);
            chain_add(chain, "this uncertainty: nothing in remaining budget can close it");
            chain_add(chain, "autonomous: do not hang, do not invent a target, do not ask for another solver");
            snprintf(chain->conclusion, sizeof chain->conclusion,
                     "IRREDUCIBLE REMAINDER. weird-if-method-held. stop without faking a pick.");
        }
        return 2;
    }
    if (chain) {
        chain_add(chain, "detect: no method completes the question");

        chain_add(chain, "missing method: %s", g->missing[0] ? g->missing : "?");
        chain_add(chain,
                  "understanding-state charge=%d ; want unity-with-solver = +1",
                  g->understanding);
        chain_add(chain,
                  "understanding is disunity with the state of being in unity with having a solver (gap_charge=%d)",
                  g->gap_charge);
        chain_add(chain, "autonomous iterate: do not hang, do not pretend solved");
    }

    /* propose stub family so the gap is named on the ladder */
    if (g->kind == IKITAKU_GAP_METHOD && strchr(g->missing, '|')) {
        char par[24], fam[32];
        par[0] = fam[0] = 0;
        sscanf(g->missing, "%23[^|]|%31s", par, fam);
        if (par[0] && fam[0]) {
            int had = ikitaku_r4_propose(par, fam, "method-gap stub; no solver yet");
            if (chain)
                chain_add(chain, "propose R4 %s|%s stub (had=%d) so the missing method is named",
                          par, fam, had);
        }
    }

    ikitaku_context_ask_t ask;
    memset(&ask, 0, sizeof ask);
    ikitaku_need_context(g->missing[0] ? g->missing : "missing-solver", NULL, &ask);
    if (chain)
        chain_add(chain, "need_context: %s", ask.ask[0] ? ask.ask : ask.english);
    {
        ikitaku_approach_t ap;
        memset(&ap, 0, sizeof ap);
        ikitaku_approach_shift(g->missing[0] ? g->missing : "hegelian",
                               75, 7, 1, 0, &ap);
        snprintf(g->iterate, sizeof g->iterate, "approach next=%s anti=%s",
                 ap.next_lead, ap.anti_lead);
        if (chain)
            chain_add(chain, "approach-shift next=%s inhibit=%d novel=%d",
                      ap.next_lead, ap.inhibited, ap.novelty_seek);
    }

    snprintf(g->iterate, sizeof g->iterate,
             "wait/ask for method '%s'; understanding remains disunity until a solver exists",
             g->missing);
    if (chain)
        snprintf(chain->conclusion, sizeof chain->conclusion,
                 "NO SOLVER. understanding=disunity with has-solver. missing=%s. stop without faking.",
                 g->missing);
    return 1;
}




void ikitaku_circle_init(ikitaku_circle_t* c) {
    if (!c) return;
    memset(c, 0, sizeof *c);
    c->macro_conf = 100;
    c->act = IKITAKU_CIRCLE_OK;
}

static int loop_fp_mix(const char* lead, const char* form,
                       int residual, int r7, int r6) {
    unsigned h = 2166136261u;
    const char* s = lead ? lead : "";
    for (; *s; ++s) h = (h ^ (unsigned)(unsigned char)*s) * 16777619u;
    s = form ? form : "";
    for (; *s; ++s) h = (h ^ (unsigned)(unsigned char)*s) * 16777619u;
    h ^= (unsigned)(residual / 5) * 2654435761u;
    h ^= (unsigned)r7 * 40503u;
    h ^= (unsigned)r6 * 7949u;
    return (int)(h & 0x7fffffff);
}

int ikitaku_circle_observe(ikitaku_circle_t* c,
                           const char* lead, const char* form,
                           int residual, int r7_pick, int r6_ready) {
    if (!c) return -1;
    ikitaku_loop_fp_t cur;
    memset(&cur, 0, sizeof cur);
    snprintf(cur.lead, sizeof cur.lead, "%s", lead ? lead : "-");
    snprintf(cur.form, sizeof cur.form, "%s", form ? form : "-");
    cur.residual = residual;
    cur.r7_pick = r7_pick;
    cur.r6_ready = r6_ready;
    cur.fingerprint = loop_fp_mix(cur.lead, cur.form, residual, r7_pick, r6_ready);

    c->progress = 0;
    if (c->n_hist > 0) {
        const ikitaku_loop_fp_t* prev = &c->hist[c->n_hist - 1];
        int same_logic = (prev->fingerprint == cur.fingerprint)
            || (!strcmp(prev->lead, cur.lead) && !strcmp(prev->form, cur.form)
                && prev->r7_pick == cur.r7_pick && prev->r6_ready == cur.r6_ready);
        int closer = (cur.residual < prev->residual - 2);
        c->progress = closer ? 1 : 0;
        if (same_logic && !closer) {
            c->identical_streak++;
            /* macro conf falls hard on each non-progressing identical loop */
            int drop = 12 + c->identical_streak * 8;
            if (drop > 35) drop = 35;
            c->macro_conf -= drop;
            if (c->macro_conf < 0) c->macro_conf = 0;
        } else if (closer) {
            c->identical_streak = 0;
            if (c->macro_conf < 90) c->macro_conf += 5;
            if (c->macro_conf > 100) c->macro_conf = 100;
        } else {
            /* different logic but not closer — mild doubt */
            c->identical_streak = 0;
            c->macro_conf -= 4;
            if (c->macro_conf < 0) c->macro_conf = 0;
        }
    }

    if (c->n_hist < IKITAKU_CIRCLE_HIST)
        c->hist[c->n_hist++] = cur;
    else {
        for (int i = 1; i < IKITAKU_CIRCLE_HIST; ++i)
            c->hist[i - 1] = c->hist[i];
        c->hist[IKITAKU_CIRCLE_HIST - 1] = cur;
    }

    /* choose act */
    if (c->identical_streak >= 2 && c->macro_conf < 40) {
        /* very stuck: prefer halt missing priors if residual still high */
        if (residual >= 35)
            c->act = IKITAKU_CIRCLE_HALT_PRIORS;
        else
            c->act = IKITAKU_CIRCLE_SELF_IMPROVE;
    } else if (c->identical_streak >= 1 && c->macro_conf < 55) {
        c->act = IKITAKU_CIRCLE_SELF_IMPROVE;
    } else if (c->identical_streak >= 1 && residual >= 40) {
        c->act = IKITAKU_CIRCLE_SEEK_PRIORS;
    } else if (c->identical_streak >= 1) {
        c->act = IKITAKU_CIRCLE_WARN;
    } else {
        c->act = IKITAKU_CIRCLE_OK;
    }

    const char* aname = "ok";
    switch (c->act) {
    case IKITAKU_CIRCLE_WARN: aname = "warn-circle"; break;
    case IKITAKU_CIRCLE_HALT_PRIORS: aname = "halt-missing-priors"; break;
    case IKITAKU_CIRCLE_SELF_IMPROVE: aname = "self-improve-meta"; break;
    case IKITAKU_CIRCLE_SEEK_PRIORS: aname = "seek-missing-priors"; break;
    default: break;
    }
    if (c->identical_streak >= 1) {
        ikitaku_approach_t ap;
        memset(&ap, 0, sizeof ap);
        ikitaku_approach_shift(cur.lead, residual,
                               residual >= 60 ? 7 : 4,
                               residual >= 50, 0, &ap);
        if (ap.next_lead[0])
            snprintf(cur.lead, sizeof cur.lead, "%s", ap.next_lead);
    }
    snprintf(c->english, sizeof c->english,
             "circle streak=%d macro_conf=%d progress=%d act=%s fp=%d residual=%d "
             "lead=%s — %s",
             c->identical_streak, c->macro_conf, c->progress, aname,
             cur.fingerprint, residual, cur.lead,
             c->act == IKITAKU_CIRCLE_HALT_PRIORS
                 ? "going in circles; halt, missing priors"
             : c->act == IKITAKU_CIRCLE_SELF_IMPROVE
                 ? "going in circles; meta-metacog: better way to think"
             : c->act == IKITAKU_CIRCLE_SEEK_PRIORS
                 ? "going in circles; consider how to find missing priors"
             : c->act == IKITAKU_CIRCLE_WARN
                 ? "loop repeated without closing gap"
                 : "loop advancing or first pass");
    return c->act;
}


int ikitaku_circle_map_concept(const ikitaku_circle_t* c,
                               int allow_teach,
                               ikitaku_circle_concept_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->r5, sizeof out->r5, "repeat");
    snprintf(out->r4, sizeof out->r4, "thought-loop");

    /* Analogy: thought-loop : cycle :: while-loop : cycle
       Shared platonic: R5 repeat / R4 cycle. Different context bags. */
    ikitaku_analogy_t an;
    memset(&an, 0, sizeof an);
    ikitaku_analogy("thought-loop", "cycle", "while-loop", "cycle", &an);
    out->analogy_sim = an.sim;

    if (c && c->identical_streak >= 1) {
        out->mapped = 1;
        if (allow_teach) {
            ikitaku_subject_teach("thought-loop", "repeat", "thought-loop",
                                  "kind", "metacog-cycle", 75);
            ikitaku_subject_teach("while-loop", "repeat", "while-axis",
                                  "kind", "control-cycle", 75);
            ikitaku_subject_teach("circular-thinking", "theorize", "circular-eval",
                                  "parent", "thought-loop", 70);
            ikitaku_r4_propose("repeat", "thought-loop",
                               "re-evaluate methods without residual drop");
            ikitaku_r4_propose("theorize", "circular-eval",
                               "same fingerprint no progress");
        }
        snprintf(out->english, sizeof out->english,
                 "circle→concept: thought-loop is a form of while-loop; "
                 "both derive from R5 repeat / R4 cycle (context differs: "
                 "methods-as-data vs control-axis). analogy sim=%d streak=%d "
                 "macro_conf=%d | %s",
                 an.sim, c->identical_streak, c->macro_conf, an.english);
    } else {
        out->mapped = 0;
        snprintf(out->english, sizeof out->english,
                 "circle→concept idle (no streak). latent analogy "
                 "thought-loop:cycle :: while-loop:cycle sim=%d under R5 repeat",
                 an.sim);
    }
    return out->mapped;
}

int ikitaku_self_improve(int residual, int math_conf, int rule_conf,
                         uint8_t depth, int allow_persist,
                         ikitaku_self_improve_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (depth > IKITAKU_R6_DEPTH_MAX) depth = IKITAKU_R6_DEPTH_MAX;
    if (residual < 0) residual = 0;
    if (residual > 100) residual = 100;
    int cur = residual;
    char prev_method[24] = "scientific";
    int prev_conf = 50;
    int start = residual;

    /* Same pathway as a normal bag: thought_plan → method_hegel_pick →
       dialectic_hunt / kant_tas → hegel_bool16. The "bag" is the method pair.
       Residual is just 100 − synthesis confidence (accept metric). */
    for (uint8_t d = 0; d <= depth && out->n < IKITAKU_SI_STEPS; ++d) {
        int desperation = 1;
        if (cur >= 40) desperation = 3;
        if (cur >= 60) desperation = 5;
        if (cur >= 80) desperation = 7;

        ikitaku_thought_plan_t plan;
        ikitaku_thought_plan(desperation, math_conf, rule_conf, &plan);

        ikitaku_method_pick_t pk;
        memset(&pk, 0, sizeof pk);
        ikitaku_method_hegel_pick(cur, math_conf, rule_conf, desperation, &pk);

        /* Set-shift: failed leftover = antithesis; inhibit last; Occam if calm. */
        ikitaku_approach_t ap;
        memset(&ap, 0, sizeof ap);
        ikitaku_approach_shift(prev_method, cur, desperation,
                               cur >= 60 /* predicted hard */, 0, &ap);

        const char* pick = ap.next_lead[0] ? ap.next_lead : "scientific";
        if (plan.n > 0 && plan.chosen >= 0 && plan.chosen < plan.n &&
            ap.occam_bias && !strcmp(plan.form[plan.chosen].name, "occam"))
            pick = "occam";
        char composed[24];
        if (ap.recomb[0] && strlen(ap.recomb) < sizeof composed) {
            snprintf(composed, sizeof composed, "%s", ap.recomb);
            pick = composed;
        }

        ikitaku_hypo_t th, anti, syn;
        memset(&th, 0, sizeof th);
        memset(&anti, 0, sizeof anti);
        memset(&syn, 0, sizeof syn);
        snprintf(th.form_id, sizeof th.form_id, "%s", prev_method);
        snprintf(th.purpose, sizeof th.purpose, "theorize");
        th.confidence = prev_conf;
        th.feasible = 1;
        snprintf(anti.form_id, sizeof anti.form_id, "%s", pick);
        snprintf(anti.purpose, sizeof anti.purpose, "theorize");
        anti.confidence = 100 - cur;
        if (anti.confidence < 15) anti.confidence = 15;
        if (anti.confidence > 95) anti.confidence = 95;
        anti.feasible = 1;

        ikitaku_hunt_t hunt;
        memset(&hunt, 0, sizeof hunt);
        ikitaku_dialectic_hunt(&th, &anti, &syn, &hunt);

        ikitaku_critique_t cr;
        memset(&cr, 0, sizeof cr);
        if (!syn.form_id[0])
            ikitaku_kant_tas(&th, &anti, &syn, &cr);
        else
            ikitaku_kant_tas(&th, &anti, &syn, &cr);

        ikitaku_hegel_bool_t hb;
        memset(&hb, 0, sizeof hb);
        int op = ikitaku_hegel_bool16(&th, &anti, 0, &hb);

        /* Standard accept metric: residual ↔ 100 − best synthesis conf */
        int conf = syn.confidence;
        if (hb.synthesis.confidence > conf) conf = hb.synthesis.confidence;
        if (hunt.solved && conf < 90) conf += 4;
        if (conf < 10) conf = 10;
        if (conf > 95) conf = 95;
        int next = 100 - conf;
        /* monotonic: never increase residual without evidence (pathway held) */
        if (next > cur && hb.synth_charge > 0) next = cur;
        if (next > cur) next = cur - 1;
        if (next < 0) next = 0;

        ikitaku_si_step_t* st = &out->step[out->n++];
        snprintf(st->method, sizeof st->method, "%s", pick);
        snprintf(st->op, sizeof st->op, "%s",
                 op >= 0 ? ikitaku_bool16_name(op) : "reflect");
        st->residual_in = cur;
        st->residual_out = next;
        st->conf = conf;
        st->invented = 0;

        if (strcmp(pick, prev_method) != 0 && cur >= 25 && next < cur) {
            char form[80];
            snprintf(form, sizeof form, "self-improve %s→%s via %s",
                     prev_method, pick, st->op);
            if (allow_persist) {
                int had = ikitaku_strategy_propose(pick, form, 0);
                if (had == 0) {
                    st->invented = 1;
                    out->invented_n++;
                    ikitaku_r3_rec_t rec;
                    ikitaku_F_merge_lower("theorize", pick, &rec);
                }
            } else {
                st->invented = 1;
                out->invented_n++;
            }
        }

        if (st->conf >= out->best_conf) {
            out->best_conf = st->conf;
            snprintf(out->best_method, sizeof out->best_method, "%s", pick);
        }
        snprintf(prev_method, sizeof prev_method, "%s", pick);
        prev_conf = st->conf;
        cur = next;
        out->depth_used = (int)d + 1;
        if (cur <= 5) break;
    }

    out->improved = (start - cur);
    if (out->improved < 0) out->improved = 0;
    snprintf(out->english, sizeof out->english,
             "self-improve (std pathway) depth=%d steps=%d residual %d→%d drop=%d "
             "best=%s conf=%d invented=%d persist=%d",
             out->depth_used, out->n, start, cur, out->improved,
             out->best_method[0] ? out->best_method : "-", out->best_conf,
             out->invented_n, allow_persist);
    return out->improved;
}



void ikitaku_trace_init(ikitaku_thought_trace_t* tr) {
    if (!tr) return;
    memset(tr, 0, sizeof *tr);
}

int ikitaku_trace_push(ikitaku_thought_trace_t* tr,
                       const char* lead, const char* form, const char* r5,
                       int residual, int conf, int r7, int r6, int outcome) {
    if (!tr || tr->n >= IKITAKU_TRACE_MAX) return -1;
    ikitaku_micro_t* m = &tr->step[tr->n++];
    snprintf(m->lead, sizeof m->lead, "%s", lead ? lead : "-");
    snprintf(m->form, sizeof m->form, "%s", form ? form : "-");
    snprintf(m->r5, sizeof m->r5, "%s", r5 ? r5 : "-");
    m->residual = residual;
    m->conf = conf;
    m->r7_pick = r7;
    m->r6_ready = r6;
    m->outcome = outcome;
    snprintf(tr->english, sizeof tr->english,
             "trace n=%d last residual=%d lead=%s", tr->n, residual, m->lead);
    return tr->n;
}

int ikitaku_trace_macro(const ikitaku_thought_trace_t* tr,
                        int goal_residual_target,
                        ikitaku_thought_macro_t* out) {
    if (!tr || !out) return -1;
    memset(out, 0, sizeof *out);
    out->n_steps = tr->n;
    if (tr->n < 1) {
        snprintf(out->english, sizeof out->english, "macro: empty trace");
        return 0;
    }
    out->residual_first = tr->step[0].residual;
    out->residual_last = tr->step[tr->n - 1].residual;
    out->delta = out->residual_last - out->residual_first;
    out->macro_conf = 100;
    for (int i = 1; i < tr->n; ++i) {
        int d = tr->step[i].residual - tr->step[i - 1].residual;
        if (d >= -2) { /* not closer */
            out->stall_n++;
            out->macro_conf -= 10;
        } else {
            out->progress_n++;
            if (out->macro_conf < 95) out->macro_conf += 3;
        }
    }
    if (out->macro_conf < 0) out->macro_conf = 0;
    if (out->macro_conf > 100) out->macro_conf = 100;

    /* pattern */
    if (out->stall_n >= 2 && out->progress_n == 0)
        snprintf(out->pattern, sizeof out->pattern, "cycle");
    else if (out->progress_n >= 2 && out->stall_n == 0)
        snprintf(out->pattern, sizeof out->pattern, "cascade");
    else if (out->delta == 0 && tr->n >= 2)
        snprintf(out->pattern, sizeof out->pattern, "flat");
    else
        snprintf(out->pattern, sizeof out->pattern, "mixed");

    /* macro relates to goal: moving residual toward target */
    int toward = 0;
    if (out->residual_last <= goal_residual_target)
        toward = 1;
    else if (out->delta < 0)
        toward = 1;
    out->relates_goal = toward;

    snprintf(out->english, sizeof out->english,
             "macro steps=%d residual %d→%d Δ=%d stall=%d progress=%d "
             "pattern=%s macro_conf=%d goal_rel=%d (target residual≤%d)",
             out->n_steps, out->residual_first, out->residual_last, out->delta,
             out->stall_n, out->progress_n, out->pattern, out->macro_conf,
             out->relates_goal, goal_residual_target);
    return out->n_steps;
}

/* Parent toward abstract (DOWN). Returns 1 if changed. */
static int flow_parent(const char* cur, int layer, char* next, size_t nnext) {
    ikitaku_r5_verb_t v;
    next[0] = 0;
    if (layer >= 6) {
        /* R6 strategies → R5 theorize (methods-as-data) */
        if (!strcmp(cur, "scientific") || !strcmp(cur, "hegelian") ||
            !strcmp(cur, "bayesian") || !strcmp(cur, "occam") ||
            !strcmp(cur, "iterative") || !strcmp(cur, "waterfall") ||
            !strcmp(cur, "kant") || strstr(cur, "_x_"))
            snprintf(next, nnext, "theorize");
        else if (!strcmp(cur, "thinking") || !strcmp(cur, "raise_lower"))
            snprintf(next, nnext, "thinking");
        else
            snprintf(next, nnext, "theorize");
        return 1;
    }
    if (layer == 5) {
        /* R5 verb → still R5 but broader sibling class, then to pattern at R4 boundary.
           Cycle family collapses to repeat; strategies already theorize. */
        if (!strcmp(cur, "thought-loop") || !strcmp(cur, "while-loop") ||
            !strcmp(cur, "cycle") || !strcmp(cur, "circular-thinking") ||
            !strcmp(cur, "circular-eval"))
            snprintf(next, nnext, "repeat");
        else if (!strcmp(cur, "theorize") || !strcmp(cur, "scientific") ||
                 !strcmp(cur, "hegelian") || !strcmp(cur, "thinking"))
            snprintf(next, nnext, "theorize");
        else if (ikitaku_r5_lookup(cur, &v) == 0) {
            /* Known R5: one step abstract = keep verb as the platonic tip,
               next drop will leave R5 into pattern. Mark by returning same once. */
            snprintf(next, nnext, "%s", cur);
            return 0; /* no name change; caller advances layer only */
        } else
            snprintf(next, nnext, "map");
        return strcmp(next, cur) != 0;
    }
    if (layer == 4) {
        /* R4 family → R5 parent */
        if (strstr(cur, "loop") || strstr(cur, "axis") || strstr(cur, "cycle") ||
            strstr(cur, "counted") || strstr(cur, "while") || !strcmp(cur, "repeat"))
            snprintf(next, nnext, "repeat");
        else if (strstr(cur, "eval") || strstr(cur, "theoriz") || strstr(cur, "method") ||
                 strstr(cur, "hegel") || strstr(cur, "scien") || strstr(cur, "occam"))
            snprintf(next, nnext, "theorize");
        else if (strstr(cur, "sort") || strstr(cur, "order") || strstr(cur, "swap"))
            snprintf(next, nnext, "order");
        else if (strstr(cur, "math") || strstr(cur, "table") || strstr(cur, "point"))
            snprintf(next, nnext, "map");
        else if (ikitaku_r5_lookup(cur, &v) == 0)
            snprintf(next, nnext, "%s", cur);
        else
            snprintf(next, nnext, "map");
        return 1;
    }
    if (layer == 3) {
        /* R3 genus → R5/R4 abstract: pattern of transforms */
        if (!strcmp(cur, "pattern") || !strcmp(cur, "genus"))
            snprintf(next, nnext, "map");
        else
            snprintf(next, nnext, "pattern");
        return 1;
    }
    if (layer == 2) {
        snprintf(next, nnext, "pattern");
        return 1;
    }
    if (layer == 1) {
        snprintf(next, nnext, "micro-schema");
        return 1;
    }
    snprintf(next, nnext, "surface-ops");
    return 1;
}

/* Child toward concrete (UP). Returns 1 if changed. */
static int flow_child(const char* cur, int layer, char* next, size_t nnext) {
    next[0] = 0;
    if (layer <= 0) {
        snprintf(next, nnext, "surface-ops");
        return 0;
    }
    if (layer == 1) {
        snprintf(next, nnext, "micro-schema");
        return 1;
    }
    if (layer == 2) {
        snprintf(next, nnext, "pattern");
        return 1;
    }
    if (layer == 3) {
        /* pattern → prefer a known R5 if cur hints, else map */
        if (strstr(cur, "repeat") || strstr(cur, "loop") || strstr(cur, "cycle"))
            snprintf(next, nnext, "repeat");
        else if (strstr(cur, "theoriz") || strstr(cur, "think"))
            snprintf(next, nnext, "theorize");
        else
            snprintf(next, nnext, "map");
        return 1;
    }
    if (layer == 4) {
        /* R4: specialize under parent */
        if (!strcmp(cur, "repeat"))
            snprintf(next, nnext, "thought-loop"); /* metacog default; while-axis is alt */
        else if (!strcmp(cur, "theorize"))
            snprintf(next, nnext, "circular-eval");
        else if (!strcmp(cur, "map"))
            snprintf(next, nnext, "map-math");
        else if (!strcmp(cur, "order"))
            snprintf(next, nnext, "compare-swap");
        else
            snprintf(next, nnext, "%s", cur);
        return 1;
    }
    if (layer == 5) {
        /* R5 → R6 strategy micro when theorize/thinking */
        if (!strcmp(cur, "theorize") || !strcmp(cur, "thinking"))
            snprintf(next, nnext, "scientific");
        else if (!strcmp(cur, "repeat"))
            snprintf(next, nnext, "thought-loop");
        else
            snprintf(next, nnext, "%s", cur);
        return 1;
    }
    /* layer 6+: already tip */
    snprintf(next, nnext, "%s", cur);
    return 0;
}

int ikitaku_layer_flow(const char* form, int from_layer, int n_steps,
                       int dir, ikitaku_layer_drop_t* out) {
    if (!out || !form || n_steps < 1) return -1;
    memset(out, 0, sizeof *out);
    if (from_layer < 0) from_layer = 0;
    if (from_layer > 6) from_layer = 6;
    out->from_layer = from_layer;
    out->dir = dir;

    char cur[32];
    snprintf(cur, sizeof cur, "%s", form);
    int layer = from_layer;
    snprintf(out->at[out->n_at++], 32, "R%d:%s", layer, cur);

    for (int s = 0; s < n_steps && out->n_at < 8; ++s) {
        char next[32];
        int changed;
        if (dir == IKITAKU_FLOW_DOWN) {
            if (layer <= 0) break;
            changed = flow_parent(cur, layer, next, sizeof next);
            layer--;
            if (!changed && next[0]) {
                /* layer advanced, name same — still record once then force broader */
                if (!strcmp(next, cur) && layer >= 0) {
                    flow_parent(cur, layer, next, sizeof next);
                }
            }
        } else {
            if (layer >= 6) break;
            changed = flow_child(cur, layer, next, sizeof next);
            layer++;
        }
        if (!next[0]) break;
        snprintf(cur, sizeof cur, "%s", next);
        snprintf(out->at[out->n_at++], 32, "R%d:%s", layer, cur);
    }
    out->to_layer = layer;
    snprintf(out->broader, sizeof out->broader, "%s", cur);

    ikitaku_r5_verb_t v;
    if (ikitaku_r5_lookup(cur, &v) == 0)
        snprintf(out->abstract_r5, sizeof out->abstract_r5, "%s", cur);
    else if (dir == IKITAKU_FLOW_DOWN) {
        /* walk path for last known R5 */
        for (int i = out->n_at - 1; i >= 0; --i) {
            const char* p = strchr(out->at[i], ':');
            if (!p) continue;
            p++;
            if (ikitaku_r5_lookup(p, &v) == 0) {
                snprintf(out->abstract_r5, sizeof out->abstract_r5, "%s", p);
                break;
            }
        }
    }

    snprintf(out->english, sizeof out->english,
             "layer-flow %s %s from R%d ×%d → R%d tip=%s r5=%s path=",
             dir == IKITAKU_FLOW_DOWN ? "DOWN(abstract)" : "UP(micro)",
             form, out->from_layer, n_steps, out->to_layer, out->broader,
             out->abstract_r5[0] ? out->abstract_r5 : "-");
    size_t len = strlen(out->english);
    for (int i = 0; i < out->n_at && len + 36 < sizeof out->english; ++i) {
        int n = snprintf(out->english + len, sizeof out->english - len,
                         "%s%s", i ? "→" : "", out->at[i]);
        if (n > 0) len += (size_t)n;
    }
    return out->n_at;
}

int ikitaku_layer_drop(const char* form, int from_layer, int n_down,
                       ikitaku_layer_drop_t* out) {
    return ikitaku_layer_flow(form, from_layer, n_down, IKITAKU_FLOW_DOWN, out);
}

int ikitaku_abstract_subject(const char* subject, int n_down,
                             ikitaku_layer_drop_t* out) {
    if (!subject || !out) return -1;
    /* Subjects often sit at R4/R5; start at R5 unless already strategy. */
    int from = 5;
    if (!strcmp(subject, "scientific") || !strcmp(subject, "hegelian") ||
        !strcmp(subject, "bayesian") || !strcmp(subject, "occam") ||
        strstr(subject, "_x_"))
        from = 6;
    int n = ikitaku_layer_flow(subject, from, n_down > 0 ? n_down : 3,
                               IKITAKU_FLOW_DOWN, out);
    if (out) out->used_for_metacog = 1;
    return n;
}

int ikitaku_concretize_form(const char* form, int from_layer, int n_up,
                            ikitaku_layer_drop_t* out) {
    if (!form || !out) return -1;
    int n = ikitaku_layer_flow(form, from_layer, n_up > 0 ? n_up : 2,
                               IKITAKU_FLOW_UP, out);
    if (out) out->used_for_metacog = 1;
    return n;
}

int ikitaku_reflect_chain(const ikitaku_thought_trace_t* tr,
                          int goal_residual_target,
                          int drop_layers,
                          int allow_teach,
                          ikitaku_reflect_t* out) {
    if (!tr || !out) return -1;
    memset(out, 0, sizeof *out);
    ikitaku_trace_macro(tr, goal_residual_target, &out->macro);
    out->reflected = 1;

    const char* seed = out->macro.pattern;
    int from = 5;
    if (!strcmp(seed, "cycle")) {
        seed = "thought-loop";
        from = 5;
    } else if (!strcmp(seed, "cascade")) {
        seed = "theorize";
        from = 5;
    } else if (!strcmp(seed, "flat") || !strcmp(seed, "mixed")) {
        seed = (tr->n > 0) ? tr->step[tr->n - 1].lead : "thinking";
        from = 6;
    } else if (tr->n > 0) {
        seed = tr->step[tr->n - 1].lead;
        from = 6;
    }

    if (drop_layers < 1) drop_layers = 3;
    /* DOWN: abstract the chain pattern for macro metacog */
    ikitaku_layer_flow(seed, from, drop_layers, IKITAKU_FLOW_DOWN, &out->drop);
    out->drop.used_for_metacog = 1;

    /* UP sample: from abstract tip back one step toward micro (shows duality) */
    if (out->drop.broader[0]) {
        ikitaku_layer_drop_t up;
        ikitaku_layer_flow(out->drop.broader,
                           out->drop.to_layer > 0 ? out->drop.to_layer : 3,
                           1, IKITAKU_FLOW_UP, &up);
        /* append brief to english only */
        (void)up;
    }

    if (!strcmp(out->macro.pattern, "cycle")) {
        ikitaku_circle_t tmp;
        ikitaku_circle_init(&tmp);
        tmp.identical_streak = out->macro.stall_n;
        tmp.macro_conf = out->macro.macro_conf;
        ikitaku_circle_map_concept(&tmp, allow_teach, &out->circle_concept);
    }

    snprintf(out->english, sizeof out->english,
             "reflect: %s | abstract DOWN tip=%s r5=%s | goal_rel=%d | %s",
             out->macro.english, out->drop.broader,
             out->drop.abstract_r5[0] ? out->drop.abstract_r5 : "-",
             out->macro.relates_goal,
             out->circle_concept.mapped ? out->circle_concept.english : "no-circle-concept");
    return out->reflected;
}



void ikitaku_expr_init(ikitaku_expr_tree_t* t) {
    if (!t) return;
    memset(t, 0, sizeof *t);
    t->root = -1;
}

int ikitaku_expr_leaf(ikitaku_expr_tree_t* t, const char* form, const char* data,
                      const char* context, int layer) {
    if (!t || t->n >= (int)(sizeof t->node / sizeof t->node[0])) return -1;
    int i = t->n++;
    ikitaku_expr_bag_t* b = &t->node[i];
    memset(b, 0, sizeof *b);
    snprintf(b->form, sizeof b->form, "%s", form ? form : "map");
    snprintf(b->data, sizeof b->data, "%s", data ? data : "");
    snprintf(b->context, sizeof b->context, "%s", context ? context : "");
    b->layer = layer;
    b->child_idx = -1;
    b->n_slot = 0;
    b->may_have_parents = 1;
    b->has_parents = 0;
    b->n_parent = 0;
    for (int pi = 0; pi < IKITAKU_EXPR_PARENTS; ++pi) b->parent[pi] = -1;
    b->parent_idx = -1;
    b->open = 0;
    b->unity = 50;
    b->composed_c = 0;
    b->remembered[0] = 0;
    snprintf(b->english, sizeof b->english, "F=%s(x=%s)+c[%s] L%d leaf",
             b->form, b->data[0]?b->data:"_", b->context[0]?b->context:"_", layer);
    if (t->root < 0) t->root = i;
    return i;
}

int ikitaku_expr_wrap(ikitaku_expr_tree_t* t, int child_idx, const char* form,
                      const char* context, int layer) {
    if (!t || child_idx < 0 || child_idx >= t->n) return -1;
    if (t->n >= (int)(sizeof t->node / sizeof t->node[0])) return -1;
    int i = t->n++;
    ikitaku_expr_bag_t* b = &t->node[i];
    memset(b, 0, sizeof *b);
    snprintf(b->form, sizeof b->form, "%s", form ? form : "map");
    snprintf(b->context, sizeof b->context, "%s", context ? context : "");
    b->layer = layer;
    b->child_idx = child_idx;
    b->n_slot = 0;
    b->may_have_parents = 1;
    b->has_parents = 0;
    b->n_parent = 0;
    for (int pi = 0; pi < IKITAKU_EXPR_PARENTS; ++pi) b->parent[pi] = -1;
    b->parent_idx = -1;
    b->open = 0; /* closed: interior is one unit */
    b->unity = 70;
    if (child_idx >= 0 && child_idx < t->n) {
        /* link child → this parent (tags + ptr) */
        ikitaku_expr_bag_t* ch = &t->node[child_idx];
        if (ch->may_have_parents && ch->n_parent < IKITAKU_EXPR_PARENTS) {
            ch->parent[ch->n_parent++] = i;
            ch->parent_idx = ch->parent[0];
            ch->has_parents = 1;
        }
        if (b->n_slot < IKITAKU_EXPR_SLOTS)
            b->slot[b->n_slot++] = child_idx;
    }
    /* data inherits child's form as nested x */
    snprintf(b->data, sizeof b->data, "%s", t->node[child_idx].form);
    snprintf(b->english, sizeof b->english,
             "F=%s( F_inner=%s )+c[%s] L%d closed (anti-derived wrap)",
             b->form, t->node[child_idx].form,
             b->context[0]?b->context:"_", layer);
    t->root = i;
    return i;
}

int ikitaku_expr_open(ikitaku_expr_tree_t* t, int idx) {
    if (!t || idx < 0 || idx >= t->n) return -1;
    ikitaku_expr_bag_t* b = &t->node[idx];
    if (b->child_idx < 0) {
        snprintf(b->english, sizeof b->english,
                 "open F=%s: leaf — no interior (already micro)", b->form);
        b->open = 1;
        return idx;
    }
    b->open = 1;
    /* focus shifts to interior: derivative into nested expression */
    snprintf(b->english, sizeof b->english,
             "OPEN derive F=%s → interior F=%s(x=%s)+c[%s] (micro focus)",
             b->form, t->node[b->child_idx].form,
             t->node[b->child_idx].data[0]?t->node[b->child_idx].data:"_",
             t->node[b->child_idx].context[0]?t->node[b->child_idx].context:"_");
    return b->child_idx;
}

int ikitaku_expr_close(ikitaku_expr_tree_t* t, int idx) {
    if (!t || idx < 0 || idx >= t->n) return -1;
    ikitaku_expr_bag_t* b = &t->node[idx];
    b->open = 0;
    snprintf(b->english, sizeof b->english,
             "CLOSE anti-derive F=%s(x=%s)+c[%s] as unit (macro focus)",
             b->form, b->data[0]?b->data:"_", b->context[0]?b->context:"_");
    return idx;
}

int ikitaku_expr_reason(ikitaku_expr_tree_t* t, int idx, int dir, int steps,
                        ikitaku_layer_drop_t* flow, char english[320]) {
    if (!t || idx < 0 || idx >= t->n) return -1;
    ikitaku_expr_bag_t* b = &t->node[idx];
    ikitaku_layer_drop_t local;
    if (!flow) flow = &local;
    int n = ikitaku_layer_flow(b->form, b->layer, steps > 0 ? steps : 2, dir, flow);
    flow->used_for_metacog = 1;

    /* If open and has child, also note interior */
    char inner[120] = "leaf";
    if (b->open && b->child_idx >= 0) {
        ikitaku_expr_bag_t* c = &t->node[b->child_idx];
        snprintf(inner, sizeof inner, "OPEN interior F=%s+c[%s]",
                 c->form, c->context[0]?c->context:"_");
    } else if (b->child_idx >= 0) {
        snprintf(inner, sizeof inner, "CLOSED nest F=%s", t->node[b->child_idx].form);
    }

    if (english)
        snprintf(english, 320,
                 "expr-reason F=%s x=%s c[%s] L%d %s dir=%s tip=%s r5=%s | %s | %s",
                 b->form, b->data[0]?b->data:"_", b->context[0]?b->context:"_",
                 b->layer, b->open ? "open" : "closed",
                 dir == IKITAKU_FLOW_DOWN ? "DOWN/derive" : "UP/anti-derive",
                 flow->broader, flow->abstract_r5[0]?flow->abstract_r5:"-",
                 inner, flow->english);
    if (t)
        snprintf(t->english, sizeof t->english, "%s", english ? english : flow->english);
    return n;
}

int ikitaku_subject_as_bag(const char* subject, const char* verb,
                           const char* descriptor, ikitaku_expr_tree_t* t) {
    if (!t || !subject) return -1;
    ikitaku_expr_init(t);
    /* Subject = bag. Verb = transform held inside. Descriptor = how. */
    int leaf = ikitaku_expr_leaf(t, verb && verb[0] ? verb : "info",
                                 subject, descriptor ? descriptor : "subject-bag", 4);
    if (leaf < 0) return -1;
    ikitaku_expr_bag_t* b = &t->node[leaf];
    if (verb) snprintf(b->verb, sizeof b->verb, "%s", verb);
    if (descriptor) snprintf(b->descriptor, sizeof b->descriptor, "%s", descriptor);
    snprintf(b->english, sizeof b->english,
             "subject-bag '%s' holds verb=%s descriptor=%s (check bag for transform)",
             subject, b->verb[0]?b->verb:"info",
             b->descriptor[0]?b->descriptor:"_");
    snprintf(t->english, sizeof t->english, "%s", b->english);
    return leaf;
}


int ikitaku_expr_add_slot(ikitaku_expr_tree_t* t, int parent_idx, int child_idx) {
    if (!t || parent_idx < 0 || parent_idx >= t->n) return -1;
    if (child_idx < 0 || child_idx >= t->n) return -1;
    ikitaku_expr_bag_t* p = &t->node[parent_idx];
    if (p->n_slot >= IKITAKU_EXPR_SLOTS) return -1;
    p->slot[p->n_slot++] = child_idx;
    ikitaku_expr_link_parent(t, child_idx, parent_idx);
    if (p->child_idx < 0) p->child_idx = child_idx;
    snprintf(p->english, sizeof p->english,
             "F=%s slots=%d (+%s) L%d", p->form, p->n_slot,
             t->node[child_idx].form, p->layer);
    return p->n_slot;
}

int ikitaku_expr_set_may_have_parents(ikitaku_expr_tree_t* t, int idx, int may) {
    if (!t || idx < 0 || idx >= t->n) return -1;
    t->node[idx].may_have_parents = may ? 1 : 0;
    if (!may && t->node[idx].has_parents) {
        /* capability removed: clear links */
        t->node[idx].has_parents = 0;
        t->node[idx].n_parent = 0;
        t->node[idx].parent_idx = -1;
        for (int i = 0; i < IKITAKU_EXPR_PARENTS; ++i)
            t->node[idx].parent[i] = -1;
    }
    return t->node[idx].may_have_parents;
}

int ikitaku_expr_link_parent(ikitaku_expr_tree_t* t, int child_idx, int parent_idx) {
    if (!t || child_idx < 0 || parent_idx < 0) return -1;
    if (child_idx >= t->n || parent_idx >= t->n) return -1;
    ikitaku_expr_bag_t* ch = &t->node[child_idx];
    if (!ch->may_have_parents) return -2; /* tagged may_have_parents=0 */
    /* already linked? */
    for (int i = 0; i < ch->n_parent; ++i)
        if (ch->parent[i] == parent_idx) {
            ch->has_parents = 1;
            ch->parent_idx = ch->parent[0];
            return ch->n_parent;
        }
    if (ch->n_parent >= IKITAKU_EXPR_PARENTS) return -3;
    ch->parent[ch->n_parent++] = parent_idx;
    ch->parent_idx = ch->parent[0];
    ch->has_parents = 1;
    return ch->n_parent;
}

int ikitaku_expr_parent_tags(const ikitaku_expr_tree_t* t, int idx,
                             int* may_out, int* has_out,
                             int parents_out[IKITAKU_EXPR_PARENTS],
                             int* n_parent_out, char english[200]) {
    if (!t || idx < 0 || idx >= t->n) return -1;
    const ikitaku_expr_bag_t* b = &t->node[idx];
    if (may_out) *may_out = b->may_have_parents;
    if (has_out) *has_out = b->has_parents;
    if (n_parent_out) *n_parent_out = b->n_parent;
    if (parents_out) {
        for (int i = 0; i < IKITAKU_EXPR_PARENTS; ++i)
            parents_out[i] = (i < b->n_parent) ? b->parent[i] : -1;
    }
    if (english) {
        char plist[80] = "";
        size_t pl = 0;
        for (int i = 0; i < b->n_parent; ++i) {
            int n = snprintf(plist + pl, sizeof plist - pl, "%s%d:%s",
                             i ? "," : "", b->parent[i],
                             (b->parent[i] >= 0 && b->parent[i] < t->n)
                                 ? t->node[b->parent[i]].form : "?");
            if (n > 0) pl += (size_t)n;
        }
        snprintf(english, 200,
                 "bag '%s' may_have_parents=%d has_parents=%d n=%d ptrs=[%s]",
                 b->form, b->may_have_parents, b->has_parents, b->n_parent,
                 plist[0] ? plist : "-");
    }
    return b->n_parent;
}


int ikitaku_expr_slot_unity(ikitaku_expr_tree_t* t, int a_idx, int b_idx,
                            int* unity_out, char english[160]) {
    if (!t || a_idx < 0 || b_idx < 0 || a_idx >= t->n || b_idx >= t->n) return -1;
    ikitaku_expr_bag_t *a = &t->node[a_idx], *b = &t->node[b_idx];
    ikitaku_lie_t lie;
    memset(&lie, 0, sizeof lie);
    ikitaku_lie_bracket(a->form, b->form, &lie);
    int u = lie.unity;
    /* same layer boost; shared context boost */
    if (a->layer == b->layer) u = (u + 10 > 100) ? 100 : u + 10;
    if (a->context[0] && b->context[0] && !strcmp(a->context, b->context))
        u = (u + 8 > 100) ? 100 : u + 8;
    if (u < 0) u = 0;
    if (u > 100) u = 100;
    a->unity = u;
    b->unity = u;
    if (unity_out) *unity_out = u;
    if (english)
        snprintf(english, 160,
                 "slot-unity %s↔%s unity=%d lie_mag=%d layer %d/%d",
                 a->form, b->form, u, lie.mag, a->layer, b->layer);
    return u;
}

static int map_seen(ikitaku_expr_map_t* m, int idx) {
    for (int i = 0; i < m->n_visited; ++i)
        if (m->visited[i] == idx) return 1;
    return 0;
}

static void map_add(ikitaku_expr_map_t* m, int idx, const char* label) {
    if (!m || m->n_visited >= (int)(sizeof m->visited / sizeof m->visited[0])) return;
    if (map_seen(m, idx)) return;
    m->visited[m->n_visited++] = idx;
    if (m->n_path < 12)
        snprintf(m->path[m->n_path++], 40, "%s", label ? label : "?");
}

int ikitaku_expr_map_vertical(ikitaku_expr_tree_t* t, int start_idx, int dir,
                              int max_depth, ikitaku_expr_map_t* out) {
    if (!t || !out || start_idx < 0 || start_idx >= t->n) return -1;
    memset(out, 0, sizeof *out);
    out->min_unity = 100;
    out->max_unity = 0;
    if (max_depth < 1) max_depth = 1;
    if (max_depth > IKITAKU_EXPR_DEPTH) max_depth = IKITAKU_EXPR_DEPTH;

    int idx = start_idx;
    for (int d = 0; d < max_depth && idx >= 0 && idx < t->n; ++d) {
        ikitaku_expr_bag_t* b = &t->node[idx];
        char lab[40];
        snprintf(lab, sizeof lab, "V%d:L%d:%s", d, b->layer, b->form);
        map_add(out, idx, lab);
        out->n_vert++;

        ikitaku_layer_drop_t flow;
        ikitaku_layer_flow(b->form, b->layer, 1, dir, &flow);

        if (b->unity < out->min_unity) out->min_unity = b->unity;
        if (b->unity > out->max_unity) out->max_unity = b->unity;

        if (dir == IKITAKU_FLOW_DOWN) {
            /* derive: open and enter primary child or first slot */
            ikitaku_expr_open(t, idx);
            int next = b->child_idx;
            if (next < 0 && b->n_slot > 0) next = b->slot[0];
            if (next < 0) break;
            idx = next;
        } else {
            /* anti-derive: climb parent */
            if (b->parent_idx < 0) break;
            idx = b->parent_idx;
            ikitaku_expr_close(t, idx);
        }
    }
    if (out->n_visited > 0)
        out->avg_unity = (out->min_unity + out->max_unity) / 2;
    snprintf(out->english, sizeof out->english,
             "map-vertical dir=%s visited=%d vert_edges=%d unity[%d..%d] path=",
             dir == IKITAKU_FLOW_DOWN ? "DOWN/derive" : "UP/anti-derive",
             out->n_visited, out->n_vert, out->min_unity, out->max_unity);
    size_t len = strlen(out->english);
    for (int i = 0; i < out->n_path && len + 42 < sizeof out->english; ++i) {
        int n = snprintf(out->english + len, sizeof out->english - len,
                         "%s%s", i ? "→" : "", out->path[i]);
        if (n > 0) len += (size_t)n;
    }
    return out->n_visited;
}

int ikitaku_expr_map_horizontal(ikitaku_expr_tree_t* t, int parent_idx,
                                ikitaku_expr_map_t* out) {
    if (!t || !out || parent_idx < 0 || parent_idx >= t->n) return -1;
    memset(out, 0, sizeof *out);
    out->min_unity = 100;
    out->max_unity = 0;
    ikitaku_expr_bag_t* p = &t->node[parent_idx];
    char lab[40];
    snprintf(lab, sizeof lab, "H0:L%d:%s", p->layer, p->form);
    map_add(out, parent_idx, lab);

    int sum_u = 0, n_u = 0;
    for (int i = 0; i < p->n_slot; ++i) {
        int s = p->slot[i];
        if (s < 0 || s >= t->n) continue;
        snprintf(lab, sizeof lab, "Hslot%d:%s", i, t->node[s].form);
        map_add(out, s, lab);
        /* peer unity with previous slot */
        if (i > 0) {
            int u = 0;
            char eu[160];
            ikitaku_expr_slot_unity(t, p->slot[i - 1], s, &u, eu);
            out->n_horiz++;
            sum_u += u;
            n_u++;
            if (u < out->min_unity) out->min_unity = u;
            if (u > out->max_unity) out->max_unity = u;
        }
    }
    if (n_u > 0) out->avg_unity = sum_u / n_u;
    else {
        out->min_unity = p->unity;
        out->max_unity = p->unity;
        out->avg_unity = p->unity;
    }
    snprintf(out->english, sizeof out->english,
             "map-horizontal parent=%s slots=%d peer_compares=%d unity avg=%d [%d..%d] path=",
             p->form, p->n_slot, out->n_horiz, out->avg_unity,
             out->min_unity, out->max_unity);
    size_t len = strlen(out->english);
    for (int i = 0; i < out->n_path && len + 42 < sizeof out->english; ++i) {
        int n = snprintf(out->english + len, sizeof out->english - len,
                         "%s%s", i ? " | " : "", out->path[i]);
        if (n > 0) len += (size_t)n;
    }
    return out->n_visited;
}

int ikitaku_expr_map_tree(ikitaku_expr_tree_t* t, int start_idx, int dir,
                          int max_depth, ikitaku_expr_map_t* out) {
    if (!t || !out || start_idx < 0 || start_idx >= t->n) return -1;
    memset(out, 0, sizeof *out);
    out->min_unity = 100;
    out->max_unity = 0;
    if (max_depth < 1) max_depth = 2;
    if (max_depth > IKITAKU_EXPR_DEPTH) max_depth = IKITAKU_EXPR_DEPTH;

    /* recursive vertical stack of indices */
    int stack[IKITAKU_EXPR_DEPTH + 2];
    int sp = 0;
    stack[sp++] = start_idx;

    while (sp > 0 && out->n_visited < 24) {
        int idx = stack[--sp];
        if (map_seen(out, idx)) continue;
        ikitaku_expr_bag_t* b = &t->node[idx];
        char lab[40];
        snprintf(lab, sizeof lab, "T:L%d:%s", b->layer, b->form);
        map_add(out, idx, lab);

        /* horizontal at this node */
        if (b->n_slot >= 2) {
            ikitaku_expr_map_t hz;
            ikitaku_expr_map_horizontal(t, idx, &hz);
            out->n_horiz += hz.n_horiz;
            if (hz.min_unity < out->min_unity) out->min_unity = hz.min_unity;
            if (hz.max_unity > out->max_unity) out->max_unity = hz.max_unity;
            for (int i = 0; i < hz.n_visited; ++i)
                map_add(out, hz.visited[i], NULL);
        }

        /* vertical: push children (slots + primary) for DOWN, parent for UP */
        if (dir == IKITAKU_FLOW_DOWN) {
            out->n_vert++;
            for (int i = b->n_slot - 1; i >= 0; --i) {
                int s = b->slot[i];
                if (s >= 0 && s < t->n && !map_seen(out, s) && sp < IKITAKU_EXPR_DEPTH + 1)
                    stack[sp++] = s;
            }
            if (b->child_idx >= 0 && !map_seen(out, b->child_idx) &&
                sp < IKITAKU_EXPR_DEPTH + 1)
                stack[sp++] = b->child_idx;
        } else if (b->parent_idx >= 0 && !map_seen(out, b->parent_idx) &&
                   sp < IKITAKU_EXPR_DEPTH + 1) {
            out->n_vert++;
            stack[sp++] = b->parent_idx;
        }
        (void)max_depth;
    }
    if (out->min_unity > out->max_unity) {
        out->min_unity = 50;
        out->max_unity = 50;
    }
    out->avg_unity = (out->min_unity + out->max_unity) / 2;
    snprintf(out->english, sizeof out->english,
             "map-tree dir=%s visited=%d vert=%d horiz=%d unity avg=%d [%d..%d] path=",
             dir == IKITAKU_FLOW_DOWN ? "DOWN" : "UP",
             out->n_visited, out->n_vert, out->n_horiz, out->avg_unity,
             out->min_unity, out->max_unity);
    size_t len = strlen(out->english);
    for (int i = 0; i < out->n_path && len + 42 < sizeof out->english; ++i) {
        int n = snprintf(out->english + len, sizeof out->english - len,
                         "%s%s", i ? "→" : "", out->path[i]);
        if (n > 0) len += (size_t)n;
    }
    snprintf(t->english, sizeof t->english, "%s", out->english);
    return out->n_visited;
}


int ikitaku_expr_compose_c(ikitaku_expr_tree_t* t, int idx,
                           const char* prop_a, const char* prop_b) {
    if (!t || idx < 0 || idx >= t->n) return -1;
    ikitaku_expr_bag_t* b = &t->node[idx];
    const char* a = (prop_a && prop_a[0]) ? prop_a : b->context;
    const char* c2 = (prop_b && prop_b[0]) ? prop_b : "";
    char composed[96];
    if (a[0] && c2[0])
        snprintf(composed, sizeof composed, "(%s)+(%s)", a, c2);
    else if (c2[0])
        snprintf(composed, sizeof composed, "%s", c2);
    else if (a[0])
        snprintf(composed, sizeof composed, "%s", a);
    else
        snprintf(composed, sizeof composed, "_");
    snprintf(b->context, sizeof b->context, "%s", composed);
    b->composed_c = (a[0] && c2[0]) ? 1 : b->composed_c;
    /* refresh remember line */
    snprintf(b->remembered, sizeof b->remembered, "F=%s(x=%s)+c[%s]",
             b->form, b->data[0]?b->data:"_", b->context);
    snprintf(b->english, sizeof b->english,
             "compose +c: %s composed=%d", b->remembered, b->composed_c);
    return b->composed_c;
}

int ikitaku_expr_remember(ikitaku_expr_tree_t* t, int idx) {
    if (!t || idx < 0 || idx >= t->n) return -1;
    ikitaku_expr_bag_t* b = &t->node[idx];
    if (b->child_idx >= 0 && b->child_idx < t->n && !b->open) {
        /* closed nest: remember outer F(inner)+c */
        ikitaku_expr_bag_t* ch = &t->node[b->child_idx];
        snprintf(b->remembered, sizeof b->remembered,
                 "F=%s(F=%s(x=%s)+c[%s])+c[%s]",
                 b->form, ch->form, ch->data[0]?ch->data:"_",
                 ch->context[0]?ch->context:"_",
                 b->context[0]?b->context:"_");
    } else {
        snprintf(b->remembered, sizeof b->remembered, "F=%s(x=%s)+c[%s]",
                 b->form, b->data[0]?b->data:"_",
                 b->context[0]?b->context:"_");
    }
    snprintf(b->english, sizeof b->english, "remember property: %s", b->remembered);
    return 0;
}

int ikitaku_expr_expand_c(ikitaku_expr_tree_t* t, int idx,
                          ikitaku_expr_tree_t* out_slots, char english[280]) {
    if (!t || idx < 0 || idx >= t->n) return -1;
    ikitaku_expr_bag_t* b = &t->node[idx];
    if (out_slots) ikitaku_expr_init(out_slots);

    /* Macro-expand context into property slots.
       Split on ")+(" from compose, or single c as one slot. */
    const char* ctx = b->context[0] ? b->context : "";
    int n_props = 0;
    char props[4][48];
    memset(props, 0, sizeof props);

    if (ctx[0] == '(' && strstr(ctx, ")+(")) {
        /* (a)+(b)+... style */
        const char* p = ctx;
        while (p && *p && n_props < 4) {
            while (*p == '(' || *p == '+') p++;
            if (*p == 0) break;
            const char* end = p;
            while (*end && *end != ')') end++;
            size_t len = (size_t)(end - p);
            if (len >= sizeof props[0]) len = sizeof props[0] - 1;
            memcpy(props[n_props], p, len);
            props[n_props][len] = 0;
            n_props++;
            p = (*end == ')') ? end + 1 : end;
        }
    } else if (ctx[0]) {
        snprintf(props[0], sizeof props[0], "%s", ctx);
        n_props = 1;
    }

    if (out_slots) {
        for (int i = 0; i < n_props; ++i) {
            int leaf = ikitaku_expr_leaf(out_slots, "info", props[i],
                                         "expanded-c", b->layer > 0 ? b->layer - 1 : 0);
            if (leaf >= 0) {
                ikitaku_expr_bag_t* s = &out_slots->node[leaf];
                snprintf(s->descriptor, sizeof s->descriptor, "property");
                snprintf(s->remembered, sizeof s->remembered, "+c[%s]", props[i]);
            }
        }
        if (out_slots->n > 0 && n_props > 1) {
            /* wrap under same F for macro unit */
            int w = ikitaku_expr_wrap(out_slots, 0, b->form, "expanded-props", b->layer);
            for (int i = 1; i < out_slots->n && i < n_props; ++i)
                ikitaku_expr_add_slot(out_slots, w >= 0 ? w : 0, i);
        }
    }

    if (english) {
        char list[120] = "";
        size_t ll = 0;
        for (int i = 0; i < n_props; ++i) {
            int n = snprintf(list + ll, sizeof list - ll, "%s+c[%s]",
                             i ? " " : "", props[i]);
            if (n > 0) ll += (size_t)n;
        }
        snprintf(english, 280,
                 "expand +c of F=%s(x=%s): %d propert%s %s | remembered=%s",
                 b->form, b->data[0]?b->data:"_", n_props,
                 n_props == 1 ? "y" : "ies",
                 list[0] ? list : "(none)",
                 b->remembered[0] ? b->remembered : "(call remember first)");
    }
    return n_props;
}

int ikitaku_expr_property(const ikitaku_expr_tree_t* t, int idx,
                          char property[128], char english[200]) {
    if (!t || idx < 0 || idx >= t->n) return -1;
    const ikitaku_expr_bag_t* b = &t->node[idx];
    if (property) {
        if (b->remembered[0])
            snprintf(property, 128, "%s", b->remembered);
        else
            snprintf(property, 128, "F=%s(x=%s)+c[%s]",
                     b->form, b->data[0]?b->data:"_",
                     b->context[0]?b->context:"_");
    }
    if (english)
        snprintf(english, 200,
                 "property bag='%s' composed_c=%d may_parent=%d has_parent=%d | %s",
                 b->form, b->composed_c, b->may_have_parents, b->has_parents,
                 property ? property : b->remembered);
    return 0;
}


int ikitaku_pattern_abstract(ikitaku_expr_tree_t* t, int idx, int n_down,
                             ikitaku_pattern_step_t* out) {
    if (!t || !out || idx < 0 || idx >= t->n) return -1;
    memset(out, 0, sizeof *out);
    out->idx = idx;
    out->dir = IKITAKU_FLOW_DOWN;
    ikitaku_expr_bag_t* b = &t->node[idx];
    out->layer_before = b->layer;
    snprintf(out->form_before, sizeof out->form_before, "%s", b->form);

    /* Remember full F(x)+c before stripping focus */
    ikitaku_expr_remember(t, idx);
    snprintf(out->remembered, sizeof out->remembered, "%s", b->remembered);

    /* Derive: open interior if nested — micro focus on structure */
    if (b->child_idx >= 0 || b->n_slot > 0)
        ikitaku_expr_open(t, idx);

    /* Flow form toward platonic (macro pattern of the *form*) */
    ikitaku_layer_drop_t flow;
    int steps = n_down > 0 ? n_down : 2;
    ikitaku_layer_flow(b->form, b->layer, steps, IKITAKU_FLOW_DOWN, &flow);
    snprintf(out->form_after, sizeof out->form_after, "%s", flow.broader);
    snprintf(out->r5, sizeof out->r5, "%s",
             flow.abstract_r5[0] ? flow.abstract_r5 : flow.broader);
    out->layer_after = flow.to_layer;

    /* Optionally retarget bag form tip to abstract for subsequent reason */
    if (flow.abstract_r5[0]) {
        /* keep original form on bag; patterning reports abstract tip */
    }

    snprintf(out->english, sizeof out->english,
             "ABSTRACT derive F=%s→%s L%d→L%d r5=%s remembered=%s open=%d",
             out->form_before, out->form_after, out->layer_before, out->layer_after,
             out->r5, out->remembered[0]?out->remembered:"_", b->open);
    return 0;
}

int ikitaku_pattern_unabstract(ikitaku_expr_tree_t* t, int idx, int n_up,
                               const char* restore_c,
                               ikitaku_pattern_step_t* out) {
    if (!t || !out || idx < 0 || idx >= t->n) return -1;
    memset(out, 0, sizeof *out);
    out->idx = idx;
    out->dir = IKITAKU_FLOW_UP;
    ikitaku_expr_bag_t* b = &t->node[idx];
    out->layer_before = b->layer;
    snprintf(out->form_before, sizeof out->form_before, "%s", b->form);

    /* Anti-derive: close as unit — macro focus */
    ikitaku_expr_close(t, idx);

    /* Restore / re-apply +c context if provided */
    if (restore_c && restore_c[0]) {
        if (b->context[0])
            ikitaku_expr_compose_c(t, idx, b->context, restore_c);
        else {
            snprintf(b->context, sizeof b->context, "%s", restore_c);
            b->composed_c = 0;
        }
    }
    ikitaku_expr_remember(t, idx);
    snprintf(out->remembered, sizeof out->remembered, "%s", b->remembered);

    /* Flow toward micro specificity */
    ikitaku_layer_drop_t flow;
    int steps = n_up > 0 ? n_up : 2;
    int from = b->layer;
    /* start from abstract tip if form is already platonic-ish */
    ikitaku_layer_flow(b->form, from, steps, IKITAKU_FLOW_UP, &flow);
    snprintf(out->form_after, sizeof out->form_after, "%s", flow.broader);
    snprintf(out->r5, sizeof out->r5, "%s",
             flow.abstract_r5[0] ? flow.abstract_r5 : b->form);
    out->layer_after = flow.to_layer;

    /* Expand +c when context is composed — count properties */
    if (b->composed_c || (b->context[0] && strstr(b->context, ")+("))) {
        char e2[280];
        out->n_props_expanded = ikitaku_expr_expand_c(t, idx, NULL, e2);
    }

    snprintf(out->english, sizeof out->english,
             "UNABSTRACT anti-derive F=%s→%s L%d→L%d +c=%s props=%d remembered=%s",
             out->form_before, out->form_after, out->layer_before, out->layer_after,
             b->context[0]?b->context:"_", out->n_props_expanded,
             out->remembered[0]?out->remembered:"_");
    return 0;
}

int ikitaku_pattern_run(ikitaku_expr_tree_t* t, int idx, int n_down, int n_up,
                        ikitaku_abs_pattern_t* out) {
    if (!t || !out || idx < 0 || idx >= t->n) return -1;
    memset(out, 0, sizeof *out);
    ikitaku_expr_bag_t* b = &t->node[idx];

    /* Classify structural pattern before walk */
    if (b->child_idx >= 0 && b->n_slot >= 2)
        snprintf(out->pattern, sizeof out->pattern, "nested");
    else if (b->child_idx >= 0)
        snprintf(out->pattern, sizeof out->pattern, "nested");
    else if (b->n_slot >= 2)
        snprintf(out->pattern, sizeof out->pattern, "mixed");
    else
        snprintf(out->pattern, sizeof out->pattern, "leaf");

    char saved_c[48];
    snprintf(saved_c, sizeof saved_c, "%s", b->context);

    if (ikitaku_pattern_abstract(t, idx, n_down, &out->abstract) != 0)
        return -1;
    ikitaku_layer_flow(out->abstract.form_before, out->abstract.layer_before,
                       n_down > 0 ? n_down : 2, IKITAKU_FLOW_DOWN, &out->flow_down);

    /* cycle family detection on abstract r5 */
    if (!strcmp(out->abstract.r5, "repeat") ||
        strstr(out->abstract.form_before, "loop") ||
        strstr(out->abstract.form_before, "cycle"))
        snprintf(out->pattern, sizeof out->pattern, "cycle");

    if (ikitaku_pattern_unabstract(t, idx, n_up, NULL, &out->unabstract) != 0)
        return -1;
    ikitaku_layer_flow(out->unabstract.form_before, out->unabstract.layer_before,
                       n_up > 0 ? n_up : 2, IKITAKU_FLOW_UP, &out->flow_up);

    out->ok = 1;
    snprintf(out->english, sizeof out->english,
             "pattern-run pattern=%s | %s | %s | down_tip=%s up_tip=%s",
             out->pattern, out->abstract.english, out->unabstract.english,
             out->flow_down.broader, out->flow_up.broader);
    return 0;
}

int ikitaku_reason_shift_any(const char* form, int kind, int from_layer,
                             ikitaku_reason_shift_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!form || !form[0]) form = "map";
    snprintf(out->seed, sizeof out->seed, "%s", form);
    out->kind = (uint8_t)kind;
    if (from_layer < 0)
        from_layer = ikitaku_form_layer_guess(form);
    if (from_layer < 0) from_layer = 5;
    if (from_layer > 6) from_layer = 6;
    out->from_layer = from_layer;

    int nd = ikitaku_layer_flow(form, from_layer,
                                from_layer > 0 ? from_layer : 1,
                                IKITAKU_FLOW_DOWN, &out->down);
    out->can_down = (nd > 0 && out->down.to_layer <= out->down.from_layer);

    int start_up = from_layer < 6 ? from_layer : 5;
    int nu = ikitaku_layer_flow(form, start_up, 6 - start_up,
                               IKITAKU_FLOW_UP, &out->up);
    out->can_up = (nu > 0 && out->up.to_layer >= out->up.from_layer);

    const char* kn = kind == 1 ? "METHOD" : kind == 2 ? "SYNTHESIS" : "DATA";
    snprintf(out->english, sizeof out->english,
             "shift-any kind=%s seed=%s from=R%d DOWN→R%d/%s UP→R%d/%s",
             kn, form, from_layer,
             out->down.to_layer,
             out->down.abstract_r5[0] ? out->down.abstract_r5 :
                 (out->down.broader[0] ? out->down.broader : "-"),
             out->up.to_layer,
             out->up.broader[0] ? out->up.broader : "-");
    return (out->can_down || out->can_up) ? 0 : -1;
}

int ikitaku_reason_shift_hypo(const ikitaku_hypo_t* h, int kind,
                              ikitaku_reason_shift_t* out) {
    if (!h || !out) return -1;
    const char* f = h->form_id[0] ? h->form_id : h->purpose;
    int layer = ikitaku_form_layer_guess(f);
    return ikitaku_reason_shift_any(f, kind, layer, out);
}

int ikitaku_approach_shift(const char* last_lead, int residual, int desperation,
                           int predicted_hard, int actually_dropped,
                           ikitaku_approach_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!last_lead || !last_lead[0]) last_lead = "hegelian";
    snprintf(out->last_lead, sizeof out->last_lead, "%s", last_lead);
    if (residual < 0) residual = 0;
    if (residual > 100) residual = 100;
    if (desperation < 0) desperation = 0;
    if (desperation > 10) desperation = 10;
    out->residual = residual;
    out->desperation = desperation;

    /* 1. failed / leftover = antithesis of last lead */
    ikitaku_hypo_t fail, anti;
    ikitaku_hypo_fill(&fail, "theorize", last_lead, 100 - residual, 0);
    ikitaku_anti_synthesis(&fail, &anti);
    snprintf(out->anti_lead, sizeof out->anti_lead, "%s",
             anti.purpose[0] ? anti.purpose : "invert");

    /* 2. lazy pool: likely methods + anti-methods */
    const char* LIKELY[] = {"occam","scientific","iterative","kant"};
    const char* ANTI[] = {0,0,0,0};
    ANTI[0] = out->anti_lead[0] ? out->anti_lead : "invert";
    ANTI[1] = "origin";
    ANTI[2] = "waterfall";
    ANTI[3] = strcmp(last_lead, "hegelian") ? "hegelian" : "bound";
    out->n_likely = 4;
    out->n_anti = 4;
    for (int i = 0; i < 4; ++i) {
        snprintf(out->likely[i], 24, "%s", LIKELY[i]);
        snprintf(out->anti_m[i], 24, "%s", ANTI[i]);
    }

    /* 7. surprise-positive: predicted hard but residual dropped a lot */
    if (predicted_hard && actually_dropped >= 25)
        out->surprise_pos = 1;
    if (out->surprise_pos) {
        char rule[80];
        snprintf(rule, sizeof rule, "surprise+ %s drop=%d", last_lead, actually_dropped);
        if (ikitaku_xp_record(rule, "theorize", "method",
                              actually_dropped, 80) == 0)
            out->xp_wrote = 1;
        ikitaku_strategy_score(last_lead, 80, 80 - actually_dropped, NULL);
    }

    /* 8 + calm → Occam. High residual + not surprise → not Occam first */
    out->occam_bias = (residual <= 35 || out->surprise_pos) ? 1 : 0;

    /* 6. novelty urge when desperate */
    out->novelty_seek = (desperation >= 6 || residual >= 70) ? 1 : 0;

    /* 3. inhibit last on loss (residual still high) */
    out->inhibited = (residual >= 30) ? 1 : 0;

    /* micro = this method's hold; macro = strategy-space health */
    out->micro_conf = 100 - residual;
    out->macro_conf = out->novelty_seek ? (40 + desperation) : (70 - residual / 3);
    if (out->macro_conf < 15) out->macro_conf = 15;
    if (out->macro_conf > 95) out->macro_conf = 95;

    /* 2. lazy pick: likely method vs anti-method */
    const char* pool[] = {
        "occam", "origin", "scientific", "hegelian", "iterative", "kant", "waterfall"
    };
    const char* pick = out->occam_bias ? "occam" :
                       (out->novelty_seek ? "origin" : "scientific");
    if (out->inhibited && !strcmp(pick, last_lead))
        pick = out->occam_bias ? "scientific" : "origin";
    if (out->inhibited && !strcmp(pick, last_lead))
        pick = "iterative";

    /* 4. selective recomb if confused (high residual + mid desperation) */
    if (residual >= 45 && desperation >= 3) {
        const char* b = out->anti_lead[0] ? out->anti_lead : "origin";
        if (!strcmp(b, "invert") || !strcmp(b, last_lead))
            b = out->novelty_seek ? "origin" : "occam";
        snprintf(out->recomb, sizeof out->recomb, "%s_x_%s", pick, b);
        pick = out->recomb;
    }
    snprintf(out->next_lead, sizeof out->next_lead, "%s", pick);

    /* 5. parallel Hegel branches on last vs next (not greedy single) */
    ikitaku_hypo_t T, N;
    ikitaku_hypo_fill(&T, "theorize", last_lead, out->micro_conf, 1);
    ikitaku_hypo_fill(&N, "theorize", out->next_lead, out->macro_conf, 1);
    ikitaku_dialectic_recurse(&T, &N, 2, &out->branches);

    /* multi-layer: down + up from last lead (width = layer) */
    int L = ikitaku_form_layer_guess(last_lead);
    if (L < 2) L = 4;
    ikitaku_layer_flow(last_lead, L, L > 0 ? L : 2, IKITAKU_FLOW_DOWN, &out->down);
    ikitaku_layer_flow(last_lead, L < 6 ? L : 5, 2, IKITAKU_FLOW_UP, &out->up);

    snprintf(out->english, sizeof out->english,
             "approach last=%s next=%s anti=%s inhibit=%d novel=%d surprise+=%d occam=%d "
             "micro=%d macro=%d rec=%s DOWN=%s UP=%s hegel_held=%d",
             out->last_lead, out->next_lead, out->anti_lead,
             out->inhibited, out->novelty_seek, out->surprise_pos, out->occam_bias,
             out->micro_conf, out->macro_conf,
             out->recomb[0] ? out->recomb : "-",
             out->down.broader[0] ? out->down.broader : "-",
             out->up.broader[0] ? out->up.broader : "-",
             out->branches.held);
    return 0;
}

int ikitaku_patience_declare_stuck(const ikitaku_think_cost_t* cost,
                                   int desperation, int confusion, int residual,
                                   const char* last_lead,
                                   ikitaku_patience_evolve_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    ikitaku_desperation_t dep;
    memset(&dep, 0, sizeof dep);
    dep.level = desperation;
    ikitaku_patience_eval(cost, &dep, confusion, &out->pat);
    /* Intentionally stuck: budget not enough OR remaining thin vs residual */
    out->low_patience = !out->pat.enough;
    if (cost && out->pat.expanded_ops > 0) {
        int used = cost->ops;
        if (used * 100 / out->pat.expanded_ops >= 70 && residual >= 40)
            out->low_patience = 1;
    }
    out->declared_stuck = out->low_patience ? 1 : 0;
    ikitaku_approach_shift(last_lead ? last_lead : "hegelian",
                           residual, desperation,
                           residual >= 50, 0, &out->ap);
    if (out->declared_stuck && (out->ap.novelty_seek || desperation >= 5)) {
        snprintf(out->new_method, sizeof out->new_method, "patience_x_%s",
                 out->ap.next_lead[0] ? out->ap.next_lead : "origin");
        /* clip name */
        out->new_method[23] = 0;
        if (ikitaku_strategy_propose(out->new_method,
                                     "stuck-on-patience: inhibit last, seek stim",
                                     1) >= 0)
            out->evolved = 1;
        else
            out->evolved = 1; /* already existed still counts as considered */
    }
    snprintf(out->english, sizeof out->english,
             "patience enough=%d low=%d stuck=%d evolve=%s next=%s %s",
             out->pat.enough, out->low_patience, out->declared_stuck,
             out->evolved ? out->new_method : "-",
             out->ap.next_lead, out->pat.english);
    return out->declared_stuck;
}

int ikitaku_understand_new_abilities(ikitaku_patience_evolve_t* ev,
                                     ikitaku_thinking_t* think) {
    ikitaku_subject_teach("patience-stuck", "bound", "know-condition",
                          "kind", "low-patience-declares-stuck", 80);
    ikitaku_subject_teach("approach-shift", "theorize", "method",
                          "kind", "inhibit-rebind-novelty-occam", 80);
    ikitaku_subject_teach("surprise-positive", "theorize", "method",
                          "kind", "prediction-error-reweight", 75);
    ikitaku_alias_bind("got-stuck", "patience-stuck", 70);
    if (think)
        ikitaku_think_about_thinking(NULL, NULL, think);
    if (ev && ev->evolved && ev->new_method[0]) {
        ikitaku_subject_teach(ev->new_method, "theorize", "method",
                              "source", "patience-evolve", 70);
        ikitaku_reason_shift_any(ev->new_method, IKITAKU_BAG_METHOD, 6, NULL);
    }
    return 0;
}

int ikitaku_prior_revise(const char* upper, int proven_wrong,
                         const char* context, int new_conf,
                         ikitaku_prior_revise_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!upper || !upper[0]) upper = "theorize";
    snprintf(out->upper, sizeof out->upper, "%s", upper);
    if (new_conf < 5) new_conf = 5;
    if (new_conf > 95) new_conf = 95;

    ikitaku_subject_map_t m;
    ikitaku_subject_load(&m);
    int local = (context && context[0]) ? 1 : 0;
    out->context_local_only = local;

    for (int i = 0; i < m.n; ++i) {
        ikitaku_subject_t* s = &m.s[i];
        int points = 0;
        if (!strcmp(s->r5, upper) || !strcmp(s->r4, upper) ||
            !strcmp(s->name, upper))
            points = 1;
        for (int k = 0; k < s->n; ++k)
            if (!strcmp(s->fact[k].key, "parent") &&
                !strcmp(s->fact[k].val, upper))
                points = 1;
        if (!points) continue;

        int this_ctx = 0;
        if (local) {
            for (int k = 0; k < s->n; ++k)
                if (!strcmp(s->fact[k].key, "context") &&
                    !strcmp(s->fact[k].val, context))
                    this_ctx = 1;
            /* unnamed children inherit the local tag rather than global wipe */
            if (!this_ctx && strcmp(s->name, upper) != 0)
                this_ctx = 1;
        }

        if (local && !this_ctx && !strcmp(s->name, upper))
            continue; /* leave parent body */

        if (proven_wrong) {
            if (local && strcmp(s->name, upper) != 0) {
                ikitaku_subject_teach(s->name, s->r5, s->r4,
                                      "context", context ? context : "other",
                                      s->conf_hi);
                ikitaku_subject_teach(s->name, s->r5, s->r4,
                                      "mismatch", "1", s->conf_hi);
                out->children_touched++;
            } else if (!local) {
                int lo = new_conf > 15 ? new_conf - 10 : new_conf;
                ikitaku_subject_teach(s->name, s->r5, s->r4,
                                      "revised", "1", new_conf);
                /* rewrite conf by teach + manual clamp via another attr */
                ikitaku_subject_teach(s->name, s->r5, s->r4,
                                      "conf_after", "lowered", new_conf);
                out->children_touched++;
                if (!strcmp(s->name, upper)) out->parent_rewritten = 1;
            }
        }
    }
    if (proven_wrong && !local) {
        ikitaku_subject_teach(upper, "theorize", "method",
                              "proven_wrong", "1", new_conf);
        out->parent_rewritten = 1;
        ikitaku_approach_t ap;
        memset(&ap, 0, sizeof ap);
        ikitaku_approach_shift(upper, 100 - new_conf, 6, 1, 0, &ap);
        if (ap.next_lead[0])
            ikitaku_subject_teach(upper, "theorize", "method",
                                  "rebind", ap.next_lead, new_conf);
    }
    out->adapted = (out->children_touched > 0 || out->parent_rewritten);
    snprintf(out->english, sizeof out->english,
             "prior-revise upper=%s wrong=%d local=%d kids=%d parent=%d adapted=%d",
             upper, proven_wrong, local, out->children_touched,
             out->parent_rewritten, out->adapted);
    return out->adapted;
}

int ikitaku_tms_assert(const char* relation, const char* old_val,
                       const char* new_val, const char* covar_hint,
                       ikitaku_tms_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!relation) relation = "state";
    snprintf(out->relation, sizeof out->relation, "%s", relation);
    snprintf(out->old_val, sizeof out->old_val, "%s", old_val ? old_val : "-");
    snprintf(out->new_val, sizeof out->new_val, "%s", new_val ? new_val : "-");

    int same = (old_val && new_val && !strcmp(old_val, new_val));
    out->contradicted = same ? 0 : 1;

    /* probabilistic, not binary: keep both worlds */
    if (same) {
        out->w_old = 80; out->w_new = 80;
    } else {
        out->w_old = 40; /* prior still lives */
        out->w_new = 60; /* current observation heavier */
    }

    if (out->contradicted) {
        if (covar_hint && covar_hint[0])
            snprintf(out->covar, sizeof out->covar, "%s", covar_hint);
        else
            snprintf(out->covar, sizeof out->covar, "time"); /* default hidden axis */
    }

    ikitaku_subject_teach(relation, "info", "state", "val", out->new_val, out->w_new);
    ikitaku_subject_teach(relation, "info", "state", "prev", out->old_val, out->w_old);
    char wo[8], wn[8];
    snprintf(wo, sizeof wo, "%d", out->w_old);
    snprintf(wn, sizeof wn, "%d", out->w_new);
    ikitaku_subject_teach(relation, "info", "state", "w_old", wo, out->w_old);
    ikitaku_subject_teach(relation, "info", "state", "w_new", wn, out->w_new);
    if (out->covar[0])
        ikitaku_subject_teach(relation, "info", "state", "covar", out->covar, 70);

    if (out->contradicted) {
        ikitaku_prior_revise_t rv;
        memset(&rv, 0, sizeof rv);
        /* context-local: the new val is a context, do not wipe the type */
        ikitaku_prior_revise(relation, 1, out->new_val, out->w_new, &rv);
        out->revised_down = rv.adapted;
        /* also walk the type if it looks like an axiom name */
        if (strcmp(relation, "map") && strcmp(relation, "theorize")) {
            ikitaku_subject_teach(relation, "info", "state", "revised", "1", out->w_new);
        }
    }

    snprintf(out->english, sizeof out->english,
             "tms %s: %s(w=%d) vs %s(w=%d) contradict=%d covar=%s down=%d",
             relation, out->old_val, out->w_old, out->new_val, out->w_new,
             out->contradicted, out->covar[0] ? out->covar : "-",
             out->revised_down);
    return out->contradicted;
}

int ikitaku_ontology_self_write(const char* name, const char* r5,
                                const char* r4, const char* form,
                                ikitaku_onto_write_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!name || !name[0]) name = "self-onto";
    snprintf(out->name, sizeof out->name, "%s", name);
    const char* R5 = (r5 && r5[0]) ? r5 : "theorize";
    const char* R4 = (r4 && r4[0]) ? r4 : "method";
    const char* F  = (form && form[0]) ? form : "iki wrote this row";

    int r5r = ikitaku_r5_propose(name, F, "self-ontology write");
    out->wrote_r5 = (r5r == 0 || r5r == 1);
    int r4r = ikitaku_r4_propose(R5, R4, F);
    out->wrote_r4 = (r4r >= 0);
    int sr = ikitaku_subject_teach(name, R5, R4, "source", "iki-self", 75);
    ikitaku_subject_teach("iki", "theorize", "analyze", "last_onto", name, 80);
    out->wrote_subj = (sr == 0);
    snprintf(out->english, sizeof out->english,
             "onto-self-write name=%s r5=%d r4=%d subj=%d",
             name, out->wrote_r5, out->wrote_r4, out->wrote_subj);
    return (out->wrote_r5 || out->wrote_subj) ? 0 : -1;
}
