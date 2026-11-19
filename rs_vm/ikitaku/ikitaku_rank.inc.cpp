/* rank / R3 slab / R6 compose / Kant / F-merge */

uint32_t ikitaku_id32(const char* s) {
    uint32_t h = 2166136261u;
    if (!s) return 1;
    for (; *s; ++s) { h ^= (uint8_t)*s; h *= 16777619u; }
    return h ? h : 1u;
}

uint32_t ikitaku_r4_id(const char* parent, const char* family) {
    char buf[64];
    snprintf(buf, sizeof buf, "%s|%s", parent ? parent : "?", family ? family : "?");
    return ikitaku_id32(buf);
}

static ikitaku_r3_rec_t g_r3[IKITAKU_R3_CAP];
static int g_r3_n = 0;
static int g_r3_loaded = 0;

static void r3_ensure(void) {
    if (g_r3_loaded) return;
    memset(g_r3, 0, sizeof g_r3);
    g_r3_n = 0;
    FILE* f = fopen(IKITAKU_R3_PATH, "rb");
    if (f) {
        uint32_t magic = 0, n = 0;
        if (fread(&magic, 4, 1, f) == 1 && magic == 0x33524B49 /* IKR3 le */) {
            fread(&n, 4, 1, f);
            if (n > IKITAKU_R3_CAP) n = IKITAKU_R3_CAP;
            for (uint32_t i = 0; i < n; ++i) {
                if (fread(&g_r3[i], sizeof(ikitaku_r3_rec_t), 1, f) != 1) break;
                if (g_r3[i].used) g_r3_n++;
            }
        }
        fclose(f);
    }
    g_r3_loaded = 1;
}

static int r3_save(void) {
    r3_ensure();
    system("mkdir -p '/workspace/artifacts/rs_vm/ikitaku/discovery'");
    FILE* f = fopen(IKITAKU_R3_PATH, "wb");
    if (!f) return -1;
    uint32_t magic = 0x33524B49, n = (uint32_t)g_r3_n;
    fwrite(&magic, 4, 1, f);
    fwrite(&n, 4, 1, f);
    int wrote = 0;
    for (int i = 0; i < IKITAKU_R3_CAP && wrote < g_r3_n; ++i) {
        if (!g_r3[i].used) continue;
        fwrite(&g_r3[i], sizeof(ikitaku_r3_rec_t), 1, f);
        wrote++;
    }
    fclose(f);
    return 0;
}

static int r3_slot(uint32_t id) {
    uint32_t h = id % IKITAKU_R3_CAP;
    for (int k = 0; k < 64; ++k) {
        int i = (int)((h + (uint32_t)k) % IKITAKU_R3_CAP);
        if (!g_r3[i].used || g_r3[i].id == id) return i;
    }
    return -1;
}

int ikitaku_r3_count(void) { r3_ensure(); return g_r3_n; }

int ikitaku_r3_put(const char* name, const char* r5,
                   const uint32_t* r4_ids, const uint16_t* w, int n_links,
                   const char attrs[][20], int n_attr) {
    if (!name) return -1;
    r3_ensure();
    uint32_t id = ikitaku_id32(name);
    int sl = r3_slot(id);
    if (sl < 0) return -1;
    int neu = !g_r3[sl].used;
    ikitaku_r3_rec_t* r = &g_r3[sl];
    memset(r, 0, sizeof *r);
    r->id = id;
    r->used = 1;
    snprintf(r->name, sizeof r->name, "%s", name);
    snprintf(r->r5, sizeof r->r5, "%s", r5 ? r5 : "info");
    r->n_links = 0;
    if (r4_ids && n_links > 0) {
        int n = n_links < IKITAKU_R3_LINKS ? n_links : IKITAKU_R3_LINKS;
        for (int i = 0; i < n; ++i) {
            r->link[i].r4_id = r4_ids[i];
            r->link[i].weight = w ? w[i] : (uint16_t)(50 + (r4_ids[i] % 40));
            if (r->link[i].weight > 100) r->link[i].weight = 100;
            r->n_links++;
        }
    }
    r->n_attr = 0;
    if (attrs && n_attr > 0) {
        int n = n_attr < IKITAKU_R3_ATTR ? n_attr : IKITAKU_R3_ATTR;
        for (int i = 0; i < n; ++i) {
            snprintf(r->attr[i], 20, "%s", attrs[i]);
            r->n_attr++;
        }
    }
    if (neu) g_r3_n++;
    r3_save();
    return 0;
}

int ikitaku_r3_get(const char* name, ikitaku_r3_rec_t* out) {
    if (!name || !out) return -1;
    return ikitaku_r3_get_id(ikitaku_id32(name), out);
}

int ikitaku_r3_get_id(uint32_t id, ikitaku_r3_rec_t* out) {
    if (!out) return -1;
    r3_ensure();
    int sl = r3_slot(id);
    if (sl < 0 || !g_r3[sl].used || g_r3[sl].id != id) return -1;
    *out = g_r3[sl];
    return 0;
}

int ikitaku_F_merge_lower(const char* Fa, const char* Fb, ikitaku_r3_rec_t* out) {
    if (!Fa || !Fb || !out) return -1;
    const char *A = Fa, *B = Fb;
    if (strcmp(A, B) > 0) { const char* t = A; A = B; B = t; }
    char name[32];
    snprintf(name, sizeof name, "%s_x_%s", A, B);
    /* antiderivatives = R4 families under each F */
    uint32_t ids[4];
    uint16_t ws[4];
    int n = 0;
    char fa[8][32], fb[8][32];
    int na = ikitaku_r4_under(A, fa, 4);
    int nb = ikitaku_r4_under(B, fb, 4);
    ids[n] = ikitaku_r4_id(A, na > 0 ? fa[0] : "instance");
    ws[n] = 80; n++;
    ids[n] = ikitaku_r4_id(B, nb > 0 ? fb[0] : "instance");
    ws[n] = 80; n++;
    if (na > 1) { ids[n] = ikitaku_r4_id(A, fa[1]); ws[n] = 55; n++; }
    if (nb > 1 && n < 4) { ids[n] = ikitaku_r4_id(B, fb[1]); ws[n] = 55; n++; }
    char attrs[4][20];
    snprintf(attrs[0], 20, "from-%s", A);
    snprintf(attrs[1], 20, "from-%s", B);
    snprintf(attrs[2], 20, "merge-anti");
    ikitaku_r3_put(name, A, ids, ws, n, attrs, 3);
    return ikitaku_r3_get(name, out);
}

int ikitaku_rank_gate(const char* goal_r5, const char* bag_r5,
                      int residual, int possible_conf,
                      ikitaku_rank_gate_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->goal, sizeof out->goal, "%s", goal_r5 ? goal_r5 : "?");
    out->r9_unknown = (residual >= 20 || possible_conf < 50) ? 1 : 0;
    /* R8 useful: goal is a known R5 object verb or r6 strategy */
    int known = 0;
    if (goal_r5 && goal_r5[0]) {
        ikitaku_r5_verb_t v;
        known = (ikitaku_r5_lookup(goal_r5, &v) == 0);
    }
    out->r8_useful = (known && possible_conf >= 30) ? 1 : 0;
    int assoc = 50;
    if (goal_r5 && bag_r5) {
        if (strcmp(goal_r5, bag_r5) == 0) assoc = 90;
        else {
            float aff = ikitaku_affinity(goal_r5, bag_r5);
            assoc = (int)(aff * 100.0f + 0.5f);
        }
    }
    out->assoc_to_goal = assoc;
    out->r7_right_task = (out->r8_useful && assoc >= 40) ? 1 : 0;
    snprintf(out->english, sizeof out->english,
             "R9 unknown=%d  R8 useful=%d  R7 right-task=%d (assoc %d goal=%s bag=%s)",
             out->r9_unknown, out->r8_useful, out->r7_right_task,
             assoc, out->goal, bag_r5 ? bag_r5 : "-");
    return out->r7_right_task;
}

static void ladder_step(ikitaku_ladder_plan_t* o, const char* fmt, ...) {
    if (!o || o->n_step >= IKITAKU_LADDER_STEPS) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(o->step[o->n_step], sizeof o->step[0], fmt, ap);
    va_end(ap);
    o->n_step++;
}

int ikitaku_ladder_plan(const char* goal_a, const char* bag_a,
                        const char* goal_b, const char* bag_b,
                        int residual, int possible_conf,
                        int pareto_conflict,
                        ikitaku_ladder_plan_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->pareto_conflict = pareto_conflict ? 1 : 0;
    out->n_goals = (goal_b && goal_b[0]) ? 2 : 1;
    ikitaku_rank_gate(goal_a, bag_a, residual, possible_conf, &out->gate_a);
    if (out->n_goals == 2)
        ikitaku_rank_gate(goal_b, bag_b, residual, possible_conf, &out->gate_b);
    out->r9 = out->gate_a.r9_unknown;
    out->r8_a = out->gate_a.r8_useful;
    out->r8_b = (out->n_goals == 2) ? out->gate_b.r8_useful : 0;

    ladder_step(out, "R9 unknown=%d (residual/incomplete priori)", out->r9);
    ladder_step(out, "R8 useful: A(%s)=%d B(%s)=%d",
                goal_a ? goal_a : "-", out->r8_a,
                out->n_goals==2 && goal_b ? goal_b : "-", out->r8_b);

    if (out->n_goals == 2 && out->r8_a && out->r8_b && out->pareto_conflict) {
        out->r7_pick = 3;
        out->r6_ready = 0;
        snprintf(out->missing, sizeof out->missing, "goal-priority");
        ladder_step(out, "R7 right-task CONFLICT: both goals useful, pareto clash");
        ladder_step(out, "R6 blocked: do not pick a strategy until a goal is ranked");
    } else if (out->n_goals == 2 && out->r8_a && out->r8_b && !out->pareto_conflict) {
        /* both useful, no clash — either works; prefer higher assoc */
        if (out->gate_a.assoc_to_goal >= out->gate_b.assoc_to_goal)
            out->r7_pick = 1;
        else
            out->r7_pick = 2;
        out->r6_ready = 1;
        ladder_step(out, "R7 pick=%d (higher assoc, no pareto clash)", out->r7_pick);
    } else if (out->r8_a && (out->gate_a.r7_right_task || out->n_goals == 1)) {
        out->r7_pick = 1;
        out->r6_ready = 1;
        ladder_step(out, "R7 pick=A (%s) right-task=%d (sole useful goal counts)",
                    goal_a ? goal_a : "?", out->gate_a.r7_right_task);
    } else if (out->n_goals == 2 && out->r8_b && out->gate_b.r7_right_task) {

        out->r7_pick = 2;
        out->r6_ready = 1;
        ladder_step(out, "R7 pick=B (%s)", goal_b ? goal_b : "?");
    } else {
        out->r7_pick = 0;
        out->r6_ready = 0;
        snprintf(out->missing, sizeof out->missing, "right-task");
        ladder_step(out, "R7 none: no useful right-task");
    }

    if (out->r6_ready) {
        ikitaku_thought_plan_t tp;
        const char* g = (out->r7_pick == 2) ? goal_b : goal_a;
        ikitaku_thought_plan(residual >= 25 ? 3 : 1, possible_conf, 60, &tp);
        snprintf(out->r6_lead, sizeof out->r6_lead, "%s",
                 tp.form[tp.chosen].name);
        ladder_step(out, "R6 lead=%s on goal %s", out->r6_lead, g ? g : "?");
    }

    snprintf(out->english, sizeof out->english,
             "ladder R9=%d R8=%d/%d R7_pick=%d R6_ready=%d lead=%s missing=%s",
             out->r9, out->r8_a, out->r8_b, out->r7_pick, out->r6_ready,
             out->r6_lead[0] ? out->r6_lead : "-",
             out->missing[0] ? out->missing : "none");
    return out->r7_pick;
}


static const ikitaku_kant_cat_t KANT_TAB[IKITAKU_KANT_N] = {
    { "unity",       "quantity",  "info",     "one bag exists" },
    { "plurality",   "quantity",  "generate", "one to many" },
    { "totality",    "quantity",  "reduce",   "many to one" },
    { "reality",     "quality",   "emit",     "appearance / out" },
    { "negation",    "quality",   "invert",   "undo / not" },
    { "limitation",  "quality",   "bound",    "constraint" },
    { "inherence",   "relation",  "name",     "accident in substance" },
    { "causality",   "relation",  "sequence", "B after A directed" },
    { "community",   "relation",  "join",     "reciprocal" },
    { "possibility", "modality",  "theorize", "can be" },
    { "existence",   "modality",  "absorb",   "is given" },
    { "necessity",   "modality",  "bound",    "must hold" },
    { "movement",    "schema",    "repeat",   "practiced: axis advance (not one of the 12)" },
};

int ikitaku_kant_table(ikitaku_kant_cat_t* out, int maxn) {
    if (!out || maxn <= 0) return 0;
    int n = maxn < IKITAKU_KANT_N ? maxn : IKITAKU_KANT_N;
    for (int i = 0; i < n; ++i) out[i] = KANT_TAB[i];
    return n;
}

int ikitaku_kant_lookup(const char* category, ikitaku_kant_cat_t* out) {
    if (!category || !out) return -1;
    for (int i = 0; i < IKITAKU_KANT_N; ++i)
        if (strcmp(KANT_TAB[i].category, category) == 0 ||
            strcmp(KANT_TAB[i].r5, category) == 0) {
            *out = KANT_TAB[i];
            return 0;
        }
    return -1;
}

int ikitaku_kant_tas(const ikitaku_hypo_t* thesis,
                     const ikitaku_hypo_t* antithesis,
                     ikitaku_hypo_t* synthesis,
                     ikitaku_critique_t* critique) {
    if (!thesis || !antithesis || !synthesis) return -1;
    ikitaku_hunt_t hunt;
    int ok = ikitaku_dialectic_hunt(thesis, antithesis, synthesis, &hunt);
    if (!ok) {
        /* fuzzy: no catalog pair — merge poles as antiderivatives */
        memset(synthesis, 0, sizeof *synthesis);
        synthesis->method = IKITAKU_THINK_SYNTHESIS;
        synthesis->synthesized = 1;
        const char* A = thesis->purpose[0] ? thesis->purpose : thesis->form_id;
        const char* B = antithesis->purpose[0] ? antithesis->purpose : antithesis->form_id;
        snprintf(synthesis->form_id, sizeof synthesis->form_id, "%s_x_%s", A, B);
        snprintf(synthesis->purpose, sizeof synthesis->purpose, "%s", A);
        snprintf(synthesis->note, sizeof synthesis->note,
                 "fuzzy TAS: no catalog pair; hunt next=%d", hunt.next_probe);
        synthesis->confidence = (thesis->confidence + antithesis->confidence) / 2;
        if (hunt.solved) synthesis->confidence += 4;
        synthesis->feasible = 1;
        ikitaku_r3_rec_t g;
        ikitaku_F_merge_lower(A, B, &g);
        ok = 1;
    } else {
        synthesis->synthesized = 1;
    }
    if (critique) {
        int residual = 100 - synthesis->confidence;
        if (residual < 0) residual = 0;
        ikitaku_critique("fuzzy TAS synthesis", 1, 0, 1, 0, critique);
        ikitaku_critique_t ch;
        ikitaku_critique_challenge(critique, synthesis->form_id,
                                   synthesis->confidence, residual, 0, &ch);
        *critique = ch;
    }
    return ok;
}

int ikitaku_r6_compose(const char* objective, int revisit_ok,
                       uint8_t depth, int math_conf, int rule_conf,
                       int desperation, ikitaku_r6_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (depth > IKITAKU_R6_DEPTH_MAX) depth = IKITAKU_R6_DEPTH_MAX;
    out->depth = depth;
    ikitaku_thought_plan(desperation, math_conf, rule_conf, &out->plan);
    ikitaku_method_pick_t pk;
    memset(&pk, 0, sizeof pk);
    {
        ikitaku_method_hegel_pick(math_conf < 50 ? 40 : 10, math_conf,
                                  rule_conf, desperation, &pk);
        for (int i = 0; i < out->plan.n; ++i)
            if (!strcmp(out->plan.form[i].name, pk.m[pk.chosen].name)) {
                out->plan.chosen = i;
                break;
            }
    }
    ikitaku_strategy_t mac;
    ikitaku_strategy_pick(objective ? objective : "read", revisit_ok, &mac);
    snprintf(out->macro, sizeof out->macro, "%s", mac.name);
    /* synthetic a priori: macro is composition of micros */
    out->n_methods = 0;
    for (int i = 0; i < out->plan.n && out->n_methods < 8; ++i)
        snprintf(out->methods[out->n_methods++], 24, "%s", out->plan.form[i].name);
    if (out->n_methods < 8)
        snprintf(out->methods[out->n_methods++], 24, "%s", out->macro);
    out->call_meta = 1;
    out->call_self = 1; /* this process is a concept; depth bounds the stack */
    if (out->call_meta) {
        ikitaku_thinking_t th;
        ikitaku_strategy_map_t sm;
        ikitaku_strategy_load(&sm);
        ikitaku_think_about_thinking(NULL, &sm, &th);
        if (out->n_methods < 8)
            snprintf(out->methods[out->n_methods++], 24, "meta");
    }
    {
        ikitaku_self_t sl;
        ikitaku_self_reflect(math_conf < 50 ? 30 : 10, 0, 0, &pk, &sl);
        if (out->n_methods < 8)
            snprintf(out->methods[out->n_methods++], 24, "self");
    }
    if (depth > 0) {
        ikitaku_r6_t inner;
        ikitaku_r6_compose(objective, revisit_ok, (uint8_t)(depth - 1),
                           math_conf, rule_conf, desperation, &inner);
    }
    snprintf(out->english, sizeof out->english,
             "R6 compose macro=%s lead=%s depth=%u self=%u meta=%u n=%d obj=%s",
             out->macro,
             out->plan.n ? out->plan.form[out->plan.chosen].name : "-",
             (unsigned)out->depth, (unsigned)out->call_self,
             (unsigned)out->call_meta, out->n_methods,
             objective ? objective : "-");
    return out->n_methods;
}

static int lie_known(const char* a, const char* b) {
    if (!a || !b) return 30;
    if (strcmp(a, b) == 0)
        return (strcmp(a, "invert")==0) ? 0 : 15;
    const char *A = a, *B = b;
    if (strcmp(A, B) > 0) { const char* t = A; A = B; B = t; }
    if (!strcmp(A,"copy")) return 5;
    if (!strcmp(A,"info") && !strcmp(B,"name")) return 10;
    if (!strcmp(A,"repeat") && !strcmp(B,"wait")) return 72;
    if (!strcmp(A,"join") && !strcmp(B,"split")) return 68;
    if (!strcmp(A,"filter") && !strcmp(B,"map")) return 48;
    if (!strcmp(A,"invert") && !strcmp(B,"map")) return 42;
    if (!strcmp(A,"bound") && !strcmp(B,"map")) return 36;
    if (!strcmp(A,"decision") && !strcmp(B,"repeat")) return 58;
    if (!strcmp(A,"absorb") || !strcmp(B,"absorb") ||
        !strcmp(A,"emit") || !strcmp(B,"emit")) return 80;
    if (!strcmp(A,"map") && !strcmp(B,"map")) return 22;
    float aff = ikitaku_affinity(a, b);
    int from_aff = 50 - (int)(aff * 40.0f);
    if (from_aff < 8) from_aff = 8;
    if (from_aff > 90) from_aff = 90;
    return from_aff;
}

static const ikitaku_cattr_t CATTR[IKITAKU_CATTR_N] = {
    {"small","magnitude","small","big",4,0,-1},
    {"big","magnitude","small","big",4,0,1},
    {"magnitude","magnitude","small","big",5,0,0},
    {"neg","polarity","neg","pos",4,0,-1},
    {"pos","polarity","neg","pos",4,0,1},
    {"polarity","polarity","neg","pos",5,0,0},
    {"impure","purity","impure","pure",5,0,-1},
    {"pure","purity","impure","pure",5,0,1},
    {"slow","rate","slow","fast",4,1,-1},
    {"fast","rate","slow","fast",4,1,1},
    {"local","span","local","far",3,0,-1},
    {"far","span","local","far",3,0,1},
    {"finite","bound","finite","open",5,0,-1},
    {"open","bound","finite","open",5,0,1},
    {"inner","side","inner","outer",4,0,-1},
    {"outer","side","inner","outer",4,0,1},
};

int ikitaku_cattr_lookup(const char* name, ikitaku_cattr_t* out) {
    if (!name || !out) return -1;
    for (int i = 0; i < IKITAKU_CATTR_N; ++i) {
        if (!strcmp(CATTR[i].name, name)) { *out = CATTR[i]; return 0; }
    }
    return -1;
}

int ikitaku_compare_axis(const char* A, const char* B, ikitaku_lie_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->A, sizeof out->A, "%s", A ? A : "?");
    snprintf(out->B, sizeof out->B, "%s", B ? B : "?");
    ikitaku_cattr_t a, b;
    int ga = ikitaku_cattr_lookup(A, &a);
    int gb = ikitaku_cattr_lookup(B, &b);
    if (ga != 0 || gb != 0) return ikitaku_lie_bracket(A, B, out);
    if (!strcmp(a.axis, b.axis)) {
        if (a.pole && b.pole && a.pole != b.pole) {
            /* opposing poles: leftover IS the axis (Hegel on the comparison) */
            out->mag = 72;
            out->unity = 28;
            out->hegel = 1;
            snprintf(out->leftover, sizeof out->leftover, "%s", a.axis);
        } else if (a.pole == 0 || b.pole == 0) {
            out->mag = 15;
            out->unity = 85;
            snprintf(out->leftover, sizeof out->leftover, "%s", a.axis);
        } else {
            out->mag = 10;
            out->unity = 90;
            snprintf(out->leftover, sizeof out->leftover, "%s", a.axis);
        }
    } else {
        out->mag = 50;
        out->unity = 50;
        out->hegel = 1;
        snprintf(out->leftover, sizeof out->leftover, "[%s,%s]", a.axis, b.axis);
    }
    snprintf(out->english, sizeof out->english,
             "compare-axis %s/%s leftover=%s mag=%d L%u/%u %s",
             out->A, out->B, out->leftover, out->mag,
             (unsigned)a.layer, (unsigned)b.layer,
             a.adverb || b.adverb ? "adverb" : "adj");
    return out->unity;
}

int ikitaku_lie_bracket(const char* A, const char* B, ikitaku_lie_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->A, sizeof out->A, "%s", A ? A : "?");
    snprintf(out->B, sizeof out->B, "%s", B ? B : "?");
    out->mag = lie_known(A, B);
    {
        ikitaku_cattr_t ca, cb;
        if (ikitaku_cattr_lookup(A, &ca)==0 && ikitaku_cattr_lookup(B, &cb)==0) {
            ikitaku_lie_t ax;
            ikitaku_compare_axis(A, B, &ax);
            out->mag = ax.mag;
            out->unity = ax.unity;
            out->hegel = ax.hegel;
            snprintf(out->leftover, sizeof out->leftover, "%s", ax.leftover);
            snprintf(out->english, sizeof out->english, "%s", ax.english);
            return out->unity;
        }
    }
    out->unity = 100 - out->mag;
    out->hegel = out->mag >= 40;
    snprintf(out->leftover, sizeof out->leftover, "[%s,%s]",
             out->A, out->B);
    snprintf(out->english, sizeof out->english,
             "Lie [%s,%s]=%d Kant-unity=%d Hegel-leftover=%s",
             out->A, out->B, out->mag, out->unity,
             out->hegel ? out->leftover : "0");
    return out->unity;
}

int ikitaku_lie_scan(char verbs[][24], int n, ikitaku_lie_t* worst) {
    if (!worst) return -1;
    memset(worst, 0, sizeof *worst);
    worst->unity = 100;
    if (!verbs || n < 2) {
        snprintf(worst->english, sizeof worst->english, "Lie scan: <2 steps, unity=100");
        return 100;
    }
    int best_i = 0, best_m = -1;
    ikitaku_lie_t tmp;
    for (int i = 0; i < n - 1; ++i) {
        ikitaku_lie_bracket(verbs[i], verbs[i+1], &tmp);
        if (tmp.mag > best_m) { best_m = tmp.mag; best_i = i; *worst = tmp; }
    }
    snprintf(worst->english, sizeof worst->english,
             "hottest joint %d [%s,%s]=%d unity=%d",
             best_i, worst->A, worst->B, worst->mag, worst->unity);
    return best_i;
}

int ikitaku_goal_compose(char verbs[][24], int n, ikitaku_goal_comp_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    int m = n < IKITAKU_GOAL_STEPS ? n : IKITAKU_GOAL_STEPS;
    for (int i = 0; i < m; ++i)
        snprintf(out->verb[i], 24, "%s", verbs[i]);
    out->n = m;
    for (int i = 0; i < IKITAKU_GOAL_STEPS; ++i) out->work_mag[i] = -1;
    ikitaku_lie_t w;
    out->joint = ikitaku_lie_scan(out->verb, out->n, &w);
    out->joint_mag = w.mag;
    out->unity = w.unity;
    snprintf(out->english, sizeof out->english,
             "compose n=%d %s", out->n, w.english);
    return out->n;
}

int ikitaku_goal_extract(const char* have_r5, const char* want_r5,
                         int residual, ikitaku_goal_comp_t* out) {
    if (!out) return -1;
    char v[IKITAKU_GOAL_STEPS][24];
    int n = 0;
    if (residual >= 25)
        snprintf(v[n++], 24, "filter");
    if (have_r5 && want_r5 && strcmp(have_r5, want_r5) != 0)
        snprintf(v[n++], 24, "invert");
    snprintf(v[n++], 24, "%s", want_r5 && want_r5[0] ? want_r5 : "map");
    if (have_r5 && want_r5 && strcmp(have_r5, want_r5) != 0)
        snprintf(v[n++], 24, "%s", have_r5);
    return ikitaku_goal_compose(v, n, out);
}

int ikitaku_goal_curve(const ikitaku_goal_comp_t* g, ikitaku_curve_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!g || g->n < 2) {
        snprintf(out->english, sizeof out->english, "curve 0 (flat / single step)");
        return 0;
    }
    out->n = g->n - 1;
    out->maxm = -1;
    for (int i = 0; i < out->n; ++i) {
        int mag;
        if (g->work_mag[i] >= 0) mag = g->work_mag[i];
        else {
            ikitaku_lie_t L;
            ikitaku_lie_bracket(g->verb[i], g->verb[i+1], &L);
            mag = L.mag;
        }
        out->mag[i] = mag;
        out->total += mag;
        if (mag > out->maxm) { out->maxm = mag; out->argmax = i; }
    }
    snprintf(out->english, sizeof out->english,
             "curve total=%d max=%d@%d n=%d",
             out->total, out->maxm, out->argmax, out->n);
    return out->total;
}

int ikitaku_goal_approach(ikitaku_goal_comp_t* g, ikitaku_curve_t* c) {
    if (!g || !c) return -1;
    int step = c->step;
    int before = ikitaku_goal_curve(g, c);
    c->step = step;
    if (g->n < 2 || c->maxm < 12) {
        c->flattened = 1;
        snprintf(c->english, sizeof c->english, "already flat total=%d", before);
        return 0;
    }
    int i = c->argmax;
    const char *A = g->verb[i], *B = g->verb[i+1];
    ikitaku_lie_t ab, ba;
    ikitaku_lie_bracket(A, B, &ab);
    ikitaku_lie_bracket(B, A, &ba);
    /* fuzzy TAS: keep / swap / merge */
    int s_keep = 100 - ab.mag;
    int s_swap = 100 - ba.mag;
    int s_merge = 55 + (ab.hegel ? 20 : 0);
    int hypo = 0, best = s_keep;
    if (s_swap > best + 8) { best = s_swap; hypo = 1; }
    if (s_merge > best + 5 && ab.mag >= 75) { best = s_merge; hypo = 2; }
    int frac = 35;
    if (ab.mag >= 70) frac = 45;
    if (ab.mag >= 40)
        ikitaku_goal_create(A, B, NULL);
    c->hypo = hypo;
    c->frac = frac;
    c->step = step + 1;

    if (hypo == 1) {
        char t[24];
        snprintf(t, 24, "%s", g->verb[i]);
        snprintf(g->verb[i], 24, "%s", g->verb[i+1]);
        snprintf(g->verb[i+1], 24, "%s", t);
        g->work_mag[i] = -1;
    } else if (hypo == 2 && best >= 70) {
        char merged[32];
        if (strcmp(A, B) > 0) snprintf(merged, 32, "%s_x_%s", B, A);
        else snprintf(merged, 32, "%s_x_%s", A, B);
        char nv[IKITAKU_GOAL_STEPS][24];
        int n = 0;
        for (int k = 0; k < g->n; ++k) {
            if (k == i) { snprintf(nv[n++], 24, "%s", merged); k++; continue; }
            snprintf(nv[n++], 24, "%s", g->verb[k]);
        }
        int wm[IKITAKU_GOAL_STEPS];
        memcpy(wm, g->work_mag, sizeof wm);
        ikitaku_goal_compose(nv, n, g);
        for (int k = 0; k < IKITAKU_GOAL_STEPS; ++k) g->work_mag[k] = -1;
    } else {
        /* fractional Δ: shrink the gap in the detected direction */
        int cur = c->maxm;
        int nxt = cur - (cur * frac) / 100;
        if (nxt < 0) nxt = 0;
        g->work_mag[i] = nxt;
    }

    ikitaku_curve_t after;
    int tot = ikitaku_goal_curve(g, &after);
    after.step = c->step;
    after.frac = frac;
    after.hypo = hypo;
    after.flattened = tot < before;
    const char* hname[] = {"keep-frac","swap","merge"};
    snprintf(after.english, sizeof after.english,
             "approach step %d hypo=%s frac=%d curve %d→%d %s",
             after.step, hname[hypo], frac, before, tot,
             after.flattened ? "closer" : "held");
    *c = after;
    return after.flattened ? 1 : 0;
}

int ikitaku_goal_create(const char* A, const char* B, ikitaku_r3_rec_t* out) {
    if (!A || !B) return -1;
    ikitaku_r3_rec_t rec;
    int st = ikitaku_F_merge_lower(A, B, &rec);
    if (st != 0) return -1;
    char fam[32];
    snprintf(fam, sizeof fam, "%s_x_%s", strcmp(A,B)>0?B:A, strcmp(A,B)>0?A:B);
    int already = ikitaku_r4_propose(A, fam, "created leftover genus");
    ikitaku_discovery_t d;
    memset(&d, 0, sizeof d);
    snprintf(d.r5, sizeof d.r5, "%s", A);
    snprintf(d.r4, sizeof d.r4, "%s", fam);
    snprintf(d.form, sizeof d.form, "%s", rec.name);
    snprintf(d.evidence, sizeof d.evidence, "TAS leftover [%s,%s] not catalog", A, B);
    d.conf = 45;
    d.locked = 0;
    ikitaku_discovery_propose(&d);
    if (out) *out = rec;
    return already == 1 ? 1 : 0; /* 1 = already had r4, 0 = new */
}

int ikitaku_objective_solve(const char* goal_r5, const char* bag_r5,
                            const char* Fa, const char* Fb,
                            int residual, int possible,
                            uint8_t depth, ikitaku_solve_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (depth > IKITAKU_R6_DEPTH_MAX) depth = IKITAKU_R6_DEPTH_MAX;
    ikitaku_rank_gate(goal_r5, bag_r5, residual, possible, &out->gate);
    ikitaku_goal_extract(bag_r5, goal_r5, residual, &out->goal);
    ikitaku_goal_curve(&out->goal, &out->curve);
    for (int s = 0; s < 3 && out->curve.maxm >= 20; ++s)
        ikitaku_goal_approach(&out->goal, &out->curve);
    int revisit = residual >= 20 || possible < 70 || out->goal.joint_mag >= 40;
    ikitaku_r6_compose(goal_r5, revisit, depth, possible, 60,
                       residual >= 25 ? 3 : 1, &out->r6);
    out->depth_used = 1;
    if (Fa && Fb && strcmp(Fa, Fb) != 0) {
        if (ikitaku_F_merge_lower(Fa, Fb, &out->genus) == 0)
            out->merged = 1;
        ikitaku_lie_bracket(Fa, Fb, &out->lie);
    } else if (out->goal.n >= 2) {
        ikitaku_lie_bracket(out->goal.verb[out->goal.joint],
                            out->goal.verb[out->goal.joint + 1 < out->goal.n
                                           ? out->goal.joint + 1 : out->goal.joint],
                            &out->lie);
    }
    out->inconsistent = (out->lie.mag >= 40);
    if (out->lie.A[0] && out->lie.B[0] && out->lie.mag >= 40) {
        ikitaku_r3_rec_t rec;
        int had = ikitaku_goal_create(out->lie.A, out->lie.B, &rec);
        if (had == 0) { out->created = 1; out->genus = rec; }
        else if (had == 1) out->genus = rec;
    }
    if (out->inconsistent && Fa && Fb) {
        ikitaku_hypo_t t, a, s;
        memset(&t, 0, sizeof t); memset(&a, 0, sizeof a);
        snprintf(t.purpose, sizeof t.purpose, "%s", Fa); t.confidence = 60;
        snprintf(a.purpose, sizeof a.purpose, "%s", Fb); a.confidence = 60;
        ikitaku_critique_t cr;
        ikitaku_kant_tas(&t, &a, &s, &cr);
        if (s.form_id[0] && !out->merged)
            ikitaku_F_merge_lower(Fa, Fb, &out->genus);
    }
    if (out->r6.call_self && depth > 0) {
        ikitaku_solve_t inner;
        ikitaku_objective_solve(goal_r5, bag_r5, NULL, NULL,
                                residual > 8 ? residual - 8 : 0,
                                possible < 95 ? possible + 5 : 95,
                                (uint8_t)(depth - 1), &inner);
        out->depth_used += inner.depth_used;
    }
    snprintf(out->english, sizeof out->english,
             "solve %s lie=%d unity=%d inconsist=%d curve=%d %s",
             out->gate.english, out->lie.mag, out->lie.unity,
             out->inconsistent, out->curve.total, out->goal.english);
    {
        ikitaku_autonomy_t au;
        ikitaku_objective_run(bag_r5, goal_r5, residual, 4, &au);
        int nlen = (int)strlen(out->english);
        snprintf(out->english + nlen, sizeof(out->english) - (size_t)nlen,
                 " | auto %s", au.english);
        if (au.solved) out->inconsistent = 0;
    }
    return out->gate.r7_right_task;
}

static void auto_tally(ikitaku_autonomy_t* a) {
    a->n_working = a->n_gap = a->n_done = 0;
    int sum = 0, mn = 100, mx = 0, cmax = 0;
    for (int i = 0; i < a->n; ++i) {
        if (!strcmp(a->sub[i].status, "done")) a->n_done++;
        else if (!strcmp(a->sub[i].status, "gap")) a->n_gap++;
        else a->n_working++;
        int cf = a->sub[i].conf;
        if (cf < mn) mn = cf;
        if (cf > mx) mx = cf;
        sum += cf;
        if (a->sub[i].confusion > cmax) cmax = a->sub[i].confusion;
    }
    a->min_conf = a->n ? mn : 0;
    a->max_conf = a->n ? mx : 0;
    int avg = a->n ? sum / a->n : 0;
    /* composition: weakest cell weighs more than the mean */
    a->macro_conf = a->n ? (2 * a->min_conf + avg) / 3 : 0;
    a->macro_confusion = cmax;
}

int ikitaku_objective_split(const char* have_r5, const char* want_r5,
                            int residual, ikitaku_autonomy_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->parent, sizeof out->parent, "%s", want_r5 ? want_r5 : "?");
    ikitaku_goal_comp_t g;
    ikitaku_goal_extract(have_r5, want_r5, residual, &g);
    ikitaku_curve_t c;
    ikitaku_goal_curve(&g, &c);
    out->assembled = g;
    for (int i = 0; i < g.n && out->n < IKITAKU_SUB_MAX; ++i) {
        ikitaku_subobj_t* s = &out->sub[out->n++];
        snprintf(s->verb, sizeof s->verb, "%s", g.verb[i]);
        ikitaku_r5_verb_t v;
        int known = ikitaku_r5_lookup(s->verb, &v) == 0;
        s->unity = known ? 70 : 35;
        s->mag = 0;
        s->conf = s->unity;
        s->confusion = known ? 15 : 50;
        snprintf(s->status, sizeof s->status, known ? "working" : "gap");
        if (i < c.n && c.mag[i] >= 40 && out->n < IKITAKU_SUB_MAX) {
            ikitaku_subobj_t* j = &out->sub[out->n++];
            snprintf(j->verb, sizeof j->verb, "[%s,%s]",
                     g.verb[i], g.verb[i+1]);
            j->mag = c.mag[i];
            j->unity = 100 - c.mag[i];
            j->conf = j->unity;
            j->confusion = j->mag;
            snprintf(j->status, sizeof j->status, "gap");
        }
    }
    auto_tally(out);
    ikitaku_cell_attend(out);
    snprintf(out->english, sizeof out->english,
             "split parent=%s n=%d working=%d gap=%d macro_conf=%d confu=%d min=%d",
             out->parent, out->n, out->n_working, out->n_gap,
             out->macro_conf, out->macro_confusion, out->min_conf);
    return out->n;
}

int ikitaku_objective_assemble(ikitaku_autonomy_t* a) {
    if (!a) return -1;
    char vs[IKITAKU_GOAL_STEPS][24];
    int n = 0;
    for (int i = 0; i < a->n && n < IKITAKU_GOAL_STEPS; ++i) {
        if (a->sub[i].verb[0]=='[')
            continue; /* joint tags never become verbs */
        if (!strcmp(a->sub[i].status, "working") ||
            !strcmp(a->sub[i].status, "done"))
            snprintf(vs[n++], 24, "%s", a->sub[i].verb);
    }
    if (n == 0) {
        for (int i = 0; i < a->assembled.n && n < IKITAKU_GOAL_STEPS; ++i)
            snprintf(vs[n++], 24, "%s", a->assembled.verb[i]);
    }
    ikitaku_goal_compose(vs, n, &a->assembled);
    ikitaku_curve_t c;
    ikitaku_goal_curve(&a->assembled, &c);
    a->solved = (a->n_gap == 0 && c.maxm < 40);
    snprintf(a->english, sizeof a->english,
             "assemble n=%d curve=%d solved=%d working=%d gap=%d",
             a->assembled.n, c.total, a->solved, a->n_working, a->n_gap);
    return a->assembled.n;
}

int ikitaku_objective_run(const char* have_r5, const char* want_r5,
                          int residual, int budget, ikitaku_autonomy_t* out) {
    if (!out) return -1;
    if (budget < 1) budget = 1;
    if (budget > 64) budget = 64;
    ikitaku_objective_split(have_r5, want_r5, residual, out);
    out->remaining = budget;
    out->give_up = 0;
    out->cap = 64;
    /* generate default when want is create/new bag */
    if (want_r5 && (!strcmp(want_r5, "generate") || !strcmp(want_r5, "create"))) {
        snprintf(out->method_pick, sizeof out->method_pick, "generate");
        out->method_score = 70;
    }
    ikitaku_agenda_t ag;
    ikitaku_agenda_init(&ag);
    for (int i = 0; i < out->n; ++i)
        ikitaku_agenda_add(&ag, out->sub[i].verb, IKITAKU_GOAL_ANALYZE,
                           strcmp(out->sub[i].status,"gap")==0 ? 80 : 40, 40);
    int done_prev = out->n_done;
    /* Healthy solve mode: cascade vs collapse */
    ikitaku_mode_pick_t mode;
    int despair0 = residual >= 60 ? 6 : (residual >= 40 ? 3 : 1);
    ikitaku_solve_mode_pick(residual, despair0, 0, out->macro_conf,
                            residual >= 55, &mode);
    if (mode.mode == IKITAKU_MODE_CASCADE || mode.mode == IKITAKU_MODE_ALTERNATE) {
        if (ikitaku_macro_eval_ready(out))
            ikitaku_upward_cascade(out);
    }
    for (out->ticks = 0; out->remaining > 0; ++out->ticks) {
        if (out->n_gap == 0) { out->solved = 1; break; }
        out->remaining--;
        /* re-pick mode each tick from residual proxy (macro_confusion) */
        int res_proxy = out->macro_confusion > 0 ? out->macro_confusion : residual;
        int despair = res_proxy >= 60 ? 6 : (res_proxy >= 40 ? 3 : 1);
        ikitaku_solve_mode_pick(res_proxy, despair, 0, out->macro_conf,
                                out->macro_conf < 45, &mode);

        ikitaku_curve_t stepc;
        memset(&stepc, 0, sizeof stepc);
        if (mode.mode == IKITAKU_MODE_COLLAPSE || mode.reason_occam) {
            /* Occam collapse: approach only, no extra cascade */
            ikitaku_goal_approach(&out->assembled, &stepc);
        } else {
            /* CASCADE: approach + cascade if ready */
            ikitaku_goal_approach(&out->assembled, &stepc);
            if (ikitaku_macro_eval_ready(out) && !out->cascade_fired)
                ikitaku_upward_cascade(out);
        }
        ikitaku_curve_t c;
        ikitaku_goal_curve(&out->assembled, &c);
        if (c.maxm >= 40 && out->assembled.n >= 2) {
            int i = c.argmax;
            ikitaku_goal_create(out->assembled.verb[i],
                                out->assembled.verb[i+1], NULL);
            ikitaku_hypo_t t, a, s;
            memset(&t,0,sizeof t); memset(&a,0,sizeof a); memset(&s,0,sizeof s);
            ikitaku_hypo_fill(&t, out->assembled.verb[i],
                              out->assembled.verb[i], 60, 1);
            ikitaku_hypo_fill(&a, out->assembled.verb[i+1],
                              out->assembled.verb[i+1], 55, 0);
            /* structured dialectic then kant TAS */
            if (!ikitaku_dialectic(&t, &a, &s)) {
                ikitaku_critique_t cr;
                ikitaku_kant_tas(&t, &a, &s, &cr);
            }
            if (s.synthesized && s.purpose[0] && out->method_pick[0] == 0)
                snprintf(out->method_pick, sizeof out->method_pick, "%s", s.purpose);
        }
        /* re-mark joints from new curve */
        for (int i = 0; i < out->n; ++i) {
            if (out->sub[i].verb[0] != '[') continue;
            int mag = c.maxm;
            for (int k = 0; k < c.n; ++k)
                if (c.mag[k] < mag) { /* keep */ }
            /* bind to assembled joint if names match */
            for (int k = 0; k + 1 < out->assembled.n; ++k) {
                char tag[32];
                snprintf(tag, sizeof tag, "[%s,%s]",
                         out->assembled.verb[k], out->assembled.verb[k+1]);
                if (strcmp(tag, out->sub[i].verb)==0) {
                    mag = (k < c.n) ? c.mag[k] : mag;
                    break;
                }
            }
            out->sub[i].mag = mag;
            out->sub[i].unity = 100 - mag;
            out->sub[i].conf = out->sub[i].unity;
            out->sub[i].confusion = mag;
            if (mag < 40)
                snprintf(out->sub[i].status, sizeof out->sub[i].status, "done");
        }
        auto_tally(out);
        ikitaku_cell_attend(out);
        if (ikitaku_macro_eval_ready(out) && !out->cascade_fired)
            ikitaku_upward_cascade(out);
        int gained = out->n_done - done_prev;
        int progress = stepc.flattened;
        if (gained > 0 && progress)
            out->cap += gained;
        if (gained > 0 && out->assembled.n >= 3 &&
            stepc.argmax > 0 && stepc.argmax + 1 < out->assembled.n - 1)
            out->cap += 15;
        if (gained > 0) {
            out->remaining += 3 * gained;
            done_prev = out->n_done;
        }
        if (out->remaining > out->cap) out->remaining = out->cap;
        if (out->n_gap == 0) { out->solved = 1; break; }
        if (out->remaining <= 0) { out->give_up = 1; break; }
    }
    if (out->remaining <= 0 && !out->solved) out->give_up = 1;
    snprintf(out->english, sizeof out->english,
             "run ticks=%d rem=%d cap=%d give_up=%d solved=%d working=%d gap=%d done=%d assembled=%d cascade=%d meth=%s/%d %s",
             out->ticks, out->remaining, out->cap, out->give_up, out->solved,
             out->n_working, out->n_gap, out->n_done,
             out->assembled.n, out->cascade_fired,
             out->method_pick[0] ? out->method_pick : "-", out->method_score,
             out->assembled.english);
    return out->solved;
}

int ikitaku_macro_eval_ready(const ikitaku_autonomy_t* a) {
    if (!a || a->n < 4) return 0;
    int gathered = a->n_working + a->n_done;
    if (gathered < 3) return 0;
    /* significant: enough cells, spread, or macro not noise */
    int spread = a->max_conf - a->min_conf;
    return (a->macro_conf >= 40 && spread >= 10) || gathered >= 5 || a->n_done >= 2;
}

int ikitaku_upward_cascade(ikitaku_autonomy_t* a) {
    if (!a) return -1;
    a->cascade_fired = 1;
    /* R3 cells → R4 leftover nature → R5 parent → R6 method as self-eval */
    const char* r5 = a->parent[0] ? a->parent : "theorize";
    int residual = a->macro_confusion;
    ikitaku_method_pick_t pk;
    ikitaku_method_hegel_pick(residual, a->macro_conf, 100 - a->min_conf,
                              a->macro_confusion >= 40 ? 3 : 1, &pk);
    snprintf(a->method_pick, sizeof a->method_pick, "%s",
             pk.m[pk.chosen].name);
    a->method_score = pk.m[pk.chosen].score;
    if (residual >= 40) {
        ikitaku_approach_t ap;
        memset(&ap, 0, sizeof ap);
        ikitaku_approach_shift(a->method_pick, residual,
                               residual >= 60 ? 6 : 3, residual >= 60, 0, &ap);
        if (ap.next_lead[0])
            snprintf(a->method_pick, sizeof a->method_pick, "%s", ap.next_lead);
    }
    ikitaku_self_t sl;
    ikitaku_self_reflect(residual, a->n_done, a->solved, &pk, &sl);
    /* if method leftover is new, that's methodology expansion */
    if (strchr(a->method_pick, '_'))
        ikitaku_goal_create("theorize", a->method_pick, NULL);
    return a->method_score;
}

int ikitaku_cell_attend(ikitaku_autonomy_t* a) {
    if (!a) return -1;
    a->n_good = a->n_bad = 0;
    int boost = 0;
    const char* near = NULL;
    const char* far = NULL;
    for (int i = 0; i < a->n; ++i) {
        ikitaku_subobj_t* s = &a->sub[i];
        int mc = s->conf - s->confusion / 2;
        if (mc < 0) mc = 0;
        if (mc > 100) mc = 100;
        s->meta_conf = mc;
        s->why_fail[0] = 0;
        int good = (s->conf >= 60 && s->confusion < 30);
        int bad  = (s->conf < 45 || s->confusion >= 45);
        if (good) {
            s->attention = 20 + s->conf / 5;
            s->meta_conf = s->meta_conf < 70 ? 70 : s->meta_conf;
            a->n_good++;
            boost++;
            if (!near) near = s->verb;
        } else if (bad) {
            s->attention = 5;
            ikitaku_critique_t cr, ch;
            ikitaku_critique(s->verb, 0, 0, 1, s->confusion >= 50, &cr);
            ikitaku_critique_challenge(&cr, s->verb, s->conf, s->confusion, 1, &ch);
            s->meta_conf = ch.conf > 30 ? 25 : ch.conf;
            snprintf(s->why_fail, sizeof s->why_fail,
                     "why-fail: %s open=%d hidden_d=%d act=%s",
                     ch.axiom_name, ch.presupposition_open, ch.hidden_d,
                     ch.action);
            a->n_bad++;
            if (!far) far = s->verb;
        } else {
            s->attention = 10;
        }
    }
    if (boost > 0) {
        a->remaining += boost;
        if (a->remaining > a->cap && a->cap > 0)
            a->remaining = a->cap;
    }
    if (near && far) {
        ikitaku_lie_t L;
        ikitaku_bind_near_far(near, far, &L);
        snprintf(a->bind, sizeof a->bind, "%s", L.leftover);
    }
    return a->n_good;
}

int ikitaku_bind_near_far(const char* near, const char* far, ikitaku_lie_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (!near || !far) return -1;
    ikitaku_lie_bracket(near, far, out);
    ikitaku_hypo_t t, an, s;
    memset(&t, 0, sizeof t); memset(&an, 0, sizeof an);
    snprintf(t.purpose, sizeof t.purpose, "%s", near);
    snprintf(an.purpose, sizeof an.purpose, "%s", far);
    t.confidence = 70; an.confidence = 30;
    ikitaku_critique_t cr;
    ikitaku_kant_tas(&t, &an, &s, &cr);
    if (s.form_id[0])
        snprintf(out->leftover, sizeof out->leftover, "%s", s.form_id);
    snprintf(out->english, sizeof out->english,
             "bind near=%s far=%s leftover=%s mag=%d (array of idea-cells)",
             near, far, out->leftover, out->mag);
    return out->unity;
}

int ikitaku_bag_delegate(const char* task, uint8_t layer, ikitaku_bag_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->name, sizeof out->name, "bag-L%u", (unsigned)layer);
    out->layer = layer > 6 ? 6 : layer;
    snprintf(out->task, sizeof out->task, "%s", task ? task : "probe");
    out->watching = (out->layer < 6); /* R6 watches; lower executes */
    /* lower layers: watch/probe/rule already exist — bag is the box, not a new VM */
    if (out->layer <= 2) out->conf = 55;
    else if (out->layer <= 4) out->conf = 60;
    else out->conf = 40;
    snprintf(out->english, sizeof out->english,
             "delegate '%s' to L%u; R6 watching=%d (exec not identity)",
             out->task, (unsigned)out->layer, out->watching);
    return out->watching;
}

int ikitaku_analogy(const char* A, const char* B, const char* C, const char* D,
                    ikitaku_analogy_t* out) {
    if (!out || !A || !B || !C) return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->A, sizeof out->A, "%s", A);
    snprintf(out->B, sizeof out->B, "%s", B);
    snprintf(out->C, sizeof out->C, "%s", C);
    ikitaku_lie_t ab;
    ikitaku_lie_bracket(A, B, &ab);
    out->mag_ab = ab.mag;
    snprintf(out->nature, sizeof out->nature, "%s",
             ab.hegel ? ab.leftover : (ab.unity >= 70 ? "commute" : "map"));
    if (D && D[0]) {
        snprintf(out->D, sizeof out->D, "%s", D);
        out->inferred = 0;
    } else {
        /* same nature: leftover of C with B's role */
        if (!strcmp(A, C)) snprintf(out->D, sizeof out->D, "%s", B);
        else if (ab.unity >= 85) snprintf(out->D, sizeof out->D, "%s", C);
        else {
            const char *lo = strcmp(C,B)>0 ? B : C;
            const char *hi = strcmp(C,B)>0 ? C : B;
            snprintf(out->D, sizeof out->D, "%s_x_%s", lo, hi);
        }
        out->inferred = 1;
        static const char* cand[] = {
            "wait","repeat","decision","invert","filter","map","order",
            "join","split","copy","bound","emit","absorb","theorize"
        };
        int best = -1, best_sim = -1;
        for (int i = 0; i < 14; ++i) {
            if (!strcmp(cand[i], C) || !strcmp(cand[i], A)) continue;
            ikitaku_lie_t cd;
            ikitaku_lie_bracket(C, cand[i], &cd);
            int da = ab.mag - cd.mag; if (da < 0) da = -da;
            float fa = ikitaku_affinity(A, B);
            float fc = ikitaku_affinity(C, cand[i]);
            int dd = (int)((fa - fc) * 100.0f); if (dd < 0) dd = -dd;
            int s = 100 - (da + dd) / 2;
            if (s > best_sim) { best_sim = s; best = i; }
        }
        if (best >= 0 && best_sim >= 55) {
            snprintf(out->D, sizeof out->D, "%s", cand[best]);
            out->inferred = 1;
        }
    }
    ikitaku_lie_t cd;
    ikitaku_lie_bracket(C, out->D, &cd);
    out->mag_cd = cd.mag;
    int da = out->mag_ab - out->mag_cd; if (da < 0) da = -da;
    float fa = ikitaku_affinity(A, B), fc = ikitaku_affinity(C, out->D);
    int dd = (int)((fa - fc) * 100.0f); if (dd < 0) dd = -dd;
    out->sim = 100 - (da + dd) / 2;
    if (out->sim < 0) out->sim = 0;
    snprintf(out->english, sizeof out->english,
             "analogy %s:%s :: %s:%s nature=%s sim=%d mag %d/%d %s",
             out->A, out->B, out->C, out->D, out->nature, out->sim,
             out->mag_ab, out->mag_cd, out->inferred ? "inferred" : "given");
    return out->sim;
}


int ikitaku_hegel_bool16(const ikitaku_hypo_t* thesis,
                         const ikitaku_hypo_t* antithesis,
                         int allow_persist,
                         ikitaku_hegel_bool_t* out) {
    if (!thesis || !antithesis || !out) return -1;
    memset(out, 0, sizeof *out);
    out->th_conf = thesis->confidence;
    out->anti_conf = antithesis->confidence;
    /* Correctness as charge: >=50 unity, <50 disunity */
    out->th_charge = (out->th_conf >= 50) ? 1 : -1;
    out->anti_charge = (out->anti_conf >= 50) ? 1 : -1;
    out->from50_th = out->th_conf - 50;
    if (out->from50_th < 0) out->from50_th = -out->from50_th;
    out->from50_anti = out->anti_conf - 50;
    if (out->from50_anti < 0) out->from50_anti = -out->from50_anti;
    /* Directional bias: more wrong pole → prefer its invert (antithesis of wrong) */
    int th_wrong = (out->th_conf < 50) ? (50 - out->th_conf) : 0;
    int anti_wrong = (out->anti_conf < 50) ? (50 - out->anti_conf) : 0;
    if (th_wrong > anti_wrong + 5) out->bias_dir = -1;      /* invert thesis */
    else if (anti_wrong > th_wrong + 5) out->bias_dir = +1; /* keep/favor thesis */
    else out->bias_dir = 0;

    /* Near-far Lie on purposes/forms for ambiguity reduction */
    const char* near = thesis->purpose[0] ? thesis->purpose : thesis->form_id;
    const char* far  = antithesis->purpose[0] ? antithesis->purpose : antithesis->form_id;
    ikitaku_lie_t L;
    memset(&L, 0, sizeof L);
    ikitaku_bind_near_far(near, far, &L);
    out->lie_mag = L.mag;
    out->ambiguity_reduced = (L.mag >= 40 && L.mag <= 90) ? 1 : 0;

    int best = -1, best_s = -9999;
    for (int op = 0; op < 16; ++op) {
        int r = ikitaku_bool16(op, out->th_charge, out->anti_charge);
        int s = 50;
        /* Extreme correctness: reward synthesis that matches favored pole */
        if (out->bias_dir < 0) {
            /* want invert of thesis → prefer result opposite th_charge */
            if (r == -out->th_charge) s += 25 + th_wrong / 2;
            if (op == IKITAKU_B_NOT_A || op == IKITAKU_B_XOR || op == IKITAKU_B_NAND)
                s += 8;
        } else if (out->bias_dir > 0) {
            if (r == out->th_charge) s += 25 + anti_wrong / 2;
            if (op == IKITAKU_B_A || op == IKITAKU_B_AND || op == IKITAKU_B_OR)
                s += 8;
        } else {
            /* mid: prefer XNOR/AND when both high, XOR when split */
            if (out->th_conf >= 60 && out->anti_conf >= 60) {
                if (op == IKITAKU_B_AND || op == IKITAKU_B_XNOR) s += 20;
            } else if ((out->th_conf >= 60) != (out->anti_conf >= 60)) {
                if (op == IKITAKU_B_XOR || op == IKITAKU_B_OR) s += 18;
            }
        }
        /* Lie near-far: large gap favors XOR/disunity ops; tight favors AND/XNOR */
        if (out->ambiguity_reduced) {
            if (L.mag < 55 && (op == IKITAKU_B_AND || op == IKITAKU_B_XNOR)) s += 10;
            if (L.mag >= 70 && (op == IKITAKU_B_XOR || op == IKITAKU_B_NAND)) s += 10;
        }
        /* Hunt extremes: weight by |conf-50| of poles */
        s += (out->from50_th + out->from50_anti) / 10;
        out->scores[op] = s;
        if (s > best_s) { best_s = s; best = op; }
    }
    out->best_op = best;
    snprintf(out->op_name, sizeof out->op_name, "%s",
             best >= 0 ? ikitaku_bool16_name(best) : "?");
    out->synth_charge = (best >= 0)
        ? ikitaku_bool16(best, out->th_charge, out->anti_charge) : 0;

    /* Build synthesis hypo */
    memset(&out->synthesis, 0, sizeof out->synthesis);
    out->synthesis.method = IKITAKU_THINK_DIALECTIC;
    out->synthesis.synthesized = 1;
    snprintf(out->synthesis.form_id, sizeof out->synthesis.form_id,
             "bool16_%s", out->op_name);
    snprintf(out->synthesis.purpose, sizeof out->synthesis.purpose, "decision");
    out->synthesis.confidence = best_s > 95 ? 95 : (best_s < 20 ? 20 : best_s);
    out->synthesis.feasible = 1;
    out->synthesis.rewrite_ok = 0;
    snprintf(out->synthesis.note, sizeof out->synthesis.note,
             "fuzzy-hegel bool16 op=%s th=%+d anti=%+d → %+d bias=%d lie_mag=%d",
             out->op_name, out->th_charge, out->anti_charge, out->synth_charge,
             out->bias_dir, out->lie_mag);

    snprintf(out->english, sizeof out->english,
             "hegel-bool16: best=%s (score %d) th_c=%d anti_c=%d synth=%+d bias=%d "
             "lie_mag=%d amb_red=%d persist=%d",
             out->op_name, best_s, out->th_conf, out->anti_conf, out->synth_charge,
             out->bias_dir, out->lie_mag, out->ambiguity_reduced, allow_persist);

    if (allow_persist) {
        char val[48];
        snprintf(val, sizeof val, "%s:%+d", out->op_name, out->synth_charge);
        ikitaku_subject_teach("hegel_bool_last", "decision", "boolean-tt",
                              "last_op", val, out->synthesis.confidence);
    }
    return best;
}


int ikitaku_method_from_context(int residual, int pure, int math_conf, int rule_conf,
                                char lead[24], char english[160]) {
    if (!lead) return -1;
    int desperation = 1;
    if (residual >= 40) desperation = 3;
    if (residual >= 60) desperation = 5;
    if (residual >= 80) desperation = 7;
    ikitaku_thought_plan_t plan;
    ikitaku_thought_plan(desperation, math_conf, rule_conf, &plan);
    const char* name = (plan.n > 0 && plan.chosen >= 0 && plan.chosen < plan.n)
        ? plan.form[plan.chosen].name : "scientific";
    /* pure + low residual → Occam; impure/high residual → origin/hegelian */
    if (pure && residual < 25)
        name = "occam";
    else if (!pure && residual >= 40)
        name = (rule_conf >= 60) ? "origin" : "hegelian";
    else if (math_conf >= 80 && rule_conf < 50)
        name = "occam";
    snprintf(lead, 24, "%s", name);
    if (english)
        snprintf(english, 160,
                 "method-from-context residual=%d pure=%d → lead=%s (dep=%d math=%d rule=%d)",
                 residual, pure, lead, desperation, math_conf, rule_conf);
    return desperation;
}

int ikitaku_self_mod_closed(int residual, int math_conf, int rule_conf,
                            uint8_t depth, int allow_persist,
                            ikitaku_self_improve_t* first,
                            ikitaku_self_improve_t* second,
                            char english[240]) {
    ikitaku_self_improve_t local1, local2;
    if (!first) first = &local1;
    if (!second) second = &local2;
    int d1 = ikitaku_self_improve(residual, math_conf, rule_conf, depth,
                                  allow_persist, first);
    int mid = residual - first->improved;
    if (mid < 0) mid = 0;
    if (mid > residual) mid = residual;
    /* re-measure same bag residual through pathway again */
    int d2 = ikitaku_self_improve(mid, math_conf, rule_conf,
                                  depth > 0 ? (uint8_t)(depth - 1) : 0,
                                  0, second);
    int final_r = mid - second->improved;
    if (final_r < 0) final_r = 0;
    if (english)
        snprintf(english, 240,
                 "self-mod closed: residual %d→%d→%d drop1=%d drop2=%d invent=%d+%d persist=%d",
                 residual, mid, final_r, d1, d2,
                 first->invented_n, second->invented_n, allow_persist);
    return residual - final_r;
}

int ikitaku_goal_spine(const char* have_r5, const char* want_r5,
                       int residual, int possible_conf,
                       int budget, int self_mod, int allow_persist,
                       ikitaku_goal_spine_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->residual_in = residual;
    if (possible_conf < 0) possible_conf = 0;
    if (possible_conf > 100) possible_conf = 100;
    if (budget < 1) budget = 8;
    ikitaku_circle_init(&out->circle);
    ikitaku_trace_init(&out->trace);
    ikitaku_expr_init(&out->goal_bag);
    ikitaku_expr_init(&out->method_bag);

    /* ---- CONTROL STACK (unchanged role): method lead + ladder R9..R6 ---- */
    char meng[160];
    ikitaku_method_from_context(residual, residual < 30 ? 1 : 0,
                                possible_conf, 55, out->method_lead, meng);
    ikitaku_ladder_plan(want_r5, have_r5, NULL, NULL,
                        residual, possible_conf, 0, &out->ladder);
    out->n_think_passes = 1;

    /* ---- GOAL DETERMINATION glue: R5 expr tree + pattern on picked goal ----
       Control stack decided R7/R6_ready; substance is F(x)+c bag. */
    {
        char cres[48];
        snprintf(cres, sizeof cres, "residual=%d", residual);
        int have_i = ikitaku_expr_leaf(&out->goal_bag,
                                       have_r5 ? have_r5 : "map",
                                       "have", "bag-state", 5);
        int goal_i = ikitaku_expr_wrap(&out->goal_bag, have_i,
                                       want_r5 ? want_r5 : "seek",
                                       cres, 5);
        /* method lead as property +c composition */
        ikitaku_expr_compose_c(&out->goal_bag, goal_i, cres, out->method_lead);
        if (out->ladder.r6_ready || out->ladder.r7_pick > 0) {
            ikitaku_pattern_run(&out->goal_bag, goal_i, 2, 1, &out->goal_pattern);
            out->ran_goal_pattern = 1;
            if (out->goal_pattern.abstract.r5[0])
                snprintf(out->abstract_r5, sizeof out->abstract_r5, "%s",
                         out->goal_pattern.abstract.r5);
            /* substance over name: if pattern is cycle, prefer hegelian/origin lead */
            if (!strcmp(out->goal_pattern.pattern, "cycle") && residual >= 30)
                snprintf(out->method_lead, sizeof out->method_lead, "hegelian");
            else if (!strcmp(out->goal_pattern.pattern, "nested") && residual >= 40)
                snprintf(out->method_lead, sizeof out->method_lead, "origin");
        }
    }

    int res = residual;
    if (out->ladder.r6_ready) {
        ikitaku_objective_run(have_r5, want_r5, residual, budget, &out->autonomy);
        out->ran_objective = 1;
        if (out->autonomy.solved && res > 15)
            res = res / 2;
        else if (out->autonomy.n_done > 0 && res > 20)
            res = res - 8;
    }
    out->residual_mid = res;
    ikitaku_trace_push(&out->trace, out->method_lead, want_r5, have_r5,
                       res, possible_conf, out->ladder.r7_pick, out->ladder.r6_ready,
                       out->autonomy.solved ? 3 : 0);

    out->circle_act = ikitaku_circle_observe(
        &out->circle, out->method_lead, want_r5 ? want_r5 : "?",
        res, out->ladder.r7_pick, out->ladder.r6_ready);

    if (out->circle_act == IKITAKU_CIRCLE_HALT_PRIORS) {
        out->residual_out = res;
        ikitaku_reflect_chain(&out->trace, 0, 3, 0, &out->reflect);
        snprintf(out->english, sizeof out->english,
                 "goal-spine HALT residual=%d abstract_r5=%s | %s | %s",
                 res, out->abstract_r5[0]?out->abstract_r5:"-",
                 out->circle.english, out->reflect.english);
        return res;
    }

    int need_think = self_mod && (res >= 25 || !out->ran_objective
                                  || (out->ran_objective && !out->autonomy.solved)
                                  || out->circle_act == IKITAKU_CIRCLE_SELF_IMPROVE
                                  || out->circle_act == IKITAKU_CIRCLE_SEEK_PRIORS);

    if (need_think) {
        /* Self-mod SUBSTANCE: method as bag F(x)+c, not only name */
        {
            char mc[48];
            snprintf(mc, sizeof mc, "residual=%d", res);
            int mi = ikitaku_expr_leaf(&out->method_bag, out->method_lead,
                                       want_r5 ? want_r5 : "goal", mc, 6);
            ikitaku_expr_remember(&out->method_bag, mi);
            ikitaku_pattern_run(&out->method_bag, mi, 2, 1, &out->method_pattern);
            out->ran_method_pattern = 1;
        }

        ikitaku_self_improve_t a, b;
        char se[240];
        int drop = ikitaku_self_mod_closed(res, possible_conf, 55,
                                           2, allow_persist, &a, &b, se);
        out->improve = a;
        out->ran_improve = 1;
        out->closed_loop = (drop > 0) ? 1 : 0;
        res = res - drop;
        if (res < 0) res = 0;
        if (a.best_method[0])
            snprintf(out->method_lead2, sizeof out->method_lead2, "%s", a.best_method);
        else
            snprintf(out->method_lead2, sizeof out->method_lead2, "%s", out->method_lead);
        /* prefer abstract tip from method pattern when invent is thin */
        if (out->method_pattern.abstract.r5[0] &&
            !strcmp(out->method_lead2, out->method_lead) &&
            strcmp(out->method_pattern.abstract.r5, "theorize") == 0)
            snprintf(out->method_lead2, sizeof out->method_lead2, "hegelian");

        ikitaku_ladder_plan(want_r5, have_r5, NULL, NULL,
                            res, possible_conf, 0, &out->ladder2);
        out->n_think_passes = 2;
        ikitaku_trace_push(&out->trace,
                           out->method_lead2[0] ? out->method_lead2 : out->method_lead,
                           want_r5, have_r5, res, a.best_conf,
                           out->ladder2.r7_pick, out->ladder2.r6_ready,
                           drop > 0 ? 1 : 2);

        out->circle_act = ikitaku_circle_observe(
            &out->circle,
            out->method_lead2[0] ? out->method_lead2 : out->method_lead,
            want_r5 ? want_r5 : "?",
            res, out->ladder2.r7_pick, out->ladder2.r6_ready);

        if (out->circle_act == IKITAKU_CIRCLE_HALT_PRIORS) {
            out->residual_out = res;
            ikitaku_reflect_chain(&out->trace, 0, 3, 0, &out->reflect);
            snprintf(out->english, sizeof out->english,
                     "goal-spine HALT after self-mod residual=%d | %s | %s",
                     res, out->circle.english, out->reflect.english);
            return res;
        }

        if (out->ladder2.r6_ready) {
            int bud2 = budget > 4 ? budget / 2 : budget;
            ikitaku_objective_run(have_r5, want_r5, res, bud2, &out->autonomy2);
            out->ran_objective2 = 1;
            if (out->autonomy2.solved && res > 10)
                res = res / 2;
            else if (out->autonomy2.n_done > 0 && res > 15)
                res = res - 5;
            ikitaku_trace_push(&out->trace,
                               out->method_lead2[0] ? out->method_lead2 : out->method_lead,
                               want_r5, have_r5, res, possible_conf,
                               out->ladder2.r7_pick, out->ladder2.r6_ready,
                               out->autonomy2.solved ? 3 : 0);
            out->circle_act = ikitaku_circle_observe(
                &out->circle,
                out->method_lead2[0] ? out->method_lead2 : out->method_lead,
                want_r5 ? want_r5 : "?",
                res, out->ladder2.r7_pick, out->ladder2.r6_ready);
        }
    } else if (self_mod) {
        int mi = ikitaku_expr_leaf(&out->method_bag, out->method_lead,
                                   want_r5 ? want_r5 : "goal", "light-mod", 6);
        ikitaku_expr_remember(&out->method_bag, mi);
        ikitaku_pattern_run(&out->method_bag, mi, 1, 1, &out->method_pattern);
        out->ran_method_pattern = 1;

        ikitaku_self_improve_t a, b;
        char se[240];
        int drop = ikitaku_self_mod_closed(res, possible_conf, 55,
                                           1, allow_persist, &a, &b, se);
        out->improve = a;
        out->ran_improve = 1;
        out->closed_loop = (drop > 0) ? 1 : 0;
        res = res - drop;
        if (res < 0) res = 0;
        if (a.best_method[0])
            snprintf(out->method_lead2, sizeof out->method_lead2, "%s", a.best_method);
        ikitaku_trace_push(&out->trace, out->method_lead, want_r5, have_r5,
                           res, a.best_conf, out->ladder.r7_pick, out->ladder.r6_ready,
                           drop > 0 ? 1 : 0);
    }

    out->residual_out = res;
    ikitaku_reflect_chain(&out->trace, 0, 3, 0, &out->reflect);

    snprintf(out->english, sizeof out->english,
             "goal-spine residual %d→%d→%d passes=%d lead=%s→%s "
             "R7=%d R6=%d goal_pat=%s abs_r5=%s method_pat=%s | %s",
             out->residual_in, out->residual_mid, out->residual_out,
             out->n_think_passes,
             out->method_lead,
             out->method_lead2[0] ? out->method_lead2 : "-",
             out->ladder.r7_pick, out->ladder.r6_ready,
             out->ran_goal_pattern ? out->goal_pattern.pattern : "-",
             out->abstract_r5[0] ? out->abstract_r5 : "-",
             out->ran_method_pattern ? out->method_pattern.pattern : "-",
             out->reflect.english);
    return out->residual_out;
}

int ikitaku_goal_spine_from_desire(const ikitaku_desire_t* d,
                                   int budget, int self_mod, int allow_persist,
                                   ikitaku_goal_spine_t* out) {
    if (!d || !out) return -1;
    const char* have = "map";
    const char* want = "seek";
    ikitaku_r5_verb_t v;
    if (d->have_pat[0] && ikitaku_r5_lookup(d->have_pat, &v) == 0)
        have = d->have_pat;
    if (d->want_pat[0] && ikitaku_r5_lookup(d->want_pat, &v) == 0)
        want = d->want_pat;
    else if (d->solved)
        want = "map";
    int residual = d->solved ? (d->mse_best > 0 ? d->mse_best : 10)
                             : (d->mse_best > 0 ? d->mse_best : 40);
    if (residual < 5) residual = 5;
    if (residual > 100) residual = 100;
    int possible = d->possible_conf > 0 ? d->possible_conf : (d->possible ? 70 : 30);

    /* Desire → MAKE under light constraints, then control spine on product form */
    const char* pos[2] = { "pure", NULL };
    const char* neg[1] = { "no-io" };
    int n_pos = 1;
    ikitaku_desire_make_t dm;
    memset(&dm, 0, sizeof dm);
    ikitaku_desire_make(&d->have, &d->want,
                        d->want_note[0] ? d->want_note : "closer",
                        pos, n_pos, neg, 1, residual, &dm);
    if (dm.made && dm.construct.form[0])
        want = dm.construct.form;
    if (dm.residual_out > 0 && dm.residual_out < residual)
        residual = dm.residual_out;

    int r = ikitaku_goal_spine(have, want, residual, possible,
                               budget, self_mod, allow_persist, out);
    /* annotate spine english with make path */
    if (out && dm.made) {
        size_t el = strlen(out->english);
        if (el + 100 < sizeof out->english)
            snprintf(out->english + el, sizeof out->english - el,
                     " | make form=%s mode=%s",
                     dm.construct.form,
                     dm.mode.mode == IKITAKU_MODE_COLLAPSE ? "COLLAPSE" :
                     dm.mode.mode == IKITAKU_MODE_CASCADE ? "CASCADE" : "ALT");
    }
    return r;
}


static double gd_val(const ikitaku_series_t* s, int i) {
    if (!s || i < 0 || i >= s->n) return 0.0;
    return s->values[i];
}

static void gd_sort_ep(ikitaku_goal_delta_t* out) {
    for (int i = 0; i < out->n; ++i)
        for (int j = i + 1; j < out->n; ++j)
            if (out->ep[j].score_pct > out->ep[i].score_pct) {
                ikitaku_endpoint_t tmp = out->ep[i];
                out->ep[i] = out->ep[j];
                out->ep[j] = tmp;
            }
    out->best = out->n > 0 ? 0 : -1;
}

static float gd_trap(float x, float a, float b, float c, float d) {
    /* trapezoidal fuzzy membership */
    if (x <= a || x >= d) return 0.f;
    if (x >= b && x <= c) return 1.f;
    if (x < b) return (b > a) ? (x - a) / (b - a) : 1.f;
    return (d > c) ? (d - x) / (d - c) : 1.f;
}

static int gd_push(ikitaku_goal_delta_t* out, const char* kind,
                   const float* vals, int n, int score, int seg, int layer,
                   const char* form, const char* why) {
    if (!out || out->n >= IKITAKU_ENDPOINT_MAX || !vals || n <= 0) return -1;
    for (int i = 0; i < out->n; ++i) {
        if (!strcmp(out->ep[i].kind, kind) && out->ep[i].seg_id == seg) {
            if (score > out->ep[i].score_pct) {
                out->ep[i].score_pct = score;
                out->ep[i].n = n < 32 ? n : 32;
                for (int k = 0; k < out->ep[i].n; ++k) out->ep[i].values[k] = vals[k];
            }
            return i;
        }
    }
    ikitaku_endpoint_t* e = &out->ep[out->n++];
    memset(e, 0, sizeof *e);
    snprintf(e->kind, sizeof e->kind, "%s", kind ? kind : "?");
    e->n = n < 32 ? n : 32;
    for (int i = 0; i < e->n; ++i) e->values[i] = vals[i];
    e->score_pct = score < 0 ? 0 : (score > 100 ? 100 : score);
    e->seg_id = seg;
    e->layer = layer;
    /* default layer conf: mid layers more "normal" for bag endpoints */
    if (layer >= 3 && layer <= 5) e->layer_conf = 70;
    else if (layer == 2 || layer == 6) e->layer_conf = 55;
    else e->layer_conf = 40;
    e->layer_meta_conf = 60;
    if (form) snprintf(e->form, sizeof e->form, "%s", form);
    snprintf(e->english, sizeof e->english, "%s score=%d%% L%d conf=%d meta=%d %s",
             e->kind, e->score_pct, e->layer, e->layer_conf, e->layer_meta_conf,
             why ? why : "");
    return out->n - 1;
}

int ikitaku_goal_delta_endpoints(const ikitaku_series_t* have,
                                 const ikitaku_series_t* want,
                                 ikitaku_goal_delta_t* out) {
    if (!have || !want || !out) return -1;
    memset(out, 0, sizeof *out);
    int nh = have->n < 32 ? have->n : 32;
    int nw = want->n < 32 ? want->n : 32;
    int n = nh < nw ? nh : nw;
    if (n < 2) {
        snprintf(out->english, sizeof out->english, "goal-delta: series too short");
        return 0;
    }

    /* ---- delta bag ---- */
    float dsum = 0, d2 = 0;
    float delta[32];
    int n_holes = 0;
    for (int i = 0; i < n; ++i) {
        delta[i] = gd_val(want, i) - gd_val(have, i);
        dsum += delta[i];
        d2 += delta[i] * delta[i];
        if (fabs(delta[i]) < 1e-6f) n_holes++; /* flat = no new info / hole-ish */
    }
    out->delta_mean = dsum / (float)n;
    out->delta_var = d2 / (float)n - out->delta_mean * out->delta_mean;
    if (out->delta_var < 0) out->delta_var = 0;

    /* ---- Fuzzy logic on delta magnitude & stability ---- */
    {
        float am = (float)fabs((double)out->delta_mean);
        float vr = (float)out->delta_var;
        /* scale-free-ish: relative to data span */
        float span = 1.f;
        for (int i = 0; i < n; ++i) {
            float a = (float)fabs(gd_val(have, i));
            float b = (float)fabs(gd_val(want, i));
            if (a > span) span = a;
            if (b > span) span = b;
        }
        if (span < 1e-6f) span = 1.f;
        float rel = am / span;
        out->fuzzy_near = gd_trap(rel, 0.f, 0.f, 0.08f, 0.20f);
        out->fuzzy_mid  = gd_trap(rel, 0.10f, 0.25f, 0.45f, 0.70f);
        out->fuzzy_far  = gd_trap(rel, 0.50f, 0.75f, 1.5f, 3.f);
        out->fuzzy_stable = gd_trap(vr, 0.f, 0.f, 0.3f, 1.0f);
        out->fuzzy_noisy  = gd_trap(vr, 0.5f, 1.5f, 10.f, 50.f);
    }

    /* ---- segment have (tree-leaf style windows) ---- */
    int seg_len = n >= 8 ? n / 4 : (n >= 4 ? n / 2 : n);
    if (seg_len < 2) seg_len = 2;
    int n_segs = 0;
    float seg_slope[IKITAKU_SEG_MAX];
    memset(seg_slope, 0, sizeof seg_slope);
    for (int off = 0; off + seg_len <= n && n_segs < IKITAKU_SEG_MAX; off += seg_len) {
        ikitaku_series_t seg;
        memset(&seg, 0, sizeof seg);
        seg.kind = have->kind;
        seg.n = seg_len;
        for (int i = 0; i < seg_len; ++i)
            seg.values[i] = gd_val(have, off + i);
        ikitaku_series_analysis_t an;
        memset(&an, 0, sizeof an);
        ikitaku_series_analyze(&seg, &an);
        snprintf(out->seg_form[n_segs], 24, "%s",
                 an.form_r4[0] ? an.form_r4 : "seg");
        /* slope from first/last of segment */
        seg_slope[n_segs] = (gd_val(have, off + seg_len - 1) - gd_val(have, off))
                            / (float)(seg_len - 1);
        /* abstraction tip of segment form */
        ikitaku_layer_drop_t flow;
        ikitaku_layer_flow(out->seg_form[n_segs], 4, 2, IKITAKU_FLOW_DOWN, &flow);
        n_segs++;
    }
    out->n_segs = n_segs;

    /* ---- endpoint candidates ---- */
    float ep[32];

    /* 1. direct want as endpoint (desire already specified) */
    for (int i = 0; i < nw; ++i) ep[i] = gd_val(want, i);
    {
        int sc = 70;
        if (fabs(out->delta_mean) > 1e-4f) sc += 10;
        if (out->delta_var < 0.5f) sc += 5;
        gd_push(out, "direct", ep, nw, sc > 100 ? 100 : sc, -1, 5, "want",
                "desired bag as endpoint");
    }

    /* 2. linear: have + mean(delta) extended */
    for (int i = 0; i < nh; ++i)
        ep[i] = gd_val(have, i) + out->delta_mean;
    {
        int sc = 55;
        if (out->delta_var < 1.0f) sc += 15;
        if (n_holes < n / 3) sc += 5;
        gd_push(out, "linear", ep, nh, sc > 100 ? 100 : sc, -1, 4, "map",
                "have + mean delta");
    }

    /* 3. per-segment slope extrapolate last segment to full length */
    if (n_segs > 0) {
        float slope = seg_slope[n_segs - 1];
        float base = gd_val(have, nh - 1);
        for (int i = 0; i < nh; ++i)
            ep[i] = base + slope * (float)(i - (nh - 1));
        int sc = 50 + (n_segs >= 2 ? 10 : 0);
        gd_push(out, "seg-slope", ep, nh, sc, n_segs - 1, 4,
                out->seg_form[n_segs - 1], "last segment slope leaf");
    }

    /* 4. hole-fill: where |delta| tiny, blend have toward neighbor want */
    {
        int holes = 0;
        for (int i = 0; i < n; ++i) {
            if (fabs(delta[i]) < 1e-5f && i > 0 && i + 1 < n) {
                ep[i] = 0.5f * (gd_val(want, i - 1) + gd_val(want, i + 1));
                holes++;
            } else
                ep[i] = gd_val(want, i);
        }
        for (int i = n; i < nw; ++i) ep[i] = gd_val(want, i);
        out->n_holes = holes;
        int sc = 40 + holes * 5;
        if (sc > 85) sc = 85;
        gd_push(out, "hole-fill", ep, nw, sc, -1, 3, "filter",
                "extrapolate flat delta positions");
    }

    /* 5. phase: if want looks periodic, shift have by mean phase of delta */
    {
        ikitaku_series_analysis_t aw;
        memset(&aw, 0, sizeof aw);
        ikitaku_series_analyze(want, &aw);
        int periodic = (aw.regularity > 0.7f && strstr(aw.english, "period")) ||
                       (aw.form_r4[0] && strstr(aw.form_r4, "period"));
        if (periodic || aw.regularity > 0.85f) {
            for (int i = 0; i < nh; ++i) {
                int j = (i + n / 4) % (nw > 0 ? nw : 1);
                ep[i] = gd_val(want, j);
            }
            gd_push(out, "phase", ep, nh, 60, -1, 4,
                    aw.form_r4[0]?aw.form_r4:"periodic",
                    "phase-shift continue of want form");
        }
    }

    /* 6. abstract tip: treat want form via layer_flow DOWN then unabstract hint */
    {
        ikitaku_series_analysis_t aw;
        memset(&aw, 0, sizeof aw);
        ikitaku_series_analyze(want, &aw);
        const char* form = aw.form_r4[0] ? aw.form_r4 : "map";
        ikitaku_layer_drop_t flow;
        ikitaku_layer_flow(form, 4, 2, IKITAKU_FLOW_DOWN, &flow);
        /* endpoint = want scaled toward mean (abstract = less variance) */
        float mean = 0;
        for (int i = 0; i < nw; ++i) mean += gd_val(want, i);
        mean /= (float)nw;
        for (int i = 0; i < nw; ++i)
            ep[i] = 0.7f * gd_val(want, i) + 0.3f * mean;
        gd_push(out, "abstract", ep, nw, 48, -1, flow.to_layer,
                flow.abstract_r5[0]?flow.abstract_r5:flow.broader,
                "abstracted want (macro blend)");
    }

    /* 7–10: mid segments as alternate leaves */
    for (int s = 0; s < n_segs && out->n < IKITAKU_ENDPOINT_MAX; ++s) {
        float slope = seg_slope[s];
        float base = gd_val(have, (s * seg_len + seg_len / 2) < nh
                                   ? (s * seg_len + seg_len / 2) : nh - 1);
        for (int i = 0; i < nh; ++i)
            ep[i] = base + slope * (float)(i - nh / 2);
        int sc = 35 + s * 3;
        char why[40];
        snprintf(why, sizeof why, "segment %d leaf slope", s);
        gd_push(out, "seg-slope", ep, nh, sc, s, 4, out->seg_form[s], why);
    }

    /* ---- Fuzzy blend scores + layer confidence / meta-confidence ---- */
    {
        float w_direct = 0.35f * out->fuzzy_near + 0.40f * out->fuzzy_mid + 0.15f * out->fuzzy_far;
        float w_linear = 0.50f * out->fuzzy_stable + 0.25f * out->fuzzy_mid + 0.10f * out->fuzzy_near;
        float w_slope  = 0.35f * out->fuzzy_mid + 0.25f * out->fuzzy_far + 0.15f * out->fuzzy_noisy;
        float w_hole   = 0.40f * out->fuzzy_near + 0.20f * out->fuzzy_stable;
        float w_phase  = 0.30f * out->fuzzy_mid + 0.20f * out->fuzzy_stable;
        float w_abs    = 0.25f * out->fuzzy_far + 0.20f * out->fuzzy_noisy + 0.15f * out->fuzzy_mid;

        int layer_sum = 0, layer_n = 0, meta_sum = 0;
        int min_lc = 100, max_lc = 0;
        for (int i = 0; i < out->n; ++i) {
            ikitaku_endpoint_t* e = &out->ep[i];
            float w = 0.3f;
            if (!strcmp(e->kind, "direct")) w = w_direct;
            else if (!strcmp(e->kind, "linear")) w = w_linear;
            else if (!strcmp(e->kind, "seg-slope")) w = w_slope;
            else if (!strcmp(e->kind, "hole-fill")) w = w_hole;
            else if (!strcmp(e->kind, "phase")) w = w_phase;
            else if (!strcmp(e->kind, "abstract")) w = w_abs;
            int adj = (int)(e->score_pct * (0.55f + 0.45f * w));
            if (adj < 0) adj = 0;
            if (adj > 100) adj = 100;
            e->score_pct = adj;

            /* layer conf: abstract endpoints lower meta if only far/noisy fit */
            if (!strcmp(e->kind, "abstract")) {
                e->layer_conf = (int)(40 + 40 * out->fuzzy_far);
                e->layer_meta_conf = (int)(50 + 30 * out->fuzzy_stable);
            } else if (!strcmp(e->kind, "direct")) {
                e->layer_conf = (int)(60 + 30 * out->fuzzy_mid + 10 * out->fuzzy_near);
                e->layer_meta_conf = (int)(65 + 25 * out->fuzzy_stable);
            } else if (!strcmp(e->kind, "seg-slope")) {
                e->layer_conf = (int)(50 + 25 * out->fuzzy_mid);
                e->layer_meta_conf = (int)(55 + 20 * (1.f - out->fuzzy_noisy));
            } else {
                e->layer_conf = (int)(50 + 20 * out->fuzzy_stable);
                e->layer_meta_conf = 60;
            }
            if (e->layer_conf > 100) e->layer_conf = 100;
            if (e->layer_meta_conf > 100) e->layer_meta_conf = 100;
            layer_sum += e->layer_conf;
            meta_sum += e->layer_meta_conf;
            layer_n++;
            if (e->layer_conf < min_lc) min_lc = e->layer_conf;
            if (e->layer_conf > max_lc) max_lc = e->layer_conf;
            snprintf(e->english, sizeof e->english,
                     "%s score=%d%% L%d conf=%d meta=%d",
                     e->kind, e->score_pct, e->layer, e->layer_conf, e->layer_meta_conf);
        }
        if (layer_n > 0) {
            out->layer_conf = layer_sum / layer_n;
            out->layer_meta_conf = meta_sum / layer_n;
            /* disagreeing layers or low meta → unsure */
            int spread = max_lc - min_lc;
            out->unsure_layer = (out->layer_meta_conf < 55 || spread > 30 ||
                                 out->layer_conf < 50) ? 1 : 0;
            out->escalate_metacog = out->unsure_layer;
            if (out->escalate_metacog)
                snprintf(out->metacog_note, sizeof out->metacog_note,
                         "unsure if right abstraction layer (layer_conf=%d meta=%d spread=%d); "
                         "examine in metacog / macro-confidence",
                         out->layer_conf, out->layer_meta_conf, spread);
            else
                snprintf(out->metacog_note, sizeof out->metacog_note,
                         "layer confidence adequate (conf=%d meta=%d)",
                         out->layer_conf, out->layer_meta_conf);
        }
    }

    gd_sort_ep(out);

    /* parametric filter: drop below 25% unless fewer than 3 */
    int kept = 0;
    for (int i = 0; i < out->n; ++i) {
        if (out->ep[i].score_pct >= 25 || kept < 3) {
            if (kept != i) out->ep[kept] = out->ep[i];
            kept++;
        }
    }
    out->n = kept > IKITAKU_ENDPOINT_MAX ? IKITAKU_ENDPOINT_MAX : kept;
    out->best = out->n > 0 ? 0 : -1;

    snprintf(out->english, sizeof out->english,
             "goal-delta n=%d best=%s@%d%% fuzzy(near=%.2f mid=%.2f far=%.2f "
             "stable=%.2f noisy=%.2f) layer_conf=%d meta=%d unsure=%d segs=%d | %s",
             out->n,
             out->best >= 0 ? out->ep[out->best].kind : "-",
             out->best >= 0 ? out->ep[out->best].score_pct : 0,
             out->fuzzy_near, out->fuzzy_mid, out->fuzzy_far,
             out->fuzzy_stable, out->fuzzy_noisy,
             out->layer_conf, out->layer_meta_conf, out->unsure_layer,
             out->n_segs, out->metacog_note);
    return out->n;
}

int ikitaku_goal_delta_from_desire(const ikitaku_desire_t* d,
                                  ikitaku_goal_delta_t* out) {
    if (!d || !out) return -1;
    return ikitaku_goal_delta_endpoints(&d->have, &d->want, out);
}


int ikitaku_constraint_set(ikitaku_expr_tree_t* t, int idx,
                           const char* constraint, const char* antithesis) {
    if (!t || idx < 0 || idx >= t->n) return -1;
    ikitaku_expr_bag_t* b = &t->node[idx];
    /* second-order +c: compose onto existing context as antithesis-context */
    char c2[64];
    snprintf(c2, sizeof c2, "constraint:%s", constraint ? constraint : "_");
    if (antithesis && antithesis[0]) {
        char c3[96];
        snprintf(c3, sizeof c3, "%s;anti=%s", c2, antithesis);
        ikitaku_expr_compose_c(t, idx, b->context[0] ? b->context : "exec", c3);
    } else
        ikitaku_expr_compose_c(t, idx, b->context[0] ? b->context : "exec", c2);
    ikitaku_expr_remember(t, idx);
    snprintf(b->english, sizeof b->english,
             "constraint +c set (antithesis-context for execution): %s", b->context);
    return 0;
}

int ikitaku_constraint_method(const ikitaku_constraint_t* c,
                              const char* method, int allow_costume,
                              ikitaku_constraint_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (c) *out = *c;
    snprintf(out->method, sizeof out->method, "%s", method ? method : "scientific");
    out->costume = allow_costume ? 1 : 0;
    /* Inverse-of-constraint: method that achieves macro without hard violate.
       Simple heuristics: if constraint mentions no-IO / pure, reject emit;
       if budget, prefer occam; if no-cycle, reject repeat-heavy. */
    out->violates = 0;
    const char* cons = out->constraint[0] ? out->constraint :
                       (c && c->constraint[0] ? c->constraint : "");
    if (strstr(cons, "pure") || strstr(cons, "no-io") || strstr(cons, "no_io")) {
        if (strstr(out->method, "emit") || strstr(out->method, "absorb"))
            out->violates = 1;
    }
    if (strstr(cons, "no-cycle") || strstr(cons, "no_loop")) {
        if (strstr(out->method, "repeat") || strstr(out->method, "iterative"))
            out->violates = allow_costume ? 0 : 1; /* costume may micro-unroll */
    }
    if (strstr(cons, "budget") || strstr(cons, "cheap")) {
        if (strstr(out->method, "origin") || strstr(out->method, "novel"))
            out->violates = allow_costume ? 0 : 1;
    }
    out->conf = out->violates ? 25 : (allow_costume ? 70 : 80);
    snprintf(out->english, sizeof out->english,
             "method=%s as inverse-of-constraint [%s] costume=%d violates=%d conf=%d",
             out->method, cons[0]?cons:"_", out->costume, out->violates, out->conf);
    return out->violates ? 1 : 0;
}

int ikitaku_constraint_shape(const ikitaku_constraint_t* c,
                             const char* desired_form,
                             ikitaku_constraint_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (c) *out = *c;
    const char* want = desired_form ? desired_form : "map";
    snprintf(out->preferred_form, sizeof out->preferred_form, "%s", want);
    out->shape_form = 0;
    const char* cons = out->constraint;
    /* Constraints shape form of solution */
    if (strstr(cons, "no-cycle") || strstr(cons, "linear")) {
        if (!strcmp(want, "repeat") || !strcmp(want, "iterative")) {
            snprintf(out->preferred_form, sizeof out->preferred_form, "sequence");
            out->shape_form = 1;
        }
    }
    if (strstr(cons, "pure") || strstr(cons, "math")) {
        if (!strcmp(want, "emit") || !strcmp(want, "absorb")) {
            snprintf(out->preferred_form, sizeof out->preferred_form, "map");
            out->shape_form = 1;
        }
    }
    if (strstr(cons, "search") || strstr(cons, "find")) {
        snprintf(out->preferred_form, sizeof out->preferred_form, "seek");
        out->shape_form = 1;
    }
    if (strstr(cons, "sort") || strstr(cons, "order")) {
        snprintf(out->preferred_form, sizeof out->preferred_form, "order");
        out->shape_form = 1;
    }
    out->conf = out->shape_form ? 75 : 55;
    snprintf(out->english, sizeof out->english,
             "constraint shapes form: desired=%s → preferred=%s shaped=%d (antithesis-context +c)",
             want, out->preferred_form, out->shape_form);
    return out->shape_form;
}

int ikitaku_symbolic_delta(const char* have_form, const char* want_form,
                           const char* have_note, const char* want_note,
                           ikitaku_sym_delta_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->have_sym, sizeof out->have_sym, "%s", have_form ? have_form : "info");
    snprintf(out->want_sym, sizeof out->want_sym, "%s", want_form ? want_form : "info");
    int same = (strcmp(out->have_sym, out->want_sym) == 0);
    if (same)
        snprintf(out->delta_sym, sizeof out->delta_sym,
                 "same form %s; gap is context/property not verb", out->have_sym);
    else
        snprintf(out->delta_sym, sizeof out->delta_sym,
                 "form gap: have %s → want %s", out->have_sym, out->want_sym);

    if (!same) {
        snprintf(out->missing_form, sizeof out->missing_form, "%s", out->want_sym);
        snprintf(out->problem, sizeof out->problem,
                 "need transform %s→%s%s%s",
                 out->have_sym, out->want_sym,
                 want_note && want_note[0] ? " because " : "",
                 want_note && want_note[0] ? want_note : "");
        out->conf = 70;
        out->needs_detail = 0;
    } else {
        /* same form: problem is property/detail level */
        snprintf(out->missing_form, sizeof out->missing_form, "property");
        snprintf(out->problem, sizeof out->problem,
                 "same R5 %s; extract detail (+c) before solve [%s → %s]",
                 out->have_sym,
                 have_note && have_note[0] ? have_note : "have",
                 want_note && want_note[0] ? want_note : "want");
        out->conf = 55;
        out->needs_detail = 1;
    }
    snprintf(out->english, sizeof out->english,
             "symbolic-delta %s | problem: %s | needs_detail=%d conf=%d",
             out->delta_sym, out->problem, out->needs_detail, out->conf);
    return out->needs_detail;
}

int ikitaku_symbolic_delta_from_series(const ikitaku_series_t* have,
                                       const ikitaku_series_t* want,
                                       ikitaku_sym_delta_t* out) {
    if (!have || !want || !out) return -1;
    ikitaku_series_analysis_t ah, aw;
    memset(&ah, 0, sizeof ah);
    memset(&aw, 0, sizeof aw);
    ikitaku_series_analyze(have, &ah);
    ikitaku_series_analyze(want, &aw);
    const char* hf = ah.form_r5[0] ? ah.form_r5 : (ah.form_r4[0] ? ah.form_r4 : "map");
    const char* wf = aw.form_r5[0] ? aw.form_r5 : (aw.form_r4[0] ? aw.form_r4 : "map");
    int nd = ikitaku_symbolic_delta(hf, wf, ah.english, aw.english, out);
    /* numeric gap strengthens detail need */
    if (have->n >= 2 && want->n >= 2) {
        double mse = 0;
        int n = have->n < want->n ? have->n : want->n;
        for (int i = 0; i < n; ++i) {
            double d = want->values[i] - have->values[i];
            mse += d * d;
        }
        mse /= (double)n;
        if (mse > 0.01 && !strcmp(hf, wf))
            out->needs_detail = 1;
        if (mse > 1.0) out->conf = out->conf > 15 ? out->conf - 10 : out->conf;
    }
    snprintf(out->english, sizeof out->english,
             "symbolic-delta series have=%s want=%s | %s | needs_detail=%d",
             hf, wf, out->problem, out->needs_detail);
    return nd;
}


int ikitaku_construct_init(ikitaku_construct_t* c) {
    if (!c) return -1;
    memset(c, 0, sizeof *c);
    return 0;
}

int ikitaku_construct_add_pos(ikitaku_construct_t* c, const char* constraint) {
    if (!c || !constraint || c->n_pos >= IKITAKU_CONS_MAX) return -1;
    snprintf(c->pos[c->n_pos++], 40, "%s", constraint);
    return c->n_pos;
}

int ikitaku_construct_add_neg(ikitaku_construct_t* c, const char* constraint) {
    if (!c || !constraint || c->n_neg >= IKITAKU_CONS_MAX) return -1;
    snprintf(c->neg[c->n_neg++], 40, "%s", constraint);
    return c->n_neg;
}

static int cons_hit(const char* bag, const char* key) {
    return bag && key && strstr(bag, key) != NULL;
}

static int micro_allowed(const ikitaku_construct_t* c, const char* m) {
    /* Negative constraints forbid methods */
    for (int i = 0; i < c->n_neg; ++i) {
        const char* n = c->neg[i];
        if (cons_hit(n, "no-io") || cons_hit(n, "pure")) {
            if (strstr(m, "emit") || strstr(m, "absorb")) return 0;
        }
        if (cons_hit(n, "no-cycle") || cons_hit(n, "no_loop") || cons_hit(n, "no-repeat")) {
            if (strstr(m, "repeat") || strstr(m, "iterative") || strstr(m, "while"))
                return 0;
        }
        if (cons_hit(n, "no-search") || cons_hit(n, "no-seek")) {
            if (strstr(m, "seek") || strstr(m, "origin")) return 0;
        }
        if (cons_hit(n, "cheap") || cons_hit(n, "budget")) {
            if (strstr(m, "novel") || strstr(m, "origin")) return 0;
        }
        if (cons_hit(n, "no-decision") || cons_hit(n, "no-branch")) {
            if (strstr(m, "decision") || strstr(m, "hegelian")) return 0;
        }
    }
    return 1;
}

static int micro_pos_boost(const ikitaku_construct_t* c, const char* m) {
    int b = 0;
    for (int i = 0; i < c->n_pos; ++i) {
        const char* p = c->pos[i];
        if (cons_hit(p, "must-map") || cons_hit(p, "math") || cons_hit(p, "transform"))
            if (strstr(m, "map") || strstr(m, "occam") || strstr(m, "scientific")) b += 2;
        if (cons_hit(p, "must-order") || cons_hit(p, "sort"))
            if (strstr(m, "order") || strstr(m, "compare")) b += 2;
        if (cons_hit(p, "must-seek") || cons_hit(p, "find"))
            if (strstr(m, "seek")) b += 2;
        if (cons_hit(p, "must-loop") || cons_hit(p, "repeat"))
            if (strstr(m, "repeat") || strstr(m, "iterative")) b += 2;
        if (cons_hit(p, "pure"))
            if (strstr(m, "map") || strstr(m, "occam") || strstr(m, "bound")) b += 1;
        if (cons_hit(p, "explain") || cons_hit(p, "theorize"))
            if (strstr(m, "theorize") || strstr(m, "hegelian") || strstr(m, "scientific"))
                b += 2;
    }
    return b;
}

int ikitaku_construct_solution(ikitaku_construct_t* c,
                               const char* have_form, const char* want_form,
                               const char* problem_sym) {
    if (!c) return -1;
    c->n_micro = c->n_rejected = c->n_assembled = 0;
    c->space_before = 0;
    c->space_after = 0;

    /* Known micro method catalog (construction palette — inverse of extract) */
    static const char* MICROS[] = {
        "map", "order", "seek", "filter", "reduce", "join", "split",
        "repeat", "sequence", "decision", "bound", "invert",
        "occam", "scientific", "hegelian", "theorize", "compare",
        NULL
    };

    /* Score + filter */
    typedef struct { char name[24]; int score; } scored_t;
    scored_t sc[IKITAKU_MICRO_MAX];
    int ns = 0;
    for (int i = 0; MICROS[i] && c->space_before < 32; ++i) {
        c->space_before++;
        const char* m = MICROS[i];
        if (!micro_allowed(c, m)) {
            if (c->n_rejected < IKITAKU_MICRO_MAX)
                snprintf(c->rejected[c->n_rejected++], 24, "%s", m);
            continue;
        }
        int score = 40 + micro_pos_boost(c, m);
        /* problem-driven: symbolic delta suggests micros */
        if (problem_sym) {
            if (strstr(problem_sym, "order") || strstr(problem_sym, "sort"))
                if (strstr(m, "order") || strstr(m, "compare")) score += 15;
            if (strstr(problem_sym, "map→") || strstr(problem_sym, "transform"))
                if (strstr(m, "map") || strstr(m, "invert")) score += 10;
            if (strstr(problem_sym, "detail") || strstr(problem_sym, "property"))
                if (strstr(m, "theorize") || strstr(m, "scientific")) score += 8;
            if (strstr(problem_sym, "seek") || strstr(problem_sym, "find"))
                if (strstr(m, "seek")) score += 12;
        }
        if (want_form && !strcmp(m, want_form)) score += 20;
        if (have_form && !strcmp(m, have_form)) score += 5;
        if (ns < IKITAKU_MICRO_MAX) {
            snprintf(sc[ns].name, 24, "%s", m);
            sc[ns].score = score;
            ns++;
        }
    }
    /* sort descending */
    for (int i = 0; i < ns; ++i)
        for (int j = i + 1; j < ns; ++j)
            if (sc[j].score > sc[i].score) {
                scored_t tmp = sc[i]; sc[i] = sc[j]; sc[j] = tmp;
            }
    for (int i = 0; i < ns && c->n_micro < IKITAKU_MICRO_MAX; ++i) {
        snprintf(c->micro[c->n_micro++], 24, "%s", sc[i].name);
    }
    c->space_after = c->n_micro;
    c->narrow_pct = c->space_before > 0
        ? (100 * (c->space_before - c->space_after)) / c->space_before : 0;

    /* Memory bias: strategy_load + lived experience (xp cases) */
    ikitaku_strategy_map_t sm;
    memset(&sm, 0, sizeof sm);
    if (ikitaku_strategy_load(&sm) == 0 && sm.n > 0) {
        c->used_memory = 1;
        for (int s = 0; s < sm.n && s < 3; ++s) {
            for (int i = 1; i < c->n_micro; ++i) {
                if (!strcmp(c->micro[i], sm.st[s].name)) {
                    char tmp[24];
                    snprintf(tmp, 24, "%s", c->micro[0]);
                    snprintf(c->micro[0], 24, "%s", c->micro[i]);
                    snprintf(c->micro[i], 24, "%s", tmp);
                    break;
                }
            }
        }
    }
    /* Construction order: inverse of extract — foundation → transform → want → bound */
    #define PUSH_ASM(v) do {         const char* _v = (v);         if (_v && c->n_assembled < 8) {             int _dup = 0;             for (int _i = 0; _i < c->n_assembled; ++_i)                 if (!strcmp(c->assembled[_i], _v)) { _dup = 1; break; }             if (!_dup) snprintf(c->assembled[c->n_assembled++], 24, "%s", _v);         }     } while (0)
    if (have_form) PUSH_ASM(have_form);
    for (int i = 0; i < c->n_micro && c->n_assembled < 6; ++i) {
        const char* m = c->micro[i];
        if (!strcmp(m, "occam") || !strcmp(m, "scientific") ||
            !strcmp(m, "hegelian") || !strcmp(m, "theorize"))
            continue;
        PUSH_ASM(m);
    }
    if (want_form) PUSH_ASM(want_form);
    PUSH_ASM("bound");
    for (int i = 0; i < c->n_pos; ++i)
        if (cons_hit(c->pos[i], "explain") || cons_hit(c->pos[i], "theorize")) {
            PUSH_ASM("theorize");
            break;
        }
    #undef PUSH_ASM

    /* Shaped macro form from constraints + want */
    ikitaku_constraint_t shape_in, shape_out;
    memset(&shape_in, 0, sizeof shape_in);
    for (int i = 0; i < c->n_neg && i < 2; ++i) {
        if (i == 0) snprintf(shape_in.constraint, sizeof shape_in.constraint, "%s", c->neg[i]);
        else {
            char tmp[64];
            snprintf(tmp, sizeof tmp, "%s;%s", shape_in.constraint, c->neg[i]);
            snprintf(shape_in.constraint, sizeof shape_in.constraint, "%s", tmp);
        }
    }
    for (int i = 0; i < c->n_pos && i < 2; ++i) {
        char tmp[64];
        snprintf(tmp, sizeof tmp, "%s;pos:%s",
                 shape_in.constraint[0] ? shape_in.constraint : "", c->pos[i]);
        snprintf(shape_in.constraint, sizeof shape_in.constraint, "%s", tmp);
    }
    ikitaku_constraint_shape(&shape_in, want_form ? want_form : "map", &shape_out);
    snprintf(c->form, sizeof c->form, "%s",
             shape_out.preferred_form[0] ? shape_out.preferred_form :
             (want_form ? want_form : "map"));

    c->conf = 40 + c->n_assembled * 5 + (c->narrow_pct > 30 ? 10 : 0);
    if (c->conf > 90) c->conf = 90;
    if (c->n_micro == 0) c->conf = 15;

    /* Metacog: construction quality needs bottom-up lived patterns */
    {
        ikitaku_xp_report_t xp;
        memset(&xp, 0, sizeof xp);
        ikitaku_xp_load(&xp);
        int inexperienced = (xp.n <= 0);
        if (inexperienced) {
            c->conf = c->conf > 30 ? c->conf - 25 : 12;
            /* escalate: without L3 cases, construct is catalog algebra not lived skill */
        } else {
            c->used_memory = 1;
            c->conf = c->conf + (xp.n >= 3 ? 12 : 6);
            if (c->conf > 92) c->conf = 92;
        }
        snprintf(c->english, sizeof c->english,
                 "construct: space %d→%d narrow=%d%% form=%s conf=%d memory=%d "
                 "xp=%d %s pos=%d neg=%d micros=",
                 c->space_before, c->space_after, c->narrow_pct, c->form, c->conf,
                 c->used_memory, xp.n,
                 inexperienced
                   ? "INEXPERIENCED(need lived micros; metacog: construct incomplete without xp)"
                   : "lived-ok",
                 c->n_pos, c->n_neg);
    }
    size_t len = strlen(c->english);
    for (int i = 0; i < c->n_assembled && len + 28 < sizeof c->english; ++i) {
        int n = snprintf(c->english + len, sizeof c->english - len,
                         "%s%s", i ? "→" : "", c->assembled[i]);
        if (n > 0) len += (size_t)n;
    }
    return c->n_assembled;
}


/* ========== Cascade / Collapse dual ========== */
int ikitaku_solve_mode_pick(int residual, int desperation, int narrow_pct,
                            int layer_conf, int unsure_layer,
                            ikitaku_mode_pick_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->residual = residual;
    out->desperation = desperation;
    out->narrow_pct = narrow_pct;
    out->layer_conf = layer_conf;

    /* Policy:
       - calm residual + decent layer conf → COLLAPSE (Occam)
       - hot residual or desperation or unsure layer → CASCADE
       - mid band → ALTERNATE (collapse first if already narrowed) */
    int calm = (residual < 30 && desperation <= 2 && !unsure_layer);
    int hot  = (residual >= 55 || desperation >= 5 || unsure_layer);
    int narrowed = (narrow_pct >= 35);

    if (calm || (narrowed && residual < 45 && !hot)) {
        out->mode = IKITAKU_MODE_COLLAPSE;
        out->reason_occam = 1;
        snprintf(out->english, sizeof out->english,
                 "mode=COLLAPSE (Occam): residual=%d despair=%d narrow=%d%% layer_conf=%d",
                 residual, desperation, narrow_pct, layer_conf);
    } else if (hot) {
        out->mode = IKITAKU_MODE_CASCADE;
        out->reason_despair = 1;
        snprintf(out->english, sizeof out->english,
                 "mode=CASCADE (desperation): residual=%d despair=%d unsure=%d layer_conf=%d",
                 residual, desperation, unsure_layer, layer_conf);
    } else {
        out->mode = IKITAKU_MODE_ALTERNATE;
        /* alternate: if already narrow collapse micro, else cascade one model */
        if (narrowed) {
            out->reason_occam = 1;
            snprintf(out->english, sizeof out->english,
                     "mode=ALTERNATE→prefer COLLAPSE step (narrow=%d%% residual=%d)",
                     narrow_pct, residual);
        } else {
            out->reason_despair = 1;
            snprintf(out->english, sizeof out->english,
                     "mode=ALTERNATE→prefer CASCADE step (space still wide residual=%d)",
                     residual);
        }
    }
    return out->mode;
}

int ikitaku_construct_metacog(ikitaku_construct_t* c, int record_xp) {
    if (!c || c->n_assembled < 1) return -1;
    /* Build expr chain from assembled micros and pattern_run (metacog on product) */
    ikitaku_expr_tree_t bag;
    ikitaku_expr_init(&bag);
    int prev = -1;
    for (int i = 0; i < c->n_assembled; ++i) {
        int leaf = ikitaku_expr_leaf(&bag, c->assembled[i],
                                     i == 0 ? "foundation" : "step",
                                     "construct-chain", 5);
        if (prev >= 0)
            ikitaku_expr_add_slot(&bag, prev >= 0 ? prev : leaf, leaf);
        if (i == 0) prev = leaf;
        else {
            /* wrap progressive */
            int w = ikitaku_expr_wrap(&bag, prev, c->assembled[i], "construct", 5);
            if (w >= 0) prev = w;
        }
    }
    int root = bag.root >= 0 ? bag.root : (bag.n > 0 ? bag.n - 1 : -1);
    if (root >= 0) {
        ikitaku_abs_pattern_t pat;
        memset(&pat, 0, sizeof pat);
        ikitaku_pattern_run(&bag, root, 2, 1, &pat);
        c->metacog_ok = 1;
        if (pat.abstract.r5[0])
            snprintf(c->chain_r5, sizeof c->chain_r5, "%s", pat.abstract.r5);
        else if (c->form[0])
            snprintf(c->chain_r5, sizeof c->chain_r5, "%s", c->form);
        /* mode from pattern + conf */
        ikitaku_mode_pick_t mp;
        int despair = c->conf < 40 ? 5 : (c->conf < 60 ? 3 : 1);
        ikitaku_solve_mode_pick(100 - c->conf, despair, c->narrow_pct,
                                pat.abstract.layer_before > 0 ? 60 : c->conf,
                                c->conf < 40 ? 1 : 0, &mp);
        c->mode = mp.mode;

        size_t el = strlen(c->english);
        if (el + 120 < sizeof c->english)
            snprintf(c->english + el, sizeof c->english - el,
                     " | metacog chain_r5=%s mode=%s",
                     c->chain_r5[0] ? c->chain_r5 : "-",
                     c->mode == IKITAKU_MODE_COLLAPSE ? "COLLAPSE" :
                     c->mode == IKITAKU_MODE_CASCADE ? "CASCADE" : "ALTERNATE");
    }

    if (record_xp && c->n_assembled > 0) {
        char rule[64];
        snprintf(rule, sizeof rule, "construct:%s",
                 c->form[0] ? c->form : (c->assembled[0]));
        int reward = c->conf >= 60 ? 1 : 0;
        ikitaku_xp_record(rule, c->form[0] ? c->form : "map",
                          c->assembled[0], c->n_assembled, reward);
        c->xp_recorded = 1;
    }
    return c->metacog_ok;
}

int ikitaku_desire_make(const ikitaku_series_t* have,
                        const ikitaku_series_t* want,
                        const char* want_note,
                        const char** pos, int n_pos,
                        const char** neg, int n_neg,
                        int residual,
                        ikitaku_desire_make_t* out) {
    if (!have || !want || !out) return -1;
    memset(out, 0, sizeof *out);
    out->residual_in = residual;
    ikitaku_expr_init(&out->work_bag);

    /* 1. Problem ID (symbolic delta) — extract before solve */
    ikitaku_symbolic_delta_from_series(have, want, &out->problem);
    if (out->problem.needs_detail && residual < 80)
        residual = residual + 5; /* detail gap raises residual slightly */

    /* 2. Construct under pos/neg (collapse method set) */
    ikitaku_construct_init(&out->construct);
    for (int i = 0; i < n_pos; ++i)
        if (pos && pos[i]) ikitaku_construct_add_pos(&out->construct, pos[i]);
    for (int i = 0; i < n_neg; ++i)
        if (neg && neg[i]) ikitaku_construct_add_neg(&out->construct, neg[i]);

    const char* have_f = out->problem.have_sym[0] ? out->problem.have_sym : "map";
    const char* want_f = out->problem.want_sym[0] ? out->problem.want_sym : "map";
    ikitaku_construct_solution(&out->construct, have_f, want_f, out->problem.problem);

    /* 3. Mode pick: cascade vs collapse */
    int despair = residual >= 60 ? 6 : (residual >= 40 ? 3 : 1);
    ikitaku_solve_mode_pick(residual, despair, out->construct.narrow_pct,
                            out->construct.conf, out->construct.conf < 40,
                            &out->mode);

    /* 4. MAKE work bag from assembled micros (anti-extract construction) */
    if (out->construct.n_assembled > 0) {
        int foundation = ikitaku_expr_leaf(&out->work_bag,
                                           out->construct.assembled[0],
                                           "have", "desire-make", 5);
        int root = foundation;
        for (int i = 1; i < out->construct.n_assembled; ++i) {
            int step = ikitaku_expr_leaf(&out->work_bag,
                                         out->construct.assembled[i],
                                         "step", "desire-make", 5);
            root = ikitaku_expr_wrap(&out->work_bag, root,
                                     out->construct.assembled[i],
                                     out->construct.form, 5);
            if (step >= 0) ikitaku_expr_add_slot(&out->work_bag, root, step);
        }
        /* compose constraint +c */
        for (int i = 0; i < out->construct.n_pos; ++i)
            ikitaku_constraint_set(&out->work_bag, root, out->construct.pos[i], "pos");
        for (int i = 0; i < out->construct.n_neg; ++i)
            ikitaku_constraint_set(&out->work_bag, root, out->construct.neg[i], "neg");
        ikitaku_expr_remember(&out->work_bag, root);
        out->made = 1;

        /* MERGE first two distinct assembled as synthesize if possible */
        if (out->construct.n_assembled >= 2) {
            ikitaku_hypo_t syn;
            ikitaku_synthesize_F(out->construct.assembled[0],
                                out->construct.assembled[1],
                                NULL, 0, 0, &syn);
            if (syn.confidence >= 50 && syn.form_id[0] &&
                strcmp(syn.form_id, "synth_rejected") != 0) {
                /* stamp synthesis on bag form */
                snprintf(out->work_bag.node[root].form,
                         sizeof out->work_bag.node[root].form, "%s", syn.form_id);
            }
            ikitaku_F_merge_lower(out->construct.assembled[0],
                                  out->construct.assembled[1], NULL);
        }
    }

    /* 5. Metacog on construct + xp fuel */
    ikitaku_construct_metacog(&out->construct, 1);

    /* 6. Residual out heuristic */
    out->residual_out = residual;
    if (out->made) {
        int drop = out->construct.narrow_pct / 5 + out->construct.conf / 20;
        if (out->mode.mode == IKITAKU_MODE_COLLAPSE) drop += 5;
        if (out->mode.mode == IKITAKU_MODE_CASCADE) drop += 2; /* explored, less commit */
        out->residual_out = residual - drop;
        if (out->residual_out < 0) out->residual_out = 0;
    }

    snprintf(out->english, sizeof out->english,
             "desire-make residual %d→%d made=%d form=%s mode=%s problem_detail=%d | %s | %s",
             out->residual_in, out->residual_out, out->made,
             out->construct.form[0] ? out->construct.form : "-",
             out->mode.mode == IKITAKU_MODE_COLLAPSE ? "COLLAPSE" :
             out->mode.mode == IKITAKU_MODE_CASCADE ? "CASCADE" : "ALTERNATE",
             out->problem.needs_detail,
             out->construct.english, out->mode.english);
    return out->residual_out;
}

int ikitaku_desire_make_from_desire(const ikitaku_desire_t* d,
                                   const char** pos, int n_pos,
                                   const char** neg, int n_neg,
                                   ikitaku_desire_make_t* out) {
    if (!d || !out) return -1;
    int residual = d->mse_best > 0 ? (d->mse_best > 100 ? 80 : d->mse_best) : 40;
    if (residual > 100) residual = 100;
    return ikitaku_desire_make(&d->have, &d->want,
                               d->want_note[0] ? d->want_note : NULL,
                               pos, n_pos, neg, n_neg, residual, out);
}


void ikitaku_hypo_fill(ikitaku_hypo_t* h, const char* purpose, const char* form,
                       int conf, int rewrite_ok) {
    if (!h) return;
    memset(h, 0, sizeof *h);
    if (purpose) snprintf(h->purpose, sizeof h->purpose, "%s", purpose);
    if (form) snprintf(h->form_id, sizeof h->form_id, "%s", form);
    else if (purpose) snprintf(h->form_id, sizeof h->form_id, "%s", purpose);
    h->confidence = conf < 0 ? 0 : (conf > 100 ? 100 : conf);
    h->feasible = 1;
    h->rewrite_ok = rewrite_ok ? 1 : 0;
    snprintf(h->note, sizeof h->note, "structured hypo purpose=%s form=%s",
             h->purpose, h->form_id);
}

int ikitaku_endpoint_make(const ikitaku_goal_delta_t* gd,
                          const char** pos, int n_pos,
                          const char** neg, int n_neg,
                          int residual,
                          ikitaku_desire_make_t* out) {
    if (!gd || !out || gd->n < 1 || gd->best < 0) return -1;
    /* Build synthetic series from best endpoint values */
    ikitaku_series_t want;
    memset(&want, 0, sizeof want);
    want.kind = IKITAKU_DATA_F32;
    const ikitaku_endpoint_t* ep = &gd->ep[gd->best];
    want.n = ep->n < IKITAKU_SERIES_MAX ? ep->n : IKITAKU_SERIES_MAX;
    for (int i = 0; i < want.n; ++i)
        want.values[i] = (double)ep->values[i];
    /* have = zeros or shifted by -delta mean as stand-in when only endpoints given */
    ikitaku_series_t have;
    memset(&have, 0, sizeof have);
    have.kind = IKITAKU_DATA_F32;
    have.n = want.n;
    for (int i = 0; i < have.n; ++i)
        have.values[i] = want.values[i] - (double)gd->delta_mean;

    char note[80];
    snprintf(note, sizeof note, "endpoint:%s@%d%%", ep->kind, ep->score_pct);
    int r = ikitaku_desire_make(&have, &want, note, pos, n_pos, neg, n_neg,
                               residual > 0 ? residual : (100 - ep->score_pct),
                               out);
    /* Prefer generate when endpoint is abstract and form is novel */
    if (out->made && !strcmp(ep->kind, "abstract") && out->construct.form[0]) {
        /* stamp generate as lead for new-bag intent */
        if (out->work_bag.n > 0 && out->work_bag.root >= 0) {
            /* keep form; note generation */
            size_t el = strlen(out->english);
            if (el + 40 < sizeof out->english)
                snprintf(out->english + el, sizeof out->english - el,
                         " | generate-path from abstract endpoint");
        }
    }
    return r;
}
