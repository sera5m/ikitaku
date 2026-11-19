#pragma once
/* PURPOSE: group hypos that share purpose/attrs. WHY: one cluster = one object.
   Two events in the same cluster should not "cause" each other (common-cause / unity). */
/* segmented from ikitaku.hpp — ikitaku_cluster.hpp */
// ---- Evidence clustering (priority) ----
#define IKITAKU_MAX_CLUSTERS  12
#define IKITAKU_CLUSTER_MEM   12
#define IKITAKU_DBG_LEN       512

typedef enum {
    IKITAKU_CL_RAW       = 0,  // just grouped
    IKITAKU_CL_COMPARED  = 1,  // Jaccard done
    IKITAKU_CL_DIALECTIC = 2,  // pure/impure resolved inside
    IKITAKU_CL_AMBIGUOUS = 3,  // partial overlap dominant
    IKITAKU_CL_SYNTH     = 4,  // produced F⊗F child
    IKITAKU_CL_PROMOTED  = 5   // R5 promotion candidate emitted
} ikitaku_cluster_state_t;

typedef struct {
    int      id;
    char     purpose[RSVM_KIND_LEN];   // shared R5 if any
    char     shared_attrs[96];         // intersection tags
    char     dissent_attrs[96];        // symmetric difference summary
    int      member_idx[IKITAKU_CLUSTER_MEM];
    int      member_n;
    int      sim_min, sim_max, sim_avg; // pairwise Jaccard among members
    int      conf_low, conf_high;
    uint8_t  state;                    // ikitaku_cluster_state_t
    uint8_t  has_pure;
    uint8_t  has_impure;
    uint8_t  has_synth;
    uint8_t  ambiguous;
    int      dialectic_resolved;
    char     kant_class[24];   /* quantity|quality|relation|modality|schema */
    char     kant_cat[24];     /* unity, negation, cause, ... */
    int      membership;       /* fuzzy: sim_avg under that category */
    int      antinomy;
    char     debug[IKITAKU_DBG_LEN];
} ikitaku_cluster_t;

typedef struct {
    ikitaku_cluster_t clusters[IKITAKU_MAX_CLUSTERS];
    int               cluster_n;
    int               compare_ops;
    int               dialectic_ops;
    int               synth_ops;
    int               promote_ops;
    int               ambiguous_marks;
    char              super_log[IKITAKU_MAX_LOG];
} ikitaku_cluster_report_t;

// Build clusters from session hypos (Jaccard + purpose)
int ikitaku_cluster_build(const ikitaku_session_t* s, int sim_join,
                          ikitaku_cluster_report_t* out);
int ikitaku_kant_cluster_tag(ikitaku_cluster_report_t* cr);

/* Gentner hint on cluster gaps: abnormal Lie gap → analogize simpler (parent).
   High parent-sim = novel species. Low = useless TAS / nonsense. */
typedef struct {
    int  gap_mag, gap_mean, abnormal;
    int  ambiguous, novel, nonsense;
    int  analog_sim;
    char A[24], B[24];
    char parent_a[24], parent_b[24];
    char hint[48];
    char strategy[24];     /* metaphor-plan R6 lead */
    char origin_lead[24];
    int  meta_jaccard;
    char english[220];
} ikitaku_gap_hint_t;

int ikitaku_cluster_gap_hint(const ikitaku_cluster_report_t* cr,
                             ikitaku_gap_hint_t* out);

// Run Hegelian + ambiguous marking inside each cluster (subloop)
int ikitaku_cluster_dialectic(ikitaku_session_t* s, ikitaku_cluster_report_t* cr,
                              const ikitaku_config_t* cfg);

// Promote clusters with enough evidence → R5 plugin candidates
int ikitaku_cluster_promote(const ikitaku_session_t* s,
                            ikitaku_cluster_report_t* cr,
                            int min_members,
                            ikitaku_promotion_t* promos, int max_promos);

// Detailed cluster debug dump
int ikitaku_cluster_report(const ikitaku_cluster_report_t* cr,
                           char* buf, int buf_len);

/* Light linear-ish discriminant: class means in embed space, fuzzy memberships. */
#define IKITAKU_DISC_MAX  8
#define IKITAKU_DISC_DIM  16
typedef struct {
    char  label[RSVM_KIND_LEN];
    float mean[IKITAKU_DISC_DIM];
    int   n;
    float within;     /* mean sq dist to members */
} ikitaku_disc_class_t;

typedef struct {
    ikitaku_disc_class_t cls[IKITAKU_DISC_MAX];
    int n;
    int membership[IKITAKU_MAX_HYPOS][IKITAKU_DISC_MAX]; /* 0..100 fuzzy */
    int primary[IKITAKU_MAX_HYPOS];
    char english[256];
} ikitaku_disc_t;

int ikitaku_discriminant(const ikitaku_session_t* s, ikitaku_disc_t* out);

// ---- Meta-cognitive super/subloop architecture ----
// Superloop: orchestrate scientific method phases
// Subloops:  compare | dialectic | synthesize | cluster | promote
typedef enum {
    IKITAKU_PHASE_OBSERVE   = 0,  // structure hits → hypos
    IKITAKU_PHASE_COMPARE   = 1,  // Jaccard / attribute structures
    IKITAKU_PHASE_CLUSTER   = 2,  // evidence grouping
    IKITAKU_PHASE_DIALECTIC = 3,  // thesis/antithesis inside clusters
    IKITAKU_PHASE_SYNTH     = 4,  // F⊗F
    IKITAKU_PHASE_SPECIALIZE= 5,  // F → f+c context
    IKITAKU_PHASE_PROMOTE   = 6,  // L4 mass → R5
    IKITAKU_PHASE_ACCEPT    = 7   // score + budget gate
} ikitaku_phase_t;

typedef struct {
    uint8_t phase;             // current
    int     phase_cost[8];     // cost spent per phase
    int     phase_hypos[8];    // hypos produced per phase
    int     depth;             // subloop nesting depth
    int     max_depth;         // cap (default 3)
    char    metacog_note[192]; // why superloop advanced / stopped
} ikitaku_metacog_t;

// Full analyze with explicit superloop + cluster report out-params
int ikitaku_analyze_ex(const rsvm_t* vm, const ikitaku_config_t* cfg,
                       ikitaku_session_t* out,
                       ikitaku_cluster_report_t* clusters,
                       ikitaku_metacog_t* meta);


int ikitaku_report(const ikitaku_session_t* s, char* buf, int buf_len);

// Attribute / pattern comparison (glass vs ceramic style)
// Returns 0..100 similarity; fills shared/only_a/only_b note if note_buf provided
int ikitaku_attr_similarity(const rsvm_structure_hit_t* a,
                            const rsvm_structure_hit_t* b,
                            char* note_buf, int note_len);

int ikitaku_hypo_similarity(const ikitaku_hypo_t* a,
                            const ikitaku_hypo_t* b,
                            char* note_buf, int note_len);

// Identity-ish: same purpose + same rewrite/predict family → commutative under merge order
int ikitaku_forms_commute(const char* Fa, const char* Fb);

// Ambiguous: partial overlap — save for later (not accepted, not rejected)
int ikitaku_mark_ambiguous(ikitaku_session_t* s, int hypo_i, const char* reason);

int ikitaku_try_promote(const ikitaku_session_t* s, int min_similar,
                        ikitaku_promotion_t* out);

// Persist promotion candidate (second step toward real plugin)
int ikitaku_promotion_save(const ikitaku_promotion_t* p);




