#pragma once
/* segmented from ikitaku.hpp — ikitaku_series.hpp */
// ---- Data-array pattern analysis (abstract, not only code IR) ----
#define IKITAKU_SERIES_MAX  256
typedef enum {
    IKITAKU_DATA_I32 = 0,
    IKITAKU_DATA_F32 = 1,
    IKITAKU_DATA_BOOL = 2
} ikitaku_data_kind_t;

typedef struct {
    ikitaku_data_kind_t kind;
    int    n;
    double values[IKITAKU_SERIES_MAX]; // unified view
} ikitaku_series_t;

typedef enum {
    IKITAKU_PAT_UNKNOWN = 0,
    IKITAKU_PAT_CONSTANT,
    IKITAKU_PAT_LINEAR,
    IKITAKU_PAT_PERIODIC,   // "walking in a circle" — returns toward start
    IKITAKU_PAT_MONOTONE,   // "walking straight"
    IKITAKU_PAT_OSCILLATE,  // forward/back micro under loop macro
    IKITAKU_PAT_IRREGULAR,
    IKITAKU_PAT_STEP
} ikitaku_pattern_t;

typedef struct {
    ikitaku_pattern_t primary;
    ikitaku_pattern_t parallel;     // second simultaneous reading
    char   form_r5[32];             // map|reduce|repeat|decision-like
    char   form_r4[32];
    char   english[192];
    int    conf_primary;
    int    conf_parallel;
    int    local_min_n, local_max_n;
    int    local_min_idx[16], local_max_idx[16];
    double slope_est;
    double regularity;              // 0..1
    // micro attention: discrete derivative
    int    deriv_n;
    double deriv[IKITAKU_SERIES_MAX];
    double deriv2[IKITAKU_SERIES_MAX]; // second difference
} ikitaku_series_analysis_t;

int ikitaku_series_from_i32(const int32_t* a, int n, ikitaku_series_t* out);
int ikitaku_series_from_f32(const float* a, int n, ikitaku_series_t* out);
int ikitaku_series_analyze(const ikitaku_series_t* s, ikitaku_series_analysis_t* out);

/* Rule-shaped MAPS — not decisions.
   map-math  = y=f(x)  (no branch)
   map-table = y=table[x] / period / alphabet (still no CFG branch)
   decision  = compare then take a control path */
typedef enum {
    IKITAKU_RULE_NONE = 0,
    IKITAKU_RULE_THRESHOLD,   /* table-like split of values, not an if-goto */
    IKITAKU_RULE_SQUARE,      /* period-k two-level map */
    IKITAKU_RULE_CATEGORICAL, /* small alphabet map */
    IKITAKU_RULE_PAIR,        /* two tracks lockstep */
    IKITAKU_RULE_RUNS         /* piecewise-constant map */
} ikitaku_rule_kind_t;

typedef struct {
    uint8_t kind;
    int     conf;
    int     period;
    int     n_unique;
    int     n_runs;
    char    purpose[24];   /* map-math | map-table | (never decision) */
    char    english[160];
} ikitaku_rule_t;

int ikitaku_rule_scan(const ikitaku_series_t* s, ikitaku_rule_t* out);

/* PURPOSE: event-array → directed rules. WHY: planted A→B is infrequent and
   order-asymmetric; top-ranked pair may only be the setup (0→1), not the plant. */
/* Infrequent asymmetric pairs in an event array → directed rules. */
#define IKITAKU_SEQ_RULES  8
typedef struct {
    int  a, b;            /* token ids */
    int  ab, ba;          /* ordered pair counts */
    int  a_n, b_n;
    int  conf_lo, conf_hi;
    int  infrequent;
    int  asymmetric;
    char species[24];     /* directed-chain | post-hoc | cum-hoc */
    char rule[64];
} ikitaku_seq_rule_t;

typedef struct {
    ikitaku_seq_rule_t r[IKITAKU_SEQ_RULES];
    int n;
    int n_tokens;
    char english[220];
} ikitaku_seq_cause_t;

int ikitaku_seq_cause(const ikitaku_series_t* s, ikitaku_seq_cause_t* out);

// Operations on series (identify|compare|merge|sort-keys)
int ikitaku_series_compare(const ikitaku_series_analysis_t* a,
                           const ikitaku_series_analysis_t* b,
                           char* english, int elen, int* assoc_score);

// ---- L6 metacognition: reasoning models as data ----
typedef enum {
    IKITAKU_MODEL_PLUGIN     = 0,  // catalog match
    IKITAKU_MODEL_ASSOCIATE  = 1,  // Jaccard+cosine+affinity
    IKITAKU_MODEL_DIALECTIC  = 2,
    IKITAKU_MODEL_SYNTH      = 3,
    IKITAKU_MODEL_SERIES     = 4,  // data pattern analysis
    IKITAKU_MODEL_NOVEL      = 5
} ikitaku_reason_model_t;

typedef struct {
    uint8_t model;
    int     conf;
    int     applied;          // 1 if used this session
    int     succeeded;        // 1 if produced accept or promotion
    char    note[96];
} ikitaku_model_trace_t;

typedef struct {
    ikitaku_model_trace_t models[6];
    int  chosen;              // model index selected by confidence cascade
    int  cascade_depth;       // how many models tried
    char english[256];
} ikitaku_l6_t;

// Cascade: try highest-conf model for this data; on fail, next; then novel
int ikitaku_l6_cascade(const ikitaku_session_t* s,
                       const ikitaku_series_analysis_t* series_opt,
                       ikitaku_l6_t* out);



// ---- Cognitive dissonance + fallacies (metacog) ----
typedef enum {
    IKITAKU_FALLACY_NONE = 0,
    IKITAKU_FALLACY_FALSE_DICHOTOMY,   // forced either/or on parallel forms
    IKITAKU_FALLACY_CIRCULAR,          // conclusion restates premise
    IKITAKU_FALLACY_HASTY_GENERALIZE,  // one span → global F
    IKITAKU_FALLACY_AFFIRM_CONSEQUENT, // rewrite_ok ⇒ pure (wrong direction)
    IKITAKU_FALLACY_EQUIVOCATION       // same name, different ranks mixed
} ikitaku_fallacy_t;

typedef struct {
    int  hypo_a, hypo_b;       // conflicting pair, or -1
    char claim[96];
    char why_incongruous[128];
    int  severity;             // 0..100
    uint8_t fallacy;           // ikitaku_fallacy_t
    uint8_t deferred;          // save for later review
} ikitaku_dissonance_t;

#define IKITAKU_MAX_DISS  12
typedef struct {
    ikitaku_dissonance_t items[IKITAKU_MAX_DISS];
    int n;
} ikitaku_dissonance_report_t;

int ikitaku_detect_dissonance(const ikitaku_session_t* s,
                              ikitaku_dissonance_report_t* out);

// ---- Function fitting (algebra / precalc / trig-ish) ----
typedef enum {
    IKITAKU_FIT_NONE = 0,
    IKITAKU_FIT_CONST,
    IKITAKU_FIT_LINEAR,      // ax+b
    IKITAKU_FIT_QUAD,        // ax^2+bx+c (rough)
    IKITAKU_FIT_SIN,         // k*sin(wx)
    IKITAKU_FIT_TAN,         // rough tan-like (large deriv spikes)
    IKITAKU_FIT_SIN_TAN,     // composite obscured: sin(tan(x))-like
    IKITAKU_FIT_EXP,         // growth
    IKITAKU_FIT_PERIODIC_UNK
} ikitaku_fit_kind_t;

typedef struct {
    uint8_t kind;
    int     conf;
    double  params[4];       // a,b,c,w etc
    char    formula[64];
    char    english[128];
    int     residual_score;  // lower better (0..100 inverted quality)
} ikitaku_fit_t;

#define IKITAKU_MAX_FITS  8
typedef struct {
    ikitaku_fit_t fits[IKITAKU_MAX_FITS];
    int n;
    int best;
} ikitaku_fit_report_t;

int ikitaku_series_fit(const ikitaku_series_t* s, ikitaku_fit_report_t* out);

/* Algebra concept map + factoring (GCF / grouping / quadratic). */
typedef struct {
    char r5[24], r4[24], r3[24], r2[24], r1[24], r0[48];
    char factored[80];
    char quad_formula[96];
    int  roots_ok;
    double root0, root1;
    char english[192];
} ikitaku_algebra_map_t;

int ikitaku_algebra_from_quad(double a, double b, double c,
                              ikitaku_algebra_map_t* out);

/* Extreme uncertainty → "probably nonsense" (pure entropy is rare). */
int ikitaku_probably_nonsense(const ikitaku_fit_report_t* fits,
                              const ikitaku_series_analysis_t* an,
                              int desperation_level,
                              char* english, int elen);


/* PURPOSE: auto compare I/O to known magnitude transforms (Kant: case under
   category). WHY: extrapolative modeling needs residual match to priors, not
   host-side exam answers. Self-teach when conf high and novel. */

#define IKITAKU_PRIOR_MAX   8
#define IKITAKU_CHAIN_MAX  16

typedef struct {
    char name[24];       /* unity_mag | disunity_mag | scale_mag | inv_scale | max | min */
    char r5[24];
    char r4[32];
    double predicted;
    double residual;     /* |c - predicted| */
    int  conf;           /* 0..100 */
    int  exact;          /* residual ~ 0 */
} ikitaku_prior_hit_t;

typedef struct {
    ikitaku_prior_hit_t hit[IKITAKU_PRIOR_MAX];
    int n;
    int best;            /* -1 if none usable */
    int ambiguous;       /* >=2 exact hits */
    int none;
    int underdetermined;
    int impossible;
    char best_name[24];
    char english[240];
} ikitaku_prior_report_t;

/* Binary op observation (a,b)->c against magnitude priors. */
int ikitaku_match_priors_binop(double a, double b, double c,
                               ikitaku_prior_report_t* out);

/* Multi-row consensus: same unknown op observed n times. */
int ikitaku_match_priors_table(const double* a, const double* b, const double* c, int n,
                               ikitaku_prior_report_t* out);

/* Unary f(x)->y against simple unaries (sin/cos identity range checks). */
int ikitaku_match_priors_unary(double x, double y,
                               ikitaku_prior_report_t* out);

/* Human-readable reasoning chain (debug only; iki does not "speak"). */
typedef struct {
    char step[IKITAKU_CHAIN_MAX][160];
    int  n;
    char conclusion[200];
} ikitaku_reason_chain_t;

int ikitaku_reason_binop(double a, double b, double c,
                         ikitaku_prior_report_t* pr,
                         ikitaku_reason_chain_t* chain);

int ikitaku_reason_unknown_value(const char* form, double known_a, double known_b,
                                 double result, ikitaku_reason_chain_t* chain);

/* Self-teach: if best exact hit and subject missing attrs, write catalog. */
int ikitaku_self_teach_prior(const ikitaku_prior_report_t* pr, int min_conf);


/* Unary integer → bool/int predicates (parity, mod, sign). */
int ikitaku_match_priors_predicate(const int* xs, const int* ys, int n,
                                   ikitaku_prior_report_t* out);
int ikitaku_reason_predicate(const int* xs, const int* ys, int n,
                             ikitaku_prior_report_t* pr,
                             ikitaku_reason_chain_t* chain);

/* ---- Information theory: bags hold extractable priori the self may lack ----
   Charge: +1 unity (id/truth), -1 disunity (invert/lie), 0 unknown.
   Nested emit is legal: one bag can emit another bag's emit (quote).
   XOR-lock: S = A ⊕ P with A unknown ⇒ P not isolable from one observation. */

typedef struct {
    char name[24];
    int  charge;     /* +1 unity, -1 disunity, 0 unknown */
    int  known;
    char r5[24];
    char r4[32];
} ikitaku_info_bag_t;

typedef struct {
    int self_has;
    int other_has;
    int gap;           /* other holds extractable priori self lacks */
    int reverse_gap;
    int nested_legal;  /* emit-of-emit is a legal probe */
    char topic[32];
    char english[220];
} ikitaku_info_gap_t;

int ikitaku_charge_compose(int a, int b);

/* 16 boolean ops on charge ±1 (unity / disunity).
   Internally bits: +1→1, -1→0. ⊕ = XNOR = multiply on ±1.
   ⊖ = XOR = -A*B. AND/OR/→ use actual Boolean, not a broken multiply rewrite. */
enum {
    IKITAKU_B_FALSE = 0,   /* -1 constant */
    IKITAKU_B_AND   = 1,
    IKITAKU_B_A_NOT_B = 2, /* A ∧ ¬B  converse nonimplication */
    IKITAKU_B_A     = 3,
    IKITAKU_B_NOT_A_B = 4, /* ¬A ∧ B */
    IKITAKU_B_B     = 5,
    IKITAKU_B_XOR   = 6,   /* A ⊖ B */
    IKITAKU_B_OR    = 7,
    IKITAKU_B_NOR   = 8,
    IKITAKU_B_XNOR  = 9,   /* A ⊕ B equivalence */
    IKITAKU_B_NOT_B = 10,
    IKITAKU_B_B_IMP_A = 11, /* B → A */
    IKITAKU_B_NOT_A = 12,
    IKITAKU_B_A_IMP_B = 13, /* A → B */
    IKITAKU_B_NAND  = 14,
    IKITAKU_B_TRUE  = 15    /* +1 constant */
};

int ikitaku_bit_from_charge(int c);
int ikitaku_charge_from_bit(int b);
int ikitaku_bool16(int op, int A, int B); /* charges in/out */
int ikitaku_bool16_named(const char* name, int A, int B);
const char* ikitaku_bool16_name(int op);
const char* ikitaku_bool16_notation(int op);

typedef struct {
    int op;
    int row[4][3]; /* A, B, out */
    char english[200];
} ikitaku_bool_tt_t;

int ikitaku_bool_tt(int op, ikitaku_bool_tt_t* out);

/* 2x2 bimatrix on charge worlds (++ +- -+ --). Not a named game.
   Dominant = same preference for all values of the other. */
typedef struct {
    int payA[4], payB[4];
    int A_pref_Bpos, A_pref_Bneg; /* +1 pick unity, -1 pick disunity, 0 tie */
    int B_pref_Apos, B_pref_Aneg;
    int A_dominant, B_dominant;
    int nash[4];
    int n_nash;
    int joint[4];
    int joint_argmax;
    int pareto_conflict; /* dominant NE joint < max joint */
    char english[240];
} ikitaku_bimatrix_t;

int ikitaku_bimatrix_eval(const int payA[4], const int payB[4],
                          ikitaku_bimatrix_t* out,
                          ikitaku_reason_chain_t* chain);


int ikitaku_info_gap(const ikitaku_info_bag_t* self,
                     const ikitaku_info_bag_t* other,
                     const char* topic,
                     ikitaku_info_gap_t* out);
int ikitaku_xor_lock(int a_known, int p_known,
                     char* english, int elen);

typedef struct {
    int speaker;           /* +1/-1/0 */
    int other;
    int path;              /* +1 freedom / -1 perdition / 0 unknown */
    int xor_agents;        /* 1 = absolute binary opposition */
    int xor_paths;
    int nested;            /* 1 = speaker emits other's map of path */
    int result;            /* utterance charge, 0 if underdetermined */
    int dissolves_speaker;
    int path_relation;     /* -1 ⇒ utterance is always ⊖ path */
    int n_worlds;
    int n_consistent;
    int n_agree_minus_path;
    char english[240];
} ikitaku_cascade_t;

int ikitaku_state_cascade(const ikitaku_cascade_t* in,
                          ikitaku_cascade_t* out,
                          ikitaku_reason_chain_t* chain);

/* Leftover after probes. Isolable iff n_targets <= 2^bits and leftover==1.
   Weird = a method held (e.g. speaker dissolved) but pick still not unique —
   uncertainty nothing in budget can close. */
typedef struct {
    int n_targets;
    int probe_bits;
    int bit_cap;
    int n_worlds;
    int n_consistent;
    int dissolved;
    int isolable;
    int irreducible;
    int weird;
    char english[280];
} ikitaku_remainder_t;

int ikitaku_remainder(int n_targets, int probe_bits,
                      const ikitaku_cascade_t* cas,
                      ikitaku_remainder_t* out);


