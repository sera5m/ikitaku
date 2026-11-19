
int ikitaku_series_from_i32(const int32_t* a, int n, ikitaku_series_t* out) {
    if (!a || !out || n <= 0) return -1;
    if (n > IKITAKU_SERIES_MAX) n = IKITAKU_SERIES_MAX;
    memset(out, 0, sizeof *out);
    out->kind = IKITAKU_DATA_I32;
    out->n = n;
    for (int i = 0; i < n; ++i) out->values[i] = (double)a[i];
    return 0;
}

int ikitaku_series_from_f32(const float* a, int n, ikitaku_series_t* out) {
    if (!a || !out || n <= 0) return -1;
    if (n > IKITAKU_SERIES_MAX) n = IKITAKU_SERIES_MAX;
    memset(out, 0, sizeof *out);
    out->kind = IKITAKU_DATA_F32;
    out->n = n;
    for (int i = 0; i < n; ++i) out->values[i] = (double)a[i];
    return 0;
}

int ikitaku_series_analyze(const ikitaku_series_t* s, ikitaku_series_analysis_t* out) {
    if (!s || !out || s->n < 1) return -1;
    memset(out, 0, sizeof *out);
    int n = s->n;
    // discrete derivative f' and f''
    out->deriv_n = n > 0 ? n - 1 : 0;
    for (int i = 0; i < out->deriv_n; ++i)
        out->deriv[i] = s->values[i + 1] - s->values[i];
    for (int i = 0; i + 1 < out->deriv_n; ++i)
        out->deriv2[i] = out->deriv[i + 1] - out->deriv[i];

    // local min/max on interior
    for (int i = 1; i + 1 < n; ++i) {
        if (s->values[i] <= s->values[i-1] && s->values[i] <= s->values[i+1]
            && out->local_min_n < 16)
            out->local_min_idx[out->local_min_n++] = i;
        if (s->values[i] >= s->values[i-1] && s->values[i] >= s->values[i+1]
            && out->local_max_n < 16)
            out->local_max_idx[out->local_max_n++] = i;
    }

    // slope estimate
    if (n >= 2)
        out->slope_est = (s->values[n-1] - s->values[0]) / (double)(n - 1);

    // regularity: variance of |deriv|
    double mean_abs = 0;
    for (int i = 0; i < out->deriv_n; ++i)
        mean_abs += out->deriv[i] < 0 ? -out->deriv[i] : out->deriv[i];
    if (out->deriv_n) mean_abs /= out->deriv_n;
    double var = 0;
    for (int i = 0; i < out->deriv_n; ++i) {
        double a = out->deriv[i] < 0 ? -out->deriv[i] : out->deriv[i];
        double d = a - mean_abs;
        var += d * d;
    }
    if (out->deriv_n) var /= out->deriv_n;
    out->regularity = mean_abs < 1e-12 ? 1.0 : 1.0 / (1.0 + var / (mean_abs * mean_abs + 1e-9));

    // pattern classification (parallel readings allowed)
    int constant = 1;
    for (int i = 1; i < n; ++i)
        if (s->values[i] != s->values[0]) { constant = 0; break; }
    if (constant) {
        out->primary = IKITAKU_PAT_CONSTANT;
        out->conf_primary = 95;
        snprintf(out->form_r5, sizeof out->form_r5, "info");
        snprintf(out->form_r4, sizeof out->form_r4, "literal");
        snprintf(out->english, sizeof out->english,
                 "Values stay constant — like standing still, not walking.");
        return 0;
    }

    // check return-to-start (circle / periodic-ish)
    double start = s->values[0], end = s->values[n-1];
    int returns = (end - start < 1e-6 && end - start > -1e-6) && n >= 4;
    // monotone
    int up = 1, down = 1;
    for (int i = 1; i < n; ++i) {
        if (s->values[i] < s->values[i-1]) up = 0;
        if (s->values[i] > s->values[i-1]) down = 0;
    }
    // oscillation: many sign flips in deriv
    int flips = 0;
    for (int i = 1; i < out->deriv_n; ++i)
        if (out->deriv[i] * out->deriv[i-1] < 0) flips++;

    if (returns && flips >= 2) {
        out->primary = IKITAKU_PAT_PERIODIC;
        out->conf_primary = 80;
        snprintf(out->form_r5, sizeof out->form_r5, "repeat");
        snprintf(out->form_r4, sizeof out->form_r4, "cycle");
        snprintf(out->english, sizeof out->english,
                 "Path returns to the start with oscillations — like walking in a circle "
                 "(loop macro); direction changes are micro-steps of that loop.");
        out->parallel = IKITAKU_PAT_OSCILLATE;
        out->conf_parallel = 70;
    } else if (up || down) {
        out->primary = IKITAKU_PAT_MONOTONE;
        out->conf_primary = 85;
        snprintf(out->form_r5, sizeof out->form_r5, "map");
        snprintf(out->form_r4, sizeof out->form_r4, "linear_scan");
        snprintf(out->english, sizeof out->english,
                 "Values move steadily %s — like walking straight in one direction "
                 "(monotone path, not a closed loop).",
                 up ? "up" : "down");
        if (out->deriv_n >= 2) {
            // check near-linear second derivative small
            double m2 = 0;
            int n2 = out->deriv_n > 1 ? out->deriv_n - 1 : 0;
            for (int i = 0; i < n2; ++i) m2 += out->deriv2[i] < 0 ? -out->deriv2[i] : out->deriv2[i];
            if (n2 && m2 / n2 < mean_abs * 0.15 + 1e-9) {
                out->parallel = IKITAKU_PAT_LINEAR;
                out->conf_parallel = 75;
            }
        }
    } else if (flips >= (out->deriv_n > 2 ? out->deriv_n / 3 : 1)) {
        out->primary = IKITAKU_PAT_OSCILLATE;
        out->conf_primary = 75;
        snprintf(out->form_r5, sizeof out->form_r5, "repeat");
        snprintf(out->form_r4, sizeof out->form_r4, "oscillate");
        snprintf(out->english, sizeof out->english,
                 "Direction keeps reversing — forward/back micro-motion; "
                 "may sit under a larger loop pattern even if not closed yet.");
        out->parallel = IKITAKU_PAT_IRREGULAR;
        out->conf_parallel = 40;
    } else if (out->local_min_n + out->local_max_n >= 1 && out->regularity < 0.5) {
        out->primary = IKITAKU_PAT_IRREGULAR;
        out->conf_primary = 60;
        snprintf(out->form_r5, sizeof out->form_r5, "unknown");
        snprintf(out->form_r4, sizeof out->form_r4, "irregular");
        snprintf(out->english, sizeof out->english,
                 "Irregular peaks/troughs with low regularity — pattern not a simple walk.");
    } else {
        out->primary = IKITAKU_PAT_STEP;
        out->conf_primary = 55;
        snprintf(out->form_r5, sizeof out->form_r5, "map");
        snprintf(out->form_r4, sizeof out->form_r4, "piecewise");
        snprintf(out->english, sizeof out->english,
                 "Piecewise motion with mixed slopes — composite path.");
    }
    return 0;
}

int ikitaku_series_compare(const ikitaku_series_analysis_t* a,
                           const ikitaku_series_analysis_t* b,
                           char* english, int elen, int* assoc_score) {
    if (!a || !b) return -1;
    int score = 0;
    if (a->primary == b->primary) score += 40;
    if (a->form_r5[0] && strcmp(a->form_r5, b->form_r5) == 0) score += 30;
    if (a->form_r4[0] && strcmp(a->form_r4, b->form_r4) == 0) score += 15;
    // regularity closeness
    double rd = a->regularity - b->regularity;
    if (rd < 0) rd = -rd;
    if (rd < 0.15) score += 15;
    if (score > 100) score = 100;
    if (assoc_score) *assoc_score = score;
    if (english && elen > 0) {
        snprintf(english, (size_t)elen,
                 "Series A is '%s' (R5=%s); B is '%s' (R5=%s). Association %d/100. "
                 "%s",
                 a->english, a->form_r5, b->english, b->form_r5, score,
                 score >= 70 ? "Same family of walk." :
                 score >= 40 ? "Related but not identical (glass vs ceramic)." :
                               "Distinct motion patterns.");
    }
    return score;
}

// ===================== L6 reasoning-model cascade =====================

int ikitaku_l6_cascade(const ikitaku_session_t* s,
                       const ikitaku_series_analysis_t* series_opt,
                       ikitaku_l6_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->chosen = -1;

    // Score each model from session evidence
    for (int i = 0; i < 6; ++i) {
        out->models[i].model = (uint8_t)i;
        out->models[i].conf = 0;
    }
    if (s) {
        int plugin_c = 0, n = 0;
        for (int i = 0; i < s->hypo_count; ++i) {
            if (s->hypos[i].method == IKITAKU_THINK_DEDUCTIVE) {
                plugin_c += s->hypos[i].confidence; n++;
            }
            if (s->hypos[i].synthesized)
                out->models[IKITAKU_MODEL_SYNTH].conf =
                    out->models[IKITAKU_MODEL_SYNTH].conf < s->hypos[i].confidence
                    ? s->hypos[i].confidence : out->models[IKITAKU_MODEL_SYNTH].conf;
            if (s->hypos[i].method == IKITAKU_THINK_DIALECTIC)
                out->models[IKITAKU_MODEL_DIALECTIC].conf =
                    out->models[IKITAKU_MODEL_DIALECTIC].conf < s->hypos[i].confidence
                    ? s->hypos[i].confidence : out->models[IKITAKU_MODEL_DIALECTIC].conf;
            if (s->hypos[i].novel)
                out->models[IKITAKU_MODEL_NOVEL].conf = 40;
        }
        if (n) out->models[IKITAKU_MODEL_PLUGIN].conf = plugin_c / n;
        out->models[IKITAKU_MODEL_ASSOCIATE].conf =
            s->hypo_count >= 2 ? 70 : 40;
        out->models[IKITAKU_MODEL_PLUGIN].applied = 1;
        out->models[IKITAKU_MODEL_ASSOCIATE].applied = 1;
        if (out->models[IKITAKU_MODEL_SYNTH].conf)
            out->models[IKITAKU_MODEL_SYNTH].applied = 1;
        if (out->models[IKITAKU_MODEL_DIALECTIC].conf)
            out->models[IKITAKU_MODEL_DIALECTIC].applied = 1;
        if (s->accepted >= 0)
            out->models[IKITAKU_MODEL_PLUGIN].succeeded = 1;
    }
    if (series_opt) {
        out->models[IKITAKU_MODEL_SERIES].conf = series_opt->conf_primary;
        out->models[IKITAKU_MODEL_SERIES].applied = 1;
        snprintf(out->models[IKITAKU_MODEL_SERIES].note, 96, "%s", series_opt->english);
    }

    // Cascade: try models sorted by conf
    int order[6] = {0,1,2,3,4,5};
    for (int i = 0; i < 6; ++i)
        for (int j = i + 1; j < 6; ++j)
            if (out->models[order[j]].conf > out->models[order[i]].conf) {
                int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
            }

    for (int k = 0; k < 6; ++k) {
        int m = order[k];
        out->cascade_depth++;
        if (out->models[m].conf < 40) continue;
        // "success" if conf high enough or already succeeded
        if (out->models[m].succeeded || out->models[m].conf >= 70) {
            out->chosen = m;
            out->models[m].succeeded = 1;
            break;
        }
        // mid conf — try but may fall through
        if (out->models[m].conf >= 55 && out->chosen < 0)
            out->chosen = m;
    }
    if (out->chosen < 0) {
        out->chosen = IKITAKU_MODEL_NOVEL;
        out->models[IKITAKU_MODEL_NOVEL].applied = 1;
        out->models[IKITAKU_MODEL_NOVEL].conf = 30;
    }

    static const char* mname[] = {
        "plugin-catalog", "association", "dialectic", "synthesis",
        "series-pattern", "novel"
    };
    snprintf(out->english, sizeof out->english,
             "L6 cascade tried %d models; chose **%s** (conf %d). "
             "Reasoning models are tracked as data: plugin=%d assoc=%d dial=%d "
             "synth=%d series=%d novel=%d.",
             out->cascade_depth,
             mname[out->chosen], out->models[out->chosen].conf,
             out->models[0].conf, out->models[1].conf, out->models[2].conf,
             out->models[3].conf, out->models[4].conf, out->models[5].conf);
    return out->chosen;
}



// ===================== Cognitive dissonance + fallacies =====================

int ikitaku_detect_dissonance(const ikitaku_session_t* s,
                              ikitaku_dissonance_report_t* out) {
    if (!s || !out) return -1;
    memset(out, 0, sizeof *out);
    for (int i = 0; i < s->hypo_count && out->n < IKITAKU_MAX_DISS; ++i) {
        for (int j = i + 1; j < s->hypo_count && out->n < IKITAKU_MAX_DISS; ++j) {
            const ikitaku_hypo_t* a = &s->hypos[i];
            const ikitaku_hypo_t* b = &s->hypos[j];
            // Same purpose, opposite rewrite_ok → dissonance
            if (strcmp(a->purpose, b->purpose) == 0
                && a->rewrite_ok != b->rewrite_ok
                && a->confidence >= 60 && b->confidence >= 60) {
                ikitaku_dissonance_t* d = &out->items[out->n++];
                d->hypo_a = i; d->hypo_b = j;
                snprintf(d->claim, sizeof d->claim, "F=%s both pure-rewrite and impure", a->purpose);
                snprintf(d->why_incongruous, sizeof d->why_incongruous,
                         "models disagree: one claims rewrite_ok, other blocks — attributes incongruous");
                d->severity = 70;
                d->fallacy = IKITAKU_FALLACY_FALSE_DICHOTOMY;
                d->deferred = 1;
            }
            // Equivocation: same form_id fragment different purposes
            if (a->form_id[0] && b->form_id[0]
                && strstr(a->form_id, "repeat") && strstr(b->form_id, "repeat")
                && strcmp(a->purpose, b->purpose) != 0
                && a->purpose[0] && b->purpose[0]) {
                ikitaku_dissonance_t* d = &out->items[out->n++];
                d->hypo_a = i; d->hypo_b = j;
                snprintf(d->claim, sizeof d->claim, "repeat-named forms with F=%s vs %s", a->purpose, b->purpose);
                snprintf(d->why_incongruous, sizeof d->why_incongruous,
                         "equivocation risk: 'repeat' token used at different conceptual ranks");
                d->severity = 45;
                d->fallacy = IKITAKU_FALLACY_EQUIVOCATION;
                d->deferred = 1;
            }
        }
        // Hasty generalization: single low-evidence novel accepted-like high conf
        if (s->hypos[i].novel && s->hypos[i].confidence >= 50) {
            ikitaku_dissonance_t* d = &out->items[out->n++];
            d->hypo_a = i; d->hypo_b = -1;
            snprintf(d->claim, sizeof d->claim, "novel hypo conf=%d", s->hypos[i].confidence);
            snprintf(d->why_incongruous, sizeof d->why_incongruous,
                     "hasty generalization: elevating unseen pattern without catalog support");
            d->severity = 55;
            d->fallacy = IKITAKU_FALLACY_HASTY_GENERALIZE;
            d->deferred = 1;
        }
        // Affirming consequent: rewrite_ok on wait
        if (s->hypos[i].rewrite_ok && strcmp(s->hypos[i].purpose, "wait") == 0) {
            ikitaku_dissonance_t* d = &out->items[out->n++];
            d->hypo_a = i; d->hypo_b = -1;
            snprintf(d->claim, sizeof d->claim, "rewrite_ok on wait");
            snprintf(d->why_incongruous, sizeof d->why_incongruous,
                     "affirming consequent: treating rewrite allowance as proof of purity for wait");
            d->severity = 85;
            d->fallacy = IKITAKU_FALLACY_AFFIRM_CONSEQUENT;
            d->deferred = 0; // hard error, not just defer
        }
    }
    return out->n;
}

// ===================== Function fitting =====================

static double mean_of(const double* v, int n) {
    double s = 0; for (int i = 0; i < n; ++i) s += v[i]; return n ? s / n : 0;
}

static int corr_score(const double* y, const double* yhat, int n) {
    if (n < 2) return 0;
    double my = mean_of(y, n), mh = mean_of(yhat, n);
    double num = 0, dy = 0, dh = 0;
    for (int i = 0; i < n; ++i) {
        double a = y[i] - my, b = yhat[i] - mh;
        num += a * b; dy += a * a; dh += b * b;
    }
    if (dy < 1e-12 || dh < 1e-12) return 0;
    double r = num / sqrt(dy * dh);
    if (r < 0) r = -r;
    return (int)(r * 100.0 + 0.5);
}

int ikitaku_series_fit(const ikitaku_series_t* s, ikitaku_fit_report_t* out) {
    if (!s || !out || s->n < 3) return -1;
    memset(out, 0, sizeof *out);
    int n = s->n;
    const double* y = s->values;
    // x = 0..n-1 as index; caller may use real domain via values scale

    // const
    {
        double m = mean_of(y, n);
        double yhat[IKITAKU_SERIES_MAX];
        for (int i = 0; i < n; ++i) yhat[i] = m;
        int c = corr_score(y, yhat, n);
        // residual var
        double rv = 0; for (int i = 0; i < n; ++i) { double d = y[i]-m; rv += d*d; }
        ikitaku_fit_t* f = &out->fits[out->n++];
        f->kind = IKITAKU_FIT_CONST;
        f->params[0] = m;
        f->conf = (rv < 1e-6) ? 95 : (c > 30 ? c / 2 : 10);
        snprintf(f->formula, sizeof f->formula, "y = %.4g", m);
        snprintf(f->english, sizeof f->english, "Constant fit");
        f->residual_score = (int)(rv < 1e-6 ? 0 : 50);
    }

    // linear least squares x=i
    {
        double sx=0,sy=0,sxx=0,sxy=0;
        for (int i = 0; i < n; ++i) {
            double x = (double)i;
            sx += x; sy += y[i]; sxx += x*x; sxy += x*y[i];
        }
        double den = n * sxx - sx * sx;
        double a = 0, b = 0;
        if (fabs(den) > 1e-12) {
            a = (n * sxy - sx * sy) / den;
            b = (sy - a * sx) / n;
        }
        double yhat[IKITAKU_SERIES_MAX];
        for (int i = 0; i < n; ++i) yhat[i] = a * i + b;
        int c = corr_score(y, yhat, n);
        ikitaku_fit_t* f = &out->fits[out->n++];
        f->kind = IKITAKU_FIT_LINEAR;
        f->params[0] = a; f->params[1] = b;
        f->conf = c;
        snprintf(f->formula, sizeof f->formula, "y = %.4g*x + %.4g", a, b);
        snprintf(f->english, sizeof f->english, "Linear (algebra) fit");
        f->residual_score = 100 - c;
    }

    // sin-like: try w in small set, k = amplitude
    {
        double amp = 0;
        double m = mean_of(y, n);
        for (int i = 0; i < n; ++i) {
            double d = y[i] - m; if (d < 0) d = -d; if (d > amp) amp = d;
        }
        int best_c = 0; double best_w = 0.5;
        double yhat[IKITAKU_SERIES_MAX];
        for (int wi = 1; wi <= 20; ++wi) {
            double w = wi * 0.15;
            for (int i = 0; i < n; ++i) yhat[i] = m + amp * sin(w * i);
            int c = corr_score(y, yhat, n);
            if (c > best_c) { best_c = c; best_w = w; }
        }
        for (int i = 0; i < n; ++i) yhat[i] = m + amp * sin(best_w * i);
        ikitaku_fit_t* f = &out->fits[out->n++];
        f->kind = IKITAKU_FIT_SIN;
        f->params[0] = amp; f->params[1] = best_w; f->params[2] = m;
        f->conf = best_c;
        snprintf(f->formula, sizeof f->formula, "y ≈ %.3g*sin(%.3g*x)+%.3g", amp, best_w, m);
        snprintf(f->english, sizeof f->english, "Sinusoidal (trig) fit");
        f->residual_score = 100 - best_c;
    }

    // tan-like: large derivative spikes
    {
        int spikes = 0;
        double max_d = 0;
        for (int i = 1; i < n; ++i) {
            double d = y[i] - y[i-1]; if (d < 0) d = -d;
            if (d > max_d) max_d = d;
        }
        double thr = max_d * 0.5;
        for (int i = 1; i < n; ++i) {
            double d = y[i] - y[i-1]; if (d < 0) d = -d;
            if (d >= thr && thr > 1e-6) spikes++;
        }
        int conf = 0;
        if (spikes >= 2 && max_d > 1.0) conf = 40 + (spikes > 5 ? 20 : spikes * 3);
        if (conf > 85) conf = 85;
        ikitaku_fit_t* f = &out->fits[out->n++];
        f->kind = IKITAKU_FIT_TAN;
        f->params[0] = max_d; f->params[1] = (double)spikes;
        f->conf = conf;
        snprintf(f->formula, sizeof f->formula, "tan-like spikes=%d maxΔ=%.3g", spikes, max_d);
        snprintf(f->english, sizeof f->english,
                 "Tan-like: steep local jumps (asymptote-ish behavior)");
        f->residual_score = 100 - conf;
    }

    // sin(tan(x))-like composite heuristic:
    // high frequency content + intermittent large jumps + non-monotone
    {
        int flips = 0;
        for (int i = 2; i < n; ++i) {
            double d0 = y[i-1]-y[i-2], d1 = y[i]-y[i-1];
            if (d0 * d1 < 0) flips++;
        }
        double amp = 0, m = mean_of(y, n);
        for (int i = 0; i < n; ++i) {
            double d = y[i]-m; if (d<0) d=-d; if (d>amp) amp=d;
        }
        // bounded roughly in [-1,1] suggests outer sin
        int bounded = (amp <= 1.05);
        int tan_fit = 0;
        for (int i = 0; i < out->n; ++i)
            if (out->fits[i].kind == IKITAKU_FIT_TAN) tan_fit = out->fits[i].conf;
        int lin_c = 0, sin_c = 0;
        for (int i = 0; i < out->n; ++i) {
            if (out->fits[i].kind == IKITAKU_FIT_LINEAR) lin_c = out->fits[i].conf;
            if (out->fits[i].kind == IKITAKU_FIT_SIN) sin_c = out->fits[i].conf;
        }
        int conf = 0;
        // obscured composite: linear fails, some oscillation, values mostly bounded
        if (bounded && lin_c < 40 && flips >= n/5)
            conf = 50 + (flips > n/4 ? 10 : 0) + (sin_c >= 50 && sin_c < 85 ? 15 : 0)
                 + (tan_fit >= 30 ? 10 : 0);
        if (bounded && flips >= n/4 && tan_fit >= 40)
            conf = conf < 55 ? 55 + (flips > n/3 ? 15 : 0) : conf;
        if (conf > 90) conf = 90;
        ikitaku_fit_t* f = &out->fits[out->n++];
        f->kind = IKITAKU_FIT_SIN_TAN;
        f->params[0] = amp; f->params[1] = (double)flips;
        f->conf = conf;
        snprintf(f->formula, sizeof f->formula, "y ~ sin(tan(x)) obscured");
        snprintf(f->english, sizeof f->english,
                 "Composite: bounded wave with tan-like inner spikes — sin(tan(.)) family");
        f->residual_score = 100 - conf;
    }

    // quadratic via 2nd differences (stable) then residual linear
    if (out->n < IKITAKU_MAX_FITS) {
        double a=0,b=0,c=0;
        if (n >= 3) {
            double sdd = 0; int ndd = 0;
            for (int i = 2; i < n; ++i) {
                sdd += (y[i] - 2*y[i-1] + y[i-2]);
                ndd++;
            }
            a = ndd ? (sdd / ndd) / 2.0 : 0; /* Δ² y ≈ 2a */
            double sx=0,sy=0,sxx=0,sxy=0;
            for (int i = 0; i < n; ++i) {
                double x = (double)i;
                double r = y[i] - a*x*x;
                sx += x; sy += r; sxx += x*x; sxy += x*r;
            }
            double den = n * sxx - sx * sx;
            if (fabs(den) > 1e-12) {
                b = (n * sxy - sx * sy) / den;
                c = (sy - b * sx) / n;
            } else c = sy / n;
        }
        double yhat[IKITAKU_SERIES_MAX];
        for (int i = 0; i < n; ++i) yhat[i] = a*i*i + b*i + c;
        int qc = corr_score(y, yhat, n);
        ikitaku_fit_t* f = &out->fits[out->n++];
        f->kind = IKITAKU_FIT_QUAD;
        f->params[0]=a; f->params[1]=b; f->params[2]=c;
        f->conf = qc;
        snprintf(f->formula, sizeof f->formula, "y = %.4g*x^2 + %.4g*x + %.4g", a,b,c);
        snprintf(f->english, sizeof f->english, "Quadratic (projectile-family) fit");
        f->residual_score = 100 - qc;
    }

    // pick best by conf; Occam: quad beats sine if within margin
    out->best = 0;
    for (int i = 1; i < out->n; ++i)
        if (out->fits[i].conf > out->fits[out->best].conf)
            out->best = i;
    {
        int qi=-1, si=-1;
        for (int i=0;i<out->n;i++) {
            if (out->fits[i].kind==IKITAKU_FIT_QUAD) qi=i;
            if (out->fits[i].kind==IKITAKU_FIT_SIN) si=i;
        }
        int m = ikitaku_occam_margin();
        if (qi>=0 && si>=0 && out->best==si
            && out->fits[qi].conf >= out->fits[si].conf - m)
            out->best = qi;
    }
    return out->n;
}

int ikitaku_algebra_from_quad(double a, double b, double c,
                              ikitaku_algebra_map_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->r5, sizeof out->r5, "map");
    snprintf(out->r4, sizeof out->r4, "polynomial");
    snprintf(out->r3, sizeof out->r3, "quadratic");
    snprintf(out->r2, sizeof out->r2, "ax^2+bx+c");
    snprintf(out->r1, sizeof out->r1, "a=%.3g b=%.3g c=%.3g", a,b,c);
    snprintf(out->r0, sizeof out->r0, "samples");
    /* quadratic formula */
    double disc = b*b - 4*a*c;
    snprintf(out->quad_formula, sizeof out->quad_formula,
             "x = (-b ± sqrt(b^2-4ac))/(2a)  disc=%.4g", disc);
    if (fabs(a) > 1e-12 && disc >= 0) {
        out->roots_ok = 1;
        out->root0 = (-b - sqrt(disc)) / (2*a);
        out->root1 = (-b + sqrt(disc)) / (2*a);
    }
    /* factoring: GCF-ish + integer quadratic factors */
    int ia = (int)lround(a), ib = (int)lround(b), ic = (int)lround(c);
    int near = (fabs(a-ia)<0.05 && fabs(b-ib)<0.05 && fabs(c-ic)<0.05);
    if (near && ia != 0) {
        /* try (px+q)(rx+s) with p*r=ia, q*s=ic, ps+qr=ib */
        int found = 0;
        for (int p = 1; p <= 16 && !found; ++p) if (ia%p==0) {
            int r = ia / p;
            for (int q = -32; q <= 32 && !found; ++q) if (q && ic%q==0) {
                int s = ic / q;
                if (p*s + q*r == ib) {
                    snprintf(out->factored, sizeof out->factored,
                             "(%d x + %d)(%d x + %d)", p,q,r,s);
                    found = 1;
                }
            }
        }
        if (!found)
            snprintf(out->factored, sizeof out->factored,
                     "GCF try: no small integer factors");
    } else {
        snprintf(out->factored, sizeof out->factored,
                 "grouping/GCF: ab+ac=a(b+c) — coeffs not near-integer");
    }
    if (out->roots_ok)
        snprintf(out->english, sizeof out->english,
                 "R5 map → R4 poly → R3 quadratic → R2 ax^2+bx+c. "
                 "Roots %.3g, %.3g. Factor %s",
                 out->root0, out->root1,
                 out->factored[0]?out->factored:"(none)");
    else
        snprintf(out->english, sizeof out->english,
                 "R5 map → R3 quadratic. No real roots. %s", out->factored);
    return 0;
}

int ikitaku_probably_nonsense(const ikitaku_fit_report_t* fits,
                              const ikitaku_series_analysis_t* an,
                              int desperation_level,
                              char* english, int elen) {
    if (!english || elen < 8) return 0;
    int best = 0, kind = 0;
    if (fits && fits->n && fits->best >= 0) {
        best = fits->fits[fits->best].conf;
        kind = fits->fits[fits->best].kind;
    }
    int irreg = an && (an->primary == IKITAKU_PAT_IRREGULAR
                    || an->regularity < 0.25);
    int residual = 0;
    if (fits && fits->n && fits->best >= 0)
        residual = fits->fits[fits->best].residual_score;
    int wrap = (kind == IKITAKU_FIT_SIN || kind == IKITAKU_FIT_TAN
             || kind == IKITAKU_FIT_SIN_TAN || kind == IKITAKU_FIT_PERIODIC_UNK);
    int shaky = an && (an->primary == IKITAKU_PAT_IRREGULAR
                    || an->primary == IKITAKU_PAT_OSCILLATE
                    || an->primary == IKITAKU_PAT_UNKNOWN
                    || an->conf_primary < 50);
    /* Pareidolia: wrap on a bag whose residual is still high — invented structure. */
    int pareidolia = wrap && residual >= 28 && (shaky || !an || an->regularity < 0.85);
    int unsure = pareidolia
              || (best < 45 && desperation_level >= 3) || (best < 35)
              || (irreg && best <= 62 && desperation_level >= 4)
              || (best <= 55 && desperation_level >= 4);
    if (unsure) {
        snprintf(english, (size_t)elen,
                 "%s — no stable law under the bag (best-fit %d, reg=%.2f, dep %d).",
                 pareidolia ? "pareidolia (wrap on entropy)"
                            : "this is probably nonsense",
                 best, an ? an->regularity : -1.0, desperation_level);
        return 1;
    }
    snprintf(english, (size_t)elen, "not calling nonsense (best-fit %d).", best);
    return 0;
}

int ikitaku_rule_scan(const ikitaku_series_t* s, ikitaku_rule_t* out) {
    if (!s || !out || s->n < 3) return -1;
    memset(out, 0, sizeof *out);
    int n = s->n;
    const double* y = s->values;
    double uniq[64]; int nu = 0;
    for (int i = 0; i < n && nu < 64; ++i) {
        int seen = 0;
        for (int k = 0; k < nu; ++k)
            if (fabs(uniq[k] - y[i]) < 1e-6) { seen = 1; break; }
        if (!seen) uniq[nu++] = y[i];
    }
    out->n_unique = nu;
    int runs = 1;
    for (int i = 1; i < n; ++i)
        if (fabs(y[i] - y[i-1]) > 1e-9) runs++;
    out->n_runs = runs;
    /* envelope rule: sign(y) is two-level even if y is analog (carrier × square) */
    {
        int spos = 0, sneg = 0;
        for (int i = 0; i < n; ++i) { if (y[i] >= 0) spos++; else sneg++; }
        if (spos >= 2 && sneg >= 2) {
            int bestp = 0, bestc = 0;
            for (int p = 2; p <= n/2 && p <= 16; ++p) {
                int ok = 0, tot = 0;
                for (int i = 0; i + p < n; ++i) {
                    tot++;
                    int a = y[i] >= 0, b = y[i+p] >= 0;
                    if (a == b) ok++;
                }
                int c = tot ? (ok * 100) / tot : 0;
                if (c > bestc) { bestc = c; bestp = p; }
            }
            if (bestc >= 70) {
                out->kind = IKITAKU_RULE_SQUARE;
                out->period = bestp;
                out->conf = bestc;
                snprintf(out->purpose, sizeof out->purpose, "map-table");
                snprintf(out->english, sizeof out->english,
                         "map-table (not decision): sign/envelope period=%d conf=%d — gate, no CFG branch.",
                         bestp, bestc);
                return out->kind;
            }
        }
    }
    if (nu == 2 && runs >= 3) {
        int bestp = 0, bestc = 0;
        for (int p = 2; p <= n/2 && p <= 16; ++p) {
            int ok = 0, tot = 0;
            for (int i = 0; i + p < n; ++i) {
                tot++;
                if (fabs(y[i] - y[i+p]) < 1e-6) ok++;
            }
            int c = tot ? (ok * 100) / tot : 0;
            if (c > bestc) { bestc = c; bestp = p; }
        }
        out->kind = IKITAKU_RULE_SQUARE;
        out->period = bestp;
        out->conf = bestc >= 50 ? bestc : 55;
        snprintf(out->purpose, sizeof out->purpose, "map-table");
        snprintf(out->english, sizeof out->english,
                 "map-table (not decision): two-level period=%d unique=2 runs=%d",
                 bestp, runs);
        return out->kind;
    }
    if (nu <= 8 && n >= 10 && nu * 3 <= n) {
        out->kind = IKITAKU_RULE_CATEGORICAL;
        out->conf = 70;
        snprintf(out->purpose, sizeof out->purpose, "map-table");
        snprintf(out->english, sizeof out->english,
                 "map-table (not decision): alphabet %d — labels, no control-flow branch.",
                 nu);
        return out->kind;
    }
    if (runs <= n/3 && runs >= 3) {
        out->kind = IKITAKU_RULE_RUNS;
        out->conf = 60;
        snprintf(out->purpose, sizeof out->purpose, "map-table");
        snprintf(out->english, sizeof out->english,
                 "map-table (not decision): piecewise-constant runs=%d",
                 runs);
        return out->kind;
    }
    if (nu > 2 && nu <= 8 && nu * 2 < n) {
        out->kind = IKITAKU_RULE_THRESHOLD;
        out->conf = 40;
        snprintf(out->purpose, sizeof out->purpose, "map-table");
        snprintf(out->english, sizeof out->english,
                 "map-table (not decision): threshold/cases unique≈%d",
                 nu);
        return out->kind;
    }
    out->kind = IKITAKU_RULE_NONE;
    snprintf(out->purpose, sizeof out->purpose, "map-math");
    snprintf(out->english, sizeof out->english,
             "map-math: no table (unique=%d runs=%d); f(x) still in play — not a decision.",
             nu, runs);
    return 0;
}

// ===================== Desperation =====================

int ikitaku_seq_cause(const ikitaku_series_t* s, ikitaku_seq_cause_t* out) {
    if (!s || !out || s->n < 4) return -1;
    memset(out, 0, sizeof *out);
    int tok[IKITAKU_SERIES_MAX];
    int n = s->n;
    if (n > IKITAKU_SERIES_MAX) n = IKITAKU_SERIES_MAX;
    int vmin = 1<<30, vmax = -(1<<30);
    for (int i = 0; i < n; ++i) {
        tok[i] = (int)(s->values[i] + (s->values[i] >= 0 ? 0.5 : -0.5));
        if (tok[i] < vmin) vmin = tok[i];
        if (tok[i] > vmax) vmax = tok[i];
    }
    /* shift to 0..K-1 if needed */
    int K = vmax - vmin + 1;
    if (K > 16) {
        /* hash to 8 bins */
        for (int i = 0; i < n; ++i) tok[i] = ((tok[i] % 8) + 8) % 8;
        vmin = 0; K = 8;
    } else {
        for (int i = 0; i < n; ++i) tok[i] -= vmin;
    }
    out->n_tokens = K;
    int cnt[16] = {0};
    int ab[16][16];
    memset(ab, 0, sizeof ab);
    for (int i = 0; i < n; ++i) {
        if (tok[i] < 0 || tok[i] >= 16) continue;
        cnt[tok[i]]++;
        if (i + 1 < n && tok[i+1] >= 0 && tok[i+1] < 16)
            ab[tok[i]][tok[i+1]]++;
    }
    /* score pairs: infrequent = ab small vs n, asymmetric = ab >> ba */
    typedef struct { int a,b,ab,ba,score; } cand_t;
    cand_t c[64]; int nc = 0;
    for (int a = 0; a < K && a < 16; ++a)
        for (int b = 0; b < K && b < 16; ++b) {
            if (a == b) continue;
            int fab = ab[a][b], fba = ab[b][a];
            if (fab < 2) continue;
            int rare = (fab * 8 <= n) ? 1 : 0;
            int asym = (fab >= fba * 2 + 1) ? 1 : 0;
            int sc = fab * 4 - fba * 3 + (rare ? 6 : 0) + (asym ? 10 : 0);
            if (sc < 8) continue;
            if (nc < 64) {
                c[nc].a = a + vmin; c[nc].b = b + vmin;
                c[nc].ab = fab; c[nc].ba = fba; c[nc].score = sc;
                nc++;
            }
        }
    for (int i = 0; i < nc; ++i)
        for (int j = i+1; j < nc; ++j)
            if (c[j].score > c[i].score) { cand_t t = c[i]; c[i] = c[j]; c[j] = t; }
    int take = nc < IKITAKU_SEQ_RULES ? nc : IKITAKU_SEQ_RULES;
    for (int i = 0; i < take; ++i) {
        ikitaku_seq_rule_t* r = &out->r[out->n++];
        r->a = c[i].a; r->b = c[i].b;
        r->ab = c[i].ab; r->ba = c[i].ba;
        r->a_n = (c[i].a - vmin >= 0 && c[i].a - vmin < 16) ? cnt[c[i].a - vmin] : 0;
        r->b_n = (c[i].b - vmin >= 0 && c[i].b - vmin < 16) ? cnt[c[i].b - vmin] : 0;
        r->infrequent = (c[i].ab * 8 <= n);
        r->asymmetric = (c[i].ab >= c[i].ba * 2 + 1);
        int conf = 40 + c[i].ab * 6 - c[i].ba * 8;
        if (r->asymmetric) conf += 12;
        if (r->infrequent) conf += 6;
        if (conf > 92) conf = 92;
        if (conf < 20) conf = 20;
        r->conf_lo = conf - 10; if (r->conf_lo < 15) r->conf_lo = 15;
        r->conf_hi = conf + 8;  if (r->conf_hi > 95) r->conf_hi = 95;
        if (r->asymmetric && r->ab >= 2 && r->ba * 2 < r->ab)
            snprintf(r->species, sizeof r->species, "directed-chain");
        else if (r->ba >= r->ab - 1)
            snprintf(r->species, sizeof r->species, "cum-hoc");
        else
            snprintf(r->species, sizeof r->species, "post-hoc");
        snprintf(r->rule, sizeof r->rule, "%d → %d", r->a, r->b);
    }
    if (out->n) {
        snprintf(out->english, sizeof out->english,
                 "seq-cause n=%d tokens=%d top %s [%d-%d] %s ab=%d ba=%d",
                 n, out->n_tokens, out->r[0].rule,
                 out->r[0].conf_lo, out->r[0].conf_hi,
                 out->r[0].species, out->r[0].ab, out->r[0].ba);
    } else {
        snprintf(out->english, sizeof out->english,
                 "seq-cause n=%d tokens=%d no directed pair", n, out->n_tokens);
    }
    return out->n;
}

/* ---- prior match + reason chain (library, not harness) ---- */

static void prior_add(ikitaku_prior_report_t* out, const char* name,
                      const char* r5, const char* r4,
                      double pred, double c) {
    if (!out || out->n >= IKITAKU_PRIOR_MAX) return;
    ikitaku_prior_hit_t* h = &out->hit[out->n++];
    memset(h, 0, sizeof *h);
    snprintf(h->name, sizeof h->name, "%s", name);
    snprintf(h->r5, sizeof h->r5, "%s", r5);
    snprintf(h->r4, sizeof h->r4, "%s", r4);
    h->predicted = pred;
    h->residual = fabs(c - pred);
    h->exact = (h->residual < 1e-6) ? 1 : 0;
    if (h->exact) h->conf = 95;
    else if (h->residual < 0.05) h->conf = 70;
    else if (h->residual < 0.5) h->conf = 40;
    else h->conf = 10;
}

int ikitaku_match_priors_binop(double a, double b, double c,
                               ikitaku_prior_report_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->best = -1;
    prior_add(out, "unity_mag", "join", "magnitude-unify", a + b, c);
    prior_add(out, "disunity_mag", "split", "magnitude-split", a - b, c);
    prior_add(out, "scale_mag", "join", "magnitude-scale", a * b, c);
    if (fabs(b) > 1e-12)
        prior_add(out, "inv_scale", "map", "magnitude-scale", a / b, c);
    prior_add(out, "max_bound", "bound", "threshold", (a > b ? a : b), c);
    prior_add(out, "min_bound", "bound", "threshold", (a < b ? a : b), c);
    /* power family: repeated scale of magnitude (a multiplied by itself b times) */
    {
        double pw = 0.0;
        if (a == 0.0 && b <= 0.0) pw = 0.0 / 0.0; /* force miss */
        else {
            pw = pow(a, b);
            if (!isfinite(pw) || fabs(pw) > 1e15) pw = 0.0 / 0.0;
        }
        if (isfinite(pw))
            prior_add(out, "pow_mag", "map", "map-math", pw, c);
    }
    prior_add(out, "square_self", "map", "map-math", a * a, c);
    prior_add(out, "cube_self", "map", "map-math", a * a * a, c);

    int exact_n = 0, best_i = -1, best_conf = -1;
    for (int i = 0; i < out->n; ++i) {
        if (out->hit[i].exact) exact_n++;
        if (out->hit[i].conf > best_conf) {
            best_conf = out->hit[i].conf;
            best_i = i;
        }
    }
    out->best = best_i;
    out->ambiguous = (exact_n >= 2) ? 1 : 0;
    out->none = (exact_n == 0 && best_conf < 40) ? 1 : 0;
    if (best_i >= 0)
        snprintf(out->best_name, sizeof out->best_name, "%s", out->hit[best_i].name);

    if (out->ambiguous) {
        snprintf(out->english, sizeof out->english,
                 "prior-match AMBIGUOUS: %d exact hits (e.g. %s). need another probe.",
                 exact_n, out->best_name);
    } else if (best_i >= 0 && out->hit[best_i].exact) {
        snprintf(out->english, sizeof out->english,
                 "prior-match EXACT %s (r5=%s r4=%s) pred=%.6g residual=0 conf=%d",
                 out->hit[best_i].name, out->hit[best_i].r5, out->hit[best_i].r4,
                 out->hit[best_i].predicted, out->hit[best_i].conf);
    } else if (best_i >= 0) {
        snprintf(out->english, sizeof out->english,
                 "prior-match weak %s residual=%.4g conf=%d — incomplete priori",
                 out->hit[best_i].name, out->hit[best_i].residual,
                 out->hit[best_i].conf);
    } else {
        snprintf(out->english, sizeof out->english,
                 "prior-match NONE — novel or multi-step transform");
        out->none = 1;
    }
    return out->n;
}


int ikitaku_match_priors_table(const double* a, const double* b, const double* c, int n,
                               ikitaku_prior_report_t* out) {
    if (!out || !a || !b || !c || n <= 0) return -1;
    memset(out, 0, sizeof *out);
    out->best = -1;
    /* score each candidate name by exact hits across rows */
    struct { char name[24]; char r5[24]; char r4[32]; int hits; int conf; } cand[16];
    int nc = 0;
    for (int i = 0; i < n; ++i) {
        ikitaku_prior_report_t row;
        ikitaku_match_priors_binop(a[i], b[i], c[i], &row);
        for (int h = 0; h < row.n; ++h) {
            if (!row.hit[h].exact) continue;
            int found = -1;
            for (int k = 0; k < nc; ++k)
                if (!strcmp(cand[k].name, row.hit[h].name)) { found = k; break; }
            if (found < 0 && nc < 16) {
                snprintf(cand[nc].name, 24, "%s", row.hit[h].name);
                snprintf(cand[nc].r5, 24, "%s", row.hit[h].r5);
                snprintf(cand[nc].r4, 32, "%s", row.hit[h].r4);
                cand[nc].hits = 0;
                cand[nc].conf = 0;
                found = nc++;
            }
            if (found >= 0) {
                cand[found].hits++;
                if (row.hit[h].conf > cand[found].conf)
                    cand[found].conf = row.hit[h].conf;
            }
        }
    }
    int best = -1;
    for (int k = 0; k < nc; ++k) {
        prior_add(out, cand[k].name, cand[k].r5, cand[k].r4,
                  (double)cand[k].hits, (double)n); /* residual = n-hits style via conf */
        /* fix conf to vote ratio */
        ikitaku_prior_hit_t* hh = &out->hit[out->n - 1];
        hh->exact = (cand[k].hits == n) ? 1 : 0;
        hh->residual = (double)(n - cand[k].hits);
        hh->conf = (cand[k].hits * 100) / n;
        hh->predicted = (double)cand[k].hits;
        if (best < 0 || hh->conf > out->hit[best].conf) best = out->n - 1;
    }
    out->best = best;
    if (best >= 0)
        snprintf(out->best_name, sizeof out->best_name, "%s", out->hit[best].name);
    out->ambiguous = 0;
    if (best >= 0) {
        for (int i = 0; i < out->n; ++i)
            if (i != best && out->hit[i].conf == out->hit[best].conf)
                out->ambiguous = 1;
    }
    if (best >= 0 && out->hit[best].exact)
        snprintf(out->english, sizeof out->english,
                 "table prior EXACT %s hits=%d/%d conf=%d — repeated scale / power family",
                 out->best_name, (int)out->hit[best].predicted, n, out->hit[best].conf);
    else if (best >= 0)
        snprintf(out->english, sizeof out->english,
                 "table prior best=%s hits=%d/%d conf=%d%s",
                 out->best_name, (int)out->hit[best].predicted, n, out->hit[best].conf,
                 out->ambiguous ? " AMBIGUOUS" : "");
    else
        snprintf(out->english, sizeof out->english, "table prior: no exact row hits");
    return out->n;
}

int ikitaku_match_priors_unary(double x, double y,
                               ikitaku_prior_report_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->best = -1;
    prior_add(out, "sin", "map", "map-math", sin(x), y);
    prior_add(out, "cos", "map", "map-math", cos(x), y);
    prior_add(out, "tan", "map", "map-math", tan(x), y);
    prior_add(out, "neg", "map", "map-math", -x, y);
    prior_add(out, "id", "map", "map-math", x, y);
    prior_add(out, "square", "map", "map-math", x * x, y);
    prior_add(out, "cube", "map", "map-math", x * x * x, y);
    prior_add(out, "scale2", "map", "map-math", 2.0 * x, y);

    /* range check: if |y|>1+eps, sin/cos image miss */
    if (fabs(y) > 1.0 + 1e-3) {
        /* mark sin/cos as impossible for this y if claiming identity range */
        for (int i = 0; i < out->n; ++i) {
            if (!strcmp(out->hit[i].name, "sin") || !strcmp(out->hit[i].name, "cos")) {
                if (out->hit[i].residual > 0.5) {
                    out->hit[i].conf = 5;
                    out->hit[i].exact = 0;
                }
            }
        }
    }

    int best_i = -1, best_conf = -1, exact_n = 0;
    for (int i = 0; i < out->n; ++i) {
        if (out->hit[i].exact) exact_n++;
        if (out->hit[i].conf > best_conf) {
            best_conf = out->hit[i].conf;
            best_i = i;
        }
    }
    out->best = best_i;
    out->ambiguous = exact_n >= 2;
    if (best_i >= 0)
        snprintf(out->best_name, sizeof out->best_name, "%s", out->hit[best_i].name);
    if (best_i >= 0 && out->hit[best_i].exact)
        snprintf(out->english, sizeof out->english,
                 "unary prior EXACT %s residual=0 conf=%d",
                 out->hit[best_i].name, out->hit[best_i].conf);
    else
        snprintf(out->english, sizeof out->english,
                 "unary prior weak/none best=%s conf=%d",
                 out->best_name[0]?out->best_name:"?", best_conf);
    return out->n;
}

static void chain_add(ikitaku_reason_chain_t* ch, const char* fmt, ...) {
    if (!ch || ch->n >= IKITAKU_CHAIN_MAX) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ch->step[ch->n], sizeof ch->step[0], fmt, ap);
    va_end(ap);
    ch->n++;
}

int ikitaku_reason_binop(double a, double b, double c,
                         ikitaku_prior_report_t* pr,
                         ikitaku_reason_chain_t* chain) {
    if (!chain) return -1;
    memset(chain, 0, sizeof *chain);
    ikitaku_prior_report_t local;
    if (!pr) {
        pr = &local;
        ikitaku_match_priors_binop(a, b, c, pr);
    }
    chain_add(chain, "observe black-box I/O: (%.6g) [?] (%.6g) → %.6g", a, b, c);
    chain_add(chain, "load magnitude priors: unity, disunity, scale, inv_scale, max, min");
    for (int i = 0; i < pr->n; ++i) {
        chain_add(chain,
                  "try %s (%s/%s): pred=%.6g residual=%.4g conf=%d%s",
                  pr->hit[i].name, pr->hit[i].r5, pr->hit[i].r4,
                  pr->hit[i].predicted, pr->hit[i].residual, pr->hit[i].conf,
                  pr->hit[i].exact ? " EXACT" : "");
    }
    if (pr->ambiguous) {
        chain_add(chain, "multiple exact priors fit one point → AMBIGUOUS");
        snprintf(chain->conclusion, sizeof chain->conclusion,
                 "ambiguous among priors; need second probe. best candidate %s",
                 pr->best_name);
    } else if (pr->best >= 0 && pr->hit[pr->best].exact) {
        chain_add(chain, "single exact residual hit → bind op to %s", pr->best_name);
        snprintf(chain->conclusion, sizeof chain->conclusion,
                 "op matches %s (%s/%s) conf=%d",
                 pr->hit[pr->best].name, pr->hit[pr->best].r5,
                 pr->hit[pr->best].r4, pr->hit[pr->best].conf);
    } else {
        chain_add(chain, "no exact prior → incomplete priori or multi-step box");
        snprintf(chain->conclusion, sizeof chain->conclusion,
                 "no prior match; mark novel/incomplete (best weak %s)",
                 pr->best_name[0]?pr->best_name:"none");
    }
    return chain->n;
}

int ikitaku_reason_unknown_value(const char* form, double known_a, double known_b,
                                 double result, ikitaku_reason_chain_t* chain) {
    if (!chain) return -1;
    memset(chain, 0, sizeof *chain);
    const char* f = form ? form : "unity";
    chain_add(chain, "unknown-value trial form='%s' knowns=%.6g,%.6g result=%.6g",
              f, known_a, known_b, result);
    if (!strcmp(f, "unity") || !strcmp(f, "add") || !strcmp(f, "?+k=r")) {
        double dep = result - known_a;
        chain_add(chain, "assume unity_mag: unknown = result - known = %.6g", dep);
        chain_add(chain, "determined system → single deposit");
        snprintf(chain->conclusion, sizeof chain->conclusion,
                 "deposit unknown=%.6g under unity inverse", dep);
    } else if (!strcmp(f, "disunity") || !strcmp(f, "sub") || !strcmp(f, "k-?=r")) {
        double dep = known_a - result;
        chain_add(chain, "assume disunity_mag: unknown = left - result = %.6g", dep);
        snprintf(chain->conclusion, sizeof chain->conclusion,
                 "deposit unknown=%.6g under split inverse", dep);
    } else if (!strcmp(f, "scale2") || !strcmp(f, "?*?=r")) {
        chain_add(chain, "scale_mag with two unknowns and one product");
        chain_add(chain, "solution set is a hyperbola — infinitely many pairs");
        chain_add(chain, "underdetermined: need a second constraint");
        snprintf(chain->conclusion, sizeof chain->conclusion,
                 "infinite solutions (underdetermined); no single deposit");
    } else if (!strcmp(f, "sin_eq") || !strcmp(f, "sin(?)=r")) {
        chain_add(chain, "probe image of sin over [-pi,pi] → typically [-1,1]");
        if (fabs(result) > 1.0 + 1e-6) {
            chain_add(chain, "target %.6g lies outside [-1,1]", result);
            chain_add(chain, "contradiction with prior map-math sin");
            snprintf(chain->conclusion, sizeof chain->conclusion,
                     "IMPOSSIBLE under real sin; raise confusion");
        } else {
            chain_add(chain, "target in range; inverse sin multi-valued");
            snprintf(chain->conclusion, sizeof chain->conclusion,
                     "solvable but multi-valued (principal + symmetry)");
        }
    } else {
        chain_add(chain, "unknown form string — cannot invert");
        snprintf(chain->conclusion, sizeof chain->conclusion, "incomplete form");
    }
    return chain->n;
}

/* forward — defined in concept.inc later in same TU */
int ikitaku_subject_teach(const char* name, const char* r5, const char* r4,
                          const char* attr, const char* val, int conf);

int ikitaku_self_teach_prior(const ikitaku_prior_report_t* pr, int min_conf) {
    if (!pr || pr->best < 0) return 0;
    if (pr->ambiguous) return 0; /* do not install ambiguous costumes */
    const ikitaku_prior_hit_t* h = &pr->hit[pr->best];
    if (!h->exact || h->conf < min_conf) return 0;
    /* reinforce subject on ladder */
    int r = ikitaku_subject_teach(h->name, h->r5, h->r4, "source", "self-teach", h->conf);
    if (r == 0)
        ikitaku_subject_teach(h->name, h->r5, h->r4, "last_residual", "0", h->conf);
    return r == 0 ? 1 : 0;
}

/* Unary int→bool/label rule extraction via residual against simple predicates */
int ikitaku_match_priors_predicate(const int* xs, const int* ys, int n,
                                   ikitaku_prior_report_t* out) {
    if (!out || !xs || !ys || n < 1) return -1;
    memset(out, 0, sizeof *out);
    out->best = -1;

    struct { const char* name; const char* r5; const char* r4; int (*pred)(int); } P[] = {
        {"even", "decision", "parity", 0},
        {"odd", "decision", "parity", 0},
        {"mod3_0", "map", "map-table", 0},
        {"mod5_0", "map", "map-table", 0},
        {"gt0", "compare", "threshold", 0},
        {"eq0", "compare", "threshold", 0},
        {"gt10", "compare", "threshold", 0},
        {"lt10", "compare", "threshold", 0},
    };
    /* evaluate each predicate as expected y in {0,1}; ys assumed 0/1 */
    auto score = [&](const char* name, const char* r5, const char* r4, int (*f)(int)) {
        if (out->n >= IKITAKU_PRIOR_MAX) return;
        int hit = 0;
        for (int i = 0; i < n; ++i) {
            int pred = f(xs[i]) ? 1 : 0;
            if (pred == (ys[i] ? 1 : 0)) hit++;
        }
        ikitaku_prior_hit_t* h = &out->hit[out->n++];
        memset(h, 0, sizeof *h);
        snprintf(h->name, sizeof h->name, "%s", name);
        snprintf(h->r5, sizeof h->r5, "%s", r5);
        snprintf(h->r4, sizeof h->r4, "%s", r4);
        h->predicted = (double)hit;
        h->residual = (double)(n - hit);
        h->exact = (hit == n) ? 1 : 0;
        h->conf = (hit * 100) / n;
    };

    score("even", "decision", "parity", [](int x)->int { return (x % 2 == 0); });
    score("odd", "decision", "parity", [](int x)->int { return (x % 2 != 0); });
    score("mod3_0", "map", "map-table", [](int x)->int { return (x % 3 == 0); });
    score("mod5_0", "map", "map-table", [](int x)->int { return (x % 5 == 0); });
    score("gt0", "compare", "threshold", [](int x)->int { return (x > 0); });
    score("eq0", "compare", "threshold", [](int x)->int { return (x == 0); });
    score("gt10", "compare", "threshold", [](int x)->int { return (x > 10); });
    score("lt10", "compare", "threshold", [](int x)->int { return (x < 10); });

    int best_i = -1, best_conf = -1, exact_n = 0;
    for (int i = 0; i < out->n; ++i) {
        if (out->hit[i].exact) exact_n++;
        if (out->hit[i].conf > best_conf) {
            best_conf = out->hit[i].conf;
            best_i = i;
        }
    }
    out->best = best_i;
    out->ambiguous = (exact_n >= 2);
    out->none = (exact_n == 0);
    if (best_i >= 0)
        snprintf(out->best_name, sizeof out->best_name, "%s", out->hit[best_i].name);
    if (out->ambiguous)
        snprintf(out->english, sizeof out->english,
                 "predicate AMBIGUOUS: %d exact rules fit all %d rows", exact_n, n);
    else if (best_i >= 0 && out->hit[best_i].exact)
        snprintf(out->english, sizeof out->english,
                 "predicate EXACT %s (%s/%s) fits %d/%d conf=%d",
                 out->hit[best_i].name, out->hit[best_i].r5, out->hit[best_i].r4,
                 n, n, out->hit[best_i].conf);
    else if (best_i >= 0)
        snprintf(out->english, sizeof out->english,
                 "predicate weak best=%s conf=%d (%d misses) — incomplete",
                 out->hit[best_i].name, out->hit[best_i].conf, (int)out->hit[best_i].residual);
    else
        snprintf(out->english, sizeof out->english, "predicate NONE");
    return out->n;
}

int ikitaku_reason_predicate(const int* xs, const int* ys, int n,
                             ikitaku_prior_report_t* pr,
                             ikitaku_reason_chain_t* chain) {
    if (!chain) return -1;
    memset(chain, 0, sizeof *chain);
    ikitaku_prior_report_t local;
    if (!pr) {
        pr = &local;
        ikitaku_match_priors_predicate(xs, ys, n, pr);
    }
    chain_add(chain, "observe %d I/O pairs (int → bool/label), op unknown", n);
    chain_add(chain, "load predicate priors: even/odd/mod3/mod5/gt0/eq0/gt10/lt10");
    for (int i = 0; i < pr->n; ++i)
        chain_add(chain, "try %s: hits=%.0f/%d conf=%d%s",
                  pr->hit[i].name, pr->hit[i].predicted, n, pr->hit[i].conf,
                  pr->hit[i].exact ? " EXACT" : "");
    if (pr->ambiguous) {
        chain_add(chain, "multiple exact predicates — AMBIGUOUS, need probe that splits them");
        snprintf(chain->conclusion, sizeof chain->conclusion,
                 "ambiguous; candidates include %s", pr->best_name);
    } else if (pr->best >= 0 && pr->hit[pr->best].exact) {
        chain_add(chain, "single exact rule → extract transform %s", pr->best_name);
        chain_add(chain, "extrapolate: same predicate applies to unseen ints");
        snprintf(chain->conclusion, sizeof chain->conclusion,
                 "rule = %s (%s/%s) conf=%d; eligible for self-teach",
                 pr->hit[pr->best].name, pr->hit[pr->best].r5,
                 pr->hit[pr->best].r4, pr->hit[pr->best].conf);
    } else {
        chain_add(chain, "no exact predicate in prior set");
        snprintf(chain->conclusion, sizeof chain->conclusion,
                 "incomplete priori; mark novel rule");
    }
    return chain->n;
}

int ikitaku_charge_compose(int a, int b) {
    if (a == 0 || b == 0) return 0;
    return (a > 0 ? 1 : -1) * (b > 0 ? 1 : -1);
}

int ikitaku_bit_from_charge(int c) { return c > 0 ? 1 : 0; }
int ikitaku_charge_from_bit(int b) { return b ? 1 : -1; }

int ikitaku_bool16(int op, int A, int B) {
    int a = ikitaku_bit_from_charge(A);
    int b = ikitaku_bit_from_charge(B);
    int r = 0;
    switch (op) {
    case IKITAKU_B_FALSE:   r = 0; break;
    case IKITAKU_B_AND:     r = a & b; break;
    case IKITAKU_B_A_NOT_B: r = a & !b; break;
    case IKITAKU_B_A:       r = a; break;
    case IKITAKU_B_NOT_A_B: r = (!a) & b; break;
    case IKITAKU_B_B:       r = b; break;
    case IKITAKU_B_XOR:     r = a ^ b; break;
    case IKITAKU_B_OR:      r = a | b; break;
    case IKITAKU_B_NOR:     r = !(a | b); break;
    case IKITAKU_B_XNOR:    r = !(a ^ b); break;
    case IKITAKU_B_NOT_B:   r = !b; break;
    case IKITAKU_B_B_IMP_A: r = b ? a : 1; break;      /* B → A */
    case IKITAKU_B_NOT_A:   r = !a; break;
    case IKITAKU_B_A_IMP_B: r = a ? b : 1; break;      /* A → B */
    case IKITAKU_B_NAND:    r = !(a & b); break;
    case IKITAKU_B_TRUE:    r = 1; break;
    default: return 0;
    }
    return ikitaku_charge_from_bit(r);
}

static const char* k_bool_name[16] = {
    "contradiction", "and", "a_not_b", "identity_a",
    "not_a_b", "identity_b", "xor", "or",
    "nor", "xnor", "not_b", "b_implies_a",
    "not_a", "a_implies_b", "nand", "tautology"
};
static const char* k_bool_note[16] = {
    "-1", "(A oplus B) oplus (A oplus 1)  [AND on bits]",
    "(A oplus ominus B) oplus (A oplus 1)", "A",
    "(ominus A oplus B) oplus (B oplus 1)", "B",
    "A ominus B", "ominus[(ominus A oplus ominus B) oplus (ominus A oplus 1)]",
    "(ominus A oplus ominus B) oplus (ominus A oplus 1)", "A oplus B",
    "ominus B", "ominus[(ominus A oplus B) oplus (B oplus 1)]",
    "ominus A", "ominus[(A oplus ominus B) oplus (A oplus 1)]",
    "ominus AND", "1"
};

const char* ikitaku_bool16_name(int op) {
    if (op < 0 || op > 15) return "?";
    return k_bool_name[op];
}
const char* ikitaku_bool16_notation(int op) {
    if (op < 0 || op > 15) return "?";
    return k_bool_note[op];
}

int ikitaku_bool16_named(const char* name, int A, int B) {
    if (!name) return 0;
    for (int i = 0; i < 16; ++i)
        if (!strcmp(name, k_bool_name[i])) return ikitaku_bool16(i, A, B);
    if (!strcmp(name, "nand")) return ikitaku_bool16(IKITAKU_B_NAND, A, B);
    if (!strcmp(name, "implies") || !strcmp(name, "imp"))
        return ikitaku_bool16(IKITAKU_B_A_IMP_B, A, B);
    if (!strcmp(name, "eq") || !strcmp(name, "iff"))
        return ikitaku_bool16(IKITAKU_B_XNOR, A, B);
    return 0;
}

int ikitaku_bool_tt(int op, ikitaku_bool_tt_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->op = op;
    int aa[4] = {1, 1, -1, -1};
    int bb[4] = {1, -1, 1, -1};
    for (int i = 0; i < 4; ++i) {
        out->row[i][0] = aa[i];
        out->row[i][1] = bb[i];
        out->row[i][2] = ikitaku_bool16(op, aa[i], bb[i]);
    }
    snprintf(out->english, sizeof out->english,
             "bool-tt %s (%s): ++=%+d +-=%+d -+=%+d --=%+d",
             ikitaku_bool16_name(op), ikitaku_bool16_notation(op),
             out->row[0][2], out->row[1][2], out->row[2][2], out->row[3][2]);
    return 0;
}

int ikitaku_bimatrix_eval(const int payA[4], const int payB[4],
                          ikitaku_bimatrix_t* out,
                          ikitaku_reason_chain_t* chain) {
    if (!payA || !payB || !out) return -1;
    memset(out, 0, sizeof *out);
    if (chain) memset(chain, 0, sizeof *chain);
    for (int i = 0; i < 4; ++i) {
        out->payA[i] = payA[i];
        out->payB[i] = payB[i];
        out->joint[i] = payA[i] + payB[i];
    }
    /* worlds: 0=++ 1=+- 2=-+ 3=--
       A unity given B unity: world 0 vs A disunity given B unity: world 2 */
    auto pref = [](int u, int d) -> int {
        if (u > d) return 1;
        if (d > u) return -1;
        return 0;
    };
    out->A_pref_Bpos = pref(payA[0], payA[2]); /* ++ vs -+ */
    out->A_pref_Bneg = pref(payA[1], payA[3]); /* +- vs -- */
    out->B_pref_Apos = pref(payB[0], payB[1]); /* ++ vs +- */
    out->B_pref_Aneg = pref(payB[2], payB[3]); /* -+ vs -- */
    out->A_dominant = (out->A_pref_Bpos == out->A_pref_Bneg) ? out->A_pref_Bpos : 0;
    out->B_dominant = (out->B_pref_Apos == out->B_pref_Aneg) ? out->B_pref_Apos : 0;

    /* Nash: A best reply to B and B best reply to A */
    int Aact[4] = {1, 1, -1, -1};
    int Bact[4] = {1, -1, 1, -1};
    for (int w = 0; w < 4; ++w) {
        int Ap = (Bact[w] > 0) ? out->A_pref_Bpos : out->A_pref_Bneg;
        int Bp = (Aact[w] > 0) ? out->B_pref_Apos : out->B_pref_Aneg;
        int Aok = (Ap == 0) || (Ap == Aact[w]);
        int Bok = (Bp == 0) || (Bp == Bact[w]);
        out->nash[w] = (Aok && Bok) ? 1 : 0;
        if (out->nash[w]) out->n_nash++;
    }
    out->joint_argmax = 0;
    for (int i = 1; i < 4; ++i)
        if (out->joint[i] > out->joint[out->joint_argmax])
            out->joint_argmax = i;
    int nash_joint = -9999;
    for (int w = 0; w < 4; ++w)
        if (out->nash[w] && out->joint[w] > nash_joint)
            nash_joint = out->joint[w];
    out->pareto_conflict = (out->n_nash > 0 &&
                            nash_joint < out->joint[out->joint_argmax]) ? 1 : 0;

    if (chain) {
        chain_add(chain, "4 worlds (A,B): ++ +- -+ -- as unity/disunity charges");
        chain_add(chain, "bool AND = both unity; XOR = mismatch; NOR = both disunity");
        chain_add(chain, "A pref | B=unity: %+d ; A pref | B=disunity: %+d",
                  out->A_pref_Bpos, out->A_pref_Bneg);
        chain_add(chain, "B pref | A=unity: %+d ; B pref | A=disunity: %+d",
                  out->B_pref_Apos, out->B_pref_Aneg);
        if (out->A_dominant)
            chain_add(chain, "A has dominant action charge=%+d (same in both B-worlds)",
                      out->A_dominant);
        else
            chain_add(chain, "A has no dominant action (pref depends on B)");
        if (out->B_dominant)
            chain_add(chain, "B has dominant action charge=%+d", out->B_dominant);
        chain_add(chain, "Nash worlds n=%d (mutual best reply)", out->n_nash);
        chain_add(chain, "joint max at world %d sum=%d ; nash joint max=%d ; pareto_conflict=%d",
                  out->joint_argmax, out->joint[out->joint_argmax],
                  nash_joint, out->pareto_conflict);
        if (out->pareto_conflict)
            snprintf(chain->conclusion, sizeof chain->conclusion,
                     "dominant NE is disunity-disunity but joint max is unity-unity; two goals");
        else if (out->A_dominant)
            snprintf(chain->conclusion, sizeof chain->conclusion,
                     "dominant action %+d; nash n=%d", out->A_dominant, out->n_nash);
        else
            snprintf(chain->conclusion, sizeof chain->conclusion,
                     "no dominant; nash n=%d", out->n_nash);
    }
    snprintf(out->english, sizeof out->english,
             "bimatrix A_dom=%+d B_dom=%+d nash=%d joint_arg=%d conflict=%d",
             out->A_dominant, out->B_dominant, out->n_nash,
             out->joint_argmax, out->pareto_conflict);
    return 0;
}



int ikitaku_info_gap(const ikitaku_info_bag_t* self,
                     const ikitaku_info_bag_t* other,
                     const char* topic,
                     ikitaku_info_gap_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->topic, sizeof out->topic, "%s", topic ? topic : "unknown");
    out->self_has = (self && self->known);
    out->other_has = (other && other->known);
    out->gap = (!out->self_has && other) ? 1 : 0;
    out->reverse_gap = (out->self_has && other && !other->known) ? 1 : 0;
    out->nested_legal = 1;
    if (out->gap && out->nested_legal)
        snprintf(out->english, sizeof out->english,
                 "info-gap: bag '%s' may hold extractable priori on '%s' that self lacks; "
                 "nested emit (quote) is a legal probe",
                 other && other->name[0] ? other->name : "other", out->topic);
    else if (out->reverse_gap)
        snprintf(out->english, sizeof out->english,
                 "info-gap reverse: self holds priori on '%s' that '%s' lacks",
                 out->topic, other && other->name[0] ? other->name : "other");
    else
        snprintf(out->english, sizeof out->english,
                 "info-gap none on '%s' (self_has=%d other_has=%d); nested emit still legal",
                 out->topic, out->self_has, out->other_has);
    return out->gap;
}

int ikitaku_xor_lock(int a_known, int p_known,
                     char* english, int elen) {
    if (!english || elen < 8) return -1;
    if (!a_known && !p_known) {
        snprintf(english, (size_t)elen,
                 "XOR-lock: S = A ⊕ P with both unknown — cannot isolate either from one observation");
        return 1;
    }
    if (!a_known && p_known) {
        snprintf(english, (size_t)elen,
                 "XOR-lock: A unknown, P known — S does not isolate A without a second constraint");
        return 1;
    }
    if (a_known && !p_known) {
        snprintf(english, (size_t)elen,
                 "XOR-lock: P unknown, A known — P isolable by A ⊕ S (invert if A is disunity)");
        return 0;
    }
    snprintf(english, (size_t)elen, "XOR-lock: both known — no lock");
    return 0;
}

int ikitaku_state_cascade(const ikitaku_cascade_t* in,
                          ikitaku_cascade_t* out,
                          ikitaku_reason_chain_t* chain) {
    if (!in || !out) return -1;
    *out = *in;
    out->result = 0;
    out->dissolves_speaker = 0;
    out->path_relation = 0;
    out->n_worlds = 0;
    out->n_consistent = 0;
    out->n_agree_minus_path = 0;
    if (chain) memset(chain, 0, sizeof *chain);

    if (chain) {
        chain_add(chain,
                  "thesis: track path through one agent: S = agent ⊕ path (subjective lock)");
        chain_add(chain,
                  "XOR-lock: agent charge unknown ⇒ path not isolable from one utterance");
    }
    char lock[180];
    ikitaku_xor_lock(in->speaker != 0, in->path != 0, lock, (int)sizeof lock);
    if (chain) chain_add(chain, "%s", lock);

    if (!in->nested) {
        if (chain) {
            chain_add(chain, "direct probe only — speaker variable remains; deadlock");
            snprintf(chain->conclusion, sizeof chain->conclusion,
                     "incomplete: need a probe that introduces the other bag");
        }
        snprintf(out->english, sizeof out->english,
                 "cascade blocked: nested=0, speaker not dissolved");
        return 0;
    }

    if (chain)
        chain_add(chain,
                  "antithesis: nested emit — speaker quotes other bag's map of path");

    int spk_lo = in->speaker ? in->speaker : -1;
    int spk_hi = in->speaker ? in->speaker : 1;
    int pth_lo = in->path ? in->path : -1;
    int pth_hi = in->path ? in->path : 1;
    if (spk_lo > spk_hi) { int t = spk_lo; spk_lo = spk_hi; spk_hi = t; }
    if (pth_lo > pth_hi) { int t = pth_lo; pth_lo = pth_hi; pth_hi = t; }

    for (int sp = spk_lo; sp <= spk_hi; sp += 2) {
        if (sp == 0) continue;
        for (int pt = pth_lo; pt <= pth_hi; pt += 2) {
            if (pt == 0) continue;
            out->n_worlds++;
            int oth = in->other;
            if (in->xor_agents)
                oth = -sp;
            else if (oth == 0)
                continue;
            int utter = ikitaku_charge_compose(sp, ikitaku_charge_compose(oth, pt));
            out->n_consistent++;
            if (utter == -pt)
                out->n_agree_minus_path++;
        }
    }

    if (chain) {
        chain_add(chain,
                  "xor_agents=%d => (speaker ominus other) collapses to disunity -1",
                  in->xor_agents);
        chain_add(chain,
                  "form: R = speaker oplus (other oplus path); worlds=%d consistent=%d",
                  out->n_worlds, out->n_consistent);
        chain_add(chain,
                  "truth-table: %d/%d worlds have utterance = ominus path",
                  out->n_agree_minus_path, out->n_consistent);
    }

    if (in->xor_agents && out->n_consistent > 0 &&
        out->n_agree_minus_path == out->n_consistent) {
        out->dissolves_speaker = 1;
        out->path_relation = -1;
        out->result = (in->path ? -in->path : 0);
        if (chain) {
            chain_add(chain, "inner: (speaker ominus other) = -1 (opposites)");
            chain_add(chain, "middle: -1 oplus path = inverted path");
            chain_add(chain,
                      "outer: speaker * (-speaker) * path = -path (speaker dissolves)");
            chain_add(chain,
                      "synthesis: utterance strictly ominus path in every world");
            snprintf(chain->conclusion, sizeof chain->conclusion,
                     "unity of opposites: pick the inverse of the nested utterance");
        }
        snprintf(out->english, sizeof out->english,
                 "cascade SYNTHESIS dissolves_speaker=1 path_relation=-1 "
                 "(%d/%d worlds); nested utterance is always invert of path",
                 out->n_agree_minus_path, out->n_consistent);
        return 1;
    }

    if (chain) {
        chain_add(chain, "worlds do not agree — speaker not dissolved");
        snprintf(chain->conclusion, sizeof chain->conclusion,
                 "no synthesis; incomplete priori or xor_agents missing");
    }
    snprintf(out->english, sizeof out->english,
             "cascade incomplete agree=%d/%d xor_agents=%d",
             out->n_agree_minus_path, out->n_consistent, in->xor_agents);
    return 0;
}

int ikitaku_remainder(int n_targets, int probe_bits,
                      const ikitaku_cascade_t* cas,
                      ikitaku_remainder_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (n_targets < 1) n_targets = 1;
    if (probe_bits < 0) probe_bits = 0;
    if (probe_bits > 8) probe_bits = 8;
    out->n_targets = n_targets;
    out->probe_bits = probe_bits;
    out->bit_cap = 1 << probe_bits;
    if (cas) {
        out->n_worlds = cas->n_worlds;
        out->n_consistent = cas->n_consistent;
        out->dissolved = cas->dissolves_speaker;
    }
    int budget_short = (n_targets > out->bit_cap);
    int polarity_short = (out->dissolved && n_targets > 2);
    int leftover = (cas && cas->n_consistent > 1 && !cas->dissolves_speaker);
    out->irreducible = (budget_short || polarity_short || leftover) ? 1 : 0;
    out->isolable = (!out->irreducible && n_targets <= out->bit_cap) ? 1 : 0;
    out->weird = (out->dissolved && out->irreducible) ? 1 : 0;
    if (out->weird)
        snprintf(out->english, sizeof out->english,
                 "remainder WEIRD: method held (speaker dissolved) but %d targets vs %d-bit probe (cap %d). leftover uncertainty; nothing in budget can close it.",
                 n_targets, probe_bits, out->bit_cap);
    else if (out->irreducible)
        snprintf(out->english, sizeof out->english,
                 "remainder: %d targets vs %d-bit probe (cap %d). cannot isolate. leftover is not a missing method.",
                 n_targets, probe_bits, out->bit_cap);
    else
        snprintf(out->english, sizeof out->english,
                 "remainder isolable: %d targets fit in %d bits (cap %d) dissolved=%d",
                 n_targets, probe_bits, out->bit_cap, out->dissolved);
    return out->irreducible;
}


