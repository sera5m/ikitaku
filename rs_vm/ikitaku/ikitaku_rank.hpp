#pragma once
/* R9–R0 gates, wide R3 genus store, F antiderivative merge, Kant table, R6 compose. */

#define IKITAKU_R3_CAP    8192
#define IKITAKU_R3_LINKS  8
#define IKITAKU_R3_ATTR   8
#define IKITAKU_R3_PATH   "/home/workdir/artifacts/rs_vm/ikitaku/discovery/r3.slab"
#define IKITAKU_R6_DEPTH_MAX 6

uint32_t ikitaku_id32(const char* s);
uint32_t ikitaku_r4_id(const char* parent, const char* family);

typedef struct {
    uint32_t r4_id;
    uint16_t weight;   /* 0..100 assoc into R4 */
} ikitaku_r3_link_t;

typedef struct {
    uint32_t id;
    char     name[32];
    char     r5[24];
    uint8_t  n_links;
    uint8_t  n_attr;
    uint8_t  used;
    ikitaku_r3_link_t link[IKITAKU_R3_LINKS];
    char     attr[IKITAKU_R3_ATTR][20];
} ikitaku_r3_rec_t;

int ikitaku_r3_put(const char* name, const char* r5,
                   const uint32_t* r4_ids, const uint16_t* w, int n_links,
                   const char attrs[][20], int n_attr);
int ikitaku_r3_get(const char* name, ikitaku_r3_rec_t* out);
int ikitaku_r3_get_id(uint32_t id, ikitaku_r3_rec_t* out);
int ikitaku_r3_count(void);

/* Two max-derivatives (platonic F) merge as antiderivatives → R3 genus. */
int ikitaku_F_merge_lower(const char* Fa, const char* Fb,
                          ikitaku_r3_rec_t* out);

/* R9 unknown / R8 useful / R7 right-task */
typedef struct {
    int r9_unknown;
    int r8_useful;
    int r7_right_task;
    int assoc_to_goal;
    char goal[32];
    char english[220];
} ikitaku_rank_gate_t;

int ikitaku_rank_gate(const char* goal_r5, const char* bag_r5,
                      int residual, int possible_conf,
                      ikitaku_rank_gate_t* out);

/* Vertical thinking plan: R9 → R8 → R7 → R6.
   Two useful goals + pareto_conflict is R7 (right-task not singular),
   not R6 “missing boolean”. */
#define IKITAKU_LADDER_STEPS  8
typedef struct {
    ikitaku_rank_gate_t gate_a;
    ikitaku_rank_gate_t gate_b;
    int n_goals;
    int pareto_conflict;
    int r9;
    int r8_a, r8_b;
    int r7_pick;          /* 0 none, 1 a, 2 b, 3 conflict */
    int r6_ready;         /* 1 only if r7_pick is 1 or 2 */
    char r6_lead[24];
    char step[IKITAKU_LADDER_STEPS][80];
    int n_step;
    char missing[48];     /* goal-priority | method | none */
    char english[280];
} ikitaku_ladder_plan_t;

int ikitaku_ladder_plan(const char* goal_a, const char* bag_a,
                        const char* goal_b, const char* bag_b,
                        int residual, int possible_conf,
                        int pareto_conflict,
                        ikitaku_ladder_plan_t* out);


/* Kant 12 + practiced movement */
#define IKITAKU_KANT_N  13
typedef struct {
    char category[24];
    char class_[24];     /* quantity|quality|relation|modality|schema */
    char r5[24];
    char note[80];
} ikitaku_kant_cat_t;

int ikitaku_kant_table(ikitaku_kant_cat_t* out, int maxn);
int ikitaku_kant_lookup(const char* category, ikitaku_kant_cat_t* out);

/* Fuzzy Hegel over the 16 Boolean ops (charges ±1).
   Thesis/antithesis confidences are correctness extremes; directional bias
   favors invert of the more-wrong pole. Optional near-far Lie tightens
   ambiguous targets. allow_persist=0 keeps subjects on isolated path only. */
typedef struct {
    int best_op;
    char op_name[24];
    int th_charge, anti_charge, synth_charge;
    int th_conf, anti_conf;
    int bias_dir;          /* +1 favor thesis, -1 favor anti/invert, 0 mid */
    int from50_th, from50_anti;
    int lie_mag;           /* near-far bracket magnitude */
    int ambiguity_reduced; /* 1 if Lie near-far shrank the gap */
    int scores[16];
    ikitaku_hypo_t synthesis;
    char english[320];
} ikitaku_hegel_bool_t;

int ikitaku_hegel_bool16(const ikitaku_hypo_t* thesis,
                         const ikitaku_hypo_t* antithesis,
                         int allow_persist,
                         ikitaku_hegel_bool_t* out);

/* Fuzzy TAS under Kant: thesis/antithesis → synthesis, then critique. */
int ikitaku_kant_tas(const ikitaku_hypo_t* thesis,
                     const ikitaku_hypo_t* antithesis,
                     ikitaku_hypo_t* synthesis,
                     ikitaku_critique_t* critique);

/* R6: compose macros from micros (synthetic a priori). Call self/meta. */
typedef struct {
    ikitaku_thought_plan_t plan;
    char macro[24];
    uint8_t call_self;
    uint8_t call_meta;
    uint8_t depth;
    char methods[8][24];
    int n_methods;
    char english[240];
} ikitaku_r6_t;

int ikitaku_r6_compose(const char* objective, int revisit_ok,
                       uint8_t depth, int math_conf, int rule_conf,
                       int desperation, ikitaku_r6_t* out);

/* Lie bracket as Kant/Hegel unity.
   [A,B]=AB-BA. Kant unity = they act as one (commute).
   Hegel leftover = the bracket itself (determinate negation of "same order"). */
#define IKITAKU_GOAL_STEPS  8
typedef struct {
    char A[24], B[24];
    int  mag;
    int  unity;
    int  hegel;
    char leftover[32];
    char english[180];
} ikitaku_lie_t;

int ikitaku_lie_bracket(const char* A, const char* B, ikitaku_lie_t* out);
int ikitaku_lie_scan(char verbs[][24], int n, ikitaku_lie_t* worst);

typedef struct {
    char verb[IKITAKU_GOAL_STEPS][24];
    int  n;
    int  joint;
    int  joint_mag;
    int  unity;
    int  work_mag[IKITAKU_GOAL_STEPS]; /* -1 = use Lie table; else fractional */
    char english[200];
} ikitaku_goal_comp_t;

int ikitaku_goal_extract(const char* have_r5, const char* want_r5,
                         int residual, ikitaku_goal_comp_t* out);
int ikitaku_goal_compose(char verbs[][24], int n, ikitaku_goal_comp_t* out);

/* Curve of a chain: leftover at each joint. Approach = flatten hottest. */
typedef struct {
    int mag[IKITAKU_GOAL_STEPS];
    int n;
    int total;
    int maxm;
    int argmax;
    int step;
    int flattened;
    int frac;          /* 0..100 of gap closed this step */
    int hypo;          /* 0 keep 1 swap 2 merge */
    char english[200];
} ikitaku_curve_t;

int ikitaku_goal_curve(const ikitaku_goal_comp_t* g, ikitaku_curve_t* out);
int ikitaku_goal_approach(ikitaku_goal_comp_t* g, ikitaku_curve_t* c);
int ikitaku_goal_create(const char* A, const char* B, ikitaku_r3_rec_t* out);

typedef struct {
    ikitaku_rank_gate_t gate;
    ikitaku_r6_t r6;
    ikitaku_r3_rec_t genus;
    ikitaku_goal_comp_t goal;
    ikitaku_lie_t lie;
    ikitaku_curve_t curve;
    int merged;
    int created;
    int inconsistent;
    int depth_used;
    char english[240];
} ikitaku_solve_t;

int ikitaku_objective_solve(const char* goal_r5, const char* bag_r5,
                            const char* Fa, const char* Fb,
                            int residual, int possible,
                            uint8_t depth, ikitaku_solve_t* out);

/* Split parent → working micros + gap joints; assemble via TAS/Lie. */
#define IKITAKU_SUB_MAX  8
typedef struct {
    char verb[24];
    char status[12];   /* working | gap | done */
    int  unity;
    int  mag;
    int  conf;         /* this cell's own */
    int  confusion;    /* this cell's own */
    int  attention;
    int  meta_conf;    /* sure this approach is right */
    char why_fail[80];
} ikitaku_subobj_t;

typedef struct {
    char parent[32];
    ikitaku_subobj_t sub[IKITAKU_SUB_MAX];
    int n, n_working, n_gap, n_done;
    ikitaku_goal_comp_t assembled;
    int solved;
    int ticks;
    int remaining;
    int give_up;
    int cap;
    int macro_conf;
    int macro_confusion;
    int min_conf, max_conf;
    int cascade_fired;
    char method_pick[24];
    int method_score;
    int n_good, n_bad;
    char bind[48];
    char english[240];
} ikitaku_autonomy_t;

int ikitaku_objective_split(const char* have_r5, const char* want_r5,
                            int residual, ikitaku_autonomy_t* out);
int ikitaku_objective_assemble(ikitaku_autonomy_t* a);
int ikitaku_objective_run(const char* have_r5, const char* want_r5,
                          int residual, int budget, ikitaku_autonomy_t* out);
int ikitaku_macro_eval_ready(const ikitaku_autonomy_t* a);
int ikitaku_upward_cascade(ikitaku_autonomy_t* a);
int ikitaku_cell_attend(ikitaku_autonomy_t* a);
int ikitaku_bind_near_far(const char* near, const char* far, ikitaku_lie_t* out);

/* R6 watches; lower layer executes. A bag is a delegated box. */
typedef struct {
    char name[24];
    uint8_t layer;     /* 0 surface .. 6 strategy */
    char task[48];
    int watching;
    int conf;
    char english[160];
} ikitaku_bag_t;

int ikitaku_bag_delegate(const char* task, uint8_t layer, ikitaku_bag_t* out);

/* Analogy: A→B is similar in nature to C→D (structure-map the transform). */
typedef struct {
    char A[24], B[24], C[24], D[24];
    int  mag_ab, mag_cd;
    int  sim;          /* 0..100 nature-similarity of the two transforms */
    int  inferred;
    char nature[48];
    char english[200];
} ikitaku_analogy_t;

int ikitaku_analogy(const char* A, const char* B, const char* C, const char* D,
                    ikitaku_analogy_t* out);

/* Composition-attributes: adj/adverb poles of an axis (R3–R5).
   small/big are not verbs — leftover of the pair IS magnitude. */
#define IKITAKU_CATTR_N  16
typedef struct {
    char name[20];
    char axis[20];
    char lo[16], hi[16];
    uint8_t layer;     /* 3..5 */
    uint8_t adverb;    /* 1 = modifies transform (how) */
    int  pole;         /* -1 lo, +1 hi, 0 axis itself */
} ikitaku_cattr_t;

int ikitaku_cattr_lookup(const char* name, ikitaku_cattr_t* out);
int ikitaku_compare_axis(const char* A, const char* B, ikitaku_lie_t* out);


/* Method-from-context: residual + purity → preferred R6 lead (light). */
int ikitaku_method_from_context(int residual, int pure, int math_conf, int rule_conf,
                                char lead[24], char english[160]);

/* Goal spine: desire/have-want → ladder R9..R6 → objective_run → optional self-mod. */
typedef struct {
    ikitaku_ladder_plan_t ladder;      /* pass 1 — CONTROL STACK only */
    ikitaku_ladder_plan_t ladder2;     /* pass 2 after self-mod */
    ikitaku_autonomy_t autonomy;
    ikitaku_autonomy_t autonomy2;
    ikitaku_self_improve_t improve;
    char method_lead[24];
    char method_lead2[24];
    int ran_objective;
    int ran_objective2;
    int ran_improve;
    int residual_in;
    int residual_mid;
    int residual_out;
    int closed_loop;
    int n_think_passes;
    ikitaku_circle_t circle;
    int circle_act;
    ikitaku_thought_trace_t trace;
    ikitaku_reflect_t reflect;
    /* Glue: R5 expr tree + patterning for GOAL DETERMINATION (not control) */
    ikitaku_expr_tree_t goal_bag;      /* lead(want)+c[residual], have as interior */
    ikitaku_expr_tree_t method_bag;    /* self-mod substance: method as F(x)+c */
    ikitaku_abs_pattern_t goal_pattern;
    ikitaku_abs_pattern_t method_pattern;
    int ran_goal_pattern;
    int ran_method_pattern;
    char abstract_r5[24];              /* from goal pattern — substance tip */
    char english[520];
} ikitaku_goal_spine_t;

int ikitaku_goal_spine(const char* have_r5, const char* want_r5,
                       int residual, int possible_conf,
                       int budget, int self_mod, int allow_persist,
                       ikitaku_goal_spine_t* out);

/* From a filled desire: map patterns to verbs, then spine. */
int ikitaku_goal_spine_from_desire(const ikitaku_desire_t* d,
                                   int budget, int self_mod, int allow_persist,
                                   ikitaku_goal_spine_t* out);

/* Self-mod closed loop: improve → persist? → re-measure residual. */
int ikitaku_self_mod_closed(int residual, int math_conf, int rule_conf,
                            uint8_t depth, int allow_persist,
                            ikitaku_self_improve_t* first,
                            ikitaku_self_improve_t* second,
                            char english[240]);


/* Goal behavior by delta → endpoint set (parametric % filter).
   Segments bag / abstraction ladder / leaf traverse + hole extrapolate.
   Top IKITAKU_ENDPOINT_MAX likely endpoints of desire. */
#define IKITAKU_ENDPOINT_MAX  10
#define IKITAKU_SEG_MAX       8
typedef struct {
    char kind[24];           /* direct|linear|seg-slope|hole-fill|phase|abstract */
    float values[32];
    int n;
    int score_pct;           /* 0..100 parametric likelihood (fuzzy-blended) */
    int seg_id;
    int layer;
    int layer_conf;          /* 0..100 confidence this layer is appropriate */
    int layer_meta_conf;     /* 0..100 meta-confidence that layer_conf is reliable */
    char form[32];
    char english[140];
} ikitaku_endpoint_t;

typedef struct {
    ikitaku_endpoint_t ep[IKITAKU_ENDPOINT_MAX];
    int n;
    int best;
    float delta_mean, delta_var;
    /* Fuzzy membership of delta magnitude (0..1 each, may overlap) */
    float fuzzy_near;        /* |mean| small */
    float fuzzy_mid;
    float fuzzy_far;         /* |mean| large */
    float fuzzy_stable;      /* low variance */
    float fuzzy_noisy;       /* high variance */
    int n_holes;
    int n_segs;
    char seg_form[IKITAKU_SEG_MAX][24];
    /* Ladder-layer confidence for the analysis as a whole */
    int layer_conf;          /* best endpoint's layer conf */
    int layer_meta_conf;
    int unsure_layer;        /* 1 → escalate to metacog */
    int escalate_metacog;    /* 1 when meta-conf low or layers disagree */
    char metacog_note[160];
    char english[360];
} ikitaku_goal_delta_t;

int ikitaku_goal_delta_endpoints(const ikitaku_series_t* have,
                                 const ikitaku_series_t* want,
                                 ikitaku_goal_delta_t* out);
int ikitaku_goal_delta_from_desire(const ikitaku_desire_t* d,
                                  ikitaku_goal_delta_t* out);


/* Constraint = second-order +c of antithesis (execution context).
   Method = inverse of constraint (may costume/micro-mutate for macro effect).
   Solution synthesis = fuzzy Hegel + memory paths. Constraints shape form. */
typedef struct {
    char constraint[64];     /* +c text */
    char antithesis[32];     /* what is opposed / forbidden axis */
    char method[32];         /* inverse tool / known method */
    int  costume;            /* 1 = micro indirection allowed */
    int  violates;           /* 1 if method conflicts with constraint hard */
    int  shape_form;         /* 1 = constraint changed preferred form */
    char preferred_form[32]; /* shaped solution form */
    int  conf;
    char english[240];
} ikitaku_constraint_t;

int ikitaku_constraint_set(ikitaku_expr_tree_t* t, int idx,
                           const char* constraint, const char* antithesis);
int ikitaku_constraint_method(const ikitaku_constraint_t* c,
                              const char* method, int allow_costume,
                              ikitaku_constraint_t* out);
int ikitaku_constraint_shape(const ikitaku_constraint_t* c,
                             const char* desired_form,
                             ikitaku_constraint_t* out);

/* Symbolic delta: problem identification before solve (not only numeric). */
typedef struct {
    char have_sym[48];
    char want_sym[48];
    char delta_sym[96];      /* "have X lacks Y" / form gap */
    char problem[96];        /* extracted problem statement */
    char missing_form[32];
    int  conf;
    int  needs_detail;       /* 1 = extract more before solve */
    char english[280];
} ikitaku_sym_delta_t;

int ikitaku_symbolic_delta(const char* have_form, const char* want_form,
                           const char* have_note, const char* want_note,
                           ikitaku_sym_delta_t* out);
int ikitaku_symbolic_delta_from_series(const ikitaku_series_t* have,
                                       const ikitaku_series_t* want,
                                       ikitaku_sym_delta_t* out);


/* Positive/negative constraints narrow solution space (+c establishment).
   Construction = inverse of symbolic extraction: known micro methods → assemble.
   Kant/Hegel: pos forces inclusion, neg is antithesis-context forbidding paths. */
#define IKITAKU_CONS_MAX   6
#define IKITAKU_MICRO_MAX  10
typedef struct {
    char pos[IKITAKU_CONS_MAX][40];  /* must-hold +c */
    char neg[IKITAKU_CONS_MAX][40];  /* must-not (antithesis context) */
    int n_pos, n_neg;
    char micro[IKITAKU_MICRO_MAX][24]; /* candidate method micros after filter */
    int n_micro;
    char rejected[IKITAKU_MICRO_MAX][24];
    int n_rejected;
    int space_before;            /* nominal candidate count pre-filter */
    int space_after;             /* after pos/neg narrowing */
    int narrow_pct;              /* 0..100 how much space closed */
    char assembled[8][24];       /* construction order */
    int n_assembled;
    char form[32];               /* shaped macro form */
    int conf;
    int used_memory;             /* 1 if strategy memory biased order */
    int mode;                    /* CASCADE or COLLAPSE preferred for next step */
    int metacog_ok;              /* 1 if pattern on assembled chain ran */
    int xp_recorded;             /* 1 if lived outcome written */
    char chain_r5[24];           /* abstract tip of constructed chain */
    char english[400];
} ikitaku_construct_t;

int ikitaku_construct_init(ikitaku_construct_t* c);
int ikitaku_construct_add_pos(ikitaku_construct_t* c, const char* constraint);
int ikitaku_construct_add_neg(ikitaku_construct_t* c, const char* constraint);
/* Narrow micros by pos/neg, then assemble solution by construction (not peel). */
int ikitaku_construct_solution(ikitaku_construct_t* c,
                               const char* have_form, const char* want_form,
                               const char* problem_sym);


/* ---- Healthy solve: cascade vs collapse (named dual) ----
   COLLAPSE: Occam preferred under calm residual; narrow method set; close bag.
   CASCADE:  desperation expands models; open bag; up-ladder.
   ALTERNATE: healthy solve switches by residual/desperation/layer conf. */
typedef enum {
    IKITAKU_MODE_COLLAPSE = 0,
    IKITAKU_MODE_CASCADE  = 1,
    IKITAKU_MODE_ALTERNATE = 2
} ikitaku_solve_mode_t;

typedef struct {
    int mode;                /* chosen primary */
    int residual;
    int desperation;
    int narrow_pct;
    int layer_conf;
    int reason_occam;        /* 1 if collapse from calm */
    int reason_despair;      /* 1 if cascade from hot residual */
    char english[200];
} ikitaku_mode_pick_t;

int ikitaku_solve_mode_pick(int residual, int desperation, int narrow_pct,
                            int layer_conf, int unsure_layer,
                            ikitaku_mode_pick_t* out);

/* Metacog on assembled construct + optional xp record (lived fuel). */
int ikitaku_construct_metacog(ikitaku_construct_t* c, int record_xp);

/* Desire → MAKE under constraints (full glue):
   symbolic delta → pos/neg → construct → mode pick → make bag → metacog → xp */
typedef struct {
    ikitaku_sym_delta_t problem;
    ikitaku_construct_t construct;
    ikitaku_mode_pick_t mode;
    ikitaku_expr_tree_t work_bag;    /* MADE product */
    ikitaku_abs_pattern_t pattern;
    int made;                        /* 1 if work bag built */
    int residual_in, residual_out;
    char english[420];
} ikitaku_desire_make_t;

int ikitaku_desire_make(const ikitaku_series_t* have,
                        const ikitaku_series_t* want,
                        const char* want_note,
                        const char** pos, int n_pos,
                        const char** neg, int n_neg,
                        int residual,
                        ikitaku_desire_make_t* out);
int ikitaku_desire_make_from_desire(const ikitaku_desire_t* d,
                                   const char** pos, int n_pos,
                                   const char** neg, int n_neg,
                                   ikitaku_desire_make_t* out);

/* Fill structured dialectic/synthesis hypos (non-empty form/purpose/conf). */
void ikitaku_hypo_fill(ikitaku_hypo_t* h, const char* purpose, const char* form,
                       int conf, int rewrite_ok);

/* Endpoint set → MAKE under constraints (auto path). */
int ikitaku_endpoint_make(const ikitaku_goal_delta_t* gd,
                          const char** pos, int n_pos,
                          const char** neg, int n_neg,
                          int residual,
                          ikitaku_desire_make_t* out);
