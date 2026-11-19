#pragma once
/* PURPOSE: associate-evaluate-consider. WHY: co-occurrence ≠ cause.
   Jaccard + cosine + affinity is the sparse "unity" check other layers audit against. */
/* segmented from ikitaku.hpp — ikitaku_assoc.hpp */
// ---- Association model (LLM-like structure) ----
// Forms as vectors; association via cosine + learned co-occurrence affinity.
#define IKITAKU_ASSOC_DIM    32
#define IKITAKU_ASSOC_VOCAB  64
#define IKITAKU_AFF_N        IKITAKU_ASSOC_VOCAB
#define IKITAKU_AFF_HEBB_CAP 64   /* bound Hebbian updates per observe */

typedef struct {
    float vec[IKITAKU_ASSOC_DIM];  // attribute + purpose embedding
    char  key[RSVM_KIND_LEN];      // form_id or purpose
} ikitaku_embed_t;

void ikitaku_embed_hypo(const ikitaku_hypo_t* h, ikitaku_embed_t* out);
int ikitaku_cosine_sim(const ikitaku_embed_t* a, const ikitaku_embed_t* b);
int ikitaku_associate(const ikitaku_hypo_t* a, const ikitaku_hypo_t* b,
                      char* note_buf, int note_len);
struct ikitaku_hunt_t;
int ikitaku_associate_hunt(const ikitaku_hypo_t* goal,
                           const ikitaku_hypo_t* avoid,
                           struct ikitaku_hunt_t* hunt,
                           char* note_buf, int note_len);

float ikitaku_affinity(const char* Fa, const char* Fb);
void ikitaku_affinity_observe(const ikitaku_session_t* s);
int ikitaku_affinity_save(const char* path);
int ikitaku_affinity_load(const char* path);

#define IKITAKU_MAX_GAPS  8
typedef struct {
    int  hypo_idx;
    char missing[64];
    char why[128];
    int  severity;
} ikitaku_gap_t;

typedef struct {
    ikitaku_gap_t gaps[IKITAKU_MAX_GAPS];
    int           gap_n;
    int           joint_severity;
    char          consider_note[192];
} ikitaku_gap_report_t;

int ikitaku_find_gaps(const rsvm_t* vm, const ikitaku_session_t* s,
                      ikitaku_gap_report_t* out);
int ikitaku_consider_gaps(const ikitaku_session_t* s,
                          const ikitaku_gap_report_t* gaps,
                          char* out_note, int note_len);

typedef enum {
    IKITAKU_AEC_OK = 0,
    IKITAKU_AEC_NO_CONTEXT,
    IKITAKU_AEC_WEAK_ASSOC,
    IKITAKU_AEC_PARTIAL_F,
    IKITAKU_AEC_MULTI_OK,
    IKITAKU_AEC_ALL_FAIL
} ikitaku_aec_fail_t;

typedef enum {
    IKITAKU_POLE_NEAR = 0,
    IKITAKU_POLE_MID,
    IKITAKU_POLE_FAR
} ikitaku_pole_t;

typedef struct {
    int assoc;
    int delta;
    uint8_t pole;
    int mid;
    int shared_avg;
    int educated;
    int mid_is_fallacy;
    int invert_recovers;
    int recombine_ok;
    char english[200];
} ikitaku_assoc_delta_t;

int ikitaku_assoc_poles(int assoc_0_100, int shared_j,
                        int recombine_possible_conf,
                        ikitaku_assoc_delta_t* out);

int ikitaku_english_report(const ikitaku_session_t* s,
                           const ikitaku_cluster_report_t* cr,
                           const ikitaku_gap_report_t* gaps,
                           char* buf, int buf_len);


/* Vertical associate: weak Jaccard across abstraction layers as deliberate tactic
   (not only flat co-occurrence). Aligns with bag open=derive(down) / close=anti-derive(up). */
typedef enum {
    IKITAKU_ASSOC_HORIZ = 0,
    IKITAKU_ASSOC_VERT  = 1,
    IKITAKU_ASSOC_MIXED = 2
} ikitaku_assoc_axis_kind_t;

typedef struct {
    int kind;              /* HORIZ | VERT | MIXED */
    int assoc;             /* 0..100 combined score */
    int horiz;             /* horizontal (same-layer) component */
    int vert;              /* vertical (cross-layer) component */
    int layer_a, layer_b;
    int layer_dist;        /* |a-b| */
    int tactic;            /* IKITAKU_FLOW_DOWN, UP, or -1 same */
    int weak_jaccard;      /* 0..100 diluted by distance */
    int shared_abstract;   /* 1 if layer_flow tips meet */
    char tip_a[24];
    char tip_b[24];
    char english[240];
} ikitaku_assoc_axis_t;

/* Infer default R-layer for a form/purpose name (R5≈5 platonic, R4≈4 family, …). */
int ikitaku_form_layer_guess(const char* form);

/* Explicit vertical/horizontal axis association. */
int ikitaku_associate_axis(const char* form_a, int layer_a,
                           const char* form_b, int layer_b,
                           ikitaku_assoc_axis_t* out);

/* Convenience: hypos with optional explicit layers (-1 = guess). */
int ikitaku_associate_vertical(const ikitaku_hypo_t* a, int layer_a,
                               const ikitaku_hypo_t* b, int layer_b,
                               ikitaku_assoc_axis_t* out);

/* Bag tactic: deliberate vertical move open(down)/close(up) between two bag forms. */
int ikitaku_assoc_bag_tactic(const char* outer_form, int outer_layer,
                             const char* inner_form, int inner_layer,
                             int open_not_close,
                             ikitaku_assoc_axis_t* out);
