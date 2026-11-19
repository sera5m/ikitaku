#pragma once
/* PURPOSE: theories about events (EOL/JOL/FOK). WHY: monitor whether the
   accept/LGG is still the right reading. Not a new R5 verb. */
/* Metacognition layer — knowledge vs regulation.
   Predictions are THEORIES ABOUT EVENTS, not budget guesses. */

#define IKITAKU_MCOG_MAX  12

typedef enum {
    IKITAKU_MK_DECL = 0,   /* what kinds of problem exist */
    IKITAKU_MK_PROC = 1,   /* how a method runs */
    IKITAKU_MK_COND = 2    /* when to use which */
} ikitaku_mkind_t;

typedef enum {
    IKITAKU_J_EOL  = 0,    /* before work: I theorize this task will be hard/easy */
    IKITAKU_J_JOL  = 1,    /* after a reading: I theorize this form will hold */
    IKITAKU_J_FOK  = 2,    /* can't name it; I theorize I would recognize it */
    IKITAKU_J_RETRO = 3    /* after accept: I theorize that answer was right */
} ikitaku_jtype_t;

typedef struct {
    uint8_t jtype;
    uint8_t mk;            /* which knowledge kind this theory uses */
    char    event[80];     /* the event being theorized */
    char    claim[96];     /* the theory */
    int     prior;         /* 0..100 strength of the theory, not a budget */
    int     scored;        /* 1 once outcome known */
    int     held;          /* 1 if event matched the theory */
    char    outcome[80];
} ikitaku_theory_t;

typedef struct {
    ikitaku_theory_t th[IKITAKU_MCOG_MAX];
    int n;
    int n_scored, n_held;
    int calibration;       /* held/scored * 100, -1 if none scored */
    char decl[64];
    char proc[64];
    char cond[80];
    char monitor[128];
    char control[128];
    char try_pattern[32];
    char kant_class[24];
    char gap_hint[48];
    char english[320];
} ikitaku_mcog_t;

int ikitaku_mcog_run(const ikitaku_session_t* s,
                     const ikitaku_cluster_report_t* cr,
                     const ikitaku_fit_report_t* fits,
                     int desperation_level,
                     ikitaku_mcog_t* out);

/* Tiny LGG: shared purpose prefix / shared tokens → general form sketch */
int ikitaku_lgg_purposes(const ikitaku_session_t* s, char* out, int olen);

/* PURPOSE: strategy = goal vs avoid, then hunt between poles.
   WHY: Hegelian thesis/antithesis with a mutex: wait for both scores.
   Bias toward the more extreme miss from Jaccard 50; invert that side. */
#define IKITAKU_HUNT_MAX  8
typedef struct {
    char side[12];     /* pos | neg | mid | invert-neg | invert-pos */
    int  score;        /* 0..100 arrived */
    int  ready;
    int  from50;       /* |score-50| */
} ikitaku_hunt_slot_t;

struct ikitaku_hunt_t {
    char goal[64];
    char avoid[64];
    ikitaku_hunt_slot_t slot[IKITAKU_HUNT_MAX];
    int n;
    int pos_score, neg_score;
    int bias;
    int invert_side;
    int next_probe;
    int solved;
    char english[240];
};

int ikitaku_hunt_init(ikitaku_hunt_t* h, const char* goal, const char* avoid);
int ikitaku_hunt_arrive(ikitaku_hunt_t* h, const char* side, int score);
int ikitaku_hunt_tick(ikitaku_hunt_t* h); /* wait for both; pick invert/mid */

/* Kant: name the axiom. Analytic/synthetic × priori/posteriori. */
typedef enum {
    IKITAKU_K_ANALYTIC = 0,
    IKITAKU_K_SYNTHETIC = 1
} ikitaku_k_judgment_t;
typedef enum {
    IKITAKU_K_PRIORI = 0,
    IKITAKU_K_POSTERIORI = 1
} ikitaku_k_source_t;
typedef struct {
    uint8_t judgment;
    uint8_t source;
    int  axiom;
    int  antinomy;
    int  thing_in_self;
    int  hidden_d;          /* weakly dependent unknown */
    int  presupposition_open; /* 1 = axiom is challengeable */
    int  conf;              /* 0..100 in this claim */
    char action[16];        /* keep | extract | ask */
    char axiom_name[48];
    char english[220];
} ikitaku_critique_t;
int ikitaku_critique(const char* claim, int has_run_evidence, int definitional,
                     int both_poles_ok, int opaque,
                     ikitaku_critique_t* out);
/* New data vs a prior claim. Hidden d / black box → extract or ask. */
int ikitaku_critique_challenge(const ikitaku_critique_t* prior,
                               const char* claim,
                               int new_evidence_conf,
                               int residual,
                               int weakly_dependent,
                               ikitaku_critique_t* out);

/* Modeling depth + place. This process is a bag of rules on a given bag.
   Reality is a superset not in the session. Not a self-model. */
typedef struct {
    int depth;
    int incomplete_priori;
    int hidden_d;
    int self_is_evaluator;
    int world_is_superset;
    char layers_used[48];
    char english[240];
} ikitaku_place_t;

int ikitaku_place_report(int used_r5, int used_r4, int used_hunt,
                         int used_critique, int xp_known, int residual,
                         ikitaku_place_t* out);

/* All bags are structured data. Evaluator and program are data acting
   on data: repeat + order + info. Cycle is the form. Not a soul. */
typedef struct {
    int all_data_structured;
    int self_is_mechanism;
    int code_is_mechanism;
    int acts_on_itself;
    char r5_cycle[48];
    char english[240];
} ikitaku_mechanism_t;

int ikitaku_mechanism_priori(ikitaku_mechanism_t* out);

/* METHOD gap vs data/info gap.
   Understanding-state is a charge: +1 = unity with having a solver,
   -1 = disunity with that state. Autonomous iterate names the missing
   method and asks; never hangs pretending to have a solver. */
#define IKITAKU_GAP_NONE      0
#define IKITAKU_GAP_DATA      1
#define IKITAKU_GAP_INFO      2
#define IKITAKU_GAP_METHOD    3
#define IKITAKU_GAP_REMAINDER 4  /* method held; leftover worlds; budget cannot close */

typedef struct {
    int  kind;
    int  has_solver;
    int  understanding;   /* +1 unity-with-solver, -1 disunity */
    int  want_solver;     /* always +1 */
    int  gap_charge;      /* understanding ⊖ want */
    int  tier;            /* 6 = cognition insufficient; 7 = right-task unclear; 9 = cannot name gap */
    char missing[48];
    char question[80];
    char iterate[160];
    char english[280];
} ikitaku_solver_gap_t;

int ikitaku_method_have(const char* parent, const char* family);
int ikitaku_solver_audit(int needs_boolean, int needs_nested, int needs_xor,
                         int has_data, int cascade_ok,
                         ikitaku_solver_gap_t* out);
int ikitaku_solver_from_remainder(const ikitaku_remainder_t* r,
                                  ikitaku_solver_gap_t* out);
int ikitaku_solver_iterate(ikitaku_solver_gap_t* g,
                           ikitaku_reason_chain_t* chain);



/* Learn new metacog types. Macro (iterative/waterfall) sits on micro
   (hunt/kant/scientific). Thinking = subject whose attrs are methods. */
#define IKITAKU_STRAT_MAX  32
#define IKITAKU_STRAT_CAT  "/home/workdir/artifacts/rs_vm/ikitaku/ikitaku_r6_macro.cat"
typedef struct {
    char name[24];
    char form[80];
    uint8_t macro;     /* 1 = schedule of micros, 0 = micro */
    uint8_t locked;    /* 1 = basial known-good — do not train score */
    uint8_t novel;     /* 1 = developed by iki — subject to scoring */
    int  conf;         /* 0..100 display confidence */
    int  trials;       /* uses of this novel strategy */
    int  wins;         /* residual dropped on use */
    int  drop_sum;     /* sum of residual_in - residual_out (clipped >=0) */
    int  score;        /* derived 0..100 for novel only; basial = conf */
} ikitaku_strategy_t;

typedef struct {
    ikitaku_strategy_t st[IKITAKU_STRAT_MAX];
    int n;
    char chosen[24];
    char english[200];
} ikitaku_strategy_map_t;

int ikitaku_strategy_load(ikitaku_strategy_map_t* out);
int ikitaku_strategy_save(const ikitaku_strategy_map_t* m);
int ikitaku_strategy_propose(const char* name, const char* form, int macro);
int ikitaku_strategy_pick(const char* objective, int revisit_ok,
                          ikitaku_strategy_t* out);

/* Score NEW strategies only (novel=1). Basial locked types keep fixed conf.
   residual_in/out from a trial; updates trials/wins/drop_sum/score/conf. */
int ikitaku_strategy_score(const char* name, int residual_in, int residual_out,
                           ikitaku_strategy_t* out_opt);

/* Rank novel strategies by score (descending). Returns count written. */
int ikitaku_strategy_rank_novel(ikitaku_strategy_t* out, int max_out);

/* Pick best novel strategy if score >= min_score, else -1. */
int ikitaku_strategy_pick_novel(int min_score, ikitaku_strategy_t* out);

typedef struct {
    char methods[8][24];
    int n;
    int thinking_is_concept;
    int thinking_is_self;   /* 1 = this process as a concept on the ladder */
    char english[220];
} ikitaku_thinking_t;

int ikitaku_think_about_thinking(const ikitaku_l6_t* l6,
                                 const ikitaku_strategy_map_t* st,
                                 ikitaku_thinking_t* out);

/* Methods as formulas; vars chained; fuzzy Hegel picks among thought-processes. */
#define IKITAKU_METH_MAX  8
typedef struct {
    char name[24];
    char formula[80];
    char vars[4][24];
    int  n_vars;
    int  score;
} ikitaku_method_form_t;

typedef struct {
    ikitaku_method_form_t m[IKITAKU_METH_MAX];
    int n, chosen, cycles;
    char english[220];
} ikitaku_method_pick_t;

int ikitaku_method_lower(const char* name, ikitaku_method_form_t* out);
int ikitaku_method_hegel_pick(int residual, int math_conf, int rule_conf,
                              int desperation, ikitaku_method_pick_t* out);

/* Kant-fuzzy cluster → try-pattern for strategy (metacog). */
typedef struct {
    char try_pattern[32];
    char formula[80];
    char kant_class[24];
    char kant_cat[24];
    int  membership;
    int  antinomy;
    int  n_tagged;
    int  conf;
    char english[200];
} ikitaku_try_pat_t;

int ikitaku_kant_try_pattern(const ikitaku_cluster_report_t* cr,
                             ikitaku_try_pat_t* out);
int ikitaku_method_hegel_pick_kant(const ikitaku_cluster_report_t* cr,
                                   int residual, int math_conf, int rule_conf,
                                   int desperation, ikitaku_method_pick_t* out);

/* Self = this process as an R5 object (analyze methods on bags). Not a person.
   Recursion: reason on own method-pick; expand leftover as a new idea. */
typedef struct {
    int is_concept;
    int is_person;
    char r5[24];
    char formula[96];
    ikitaku_method_pick_t last;
    int expansions;
    char english[220];
} ikitaku_self_t;

int ikitaku_self_card(ikitaku_self_t* out);
int ikitaku_self_reflect(int residual, int n_done, int solved,
                         const ikitaku_method_pick_t* methods,
                         ikitaku_self_t* out);


/* Circle metacog: identical super-loop without progress → macro conf drops.
   Prevents endless reasoning. Act: halt (missing priors), self-improve
   (meta-metacog), or seek priors. */
#define IKITAKU_CIRCLE_HIST  4
typedef struct {
    char lead[24];
    char form[32];
    int  residual;
    int  r7_pick;
    int  r6_ready;
    int  fingerprint;      /* simple mix of the above */
} ikitaku_loop_fp_t;

typedef enum {
    IKITAKU_CIRCLE_OK = 0,
    IKITAKU_CIRCLE_WARN = 1,
    IKITAKU_CIRCLE_HALT_PRIORS = 2,
    IKITAKU_CIRCLE_SELF_IMPROVE = 3,
    IKITAKU_CIRCLE_SEEK_PRIORS = 4
} ikitaku_circle_act_t;

typedef struct {
    ikitaku_loop_fp_t hist[IKITAKU_CIRCLE_HIST];
    int n_hist;
    int identical_streak;
    int macro_conf;        /* starts 100, drops on circles */
    int progress;          /* 1 if residual improved vs prior */
    int act;               /* ikitaku_circle_act_t */
    char english[280];
} ikitaku_circle_t;

void ikitaku_circle_init(ikitaku_circle_t* c);
int  ikitaku_circle_observe(ikitaku_circle_t* c,
                            const char* lead, const char* form,
                            int residual, int r7_pick, int r6_ready);
/* returns act; updates macro_conf and identical_streak */

/* Map circle to ladder: thought-loop :: while-loop under R5 repeat/cycle. */
typedef struct {
    int mapped;
    int analogy_sim;
    char r5[24];
    char r4[32];
    char english[240];
} ikitaku_circle_concept_t;
int ikitaku_circle_map_concept(const ikitaku_circle_t* c,
                              int allow_teach,
                              ikitaku_circle_concept_t* out);

/* ---- Thought-state tracking (the missing metacog) ----
   Micro steps → macro pattern → relation to goal.
   Any form can layer-drop N levels for broader pattern (generic, not per-case).
   Reflect after fail / succeed / end of chain. */

#define IKITAKU_TRACE_MAX  16
typedef struct {
    char lead[24];
    char form[32];       /* goal/want or method form */
    char r5[24];
    int  residual;
    int  conf;
    int  r7_pick;
    int  r6_ready;
    int  outcome;        /* 0=mid 1=progress 2=stall 3=solved 4=fail */
} ikitaku_micro_t;

typedef struct {
    ikitaku_micro_t step[IKITAKU_TRACE_MAX];
    int n;
    char english[200];
} ikitaku_thought_trace_t;

typedef struct {
    int n_steps;
    int residual_first;
    int residual_last;
    int delta;             /* last - first (negative = progress) */
    int stall_n;           /* consecutive non-progress */
    int progress_n;
    int macro_conf;        /* from stalls vs progress */
    int relates_goal;      /* 1 if delta moves toward goal residual 0 */
    char pattern[32];      /* cycle | cascade | flat | mixed */
    char english[280];
} ikitaku_thought_macro_t;

void ikitaku_trace_init(ikitaku_thought_trace_t* tr);
int  ikitaku_trace_push(ikitaku_thought_trace_t* tr,
                        const char* lead, const char* form, const char* r5,
                        int residual, int conf, int r7, int r6, int outcome);
int  ikitaku_trace_macro(const ikitaku_thought_trace_t* tr,
                         int goal_residual_target,
                         ikitaku_thought_macro_t* out);

/* Bidirectional layer flow for metacog abstraction.
   DOWN = derivative / abstract / macro pattern (specificity falls).
   UP   = anti-derivative / concrete / micro (specificity rises).
   Any subject or thought-form can flow; not a per-case table. */
typedef enum {
    IKITAKU_FLOW_DOWN = 0,  /* abstract toward platonic */
    IKITAKU_FLOW_UP   = 1   /* concretize toward surface */
} ikitaku_flow_dir_t;

typedef struct {
    int from_layer;
    int to_layer;
    int dir;               /* IKITAKU_FLOW_DOWN or UP */
    char at[8][32];
    int  n_at;
    char broader[32];      /* DOWN: coarsest; UP: most specific tip */
    char abstract_r5[24];  /* platonic verb if reached */
    int  used_for_metacog; /* 1 when reflect/spine consumed this */
    char english[280];
} ikitaku_layer_drop_t;

/* Legacy: drop = flow DOWN. */
int ikitaku_layer_drop(const char* form, int from_layer, int n_down,
                       ikitaku_layer_drop_t* out);
/* Full bidirectional. n_steps > 0. */
int ikitaku_layer_flow(const char* form, int from_layer, int n_steps,
                       int dir, ikitaku_layer_drop_t* out);
/* Abstract a subject name (or any form) for macro thinking. */
int ikitaku_abstract_subject(const char* subject, int n_down,
                             ikitaku_layer_drop_t* out);
/* Concretize an abstract form toward micro (up). */
int ikitaku_concretize_form(const char* form, int from_layer, int n_up,
                            ikitaku_layer_drop_t* out);

/* ---- Expression bags: nested F(x)+c (derive / anti-derive) ----
   Platonic F is the form tip. x = data payload. c = context (specificity).
   DOWN/open  = derivative: strip c, enter interior subexpression (macro→micro focus).
   UP/close   = anti-derivative: wrap with +c, treat interior as unit (micro→macro).
   Nested maps are multi-slot expressions, not only single-layer names.
   Subject is always a bag; may hold verb + descriptor slots. */

#define IKITAKU_EXPR_SLOTS    6
#define IKITAKU_EXPR_DEPTH    4
#define IKITAKU_EXPR_PARENTS  4

typedef struct ikitaku_expr_bag {
    char form[32];           /* F — platonic or current tip */
    char data[32];           /* x — payload / subject name */
    char context[48];        /* c — circumstance / specificity */
    int  layer;              /* abstraction level 0..6 */
    int  open;               /* 1 if interior is exposed */
    int  child_idx;          /* vertical: single primary interior, -1 if leaf */
    int  slot[IKITAKU_EXPR_SLOTS]; /* horizontal: sibling sub-bags (indices) */
    int  n_slot;
    /* Parent tags + pointers (multi-parent DAG, not only tree) */
    int  may_have_parents;   /* 1 = allowed to link upward (default 1) */
    int  has_parents;        /* 1 = at least one parent_idx set */
    int  parent[IKITAKU_EXPR_PARENTS]; /* ptrs to parent bag indices */
    int  n_parent;
    int  parent_idx;         /* primary parent = parent[0], or -1 */
    char verb[24];
    char descriptor[32];
    int  unity;              /* 0..100 vs parent / peer average */
    int  composed_c;         /* 1 if context is a composition of properties */
    char remembered[96];     /* snapshot of F(x)+c as property unit */
    char english[160];
} ikitaku_expr_bag_t;

typedef struct {
    ikitaku_expr_bag_t node[IKITAKU_EXPR_DEPTH * IKITAKU_EXPR_SLOTS];
    int n;
    int root;
    char english[320];
} ikitaku_expr_tree_t;

/* Horizontal slot ops + recursive tree map (vertical depth, horizontal peers). */
typedef struct {
    int visited[IKITAKU_EXPR_DEPTH * IKITAKU_EXPR_SLOTS];
    int n_visited;
    int n_vert;              /* vertical edge walks */
    int n_horiz;             /* horizontal peer compares */
    int min_unity, max_unity, avg_unity;
    char path[12][40];
    int n_path;
    char english[360];
} ikitaku_expr_map_t;

void ikitaku_expr_init(ikitaku_expr_tree_t* t);
int  ikitaku_expr_leaf(ikitaku_expr_tree_t* t, const char* form, const char* data,
                       const char* context, int layer);
int  ikitaku_expr_wrap(ikitaku_expr_tree_t* t, int child_idx, const char* form,
                       const char* context, int layer);  /* anti-derive close */
int  ikitaku_expr_open(ikitaku_expr_tree_t* t, int idx); /* derive into interior */
int  ikitaku_expr_close(ikitaku_expr_tree_t* t, int idx);/* treat as unit again */
/* Reason on bag: flow form DOWN/UP and report nested structure. */
int  ikitaku_expr_reason(ikitaku_expr_tree_t* t, int idx, int dir, int steps,
                         ikitaku_layer_drop_t* flow, char english[320]);
/* Subject as bag: name + optional verb + descriptor. */
int  ikitaku_subject_as_bag(const char* subject, const char* verb,
                            const char* descriptor, ikitaku_expr_tree_t* t);

int  ikitaku_expr_add_slot(ikitaku_expr_tree_t* t, int parent_idx, int child_idx);

/* Parent tags: may_have_parents (capability) vs has_parents (ptrs set). */
int  ikitaku_expr_set_may_have_parents(ikitaku_expr_tree_t* t, int idx, int may);
int  ikitaku_expr_link_parent(ikitaku_expr_tree_t* t, int child_idx, int parent_idx);
int  ikitaku_expr_parent_tags(const ikitaku_expr_tree_t* t, int idx,
                              int* may_out, int* has_out, int parents_out[IKITAKU_EXPR_PARENTS],
                              int* n_parent_out, char english[200]);

/* Property (+c) composition, remember F(x)+c, macro-expand context. */
int  ikitaku_expr_compose_c(ikitaku_expr_tree_t* t, int idx,
                            const char* prop_a, const char* prop_b);
int  ikitaku_expr_remember(ikitaku_expr_tree_t* t, int idx);
int  ikitaku_expr_expand_c(ikitaku_expr_tree_t* t, int idx,
                           ikitaku_expr_tree_t* out_slots, char english[280]);
/* Read remembered property line. */
int  ikitaku_expr_property(const ikitaku_expr_tree_t* t, int idx,
                           char property[128], char english[200]);

/* Unified abstraction patterning: micro/macro via derive / anti-derive on bags.
   abstract   = DOWN: open nest, flow F toward platonic, remember stripped +c
   unabstract = UP:   close unit, re-apply +c, flow F toward micro
   pattern_run does abstract → (optional expand) → unabstract */
typedef struct {
    int idx;
    int layer_before, layer_after;
    char form_before[32], form_after[32];
    char r5[24];
    char remembered[96];
    int  n_props_expanded;
    int  dir;                /* FLOW_DOWN or UP */
    char english[320];
} ikitaku_pattern_step_t;

typedef struct {
    ikitaku_pattern_step_t abstract;
    ikitaku_pattern_step_t unabstract;
    ikitaku_layer_drop_t flow_down;
    ikitaku_layer_drop_t flow_up;
    int ok;
    char pattern[32];        /* cycle|cascade|nested|leaf|mixed */
    char english[400];
} ikitaku_abs_pattern_t;

int ikitaku_pattern_abstract(ikitaku_expr_tree_t* t, int idx, int n_down,
                             ikitaku_pattern_step_t* out);
int ikitaku_pattern_unabstract(ikitaku_expr_tree_t* t, int idx, int n_up,
                               const char* restore_c,
                               ikitaku_pattern_step_t* out);
int ikitaku_pattern_run(ikitaku_expr_tree_t* t, int idx, int n_down, int n_up,
                        ikitaku_abs_pattern_t* out);



int  ikitaku_expr_slot_unity(ikitaku_expr_tree_t* t, int a_idx, int b_idx,
                             int* unity_out, char english[160]);
/* Vertical recursion: walk child_idx chain + open each; apply flow at each depth. */
int  ikitaku_expr_map_vertical(ikitaku_expr_tree_t* t, int start_idx, int dir,
                               int max_depth, ikitaku_expr_map_t* out);
/* Horizontal recursion: walk slots[] peers at one node; unity between neighbors. */
int  ikitaku_expr_map_horizontal(ikitaku_expr_tree_t* t, int parent_idx,
                                 ikitaku_expr_map_t* out);
/* Full tree map: vertical then horizontal at each opened node (bounded). */
int  ikitaku_expr_map_tree(ikitaku_expr_tree_t* t, int start_idx, int dir,
                           int max_depth, ikitaku_expr_map_t* out);



/* End-of-chain reflection: micro→macro→goal + optional drop of the pattern. */
typedef struct {
    ikitaku_thought_macro_t macro;
    ikitaku_layer_drop_t drop;
    ikitaku_circle_concept_t circle_concept;
    int reflected;
    char english[360];
} ikitaku_reflect_t;

int ikitaku_reflect_chain(const ikitaku_thought_trace_t* tr,
                          int goal_residual_target,
                          int drop_layers,
                          int allow_teach,
                          ikitaku_reflect_t* out);



/* Self-targeted recursion for strategy improvement.
   Missing pieces this closes:
   - outcome-closed loop (residual delta scores the pick)
   - method-from-context (math/rule residual → desperation → Hegel pick)
   - progressive invent via fuzzy Hegel×bool16 + Kant TAS on methods-as-hypos
   - anti-derivative of strategy (theorize ⊗ new method → R3; strategy_propose)
   Depth-bounded; optional persist of improved strategy. */
#define IKITAKU_SI_STEPS  8
typedef struct {
    char method[24];
    char op[24];           /* bool16 op used this step, or "reflect" */
    int  residual_in;
    int  residual_out;
    int  conf;
    int  invented;          /* 1 if strategy_propose fired */
} ikitaku_si_step_t;

typedef struct {
    ikitaku_si_step_t step[IKITAKU_SI_STEPS];
    int n;
    int depth_used;
    int improved;          /* residual dropped across the loop */
    int invented_n;
    char best_method[24];
    int  best_conf;
    char english[320];
} ikitaku_self_improve_t;

int ikitaku_self_improve(int residual, int math_conf, int rule_conf,
                         uint8_t depth, int allow_persist,
                         ikitaku_self_improve_t* out);

/* Time as a transform: present + Δt lifts time-affected vars.
   Inverse = counterfactual "what if X instead of Y in the past." */
typedef struct {
    int  has_wait;
    int  has_progress;
    int  impure;
    char forward[128];
    char inverse[128];
    char english[220];
} ikitaku_time_xform_t;

int ikitaku_time_transform(const ikitaku_session_t* s, ikitaku_time_xform_t* out);

/* Genus / species on the idea tree (R5 genus → R3/R2 species). */
typedef struct {
    char genus[32];
    char species[48];
    int  conf;
    char note[96];
} ikitaku_taxon_t;

#define IKITAKU_TAXON_MAX  8
typedef struct {
    ikitaku_taxon_t item[IKITAKU_TAXON_MAX];
    int n;
    char english[160];
} ikitaku_taxon_report_t;

int ikitaku_taxon(const ikitaku_session_t* s, ikitaku_taxon_report_t* out);

typedef struct {
    char r5[32];
    char r4[40];
    char r3[40];
    char r2[48];
    char r1[40];
    char r0[40];
    char axis[24];
    int  conf;
} ikitaku_onto_card_t;

#define IKITAKU_ONTO_MAX  16
typedef struct {
    ikitaku_onto_card_t card[IKITAKU_ONTO_MAX];
    int n;
    char english[200];
} ikitaku_onto_t;

int ikitaku_ontology(const ikitaku_session_t* s, ikitaku_onto_t* out);

typedef struct {
    int  tautology;      /* R5 restates R3 */
    int  mimicry;        /* surface looks like F, general form missing */
    int  instance_ok;    /* card is a real F+c instance */
    char stripped[48];
    char note[160];
} ikitaku_extract_t;

int ikitaku_extract_card(const ikitaku_onto_card_t* c, ikitaku_extract_t* out);
int ikitaku_extract_session(const ikitaku_session_t* s, char* english, int elen);

/* Layer-shift ANY bag — data, method, synthesis, internal process.
   Same flow as numbers. No exemption for "this is reasoning." */
typedef struct {
    uint8_t kind;
    char    seed[32];
    int     from_layer;
    ikitaku_layer_drop_t down;
    ikitaku_layer_drop_t up;
    int     can_down, can_up;
    char    english[280];
} ikitaku_reason_shift_t;

int ikitaku_reason_shift_any(const char* form, int kind, int from_layer,
                             ikitaku_reason_shift_t* out);
int ikitaku_reason_shift_hypo(const ikitaku_hypo_t* h, int kind,
                              ikitaku_reason_shift_t* out);

/* Approach change when stuck (human set-shift, not AGI).
   Failed product = fuzzy antithesis. Inhibit last lead. Lazy method+anti.
   Novelty when desperate. Surprise-positive reweights. Occam when calm. */
#define IKITAKU_APPR_MAX  8
typedef struct {
    char last_lead[24];
    char next_lead[24];
    char anti_lead[24];
    char recomb[32];
    char likely[4][24];
    char anti_m[4][24];
    int  n_likely, n_anti;
    int  inhibited;
    int  novelty_seek;
    int  surprise_pos;
    int  occam_bias;
    int  xp_wrote;
    int  micro_conf;
    int  macro_conf;
    int  desperation;
    int  residual;
    ikitaku_layer_drop_t down;
    ikitaku_layer_drop_t up;
    ikitaku_hegel_rec_t  branches;
    char english[280];
} ikitaku_approach_t;

int ikitaku_approach_shift(const char* last_lead, int residual, int desperation,
                           int predicted_hard, int actually_dropped,
                           ikitaku_approach_t* out);

/* Low patience can INTENTIONALLY declare stuck (not only residual stall).
   Understanding those abilities → consider/propose a new method. */
typedef struct {
    int low_patience;
    int declared_stuck;
    int evolved;
    ikitaku_patience_t pat;
    ikitaku_approach_t ap;
    char new_method[32];
    char english[280];
} ikitaku_patience_evolve_t;

int ikitaku_patience_declare_stuck(const ikitaku_think_cost_t* cost,
                                   int desperation, int confusion, int residual,
                                   const char* last_lead,
                                   ikitaku_patience_evolve_t* out);
int ikitaku_understand_new_abilities(ikitaku_patience_evolve_t* ev,
                                     ikitaku_thinking_t* think);

/* When an upper form is proven wrong, walk children that point at it.
   Context-local mismatch does not erase the parent. */
typedef struct {
    char upper[32];
    int  children_touched;
    int  parent_rewritten;
    int  context_local_only;
    int  adapted;
    char english[240];
} ikitaku_prior_revise_t;

int ikitaku_prior_revise(const char* upper, int proven_wrong,
                         const char* context, int new_conf,
                         ikitaku_prior_revise_t* out);

/* Truth maintenance: contradicting ontologies keep weights, not a binary wipe.
   Hidden covar (time, context) attached. Downward prior_revise on the axiom. */
typedef struct {
    char relation[32];
    char old_val[32];
    char new_val[32];
    char covar[24];
    int  w_old, w_new;     /* 0..100, not 0/1 logic */
    int  contradicted;
    int  revised_down;
    char english[240];
} ikitaku_tms_t;

int ikitaku_tms_assert(const char* relation, const char* old_val,
                       const char* new_val, const char* covar_hint,
                       ikitaku_tms_t* out);

/* Self-reference: write the ontology table (R5/R4/subject) from iki. */
typedef struct {
    int wrote_r5, wrote_r4, wrote_subj;
    char name[32];
    char english[200];
} ikitaku_onto_write_t;

int ikitaku_ontology_self_write(const char* name, const char* r5,
                                const char* r4, const char* form,
                                ikitaku_onto_write_t* out);
