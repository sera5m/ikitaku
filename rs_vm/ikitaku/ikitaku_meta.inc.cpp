
int ikitaku_desperation_step(const ikitaku_session_t* s,
                             const ikitaku_fit_report_t* fits,
                             const ikitaku_dissonance_report_t* diss,
                             ikitaku_desperation_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    int failed = 0;
    if (!s || s->accepted < 0) failed++;
    if (s && s->novel_count > 0 && s->accepted < 0) failed++;
    if (fits && fits->n > 0 && fits->fits[fits->best].conf < 70) failed++;
    if (diss && diss->n >= 2) failed++;

    out->models_failed = failed;
    // level 0..5
    out->level = failed;
    if (failed >= 3) out->level = 4;
    if (failed >= 4) out->level = 5;

    ikitaku_occam_from_desperation(out->level);

    if (out->level <= 1) {
        snprintf(out->action, sizeof out->action, "calm: trust top model");
        return out->level;
    }
    if (out->level == 2) {
        snprintf(out->action, sizeof out->action, "retry next cascade model");
        return out->level;
    }
    if (out->level == 3) {
        snprintf(out->action, sizeof out->action, "novel mark + gap expansion");
        return out->level;
    }

    // Desperate: free-associate from data (partial fits)
    out->free_assoc_tried = 1;
    if (fits && fits->n > 0) {
        // take second-best if partial
        int best = fits->best;
        int second = -1, sc = -1;
        for (int i = 0; i < fits->n; ++i) {
            if (i == best) continue;
            if (fits->fits[i].conf > sc) { sc = fits->fits[i].conf; second = i; }
        }
        if (fits->fits[best].conf >= 40 && fits->fits[best].conf < 70) {
            out->partial_hit = 1;
            snprintf(out->partial_note, sizeof out->partial_note,
                     "partial fit %s conf=%d — save for meta-pattern probe",
                     fits->fits[best].formula, fits->fits[best].conf);
            snprintf(out->action, sizeof out->action,
                     "desperate: focus tests on why partial fit worked; defer full commit");
        } else if (second >= 0 && sc >= 35) {
            out->partial_hit = 1;
            snprintf(out->partial_note, sizeof out->partial_note,
                     "free-assoc secondary %s conf=%d", fits->fits[second].formula, sc);
            snprintf(out->action, sizeof out->action,
                     "desperate free-assoc: probe secondary fit as meta-pattern");
        } else {
            snprintf(out->action, sizeof out->action,
                     "desperate: no partial — mark novel, expand gap-why, budget attention");
        }
    } else {
        snprintf(out->action, sizeof out->action,
                 "desperate without series fits — request more beacons");
    }
    return out->level;
}



#include <time.h>

// ===================== think cost =====================
static clock_t g_think_t0;
static int g_think_ops = 0;

void ikitaku_think_cost_begin(void) {
    g_think_t0 = clock();
    g_think_ops = 0;
}
void ikitaku_think_cost_add_ops(int n) {
    if (n > 0) g_think_ops += n;
}
int ikitaku_think_cost_end(ikitaku_think_cost_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    clock_t t1 = clock();
    out->seconds = (double)(t1 - g_think_t0) / (double)CLOCKS_PER_SEC;
    out->ops = g_think_ops;
    if (g_think_ops < 20) snprintf(out->big_o_guess, sizeof out->big_o_guess, "O(1)~O(n)");
    else if (g_think_ops < 200) snprintf(out->big_o_guess, sizeof out->big_o_guess, "O(n)");
    else snprintf(out->big_o_guess, sizeof out->big_o_guess, "O(n^2)-ish");
    snprintf(out->english, sizeof out->english,
             "thinking took %.4f s and %d internal ops (%s heuristic)",
             out->seconds, out->ops, out->big_o_guess);
    return 0;
}

static double corr_xy(const double* a, const double* b, int n) {
    if (n < 3) return 0;
    double ma = 0, mb = 0;
    for (int i = 0; i < n; ++i) { ma += a[i]; mb += b[i]; }
    ma /= n; mb /= n;
    double num = 0, da = 0, db = 0;
    for (int i = 0; i < n; ++i) {
        double x = a[i] - ma, y = b[i] - mb;
        num += x * y; da += x * x; db += y * y;
    }
    if (da < 1e-12 || db < 1e-12) return 0;
    return num / sqrt(da * db);
}

int ikitaku_extract_variables(const ikitaku_series_t* y,
                              const ikitaku_series_t* factors, int n_factors,
                              const char** names,
                              ikitaku_var_report_t* out) {
    if (!y || !factors || !out || n_factors <= 0) return -1;
    memset(out, 0, sizeof *out);
    int n = y->n;
    double absr[IKITAKU_MAX_VARS];
    int m = n_factors < IKITAKU_MAX_VARS ? n_factors : IKITAKU_MAX_VARS;
    for (int i = 0; i < m; ++i) {
        int nn = factors[i].n < n ? factors[i].n : n;
        double r = corr_xy(y->values, factors[i].values, nn);
        if (r < 0) r = -r;
        absr[i] = r;
        ikitaku_var_role_t_ent* v = &out->vars[out->n++];
        if (names && names[i]) snprintf(v->name, sizeof v->name, "%s", names[i]);
        else snprintf(v->name, sizeof v->name, "x%d", i);
        v->influence = (int)(r * 100.0 + 0.5);
    }
    // roles: highest = determinant; mid jointly = co; low = irrelevant
    for (int i = 0; i < out->n; ++i) {
        int inf = out->vars[i].influence;
        if (inf >= 70) {
            out->vars[i].role = IKITAKU_VAR_DETERMINANT;
            snprintf(out->vars[i].why, sizeof out->vars[i].why,
                     "tracks the target closely (r~%.2f) — changing it moves Y", inf/100.0);
        } else if (inf >= 35) {
            out->vars[i].role = IKITAKU_VAR_CODETERMINANT;
            snprintf(out->vars[i].why, sizeof out->vars[i].why,
                     "partial correlation — likely needed with another factor");
        } else {
            out->vars[i].role = IKITAKU_VAR_IRRELEVANT;
            snprintf(out->vars[i].why, sizeof out->vars[i].why,
                     "weak link to Y — treat as noise / control");
        }
    }
    snprintf(out->english, sizeof out->english,
             "Classified %d experimental variables into determinant / co-determinant / irrelevant.",
             out->n);
    ikitaku_think_cost_add_ops(m * n);
    return out->n;
}

int ikitaku_try_undo_transforms(const ikitaku_series_t* s,
                                ikitaku_undo_report_t* out) {
    if (!s || !out || s->n < 3) return -1;
    memset(out, 0, sizeof *out);
    out->unsure = 1; // always admit uncertainty
    out->best = -1;
    ikitaku_series_t tmp;
    tmp.kind = IKITAKU_DATA_F32;
    tmp.n = s->n;

    // baseline fit conf
    ikitaku_fit_report_t base;
    ikitaku_series_fit(s, &base);
    int conf0 = (base.n && base.best >= 0) ? base.fits[base.best].conf : 0;

    // attempts: identity, asin(clip), asinh-ish (log), abs, square
    struct { const char* name; } att[] = {
        { "identity" }, { "asin(clip y)" }, { "tanh^-1 clip" }, { "|y|" }, { "y^2" }
    };
    for (int a = 0; a < 5 && out->n < IKITAKU_MAX_UNDO; ++a) {
        for (int i = 0; i < s->n; ++i) {
            double v = s->values[i];
            if (a == 0) tmp.values[i] = v;
            else if (a == 1) {
                if (v > 1) v = 1; if (v < -1) v = -1;
                tmp.values[i] = asin(v);
            } else if (a == 2) {
                if (v > 0.999) v = 0.999; if (v < -0.999) v = -0.999;
                tmp.values[i] = 0.5 * log((1 + v) / (1 - v + 1e-12));
            } else if (a == 3) tmp.values[i] = v < 0 ? -v : v;
            else tmp.values[i] = v * v;
        }
        ikitaku_fit_report_t fr;
        ikitaku_series_fit(&tmp, &fr);
        int c1 = (fr.n && fr.best >= 0) ? fr.fits[fr.best].conf : 0;
        ikitaku_undo_t* u = &out->tries[out->n++];
        snprintf(u->attempt, sizeof u->attempt, "%s", att[a].name);
        u->conf_before = conf0;
        u->conf_after = c1;
        u->improved = c1 > conf0 + 5;
        snprintf(u->note, sizeof u->note, u->improved
                 ? "maybe undid a wrapper — still not sure"
                 : "no clear improvement — model may be incomplete");
        if (out->best < 0 || c1 > out->tries[out->best].conf_after)
            out->best = out->n - 1;
        ikitaku_think_cost_add_ops(s->n * 8);
    }
    snprintf(out->english, sizeof out->english,
             "I am NOT sure of the hidden transform. Undo order follows "
             "scientific then Hegelian probes. Tried %d undos; best='%s' "
             "conf %d→%d. Escalating only if desperation warrants.",
             out->n,
             out->best >= 0 ? out->tries[out->best].attempt : "none",
             conf0,
             out->best >= 0 ? out->tries[out->best].conf_after : 0);
    return out->n;
}

void ikitaku_agenda_init(ikitaku_agenda_t* a) {
    if (!a) return;
    memset(a, 0, sizeof *a);
    a->active = -1;
    snprintf(a->schedule_note, sizeof a->schedule_note, "Nothing on my schedule.");
}

int ikitaku_agenda_add(ikitaku_agenda_t* a, const char* id, uint8_t kind,
                       int priority, int deadline_ops) {
    if (!a || a->n >= IKITAKU_MAX_GOALS) return -1;
    ikitaku_goal_t* g = &a->goals[a->n];
    memset(g, 0, sizeof *g);
    snprintf(g->id, sizeof g->id, "%s", id ? id : "task");
    g->kind = kind;
    g->priority = priority;
    g->deadline_ops = deadline_ops;
    snprintf(g->status, sizeof g->status, "queued");
    if (a->active < 0 || priority > a->goals[a->active].priority)
        a->active = a->n;
    a->n++;
    return a->n - 1;
}

int ikitaku_agenda_tick(ikitaku_agenda_t* a, const ikitaku_desperation_t* dep,
                        int confusion, char* english, int elen) {
    if (!a) return -1;
    if (a->n == 0 || a->active < 0) {
        snprintf(a->schedule_note, sizeof a->schedule_note,
                 "Nothing on my schedule. Let me think this over.");
        if (english && elen) snprintf(english, (size_t)elen, "%s", a->schedule_note);
        return 0;
    }
    ikitaku_goal_t* g = &a->goals[a->active];
    g->confusion = confusion;
    g->desperation = dep ? dep->level * 20 : 0;
    if (g->deadline_ops > 0) g->deadline_ops--;
    if (g->priority >= 80) {
        snprintf(g->status, sizeof g->status, "extreme_priority");
        if (g->deadline_ops <= 0) {
            if (g->desperation > g->confusion)
                snprintf(a->schedule_note, sizeof a->schedule_note,
                         "Priority '%s' budget exhausted; desperation>confusion — keep pushing / add probes.",
                         g->id);
            else
                snprintf(a->schedule_note, sizeof a->schedule_note,
                         "Priority '%s' budget exhausted; confusion high — wait / query origin.",
                         g->id);
        } else {
            snprintf(a->schedule_note, sizeof a->schedule_note,
                     "This is the most relevant thing right now (%s). I want to solve it.",
                     g->id);
        }
    } else {
        snprintf(g->status, sizeof g->status, "active");
        snprintf(a->schedule_note, sizeof a->schedule_note,
                 "Working '%s' (pri=%d). Confusion=%d desperation=%d.",
                 g->id, g->priority, g->confusion, g->desperation);
    }
    if (english && elen) snprintf(english, (size_t)elen, "%s", a->schedule_note);
    return a->active;
}

int ikitaku_query_origin(ikitaku_query_t* q, const char* topic) {
    if (!q) return -1;
    memset(q, 0, sizeof *q);
    q->to = IKITAKU_ENTITY_HUMAN;
    q->pending = 1;
    snprintf(q->question, sizeof q->question,
             "ORIGIN QUERY (stub): where does '%s' come from? I am not sure.",
             topic ? topic : "this pattern");
    snprintf(q->answer, sizeof q->answer, "(pending — English NL not implemented)");
    return 0;
}

int ikitaku_debug_post(const char* webhook_url, const char* text) {
    if (!webhook_url || !webhook_url[0] || !text) return -1;
    // sanitize quotes for JSON
    char body[1800];
    char esc[1600];
    int e = 0;
    for (const char* p = text; *p && e < (int)sizeof(esc) - 2; ++p) {
        if (*p == '"' || *p == '\\') { esc[e++] = '\\'; esc[e++] = *p; }
        else if (*p == '\n') { esc[e++] = '\\'; esc[e++] = 'n'; }
        else esc[e++] = *p;
    }
    esc[e] = 0;
    snprintf(body, sizeof body,
             "{\"content\":\"[ikitaku] %.1500s\"}", esc);
    char cmd[2400];
    snprintf(cmd, sizeof cmd,
             "curl -sS -m 8 -H 'Content-Type: application/json' -d '%s' '%s' >/tmp/ikitaku_hook.out 2>/tmp/ikitaku_hook.err",
             body, webhook_url);
    int rc = system(cmd);
    return rc == 0 ? 0 : -1;
}

int ikitaku_query_dispatch(const ikitaku_query_t* q, const char* webhook_url) {
    if (!q) return -1;
    char msg[256];
    snprintf(msg, sizeof msg, "query to entity=%u pending=%d | %s",
             (unsigned)q->to, q->pending, q->question);
    if (webhook_url && webhook_url[0])
        return ikitaku_debug_post(webhook_url, msg);
    return 0;
}



int ikitaku_classify_uncertainty(const ikitaku_fit_report_t* fits,
                                 const ikitaku_var_report_t* vars,
                                 const ikitaku_dissonance_report_t* diss,
                                 ikitaku_uncertainty_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    int best = 0, second = 0;
    if (fits && fits->n > 0) {
        best = fits->fits[fits->best].conf;
        for (int i = 0; i < fits->n; ++i) {
            if (i == fits->best) continue;
            if (fits->fits[i].conf > second) second = fits->fits[i].conf;
        }
        out->bayesian = best - second;
        if (out->bayesian < 0) out->bayesian = -out->bayesian;
        // high two-way competition → bayesian interference
        if (best < 80 && second >= best - 15)
            out->bayesian = 70 + (15 - (best - second));
        if (best < 50) out->epistemic = 80;
        else if (best < 70) out->epistemic = 55;
        else out->epistemic = 25;
    } else {
        out->epistemic = 90;
        out->bayesian = 20;
    }
    int irr = 0, n = 0;
    if (vars) {
        n = vars->n;
        for (int i = 0; i < vars->n; ++i)
            if (vars->vars[i].role == IKITAKU_VAR_IRRELEVANT) irr++;
        out->aleatoric = n ? (irr * 100) / n : 30;
    } else out->aleatoric = 20;
    if (diss && diss->n > 0)
        out->epistemic = out->epistemic + 10 > 100 ? 100 : out->epistemic + 10;

    if (out->epistemic >= out->aleatoric && out->epistemic >= out->bayesian)
        out->dominant = IKITAKU_U_EPISTEMIC;
    else if (out->aleatoric >= out->bayesian)
        out->dominant = IKITAKU_U_ALEATORIC;
    else
        out->dominant = IKITAKU_U_BAYESIAN;

    const char* dname = out->dominant == IKITAKU_U_EPISTEMIC ? "epistemic (I lack the model)"
                      : out->dominant == IKITAKU_U_ALEATORIC ? "aleatoric (noise in the world)"
                      : "bayesian (competing posteriors)";
    snprintf(out->english, sizeof out->english,
             "Uncertainty parallel: epistemic=%d aleatoric=%d bayesian-interference=%d — dominant: %s",
             out->epistemic, out->aleatoric, out->bayesian, dname);
    return 0;
}

int ikitaku_bound_variables(ikitaku_var_report_t* vars,
                            const ikitaku_desperation_t* dep,
                            int willing,
                            char* english, int elen) {
    if (!vars) return -1;
    // Clamp influence to [0,100]; keep at least one of each class if present
    for (int i = 0; i < vars->n; ++i) {
        if (vars->vars[i].influence < 0) vars->vars[i].influence = 0;
        if (vars->vars[i].influence > 100) vars->vars[i].influence = 100;
    }
    int collapsed = 0;
    int level = dep ? dep->level : 0;
    // Willing only if desperation >= 3 AND caller allows
    if (willing && level >= 3) {
        // Slightly promote highest co-determinant toward determinant
        // without deleting irrelevant (no full resolution without a rule)
        int best_co = -1, best_inf = -1;
        for (int i = 0; i < vars->n; ++i) {
            if (vars->vars[i].role == IKITAKU_VAR_CODETERMINANT
                && vars->vars[i].influence > best_inf) {
                best_inf = vars->vars[i].influence;
                best_co = i;
            }
        }
        if (best_co >= 0 && vars->vars[best_co].influence >= 50) {
            vars->vars[best_co].influence += 8;
            if (vars->vars[best_co].influence > 69)
                vars->vars[best_co].influence = 69; // still co, not full determinant
            snprintf(vars->vars[best_co].why, sizeof vars->vars[best_co].why,
                     "desperation nudge +8; still co-determinant until a rule is extracted");
            collapsed = 1;
        }
    }
    const char* msg = !willing
        ? "Roles bounded; refused desperation collapse (not willing)."
        : (collapsed
           ? "Roles bounded; slight desperation collapse of co-determinant — dissonance reduced, not resolved."
           : "Roles bounded; desperation not applied (level too low or no co-determinant).");
    if (english && elen) snprintf(english, (size_t)elen, "%s", msg);
    return collapsed;
}

int ikitaku_patience_eval(const ikitaku_think_cost_t* cost,
                          const ikitaku_desperation_t* dep,
                          int confusion,
                          ikitaku_patience_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->base_ops = IKITAKU_MIN_OPS;
    out->base_seconds = IKITAKU_MIN_SECONDS;
    double extra = 0.0;
    if (dep && dep->level >= 2) extra += 0.15;
    if (confusion >= 50) extra += 0.15;
    if (extra > 0.30) extra = 0.30;
    if (dep && dep->level >= 4 && confusion >= 60) extra = 0.30;
    out->expanded_ops = (int)(out->base_ops * (1.0 + extra));
    out->expanded_seconds = out->base_seconds * (1.0 + extra);
    int ops = cost ? cost->ops : 0;
    double sec = cost ? cost->seconds : 0;
    out->enough = (ops >= out->expanded_ops) || (sec >= out->expanded_seconds);
    if (!out->enough)
        snprintf(out->english, sizeof out->english,
                 "Not enough thought-budget (used %d ops / %.3fs; need ≥%d ops or ≥%.2fs after +%.0f%% expand).",
                 ops, sec, out->expanded_ops, out->expanded_seconds, extra * 100.0);
    else
        snprintf(out->english, sizeof out->english,
                 "Thought-budget met (%d ops / %.3fs vs floor %d / %.2fs).",
                 ops, sec, out->expanded_ops, out->expanded_seconds);
    return out->enough;
}

int ikitaku_thought_log(const char* text) {
    if (!text) return -1;
    const char* dir = "/home/workdir/artifacts/files/itikatu_thought_output";
    char cmd[256];
    snprintf(cmd, sizeof cmd, "mkdir -p '%s'", dir);
    system(cmd);
    time_t now = time(NULL);
    char path[320];
    snprintf(path, sizeof path, "%s/raw_%ld_out.txt", dir, (long)now);
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s\n", text);
    fclose(f);
    return 0;
}



static int corr_series_fn(const ikitaku_series_t* s, double (*fn)(double), double dx, double x0) {
    double yhat[IKITAKU_SERIES_MAX];
    int n = s->n;
    for (int i = 0; i < n; ++i) {
        double x = x0 + (double)i * dx;
        yhat[i] = fn(x);
    }
    return corr_score(s->values, yhat, n);
}

static double fn_sin(double x) { return sin(x); }
static double fn_tan(double x) { return tan(x); }
static double fn_sin_tan(double x) { return sin(tan(x)); }
static double fn_tan_sin(double x) { return tan(sin(x)); }
static double fn_sin_sin(double x) { return sin(sin(x)); }
static double fn_id(double x) { return x; }

static int inverse_check_sin_outer(const ikitaku_series_t* s, double dx) {
    // If y = sin(inner), asin(clip y) should look like inner
    double inner[IKITAKU_SERIES_MAX], asin_y[IKITAKU_SERIES_MAX];
    int n = s->n;
    for (int i = 0; i < n; ++i) {
        double v = s->values[i];
        if (v > 1) v = 1; if (v < -1) v = -1;
        asin_y[i] = asin(v);
        inner[i] = tan((double)i * dx);
    }
    return corr_score(asin_y, inner, n);
}

int ikitaku_stack_complete(const ikitaku_series_t* s,
                           const ikitaku_fit_report_t* partial,
                           ikitaku_stack_report_t* out) {
    if (!s || !out || s->n < 4) return -1;
    memset(out, 0, sizeof *out);
    out->best = -1;
    int pconf = 0;
    int partial_sin = 0;
    if (partial && partial->n && partial->best >= 0) {
        pconf = partial->fits[partial->best].conf;
        if (partial->fits[partial->best].kind == IKITAKU_FIT_SIN)
            partial_sin = 1;
    }
    out->right_track = (pconf >= 40 && pconf < 80) || partial_sin;

    struct { const char* name; double (*fn)(double); } fam[] = {
        { "sin(x)", fn_sin },
        { "tan(x)", fn_tan },
        { "sin(tan(x))", fn_sin_tan },
        { "tan(sin(x))", fn_tan_sin },
        { "sin(sin(x))", fn_sin_sin },
        { "x", fn_id },
    };
    double dxs[] = { 0.05, 0.06, 0.08, 0.10, 0.13, 0.15, 0.20, 0.30 };
    for (int fi = 0; fi < 6 && out->n < IKITAKU_MAX_STACK; ++fi) {
        int bestc = 0; double bestdx = dxs[0];
        for (int d = 0; d < 8; ++d) {
            int c = corr_series_fn(s, fam[fi].fn, dxs[d], 0.0);
            int c2 = corr_series_fn(s, fam[fi].fn, dxs[d], 0.05);
            if (c2 > c) c = c2;
            if (c > bestc) { bestc = c; bestdx = dxs[d]; }
        }
        ikitaku_stack_try_t* tr = &out->tries[out->n++];
        snprintf(tr->formula, sizeof tr->formula, "%s dx=%.2f", fam[fi].name, bestdx);
        tr->conf = bestc;
        tr->inverse_ok = 0;
        if (strcmp(fam[fi].name, "sin(tan(x))") == 0) {
            int ic = inverse_check_sin_outer(s, bestdx);
            tr->inverse_ok = ic >= 50;
            snprintf(tr->note, sizeof tr->note,
                     "asin(y)~tan(x) check conf=%d %s", ic,
                     tr->inverse_ok ? "supports stack" : "weak identity");
        } else if (strcmp(fam[fi].name, "sin(x)") == 0) {
            snprintf(tr->note, sizeof tr->note, "base outer sin (partial family)");
        } else {
            snprintf(tr->note, sizeof tr->note, "candidate stack");
        }
        int wraps = 0;
        for (const char* p = tr->formula; *p; ++p) if (*p == '(') wraps++;
        int take = 0;
        if (out->best < 0) take = 1;
        else {
            int bw = 0;
            for (const char* p = out->tries[out->best].formula; *p; ++p)
                if (*p == '(') bw++;
            int m = ikitaku_occam_margin();
            if (tr->conf > out->tries[out->best].conf + m) take = 1;
            else if (tr->conf >= out->tries[out->best].conf - m && wraps < bw)
                take = 1;
            else if (tr->conf > out->tries[out->best].conf && wraps <= bw)
                take = 1;
        }
        if (take) out->best = out->n - 1;
        ikitaku_think_cost_add_ops(s->n * 8);
    }

    if (out->right_track) {
        // treat common transforms as variables: extra reverse of top-2
        snprintf(out->english, sizeof out->english,
                 "On the right track (partial conf=%d). Completed stacked families; "
                 "best %s conf=%d. Transforms treated as variables.",
                 pconf,
                 out->best>=0 ? out->tries[out->best].formula : "?",
                 out->best>=0 ? out->tries[out->best].conf : 0);
    } else {
        snprintf(out->english, sizeof out->english,
                 "Stacked completion: best %s conf=%d.",
                 out->best>=0 ? out->tries[out->best].formula : "?",
                 out->best>=0 ? out->tries[out->best].conf : 0);
    }
    return out->n;
}


static void thought_add(ikitaku_thought_plan_t* p, const char* n,
                        const char* when, const char* move, int w) {
    if (!p || p->n >= IKITAKU_THOUGHT_MAX) return;
    ikitaku_thought_form_t* f = &p->form[p->n++];
    snprintf(f->name, sizeof f->name, "%s", n);
    snprintf(f->when, sizeof f->when, "%s", when);
    snprintf(f->move, sizeof f->move, "%s", move);
    f->weight = w;
}

int ikitaku_thought_plan(int desperation_level,
                         int math_conf,
                         int rule_conf,
                         ikitaku_thought_plan_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    int occ = 80;
    if (desperation_level >= 3) occ = 40;
    if (desperation_level >= 5) occ = 15;
    thought_add(out, "occam",
                "prefer shallower wrap unless thrown",
                "keep F if extra wrap < margin", occ);
    thought_add(out, "scientific",
                "try / score / revise under budget",
                "fit then peel then rule-scan",
                70 + (desperation_level >= 2 ? 10 : 0));
    thought_add(out, "hegelian",
                "determinate negation of a specific claim",
                "collide map-math vs map-table",
                (math_conf >= 50 && rule_conf >= 50) ? 75 : 45);
    thought_add(out, "bayesian",
                "compete posteriors on forms",
                "weights from confs, do not pick a costume",
                (math_conf > 0 && rule_conf > 0) ? 65 : 40);
    thought_add(out, "origin",
                "B causes C; if B is frequent invert for A",
                "associate-consider then mental-test the rule",
                55 + (rule_conf >= 60 ? 15 : 0));
    thought_add(out, "ladder",
                "walk R9 then R8 then R7 then R6; do not skip to strategy",
                "gate unknown → useful → right-task → thought-plan",
                72);
    int best = 0;

    for (int i = 1; i < out->n; ++i)
        if (out->form[i].weight > out->form[best].weight) best = i;
    /* calm + strong math → occam; conflict → hegelian */
    if (desperation_level <= 1 && math_conf >= 85 && rule_conf < 50)
        best = 0;
    if (math_conf >= 50 && rule_conf >= 70 && math_conf < 85)
        for (int i = 0; i < out->n; ++i)
            if (strcmp(out->form[i].name, "hegelian") == 0) best = i;
    out->chosen = best;
    if (desperation_level >= 4 || math_conf < 35) {
        ikitaku_approach_t ap;
        memset(&ap, 0, sizeof ap);
        const char* last = out->form[best].name;
        int res = desperation_level * 12;
        if (res > 90) res = 90;
        ikitaku_approach_shift(last, res, desperation_level, res >= 60, 0, &ap);
        if (ap.next_lead[0] && strcmp(ap.next_lead, last) != 0) {
            /* prefer exact name match in plan, else boost origin/occam slot */
            int hit = -1;
            for (int i = 0; i < out->n; ++i)
                if (strstr(ap.next_lead, out->form[i].name)) { hit = i; break; }
            if (hit >= 0) {
                out->form[hit].weight += 20;
                if (out->form[hit].weight > 100) out->form[hit].weight = 100;
                out->chosen = hit;
                best = hit;
            }
        }
    }
    snprintf(out->english, sizeof out->english,
             "R6 plan: lead with %s (w=%d). dep=%d math=%d rule=%d",
             out->form[best].name, out->form[best].weight,
             desperation_level, math_conf, rule_conf);
    return out->n;
}

int ikitaku_schema_extract(const ikitaku_series_t* s, ikitaku_schema_t* out) {
    if (!s || !out) return -1;
    memset(out, 0, sizeof *out);
    ikitaku_fit_report_t fits;
    memset(&fits, 0, sizeof fits);
    if (s->n >= 3) ikitaku_series_fit(s, &fits);
    ikitaku_rule_t rule;
    memset(&rule, 0, sizeof rule);
    if (s->n >= 3) ikitaku_rule_scan(s, &rule);
    int mc = (fits.n && fits.best >= 0) ? fits.fits[fits.best].conf : 0;
    int rc = rule.conf;
    ikitaku_session_t dummy; memset(&dummy, 0, sizeof dummy);
    dummy.accepted = -1;
    ikitaku_desperation_t dep;
    ikitaku_desperation_step(&dummy, &fits, NULL, &dep);
    ikitaku_thought_plan_t plan;
    ikitaku_thought_plan(dep.level, mc, rc, &plan);

    snprintf(out->strategy, sizeof out->strategy, "%s",
             plan.form[plan.chosen].name);
    snprintf(out->used, sizeof out->used, "seek,compare,reduce,map");

    int table_wins = (rc >= 70 && rc >= mc - 5);
    int math_wins = (mc >= 80 && !table_wins);
    if (table_wins) {
        snprintf(out->purpose, sizeof out->purpose, "map-table");
        snprintf(out->schema, sizeof out->schema, "%s",
                 rule.period ? "period-k table" : "alphabet/table");
        out->conf = rc;
        snprintf(out->used, sizeof out->used, "seek,compare,reduce,map-table");
    } else if (math_wins) {
        snprintf(out->purpose, sizeof out->purpose, "map-math");
        snprintf(out->schema, sizeof out->schema, "%s",
                 fits.fits[fits.best].formula);
        out->conf = mc;
        snprintf(out->used, sizeof out->used, "seek,compare,reduce,map-math");
    } else if (mc < 45 && rc < 45) {
        snprintf(out->purpose, sizeof out->purpose, "generate");
        snprintf(out->schema, sizeof out->schema, "no stable schema");
        out->conf = 20;
        snprintf(out->used, sizeof out->used, "seek,compare,generate");
    } else {
        snprintf(out->purpose, sizeof out->purpose, "info");
        snprintf(out->schema, sizeof out->schema, "partial form, not locked");
        out->conf = mc > rc ? mc : rc;
        snprintf(out->used, sizeof out->used, "seek,compare,reduce");
    }
    snprintf(out->english, sizeof out->english,
             "schema-extract [%s]: %s as %s conf=%d (used %s)",
             out->strategy, out->schema, out->purpose, out->conf, out->used);
    return out->conf;
}

/* Score want as a *section* of have. c = sample-ratio context (f'+c). */
static int desire_mse100_win(const ikitaku_series_t* have,
                             const ikitaku_series_t* want,
                             int* off_out, int* span_out) {
    if (!have || !want || have->n < 2 || want->n < 2) return 100;
    if (have->n == want->n) {
        double acc = 0;
        for (int i = 0; i < have->n; ++i) {
            double e = have->values[i] - want->values[i];
            acc += e * e;
        }
        acc = sqrt(acc / have->n);
        int s = (int)(acc * 50.0);
        if (s > 100) s = 100;
        if (off_out) *off_out = 0;
        if (span_out) *span_out = have->n;
        return s < 0 ? 0 : s;
    }
    const ikitaku_series_t* longp = have;
    const ikitaku_series_t* tmpl = want;
    if (want->n > have->n) { longp = want; tmpl = have; }
    double ratios[] = { 1.0, 1.25, 1.5, 1.6, 1.75, 2.0, 0.8, 0.625, 0.5 };
    int nr = 9;
    double best = 1e9;
    int best_off = 0, best_span = tmpl->n;
    for (int r = 0; r < nr; ++r) {
        double span_f = tmpl->n * ratios[r];
        int span = (int)(span_f + 0.5);
        if (span < tmpl->n / 2) span = tmpl->n / 2;
        if (span >= longp->n) span = longp->n - 1;
        if (span < 4) continue;
        int maxoff = longp->n - span;
        if (maxoff < 0) continue;
        int step = maxoff > 40 ? maxoff / 20 : 1;
        if (step < 1) step = 1;
        for (int off = 0; off <= maxoff; off += step) {
            double acc = 0;
            for (int i = 0; i < tmpl->n; ++i) {
                double u = (double)i / (tmpl->n - 1);
                double src = off + u * (span - 1);
                int j = (int)src;
                double f = src - j;
                if (j + 1 >= longp->n) j = longp->n - 2;
                double hv = longp->values[j] * (1 - f) + longp->values[j + 1] * f;
                double e = hv - tmpl->values[i];
                acc += e * e;
            }
            acc = sqrt(acc / tmpl->n);
            if (acc < best) { best = acc; best_off = off; best_span = span; }
        }
    }
    if (off_out) *off_out = best_off;
    if (span_out) *span_out = best_span;
    int s = (int)(best * 50.0);
    if (s > 100) s = 100;
    if (s < 0) s = 0;
    return s;
}

static int desire_mse100(const ikitaku_series_t* a, const ikitaku_series_t* b) {
    return desire_mse100_win(a, b, 0, 0);
}

int ikitaku_template_match(const ikitaku_series_t* have,
                           const ikitaku_series_t* want,
                           ikitaku_template_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->r4, sizeof out->r4, "template");
    out->mse = desire_mse100_win(have, want, &out->off, &out->span);
    snprintf(out->english, sizeof out->english,
             "template-match mse=%d off=%d span=%d (short bag is a witness)",
             out->mse, out->off, out->span);
    return out->mse;
}

static void expand_recur(const ikitaku_series_t* have,
                         const ikitaku_series_t* want,
                         int ctx, int depth, int max_depth,
                         int prev_mse, ikitaku_expand_t* out) {
    if (!out || depth >= max_depth || out->n >= IKITAKU_EXPAND_MAX) return;
    if (ctx < 6) ctx = 6;
    if (ctx > want->n) ctx = want->n;
    ikitaku_series_t slice = *want;
    slice.n = ctx;
    ikitaku_template_t tm;
    ikitaku_template_match(have, &slice, &tm);
    ikitaku_expand_step_t* st = &out->step[out->n++];
    st->ctx = ctx;
    st->mse = tm.mse;
    st->off = tm.off;
    st->span = tm.span;
    st->held = (prev_mse < 0 || tm.mse <= prev_mse + 8) ? 1 : 0;
    out->depth = depth + 1;
    if (ctx >= want->n) return;
    if (!st->held && depth >= 2) return; /* unwrap got fucky; stop */
    int next = ctx + (want->n - ctx + 2) / 3;
    if (next <= ctx) next = ctx + 8;
    if (next > want->n) next = want->n;
    expand_recur(have, want, next, depth + 1, max_depth, tm.mse, out);
}

int ikitaku_template_expand(const ikitaku_series_t* have,
                            const ikitaku_series_t* want,
                            int start_n, int max_depth,
                            ikitaku_expand_t* out) {
    if (!have || !want || !out) return -1;
    memset(out, 0, sizeof *out);
    if (max_depth < 1) max_depth = 1;
    if (max_depth > IKITAKU_EXPAND_MAX) max_depth = IKITAKU_EXPAND_MAX;
    if (start_n < 6) start_n = 6;
    expand_recur(have, want, start_n, 0, max_depth, -1, out);
    int holds = 0;
    for (int i = 0; i < out->n; ++i) if (out->step[i].held) holds++;
    out->stable = (out->n >= 2 && holds * 2 >= out->n &&
                   out->step[out->n-1].held) ? 1 : 0;
    snprintf(out->english, sizeof out->english,
             "expand depth=%d ctx %d→%d mse %d→%d stable=%d",
             out->depth,
             out->n ? out->step[0].ctx : 0,
             out->n ? out->step[out->n-1].ctx : 0,
             out->n ? out->step[0].mse : -1,
             out->n ? out->step[out->n-1].mse : -1,
             out->stable);
    for (int i = 0; i < out->n; ++i)
        ikitaku_xp_record("template-expand", "seek", "template",
                          out->step[i].ctx,
                          out->step[i].held ? 1 : -1);
    return out->stable;
}

int ikitaku_xp_load(ikitaku_xp_report_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    FILE* f = fopen(IKITAKU_XP_PATH, "r");
    if (!f) {
        snprintf(out->english, sizeof out->english, "xp: no cases yet (inexperienced)");
        return 0;
    }
    char line[192];
    while (out->n < IKITAKU_XP_MAX && fgets(line, sizeof line, f)) {
        ikitaku_xp_t* x = &out->item[out->n];
        if (sscanf(line, "%63[^|]|%23[^|]|%23[^|]|%d|%d|%d",
                   x->rule, x->r5, x->r4, &x->ctx, &x->reward, &x->count) >= 5)
            out->n++;
    }
    fclose(f);
    snprintf(out->english, sizeof out->english, "xp: %d case rows loaded", out->n);
    return out->n;
}

int ikitaku_xp_record(const char* rule, const char* r5, const char* r4,
                      int ctx, int reward) {
    if (!rule) return -1;
    system("mkdir -p '/home/workdir/artifacts/rs_vm/ikitaku/discovery/experience'");
    ikitaku_xp_t prev;
    memset(&prev, 0, sizeof prev);
    int known = ikitaku_xp_lookup(rule, &prev);
    FILE* f = fopen(IKITAKU_XP_PATH, "a");
    if (!f) return -1;
    fprintf(f, "%s|%s|%s|%d|%d|%d\n",
            rule, r5 ? r5 : "-", r4 ? r4 : "-",
            ctx, reward, known + 1);
    fclose(f);
    return 0;
}

int ikitaku_xp_lookup(const char* rule, ikitaku_xp_t* out) {
    if (!rule || !out) return 0;
    memset(out, 0, sizeof *out);
    FILE* f = fopen(IKITAKU_XP_PATH, "r");
    if (!f) return 0;
    char line[192];
    int n = 0;
    while (fgets(line, sizeof line, f)) {
        ikitaku_xp_t x;
        memset(&x, 0, sizeof x);
        if (sscanf(line, "%63[^|]|%23[^|]|%23[^|]|%d|%d|%d",
                   x.rule, x.r5, x.r4, &x.ctx, &x.reward, &x.count) < 5)
            continue;
        if (strcmp(x.rule, rule) == 0) {
            *out = x;
            n++;
        }
    }
    fclose(f);
    return n;
}

int ikitaku_gap_window(const ikitaku_series_t* have,
                       int hole_lo, int hole_hi,
                       ikitaku_template_t* out) {
    if (!have || !out) return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->r4, sizeof out->r4, "template");
    if (hole_lo < 0) hole_lo = 0;
    if (hole_hi > have->n) hole_hi = have->n;
    if (hole_hi - hole_lo < 3) {
        snprintf(out->english, sizeof out->english, "gap-window: hole too small");
        return -1;
    }
    /* flanks of the hole are the template; the hole is the missing c */
    ikitaku_series_t tmpl;
    memset(&tmpl, 0, sizeof tmpl);
    tmpl.kind = have->kind;
    int left = hole_lo;
    if (left > 16) left = 16;
    for (int i = 0; i < left; ++i)
        tmpl.values[tmpl.n++] = have->values[hole_lo - left + i];
    int right = have->n - hole_hi;
    if (right > 16) right = 16;
    for (int i = 0; i < right && tmpl.n < IKITAKU_SERIES_MAX; ++i)
        tmpl.values[tmpl.n++] = have->values[hole_hi + i];
    if (tmpl.n < 4) {
        snprintf(out->english, sizeof out->english, "gap-window: flanks thin");
        return -1;
    }
    int mse = ikitaku_template_match(have, &tmpl, out);
    snprintf(out->english, sizeof out->english,
             "gap-window hole[%d,%d) flanks=%d mse=%d off=%d (gap treated as window)",
             hole_lo, hole_hi, tmpl.n, out->mse, out->off);
    return mse;
}

int ikitaku_template_strategy(const ikitaku_series_t* have,
                              const ikitaku_series_t* want,
                              ikitaku_tstrat_t* out) {
    if (!have || !want || !out) return -1;
    memset(out, 0, sizeof *out);
    out->best = -1;
    ikitaku_xp_t xp;
    out->xp_known = ikitaku_xp_lookup("template-expand", &xp)
                  + ikitaku_xp_lookup("template-match", &xp);
    if (out->xp_known < 2) {
        out->insufficient_priori = 1;
        out->dissonance = 25;   /* mild; not a high-severity lie */
        out->confusion = 35;
    } else {
        out->dissonance = 8;
        out->confusion = 12;
    }

    int n = want->n;
    int cuts[6][2];
    int nc = 0;
    auto addc = [&](int lo, int hi) {
        if (hi - lo < 6) return;
        if (lo < 0) lo = 0;
        if (hi > n) hi = n;
        if (nc < 6) { cuts[nc][0] = lo; cuts[nc][1] = hi; nc++; }
    };
    addc(0, n < 16 ? n : 16);           /* short prefix */
    addc(0, n / 3);                     /* first third */
    addc(n / 3, (2 * n) / 3);           /* middle */
    addc((2 * n) / 3, n);               /* last third */
    addc(n / 4, n / 4 + 16);            /* mid-short */
    addc(0, n);                         /* whole want — last, often sloppy */

    int best_mse = 999, worst_mse = -1;
    for (int i = 0; i < nc && out->n < IKITAKU_TSTRAT_MAX; ++i) {
        ikitaku_series_t sl = *want;
        sl.n = cuts[i][1] - cuts[i][0];
        for (int k = 0; k < sl.n; ++k)
            sl.values[k] = want->values[cuts[i][0] + k];
        ikitaku_template_t tm;
        ikitaku_template_match(have, &sl, &tm);
        ikitaku_tstrat_step_t* st = &out->step[out->n];
        st->lo = cuts[i][0]; st->hi = cuts[i][1];
        st->mse = tm.mse; st->off = tm.off; st->span = tm.span;
        st->d_mse = (out->n == 0) ? 0 : tm.mse - best_mse;
        st->held = (tm.mse <= best_mse + 8) ? 1 : 0;
        if (tm.mse < best_mse) { best_mse = tm.mse; out->best = out->n; }
        if (tm.mse > worst_mse) worst_mse = tm.mse;
        out->n++;
    }

    /* Micro: Hegelian hunt between best witness and worst (avoid sloppy whole). */
    ikitaku_hunt_t hunt;
    ikitaku_hunt_init(&hunt, "best-partial", "worst-slice");
    int pos = (best_mse >= 0 && best_mse <= 100) ? (100 - best_mse) : 50;
    int neg = (worst_mse >= 0 && worst_mse <= 100) ? (100 - worst_mse) : 50;
    if (pos < 0) pos = 0; if (pos > 100) pos = 100;
    if (neg < 0) neg = 0; if (neg > 100) neg = 100;
    ikitaku_hunt_arrive(&hunt, "pos", pos);
    ikitaku_hunt_arrive(&hunt, "neg", neg);
    out->hunt_next = hunt.next_probe;
    out->hunt_hold = hunt.solved;

    /* Macro: Kant — template rule is synthetic a priori if no run-bag
       evidence beyond this pair; appearance is the match itself. */
    ikitaku_critique_t cr;
    ikitaku_critique("partial want is a witness",
                     1, 0, 1, out->insufficient_priori, &cr);
    snprintf(out->critique_name, sizeof out->critique_name, "%s", cr.axiom_name);
    if (out->insufficient_priori)
        snprintf(out->english, sizeof out->english,
                 "tstrat INSUFFICIENT priori (xp=%d) mild-diss=%d confuse=%d "
                 "best mse=%d n=%d hunt next=%d %s kant=%s",
                 out->xp_known, out->dissonance, out->confusion,
                 best_mse > 900 ? -1 : best_mse, out->n,
                 out->hunt_next, out->hunt_hold ? "HOLD" : "open",
                 out->critique_name);
    else
        snprintf(out->english, sizeof out->english,
                 "tstrat xp=%d best mse=%d/%d slices hunt next=%d %s kant=%s",
                 out->xp_known, best_mse > 900 ? -1 : best_mse, out->n,
                 out->hunt_next, out->hunt_hold ? "HOLD" : "open",
                 out->critique_name);
    ikitaku_xp_record("template-strategy", "seek", "template",
                      out->n, out->best >= 0 ? 1 : -1);
    return out->best;
}

static void desire_copy(ikitaku_series_t* dst, const ikitaku_series_t* src) {
    *dst = *src;
}

static void desire_smooth(ikitaku_series_t* s, int w) {
    if (w < 3) w = 3;
    if ((w & 1) == 0) w++;
    int h = w / 2;
    double tmp[IKITAKU_SERIES_MAX];
    int n = s->n;
    for (int i = 0; i < n; ++i) {
        double acc = 0; int c = 0;
        for (int k = -h; k <= h; ++k) {
            int j = i + k;
            if (j < 0 || j >= n) continue;
            acc += s->values[j]; c++;
        }
        tmp[i] = c ? acc / c : s->values[i];
    }
    for (int i = 0; i < n; ++i) s->values[i] = tmp[i];
}

static void desire_shift(ikitaku_series_t* s, int k) {
    int n = s->n;
    if (n < 2) return;
    k %= n; if (k < 0) k += n;
    double tmp[IKITAKU_SERIES_MAX];
    for (int i = 0; i < n; ++i) tmp[i] = s->values[(i + k) % n];
    for (int i = 0; i < n; ++i) s->values[i] = tmp[i];
}

static void desire_neg(ikitaku_series_t* s) {
    for (int i = 0; i < s->n; ++i) s->values[i] = -s->values[i];
}

static void desire_log_step(ikitaku_desire_t* d, const char* verb, const char* how,
                            int before, int after, int spent) {
    if (d->n_steps >= IKITAKU_DESIRE_STEPS) return;
    ikitaku_desire_step_t* st = &d->step[d->n_steps++];
    snprintf(st->verb, sizeof st->verb, "%s", verb);
    snprintf(st->how, sizeof st->how, "%s", how);
    st->mse_before = before;
    st->mse_after = after;
    st->improved = after < before;
    st->budget_spent = spent;
    int len = (int)strlen(d->chain);
    if (len + 16 < (int)sizeof d->chain)
        snprintf(d->chain + len, sizeof d->chain - len, "%s%s",
                 len ? "+" : "", verb);
}

int ikitaku_desire_init(ikitaku_desire_t* d,
                        const ikitaku_series_t* have,
                        const ikitaku_series_t* want,
                        const char* want_note) {
    if (!d || !have || !want) return -1;
    memset(d, 0, sizeof *d);
    desire_copy(&d->have, have);
    desire_copy(&d->want, want);
    desire_copy(&d->work, have);
    d->delta.kind = have->kind;
    d->delta.n = have->n < want->n ? have->n : want->n;
    for (int i = 0; i < d->delta.n; ++i)
        d->delta.values[i] = want->values[i] - have->values[i];
    snprintf(d->want_note, sizeof d->want_note, "%s",
             want_note ? want_note : "closer to that other bag");
    d->budget = 80;
    d->desperation = 20;
    d->focus = 20;
    d->mse_have = desire_mse100(have, want);
    d->mse_best = d->mse_have;
    return 0;
}

int ikitaku_desire_possible(ikitaku_desire_t* d) {
    if (!d) return -1;
    ikitaku_series_analysis_t ah, aw, ad;
    memset(&ah, 0, sizeof ah);
    memset(&aw, 0, sizeof aw);
    memset(&ad, 0, sizeof ad);
    ikitaku_series_analyze(&d->have, &ah);
    ikitaku_series_analyze(&d->want, &aw);
    ikitaku_series_analyze(&d->delta, &ad);
    snprintf(d->have_pat, sizeof d->have_pat, "%s",
             ah.form_r4[0] ? ah.form_r4 : ah.english);
    snprintf(d->want_pat, sizeof d->want_pat, "%s",
             aw.form_r4[0] ? aw.form_r4 : aw.english);

    int conf = 40;
    if (d->have.n >= 8 && d->want.n >= 8) conf += 10;
    if (abs(d->have.n - d->want.n) <= 2) conf += 10;
    else if (d->want.n * 2 < d->have.n || d->have.n * 2 < d->want.n)
        conf += 6; /* partial section is allowed context */
    else conf -= 8;
    /* similar amplitude */
    double hmax = 0, wmax = 0;
    for (int i = 0; i < d->have.n; ++i)
        if (fabs(d->have.values[i]) > hmax) hmax = fabs(d->have.values[i]);
    for (int i = 0; i < d->want.n; ++i)
        if (fabs(d->want.values[i]) > wmax) wmax = fabs(d->want.values[i]);
    if (hmax > 1e-6 && wmax > 1e-6) {
        double r = hmax / wmax;
        if (r > 0.25 && r < 4.0) conf += 15;
        else conf -= 10;
    }
    /* both structured or delta structured → not impossible */
    if (ah.regularity > 0.25 || aw.regularity > 0.25) conf += 10;
    if (ad.regularity > 0.35) conf += 8;
    if (ah.primary == IKITAKU_PAT_IRREGULAR &&
        aw.primary != IKITAKU_PAT_IRREGULAR)
        conf += 5; /* filter might exist */
    if (conf < 0) conf = 0;
    if (conf > 95) conf = 95;
    d->possible_conf = conf;
    /* not-impossible: we only reject if clearly mismatched */
    d->possible = (conf >= 35 && d->have.n >= 4 && d->want.n >= 4) ? 1 : 0;
    return d->possible;
}

int ikitaku_desire_pursue(ikitaku_desire_t* d) {
    if (!d) return -1;
    if (!d->possible) ikitaku_desire_possible(d);
    if (!d->possible) {
        snprintf(d->english, sizeof d->english,
                 "desire: impossible-ish (conf=%d) note=%s",
                 d->possible_conf, d->want_note);
        return 0;
    }
    /* pass confidence vs desperation → extra budget */
    if (d->possible_conf + d->desperation >= 50)
        d->budget += 60 + d->possible_conf;

    auto try_one = [&](const char* verb, const char* how,
                       void (*fn)(ikitaku_series_t*, int), int arg) {
        if (d->n_steps >= IKITAKU_DESIRE_STEPS) return;
        if (d->budget_used >= d->budget) return;
        int before = desire_mse100(&d->work, &d->want);
        ikitaku_series_t snap = d->work;
        fn(&d->work, arg);
        int after = desire_mse100(&d->work, &d->want);
        int spent = 12 + (before / 8);
        d->budget_used += spent;
        desire_log_step(d, verb, how, before, after, spent);
        if (after < d->mse_best) {
            d->mse_best = after;
            d->focus += 18;
            d->budget += 40; /* reward successful delta */
        } else {
            d->work = snap; /* revert failed transform */
            d->focus -= 2;
            if (d->focus < 5) d->focus = 5;
        }
    };

    /* platonic tries — not labeled as "phase of sine" */
    try_one("filter", "smooth-3", [](ikitaku_series_t* s, int){ desire_smooth(s, 3); }, 3);
    try_one("filter", "smooth-5", [](ikitaku_series_t* s, int){ desire_smooth(s, 5); }, 5);
    try_one("map", "negate", [](ikitaku_series_t* s, int){ desire_neg(s); }, 0);

    /* circular shifts = invert-along-repeat-axis without naming trig */
    int n = d->work.n;
    int cands[] = { n/4, n/4-1, n/4+1, n/6, n/3, n/2, 31, 30, 32, 16, 8 };
    int nc = (int)(sizeof cands / sizeof cands[0]);
    for (int i = 0; i < nc && d->n_steps < IKITAKU_DESIRE_STEPS; ++i) {
        int k = cands[i];
        if (k < 1 || k >= n) continue;
        char how[48];
        snprintf(how, sizeof how, "circ-shift %d", k);
        try_one("invert", how, desire_shift, k);
        if (d->mse_best <= 12) break;
    }

    d->solved = (d->mse_best <= 18) ? 1 : 0;
    desire_mse100_win(&d->work, &d->want, &d->win_off, &d->win_span);
    snprintf(d->english, sizeof d->english,
             "desire %s mse %d→%d possible=%d/%d focus=%d win@%d/%d chain=%s note=%s",
             d->solved ? "HOLD" : "open",
             d->mse_have, d->mse_best,
             d->possible, d->possible_conf, d->focus,
             d->win_off, d->win_span,
             d->chain[0] ? d->chain : "-",
             d->want_note);
    return d->solved;
}

int ikitaku_need_context(const char* rule, const ikitaku_gap_report_t* gaps,
                         ikitaku_context_ask_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    ikitaku_xp_t xp;
    int known = rule ? ikitaku_xp_lookup(rule, &xp) : 0;
    int gap_sev = gaps ? gaps->joint_severity : 0;
    if (known == 0 && gap_sev >= 40) {
        out->fail = IKITAKU_AEC_NO_CONTEXT;
        out->waiting = 1;
        snprintf(out->ask, sizeof out->ask,
                 "no L3 cases for '%s' (aec=no-context). what is this at R4/R3?",
                 rule ? rule : "?");
    } else if (known == 0) {
        out->fail = IKITAKU_AEC_NO_CONTEXT;
        out->waiting = 1;
        snprintf(out->ask, sizeof out->ask,
                 "I don't have context for '%s'. teach a form with conf interval.",
                 rule ? rule : "?");
    } else if (gap_sev >= 50) {
        out->fail = IKITAKU_AEC_WEAK_ASSOC;
        out->waiting = 1;
        snprintf(out->ask, sizeof out->ask,
                 "assoc is thin on '%s' (aec=weak-assoc). which attributes overlap?",
                 rule ? rule : "?");
    } else {
        out->fail = IKITAKU_AEC_OK;
        snprintf(out->ask, sizeof out->ask, "have %d cases for '%s'", known, rule ? rule : "?");
    }
    snprintf(out->english, sizeof out->english,
             "need-context fail=%d waiting=%d ask=%s",
             out->fail, out->waiting, out->ask);
    return out->waiting;
}

int ikitaku_teach_context(ikitaku_context_ask_t* a,
                          const char* fact, int conf_lo, int conf_hi) {
    if (!a || !fact) return -1;
    snprintf(a->taught, sizeof a->taught, "%s", fact);
    a->teach_lo = conf_lo;
    a->teach_hi = conf_hi;
    a->waiting = 0;
    if (a->fail == IKITAKU_AEC_NO_CONTEXT)
        a->fail = IKITAKU_AEC_OK;
    ikitaku_xp_record(fact, "info", "teach", (conf_lo + conf_hi) / 2, 1);
    snprintf(a->english, sizeof a->english,
             "taught [%d-%d]: %s", conf_lo, conf_hi, fact);
    return 0;
}

int ikitaku_metaphor(const ikitaku_attrbag_t* a, const ikitaku_attrbag_t* b,
                     const char* r5, const char* r4a, const char* r4b,
                     ikitaku_metaphor_t* out) {
    if (!a || !b || !out) return -1;
    memset(out, 0, sizeof *out);
    if (r5) snprintf(out->r5, sizeof out->r5, "%s", r5);
    if (r4a) snprintf(out->r4_a, sizeof out->r4_a, "%s", r4a);
    if (r4b) snprintf(out->r4_b, sizeof out->r4_b, "%s", r4b);
    out->same_r5 = (r5 && r5[0]) ? 1 : 0;
    out->same_r4 = (r4a && r4b && strcmp(r4a, r4b)==0) ? 1 : 0;
    char sh[80]; sh[0] = 0;
    for (int i = 0; i < a->n; ++i) {
        int hit = 0;
        for (int j = 0; j < b->n; ++j)
            if (strcmp(a->attr[i], b->attr[j])==0) hit = 1;
        if (hit) {
            out->shared++;
            int L = (int)strlen(sh);
            snprintf(sh + L, sizeof sh - L, "%s%s", L ? "," : "", a->attr[i]);
        } else out->only_a++;
    }
    for (int j = 0; j < b->n; ++j) {
        int hit = 0;
        for (int i = 0; i < a->n; ++i)
            if (strcmp(b->attr[j], a->attr[i])==0) hit = 1;
        if (!hit) out->only_b++;
    }
    snprintf(out->shared_list, sizeof out->shared_list, "%s", sh);
    int uni = out->shared + out->only_a + out->only_b;
    out->jaccard = uni ? (out->shared * 100 / uni) : 0;
    snprintf(out->english, sizeof out->english,
             "metaphor j=%d shared=[%s] onlyA=%d onlyB=%d r5=%s r4 %s/%s %s",
             out->jaccard, sh[0]?sh:"-", out->only_a, out->only_b,
             out->r5[0]?out->r5:"-",
             out->r4_a[0]?out->r4_a:"-", out->r4_b[0]?out->r4_b:"-",
             out->same_r4 ? "same-family" : "diff-R4");
    return out->jaccard;
}

int ikitaku_alts_rank(ikitaku_alts_t* out,
                      const char** r4, const int* conf, int n) {
    if (!out || !r4 || !conf) return -1;
    memset(out, 0, sizeof *out);
    if (n > IKITAKU_ALT_MAX) n = IKITAKU_ALT_MAX;
    for (int i = 0; i < n; ++i) {
        out->alt[i].conf = conf[i];
        snprintf(out->alt[i].r4, sizeof out->alt[i].r4, "%s", r4[i]);
    }
    out->n = n;
    for (int i = 0; i < n; ++i)
        for (int j = i+1; j < n; ++j)
            if (out->alt[j].conf > out->alt[i].conf) {
                ikitaku_alt_t t = out->alt[i];
                out->alt[i] = out->alt[j];
                out->alt[j] = t;
            }
    out->chosen = -1;
    snprintf(out->english, sizeof out->english,
             "alts n=%d top=%s/%d (try falling conf; several R4 may hold)",
             n, n?out->alt[0].r4:"-", n?out->alt[0].conf:0);
    return n;
}

int ikitaku_alts_next(ikitaku_alts_t* a) {
    if (!a) return -1;
    for (int i = 0; i < a->n; ++i) {
        if (a->alt[i].tried) continue;
        a->alt[i].tried = 1;
        a->chosen = i;
        return i;
    }
    return -1;
}

int ikitaku_form_attrbag(const char* form, ikitaku_attrbag_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!form || !form[0]) form = "map";
    snprintf(out->name, sizeof out->name, "%s", form);
    auto add = [&](const char* a) {
        if (!a || !a[0] || out->n >= IKITAKU_ATTR_MAX) return;
        for (int i = 0; i < out->n; ++i)
            if (!strcmp(out->attr[i], a)) return;
        snprintf(out->attr[out->n++], sizeof out->attr[0], "%s", a);
    };
    add(form);
    ikitaku_r5_verb_t v;
    if (ikitaku_r5_lookup(form, &v) == 0) {
        add(v.verb[0] ? v.verb : form);
        add("r5");
    }
    const char* x = strstr(form, "_x_");
    if (x && x != form) {
        char left[24];
        int k = (int)(x - form);
        if (k > 23) k = 23;
        memcpy(left, form, (size_t)k);
        left[k] = 0;
        add(left);
        add(x + 3);
        add("compose");
    }
    if (strstr(form, "loop") || strstr(form, "cycle") ||
        !strcmp(form, "repeat") || !strcmp(form, "iterative") ||
        !strcmp(form, "spiral"))
        add("cycle");
    if (!strcmp(form, "origin") || strstr(form, "cause") || strstr(form, "parent"))
        add("origin");
    if (!strcmp(form, "split") || !strcmp(form, "join") ||
        !strcmp(form, "emit") || !strcmp(form, "absorb"))
        add("pole");
    /* know-condition ≡ bound of evaluate (transcendental meaning, not a name) */
    if (!strcmp(form, "know-condition") || !strcmp(form, "transcendental") ||
        !strcmp(form, "apperception") || !strcmp(form, "condition-of-knowing")) {
        add("bound");
        add("theorize");
        add("condition");
        add("evaluate");
    }
    if (!strcmp(form, "bound") || !strcmp(form, "theorize")) {
        add("condition");
        add("evaluate");
        add("bound");
        add("theorize");
    }
    int L = ikitaku_form_layer_guess(form);
    char ly[8];
    snprintf(ly, sizeof ly, "L%d", L);
    add(ly);
    return out->n;
}

static int metaphor_is_basial(const char* name) {
    if (!name || !name[0]) return 1;
    static const char* lock[] = {
        "scientific","hegelian","kant","bayesian","occam",
        "iterative","waterfall","origin","ladder", NULL
    };
    for (int i = 0; lock[i]; ++i)
        if (!strcmp(name, lock[i])) return 1;
    return 0;
}

int ikitaku_metaphor_plan(const char* src, const char* dst,
                          int persist_novel,
                          ikitaku_metaphor_plan_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!src || !src[0]) src = "map";
    if (!dst || !dst[0]) dst = "order";
    snprintf(out->src, sizeof out->src, "%s", src);
    snprintf(out->dst, sizeof out->dst, "%s", dst);
    out->propose_rc = -1;

    ikitaku_attrbag_t ba, bb;
    ikitaku_form_attrbag(src, &ba);
    ikitaku_form_attrbag(dst, &bb);
    ikitaku_metaphor_t meta;
    memset(&meta, 0, sizeof meta);
    ikitaku_metaphor(&ba, &bb, src, src, dst, &meta);
    out->jaccard = meta.jaccard;

    /* parent climb */
    const char* xs = strstr(src, "_x_");
    if (xs && xs != src) {
        int k = (int)(xs - src); if (k > 23) k = 23;
        memcpy(out->parent_src, src, (size_t)k); out->parent_src[k] = 0;
    } else {
        ikitaku_r5_verb_t v;
        snprintf(out->parent_src, sizeof out->parent_src, "%s",
                 ikitaku_r5_lookup(src, &v) == 0 ? src : "map");
    }
    const char* xd = strstr(dst, "_x_");
    if (xd && xd != dst) {
        int k = (int)(xd - dst); if (k > 23) k = 23;
        memcpy(out->parent_dst, dst, (size_t)k); out->parent_dst[k] = 0;
    } else {
        ikitaku_r5_verb_t v;
        snprintf(out->parent_dst, sizeof out->parent_dst, "%s",
                 ikitaku_r5_lookup(dst, &v) == 0 ? dst : "map");
    }
    out->climbed = strcmp(out->parent_src, src) != 0 ||
                   strcmp(out->parent_dst, dst) != 0;

    ikitaku_analogy_t an;
    memset(&an, 0, sizeof an);
    if (out->climbed)
        ikitaku_analogy(src, out->parent_src, dst, out->parent_dst, &an);
    else
        ikitaku_analogy(src, dst, out->parent_src, out->parent_dst, &an);
    out->analog_sim = an.sim;
    snprintf(out->nature, sizeof out->nature, "%s",
             an.nature[0] ? an.nature : "map");

    out->layer_src = ikitaku_form_layer_guess(src);
    out->layer_dst = ikitaku_form_layer_guess(dst);
    ikitaku_assoc_axis_t ax;
    memset(&ax, 0, sizeof ax);
    ikitaku_associate_axis(src, out->layer_src, dst, out->layer_dst, &ax);
    out->vert_score = ax.assoc;

    /* origin vs hold */
    if (out->climbed && out->analog_sim >= 40) {
        snprintf(out->origin_lead, sizeof out->origin_lead, "peel-parent");
    } else if (out->jaccard < 25 && out->analog_sim < 40) {
        snprintf(out->origin_lead, sizeof out->origin_lead, "origin");
    } else {
        snprintf(out->origin_lead, sizeof out->origin_lead, "hold");
    }

    /* R6 strategy pick from metaphor shape */
    int cycle = 0;
    for (int i = 0; i < ba.n; ++i) if (!strcmp(ba.attr[i], "cycle")) cycle = 1;
    for (int i = 0; i < bb.n; ++i) if (!strcmp(bb.attr[i], "cycle")) cycle = 1;
    int pole = 0;
    for (int i = 0; i < ba.n; ++i) if (!strcmp(ba.attr[i], "pole")) pole = 1;
    for (int i = 0; i < bb.n; ++i) if (!strcmp(bb.attr[i], "pole")) pole = 1;

    if (cycle) {
        snprintf(out->strategy, sizeof out->strategy, "iterative");
        snprintf(out->form, sizeof out->form, "cycle metaphor %s~%s", src, dst);
    } else if (pole) {
        snprintf(out->strategy, sizeof out->strategy, "hegelian");
        snprintf(out->form, sizeof out->form, "pole metaphor %s~%s", src, dst);
    } else if (out->jaccard >= 50) {
        snprintf(out->strategy, sizeof out->strategy, "occam");
        snprintf(out->form, sizeof out->form, "shared-attr collapse %s~%s", src, dst);
    } else if (!strcmp(out->origin_lead, "origin") ||
               !strcmp(out->origin_lead, "peel-parent")) {
        snprintf(out->strategy, sizeof out->strategy, "origin");
        snprintf(out->form, sizeof out->form, "parent/origin %s→%s via %s/%s",
                 src, dst, out->parent_src, out->parent_dst);
    } else {
        snprintf(out->strategy, sizeof out->strategy, "%s_x_%s",
                 strcmp(src, dst) < 0 ? src : dst,
                 strcmp(src, dst) < 0 ? dst : src);
        if ((int)strlen(out->strategy) > 23)
            snprintf(out->strategy, sizeof out->strategy, "map_x_order");
        snprintf(out->form, sizeof out->form, "analogy compose %s:%s nature=%s",
                 src, dst, out->nature);
    }

    /* Novel persist: only non-basial composed names with enough analog/meta */
    int strong = (out->analog_sim >= 55) || (out->jaccard >= 40 && out->vert_score >= 20);
    if (persist_novel && strong && !metaphor_is_basial(out->strategy) &&
        strchr(out->strategy, '_')) {
        out->propose_rc = ikitaku_strategy_propose(out->strategy, out->form, 0);
        out->novel = (out->propose_rc == 0 || out->propose_rc == 1);
        if (out->novel) {
            int drop = (out->analog_sim + out->jaccard) / 4;
            if (drop < 8) drop = 8;
            ikitaku_strategy_t scored;
            memset(&scored, 0, sizeof scored);
            out->novel_score = ikitaku_strategy_score(out->strategy,
                                                     50, 50 - drop, &scored);
            if (out->novel_score < 0) out->novel_score = 0;
        }
    }

    snprintf(out->english, sizeof out->english,
             "metaphor-plan %s~%s j=%d analog=%s/%d climb=%d L%d↔L%d vert=%d "
             "origin=%s strat=%s novel=%d score=%d %s",
             out->src, out->dst, out->jaccard, out->nature, out->analog_sim,
             out->climbed, out->layer_src, out->layer_dst, out->vert_score,
             out->origin_lead, out->strategy, out->novel, out->novel_score,
             meta.english);
    return out->analog_sim;
}
