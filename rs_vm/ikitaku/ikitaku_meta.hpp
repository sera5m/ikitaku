#pragma once
/* PURPOSE: R6 strategy + budget + undo. WHY: pick how to think (Occam / scientific /
   Hegelian / Bayesian / origin), not what the program is. Desperation only widens search. */
/* segmented from ikitaku.hpp — ikitaku_meta.hpp */
// ---- Desperation cascade: high → low → novel → free-associate ----
typedef struct {
    int  level;              // 0 calm .. 5 desperate
    int  models_failed;
    int  free_assoc_tried;
    int  partial_hit;        // free assoc partially worked
    char partial_note[128];
    char action[128];        // what to do next
} ikitaku_desperation_t;

/* R6 base thought-forms (strategies). Not R5 work-verbs. */
#define IKITAKU_THOUGHT_MAX  8
typedef struct {
    char name[24];
    char when[80];
    char move[80];
    int  weight;       /* 0..100 how much to trust this strategy now */
} ikitaku_thought_form_t;

typedef struct {
    ikitaku_thought_form_t form[IKITAKU_THOUGHT_MAX];
    int n;
    int chosen;
    char english[200];
} ikitaku_thought_plan_t;

int ikitaku_thought_plan(int desperation_level,
                         int math_conf,
                         int rule_conf,
                         ikitaku_thought_plan_t* out);

/* L5 task: find schema by composing verbs. Not a new R5. */
typedef struct {
    char schema[64];       /* F found */
    char purpose[24];      /* map-math | map-table | generate | info */
    char used[80];         /* verbs composed */
    char strategy[24];     /* R6 form chosen */
    int  conf;
    char english[192];
} ikitaku_schema_t;

int ikitaku_schema_extract(const ikitaku_series_t* s, ikitaku_schema_t* out);

int  ikitaku_occam_margin(void);
void ikitaku_occam_from_desperation(int level);

int ikitaku_desperation_step(const ikitaku_session_t* s,
                             const ikitaku_fit_report_t* fits,
                             const ikitaku_dissonance_report_t* diss,
                             ikitaku_desperation_t* out);



// ---- Experimental variable roles ----
typedef enum {
    IKITAKU_VAR_DETERMINANT = 0,   // changing it changes the outcome
    IKITAKU_VAR_CODETERMINANT,     // jointly required with another
    IKITAKU_VAR_IRRELEVANT         // variation does not move the target
} ikitaku_var_role_t;

typedef struct {
    char name[32];
    uint8_t role;
    int  influence;     // 0..100 estimated effect on target
    char why[96];
} ikitaku_var_role_t_ent;

#define IKITAKU_MAX_VARS  12
typedef struct {
    ikitaku_var_role_t_ent vars[IKITAKU_MAX_VARS];
    int n;
    char english[192];
} ikitaku_var_report_t;

// Compare series Y against candidate X factors (same length); classify roles
int ikitaku_extract_variables(const ikitaku_series_t* y,
                              const ikitaku_series_t* factors, int n_factors,
                              const char** names,
                              ikitaku_var_report_t* out);

// Inverse-transform probe: try undo wrappers (asin(sin), etc.) when unsure
typedef struct {
    char attempt[64];
    int  conf_before;
    int  conf_after;
    int  improved;
    char note[96];
} ikitaku_undo_t;

#define IKITAKU_MAX_UNDO  8
typedef struct {
    ikitaku_undo_t tries[IKITAKU_MAX_UNDO];
    int n;
    int best;
    int unsure;          // 1 = does not claim identity
    char english[192];
} ikitaku_undo_report_t;

int ikitaku_try_undo_transforms(const ikitaku_series_t* s,
                                ikitaku_undo_report_t* out);

// ---- Timing / complexity of a thinking session ----
typedef struct {
    double seconds;
    int    ops;              // hypo+cluster+fit+compare steps
    char   big_o_guess[24];
    char   english[128];
} ikitaku_think_cost_t;

void ikitaku_think_cost_begin(void);
void ikitaku_think_cost_add_ops(int n);
int  ikitaku_think_cost_end(ikitaku_think_cost_t* out);

// ---- Goal / task tracker (category-split objectives) ----
typedef enum {
    IKITAKU_GOAL_IDLE = 0,
    IKITAKU_GOAL_ANALYZE,
    IKITAKU_GOAL_FIT,
    IKITAKU_GOAL_QUERY,
    IKITAKU_GOAL_LEARN,
    IKITAKU_GOAL_PRIORITY
} ikitaku_goal_kind_t;

typedef struct {
    char id[32];
    uint8_t kind;
    int  priority;         // 0..100
    int  confusion;        // 0..100
    int  desperation;      // 0..100
    int  deadline_ops;     // attention budget remaining
    char status[48];
} ikitaku_goal_t;

#define IKITAKU_MAX_GOALS  8
typedef struct {
    ikitaku_goal_t goals[IKITAKU_MAX_GOALS];
    int n;
    int active;
    char schedule_note[160];
} ikitaku_agenda_t;

void ikitaku_agenda_init(ikitaku_agenda_t* a);
int  ikitaku_agenda_add(ikitaku_agenda_t* a, const char* id, uint8_t kind,
                        int priority, int deadline_ops);
int  ikitaku_agenda_tick(ikitaku_agenda_t* a, const ikitaku_desperation_t* dep,
                         int confusion, char* english, int elen);

/* PURPOSE: desire = delta on an ordered set (the box).
   WHY: assess "not impossible" first; then spend budget on transforms
   (filter/map/invert/shift) scored against the wanted state. Vague OK. */
#define IKITAKU_DESIRE_STEPS  12
typedef struct {
    char verb[24];         /* R5 used */
    char how[48];          /* R4-ish */
    int  improved;         /* residual dropped */
    int  mse_before;
    int  mse_after;
    int  budget_spent;
} ikitaku_desire_step_t;

typedef struct {
    char want_note[80];    /* may be vague */
    int  possible;         /* 1 = not-impossible passed */
    int  possible_conf;    /* 0..100 */
    int  desperation;
    int  budget;
    int  budget_used;
    int  focus;            /* climbs on successful steps */
    int  mse_have;
    int  mse_best;
    int  solved;
    ikitaku_series_t have;
    ikitaku_series_t want;
    ikitaku_series_t delta;
    ikitaku_series_t work;
    ikitaku_desire_step_t step[IKITAKU_DESIRE_STEPS];
    int n_steps;
    char have_pat[32];
    char want_pat[32];
    char chain[120];       /* verbs applied */
    int  win_off;          /* best alignment of partial want */
    int  win_span;         /* have-samples the want window covered */
    char english[280];
} ikitaku_desire_t;

int ikitaku_desire_init(ikitaku_desire_t* d,
                        const ikitaku_series_t* have,
                        const ikitaku_series_t* want,
                        const char* want_note);
int ikitaku_desire_possible(ikitaku_desire_t* d);
int ikitaku_desire_pursue(ikitaku_desire_t* d);

/* PURPOSE: template = partial form. WHY: short bag is a witness, not a
   second whole object. Offset + ratio are f'+c. */
typedef struct {
    int mse;
    int off;
    int span;
    char r4[24];
    char english[160];
} ikitaku_template_t;

int ikitaku_template_match(const ikitaku_series_t* have,
                           const ikitaku_series_t* want,
                           ikitaku_template_t* out);

/* Grow context on a template. Desire may sit at any f' with little c;
   recurse by feeding a longer slice of want until mse holds or budget dies. */
#define IKITAKU_EXPAND_MAX  8
typedef struct {
    int ctx;       /* want samples used this step */
    int mse;
    int off;
    int span;
    int held;      /* 1 if mse did not blow up vs previous */
} ikitaku_expand_step_t;

typedef struct {
    ikitaku_expand_step_t step[IKITAKU_EXPAND_MAX];
    int n;
    int stable;
    int depth;
    char english[200];
} ikitaku_expand_t;

int ikitaku_template_expand(const ikitaku_series_t* have,
                            const ikitaku_series_t* want,
                            int start_n, int max_depth,
                            ikitaku_expand_t* out);

/* PURPOSE: experience = apply a rule, keep the outcome.
   WHY: L4 metaphor without L3 cases is inexperience, not a missing R6.
   Reward is HOLD/unwrap — case memory, not policy-gradient RL. */
#define IKITAKU_XP_PATH "/home/workdir/artifacts/rs_vm/ikitaku/discovery/experience/cases.cat"
#define IKITAKU_XP_MAX  24
typedef struct {
    char rule[64];
    char r5[24];
    char r4[24];
    int  ctx;
    int  reward;
    int  count;
} ikitaku_xp_t;

typedef struct {
    ikitaku_xp_t item[IKITAKU_XP_MAX];
    int n;
    int known;
    char english[180];
} ikitaku_xp_report_t;

int ikitaku_xp_load(ikitaku_xp_report_t* out);
int ikitaku_xp_record(const char* rule, const char* r5, const char* r4,
                      int ctx, int reward);
int ikitaku_xp_lookup(const char* rule, ikitaku_xp_t* out);
int ikitaku_gap_window(const ikitaku_series_t* have,
                       int hole_lo, int hole_hi,
                       ikitaku_template_t* out);

/* PURPOSE: finite-c strategy for partial wants.
   WHY: vague desire without experience is insufficient priori, not a lie.
   Micro score = Hegelian hunt. Macro label = Kant critique.
   Several witnesses; track divergence. No self-model. */
#define IKITAKU_TSTRAT_MAX  6
typedef struct {
    int lo, hi;            /* want slice */
    int mse, off, span;
    int held;
    int d_mse;             /* divergence vs previous best */
} ikitaku_tstrat_step_t;

typedef struct {
    ikitaku_tstrat_step_t step[IKITAKU_TSTRAT_MAX];
    int n;
    int best;
    int xp_known;
    int insufficient_priori; /* 1 = mark later; mild dissonance */
    int confusion;           /* 0..100, not a high-severity flag */
    int dissonance;          /* mild unless pre-score eval */
    int hunt_next;
    int hunt_hold;
    char critique_name[48];
    char english[260];
} ikitaku_tstrat_t;

int ikitaku_template_strategy(const ikitaku_series_t* have,
                              const ikitaku_series_t* want,
                              ikitaku_tstrat_t* out);

// ---- Query stub: ask an entity for origin / new info ----
typedef enum {
    IKITAKU_ENTITY_SELF = 0,
    IKITAKU_ENTITY_HUMAN,
    IKITAKU_ENTITY_ANY,
    IKITAKU_ENTITY_WEBHOOK,
    IKITAKU_ENTITY_ONLINE
} ikitaku_entity_t;

typedef struct {
    uint8_t to;
    char question[160];
    char answer[192];      // stub / filled if webhook ack
    int  pending;
} ikitaku_query_t;

int ikitaku_query_origin(ikitaku_query_t* q, const char* topic);
int ikitaku_query_dispatch(const ikitaku_query_t* q, const char* webhook_url);

/* "I don't have context" → ask. Human may answer with a conf interval. */
typedef struct {
    uint8_t fail;          /* ikitaku_aec_fail_t */
    char    ask[160];
    char    taught[160];
    int     teach_lo, teach_hi;
    int     waiting;
    char    english[200];
} ikitaku_context_ask_t;

int ikitaku_need_context(const char* rule, const ikitaku_gap_report_t* gaps,
                         ikitaku_context_ask_t* out);
int ikitaku_teach_context(ikitaku_context_ask_t* a,
                          const char* fact, int conf_lo, int conf_hi);

/* Metaphor = overlap of SOME F attributes. Not the whole costume. */
#define IKITAKU_ATTR_MAX  8
typedef struct {
    char name[24];
    char attr[IKITAKU_ATTR_MAX][24];
    int  n;
} ikitaku_attrbag_t;

typedef struct {
    int shared;
    int only_a;
    int only_b;
    int jaccard;           /* 0..100 */
    char shared_list[80];
    char r5[24];           /* common purpose if any */
    char r4_a[24], r4_b[24];
    int  same_r5;
    int  same_r4;
    char english[200];
} ikitaku_metaphor_t;

int ikitaku_metaphor(const ikitaku_attrbag_t* a, const ikitaku_attrbag_t* b,
                     const char* r5, const char* r4a, const char* r4b,
                     ikitaku_metaphor_t* out);

/* Several R3/R4 may solve one problem; try falling confidence. */
#define IKITAKU_ALT_MAX  6
typedef struct {
    char r4[24];
    int  conf;
    int  tried;
    int  held;
} ikitaku_alt_t;

typedef struct {
    ikitaku_alt_t alt[IKITAKU_ALT_MAX];
    int n;
    int chosen;
    char english[160];
} ikitaku_alts_t;

int ikitaku_alts_rank(ikitaku_alts_t* out,
                      const char** r4, const int* conf, int n);
int ikitaku_alts_next(ikitaku_alts_t* a); /* next untried by falling conf */

// Debug archive (webhook URL passed in; never hardcode secrets in headers)
int ikitaku_debug_post(const char* webhook_url, const char* text);



// ---- Uncertainty types (methodological parallel) ----
typedef enum {
    IKITAKU_U_EPISTEMIC = 0,   // don't know the model / missing knowledge
    IKITAKU_U_ALEATORIC,       // inherent noise in the data
    IKITAKU_U_BAYESIAN         // posterior over competing models
} ikitaku_uncertainty_kind_t;

typedef struct {
    int epistemic;     // 0..100
    int aleatoric;     // 0..100
    int bayesian;      // 0..100 disagreement among fits
    uint8_t dominant;
    char english[192];
} ikitaku_uncertainty_t;

int ikitaku_classify_uncertainty(const ikitaku_fit_report_t* fits,
                                 const ikitaku_var_report_t* vars,
                                 const ikitaku_dissonance_report_t* diss,
                                 ikitaku_uncertainty_t* out);

// Bound roles; optionally apply desperation to *slightly* collapse
// determinant/co/irrelevant toward a pattern — never full resolution
// without extracted rule.
int ikitaku_bound_variables(ikitaku_var_report_t* vars,
                            const ikitaku_desperation_t* dep,
                            int willing,   // 0 = refuse extra collapse
                            char* english, int elen);

// Patience / thought-budget
#define IKITAKU_MIN_OPS      1680   /* 1400 + 20% default */
#define IKITAKU_MIN_SECONDS  0.50

typedef struct {
    int    base_ops;
    int    expanded_ops;     // +30% if hard
    double base_seconds;
    double expanded_seconds;
    int    enough;           // 1 if spent >= min after expansion
    char   english[160];
} ikitaku_patience_t;

int ikitaku_patience_eval(const ikitaku_think_cost_t* cost,
                          const ikitaku_desperation_t* dep,
                          int confusion,
                          ikitaku_patience_t* out);

// Thought log: files/itikatu_thought_output/raw_<unix>_out.txt
int ikitaku_thought_log(const char* text);



// Stacked-transform completion + "on the right track"
typedef struct {
    char formula[64];
    int  conf;
    int  inverse_ok;     // asin/identity-style check improved residual
    char note[96];
} ikitaku_stack_try_t;

#define IKITAKU_MAX_STACK  12
typedef struct {
    ikitaku_stack_try_t tries[IKITAKU_MAX_STACK];
    int n;
    int best;
    int right_track;     // partial indicator → keep probing
    char english[192];
} ikitaku_stack_report_t;

int ikitaku_stack_complete(const ikitaku_series_t* s,
                           const ikitaku_fit_report_t* partial,
                           ikitaku_stack_report_t* out);

// Deductive (top-down F→data) vs inductive (bottom-up data→F)
int ikitaku_layered_reason(const ikitaku_series_t* s,
                           ikitaku_stack_report_t* out);

/* Invert suspected outer wrap, identify inner, rescore composition. */
int ikitaku_peel_transforms(const ikitaku_series_t* s,
                            ikitaku_stack_report_t* io);




/* Metaphor/analogy → R6 plan. Parent climb + origin when current layer is weak.
   Extraction is Jaccard/Lie; generation proposes NOVEL strategy only (basial locked). */
typedef struct {
    int  jaccard;
    int  analog_sim;
    int  climbed;
    int  layer_src, layer_dst;
    int  vert_score;
    char src[24], dst[24];
    char parent_src[24], parent_dst[24];
    char nature[24];
    char origin_lead[24];   /* origin | peel-parent | hold */
    char strategy[24];      /* R6 lead to try */
    char form[80];          /* proposed method form */
    int  novel;             /* 1 if we proposed a developed strategy */
    int  propose_rc;        /* 0 new, 1 existed, -1 skipped */
    int  novel_score;
    char english[240];
} ikitaku_metaphor_plan_t;

int ikitaku_metaphor_plan(const char* src, const char* dst,
                          int persist_novel,
                          ikitaku_metaphor_plan_t* out);

/* Fill a small attribute bag from a form name (parent, R5, layer). */
int ikitaku_form_attrbag(const char* form, ikitaku_attrbag_t* out);
