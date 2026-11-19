#pragma once
/* PURPOSE: session + hypos + F⊗F. WHY: every reading is a scored hypothesis,
   not a costume name. Dialectic/synth live here so later layers have events. */
/* segmented from ikitaku.hpp — ikitaku_core.hpp */
#define IKITAKU_MAX_HYPOS     32
#define IKITAKU_MAX_LOG       4096
#define IKITAKU_NOTE_LEN      120
#define IKITAKU_LEARNED_MAX   16
#define IKITAKU_LEARNED_PATH  "rs_vm/ikitaku_learned.cat"

typedef enum {
    IKITAKU_THINK_ABDUCTIVE  = 0,
    IKITAKU_THINK_DEDUCTIVE  = 1,
    IKITAKU_THINK_INDUCTIVE  = 2,
    IKITAKU_THINK_ANALOGICAL = 3,
    IKITAKU_THINK_SEARCH     = 4,
    IKITAKU_THINK_SYNTHESIS  = 5,  // Hegelian merge of two F's
    IKITAKU_THINK_DIALECTIC  = 6   // thesis/antithesis resolve
} ikitaku_think_t;

// Cost model: budget = attention_desire; simple cheap, novel/synthesis expensive
typedef struct {
    int  attention_desire;      // total cost budget (default 24)
    int  allow_infeasible;      // stretch hypos
    int  novelty_focus_boost;   // cost multiplier when novel
    int  min_accept_conf;
    int  min_novel_conf;
    int  cost_simple;           // cost of high-conf catalog hypo (default 1)
    int  cost_stretch;          // stretch branch (default 2)
    int  cost_novel;            // novel detection (default 4)
    int  cost_synthesize;       // F⊗F merge attempt (default 3)
    int  cost_dialectic;        // thesis/antithesis step (default 2)
    int  exceptional_threshold; // conf gap that justifies overspend (default 25)
    int  enable_learn;          // persist novel stubs to learned catalog
    int  enable_synthesis;      // allow F⊗F merges
} ikitaku_config_t;

void ikitaku_config_default(ikitaku_config_t* c);

typedef struct {
    char     form_id[RSVM_KIND_LEN];
    char     purpose[RSVM_KIND_LEN];
    char     note[IKITAKU_NOTE_LEN];
    int      confidence;
    int      feasible;
    int      cost;                 // attention spent to produce this
    int      score;                // conf - cost_penalty (ranking)
    uint8_t  method;
    uint8_t  novel;
    uint8_t  rewrite_ok;
    uint8_t  synthesized;
    uint8_t  ambiguous;
    uint8_t  micro_schema;
    char     purpose_alt[RSVM_KIND_LEN]; // parallel form (second reading)
    int      parallel_conf;             // conf of alternate reading
    uint16_t span_start, span_end;
    int16_t  structure_hit;
    int16_t  parent_a, parent_b;   // hypo indices for synthesis
} ikitaku_hypo_t;

typedef struct {
    ikitaku_hypo_t hypos[IKITAKU_MAX_HYPOS];
    int            hypo_count;
    int            accepted;
    int            novel_count;
    int            cost_used;      // total attention spent
    int            synth_count;
    int            learned_added;
    char           log[IKITAKU_MAX_LOG];
} ikitaku_session_t;

// Learned catalog entry (persisted novel / synthesis stubs)
typedef struct {
    char form_id[RSVM_KIND_LEN];
    char purpose[RSVM_KIND_LEN];
    char note[IKITAKU_NOTE_LEN];
    int  times_seen;
} ikitaku_learned_t;

int  ikitaku_learned_load(ikitaku_learned_t* out, int max_out);
int  ikitaku_learned_save_append(const ikitaku_learned_t* e);

int ikitaku_analyze(const rsvm_t* vm, const ikitaku_config_t* cfg,
                    ikitaku_session_t* out);

int ikitaku_specialize(const rsvm_t* vm, const char* purpose_F,
                       uint16_t start, uint16_t end,
                       ikitaku_hypo_t* out);

// Hegelian: thesis + antithesis → synthesis hypo (or reject)
int ikitaku_dialectic(const ikitaku_hypo_t* thesis,
                      const ikitaku_hypo_t* antithesis,
                      ikitaku_hypo_t* synthesis_out);
/* Same collide, but mutex-hunt scores both sides first. */
struct ikitaku_hunt_t;
int ikitaku_dialectic_hunt(const ikitaku_hypo_t* thesis,
                           const ikitaku_hypo_t* antithesis,
                           ikitaku_hypo_t* synthesis_out,
                           struct ikitaku_hunt_t* hunt);

// Merge two platonic purposes into a composite form when attributes allow
int ikitaku_synthesize_F(const char* Fa, const char* Fb,
                         const rsvm_t* vm, uint16_t start, uint16_t end,
                         ikitaku_hypo_t* out);


// Promotion candidate (must precede cluster_promote)
typedef struct {
    char form_id[RSVM_KIND_LEN];
    char purpose[RSVM_KIND_LEN];
    char note[IKITAKU_NOTE_LEN];
    int  conf_low;
    int  conf_high;
    int  evidence_n;
    int  evidence_idx[8];
} ikitaku_promotion_t;


/* Hegel recursion: S vs anti-S. Reason on the product of reasoning.
   Depth-bounded. Synthesis becomes next thesis; anti-synthesis is invert/compose. */
#define IKITAKU_HEGEL_REC_MAX  4
typedef struct {
    ikitaku_hypo_t step[IKITAKU_HEGEL_REC_MAX];
    int n;
    int held;            /* last synthesis survived anti-S */
    int flipped;         /* anti-S won a round */
    int depth_used;
    int conf_in, conf_out;
    char english[220];
} ikitaku_hegel_rec_t;

int ikitaku_anti_synthesis(const ikitaku_hypo_t* syn, ikitaku_hypo_t* anti_out);
int ikitaku_dialectic_recurse(const ikitaku_hypo_t* thesis,
                              const ikitaku_hypo_t* antithesis,
                              int max_depth,
                              ikitaku_hegel_rec_t* out);

/* Reasoning is not special: same R5 verbs on any bag kind. */
typedef enum {
    IKITAKU_BAG_DATA = 0,
    IKITAKU_BAG_METHOD = 1,
    IKITAKU_BAG_SYNTHESIS = 2
} ikitaku_bag_kind_t;

#define IKITAKU_XFORM_MAX  8
typedef struct {
    char verb[16];         /* compare|split|join|invert|bound|repeat|reduce|generate */
    char on[24];
    int  conf;
} ikitaku_xform_step_t;

typedef struct {
    uint8_t kind;          /* ikitaku_bag_kind_t */
    ikitaku_xform_step_t step[IKITAKU_XFORM_MAX];
    int n;
    ikitaku_hypo_t product;
    ikitaku_hegel_rec_t hegel;
    int exempted;          /* must stay 0 — reasoning gets no skip */
    char english[240];
} ikitaku_reason_pipe_t;

int ikitaku_reason_pipeline(const ikitaku_hypo_t* a, const ikitaku_hypo_t* b,
                            int kind, int max_hegel_depth,
                            ikitaku_reason_pipe_t* out);
