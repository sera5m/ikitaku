#pragma once
/* PURPOSE: ontology + watch + origin + probe + causal audit.
   WHY: R5 verbs are work; R4 is method-family; origin is R6 (B→C, invert A). */
#define IKITAKU_CHAIN_LEN   24
#define IKITAKU_DEPTH_MIN   0
#define IKITAKU_DEPTH_MAX   8   /* uint8 integrate budget bound */

typedef struct {
    int  order;
    char concept[32];
    char bound[32];
    int  conf;
    int  confusion;    /* this cell, not the chain */
    int  layer_sure;
    int  sure_lo, sure_hi;
    int  above_lo, above_hi;
    int  below_lo, below_hi;
    int  meta_lo, meta_hi;
    /* 0 = false/none; 1..IKITAKU_DEPTH_MAX = how far this quality holds */
    uint8_t platonic_depth;
    uint8_t var_depth;
} ikitaku_concept_slot_t;

typedef struct {
    ikitaku_concept_slot_t slot[IKITAKU_CHAIN_LEN];
    int n;
    int dropped;
    int max_drop;
    int macro_conf;
    int macro_confusion;
    uint8_t form_depth;       /* 0 = F platonic; 1..8 demoted toward form */
    uint8_t integrate_depth;  /* remaining recursion budget */
    uint8_t integrate_bound;  /* cap for this chain */
    char english[192];
} ikitaku_concept_chain_t;

int ikitaku_concept_chain_init(const char* F, const char* bound_transform,
                               ikitaku_concept_chain_t* out);

#define IKITAKU_R5_MAX  48
#define IKITAKU_R5_CAT  "/home/workdir/artifacts/rs_vm/ikitaku/ikitaku_r5_verbs.cat"

typedef struct {
    char    verb[24];
    char    form[80];
    uint8_t locked;
    uint8_t r6;
    char    note[96];
} ikitaku_r5_verb_t;

typedef struct {
    ikitaku_r5_verb_t v[IKITAKU_R5_MAX];
    int n;
    int n_locked;
    int n_novel;
} ikitaku_r5_map_t;

int ikitaku_r5_load(ikitaku_r5_map_t* out);
int ikitaku_r5_lookup(const char* verb, ikitaku_r5_verb_t* out);
int ikitaku_r5_propose(const char* verb, const char* form, const char* note);
int ikitaku_r5_is_platonic(const char* verb);
int ikitaku_r5_object_list(char names[][24], int maxn);

#define IKITAKU_R4_MAX  80
#define IKITAKU_R4_CAT  "/home/workdir/artifacts/rs_vm/ikitaku/ikitaku_r4_families.cat"
typedef struct {
    char parent[24];
    char family[32];
    char form[80];
    uint8_t locked;
    char note[64];
} ikitaku_r4_fam_t;

typedef struct {
    ikitaku_r4_fam_t f[IKITAKU_R4_MAX];
    int n;
} ikitaku_r4_map_t;

int ikitaku_r4_load(ikitaku_r4_map_t* out);
int ikitaku_r4_under(const char* parent, char fams[][32], int maxn);
int ikitaku_r4_propose(const char* parent, const char* family, const char* form);

#define IKITAKU_DISCOVERY_ROOT "/home/workdir/artifacts/rs_vm/ikitaku/discovery"
typedef struct {
    char r5[24];
    char r4[32];
    char form[48];
    char alias[32];
    char evidence[96];
    int  conf;
    uint8_t locked;
} ikitaku_discovery_t;

int ikitaku_discovery_propose(const ikitaku_discovery_t* d);
int ikitaku_discovery_load(const char* r5, const char* r4,
                           ikitaku_discovery_t* out, int maxn);

typedef struct {
    char verb[24];
    char r4[32];
    int  hit;
    int  conf;
    char evidence[80];
} ikitaku_r5_id_t;

typedef struct {
    ikitaku_r5_id_t item[24];
    int n;
    int n_hit;
    char english[220];
} ikitaku_r5_id_report_t;

int ikitaku_r5_identify(const rsvm_t* vm,
                        const ikitaku_session_t* sess,
                        const ikitaku_series_t* series,
                        ikitaku_r5_id_report_t* out);

#define IKITAKU_WATCH_SLOTS  32
typedef struct {
    int  curiosity;
    int  mental_ok;
    int  ran;
    int  nslot;
    int32_t before[IKITAKU_WATCH_SLOTS];
    int32_t after[IKITAKU_WATCH_SLOTS];
    int  inversions_before;
    int  inversions_after;
    int  lift_order;
    int  lift_bound;
    int  lift_seek;
    char r4[32];
    char english[220];
} ikitaku_watch_t;

int ikitaku_curiosity(const rsvm_t* vm);
int ikitaku_watch_run(rsvm_t* vm, ikitaku_watch_t* out);
int ikitaku_watch_apply(const ikitaku_watch_t* w, ikitaku_r5_id_report_t* id);
int ikitaku_r4_attach(ikitaku_r5_id_report_t* id, const ikitaku_watch_t* w);
int ikitaku_discovery_consider(const ikitaku_r5_id_report_t* id,
                               const ikitaku_watch_t* w,
                               ikitaku_discovery_t* out);

#define IKITAKU_CAUSE_MAX  6
typedef struct {
    char a[32];
    char b[32];
    char c[32];
    int  ab_lo, ab_hi;
    int  bc_lo, bc_hi;
    int  b_freq;
    int  c_unexpected;
    int  reversed;
    int  tested;
    int  test_ok;
    char rule[96];
    char why[120];
} ikitaku_cause_link_t;

typedef struct {
    ikitaku_cause_link_t link[IKITAKU_CAUSE_MAX];
    int n;
    int reversed_n;
    int confusion;
    int fired;
    int pattern_hits;
    char english[240];
} ikitaku_origin_t;

int ikitaku_origin_trace(const ikitaku_session_t* s,
                         const ikitaku_watch_t* w,
                         const ikitaku_r5_id_report_t* id,
                         ikitaku_origin_t* out);

typedef struct {
    char species[24];
    char hidden_d[32];
    int  commutator;
    int  assoc_ab;
    int  assoc_ba;
    int  cluster_unity;
    int  stand;
    char english[200];
} ikitaku_cause_audit_t;

int ikitaku_cause_audit(const ikitaku_session_t* s,
                        const ikitaku_cause_link_t* link,
                        const ikitaku_cluster_report_t* cr,
                        ikitaku_cause_audit_t* out);

#define IKITAKU_ORIGIN_THRESH  35
#define IKITAKU_ORIGIN_PAT     "/home/workdir/artifacts/rs_vm/ikitaku/discovery/origin/patterns.cat"

#define IKITAKU_PROBE_MAX  48
typedef struct {
    int  surface;
    int  hidden_prior;
    int  complexity;
    int  budget_used;
    int  trials;
    int  depth;
    int  box;
    int  confusion_in;
    int  confusion_out;
    int  period;
    char rule[80];
    char english[220];
} ikitaku_probe_t;

typedef struct {
    int confusion;
    int origin_fired;
    ikitaku_r5_id_report_t id;
    ikitaku_watch_t watch;
    ikitaku_origin_t origin;
    ikitaku_discovery_t discovery;
    ikitaku_probe_t probe;
    char english[280];
} ikitaku_behavior_t;

int ikitaku_complexity_est(const rsvm_t* vm, const ikitaku_session_t* s,
                           ikitaku_probe_t* out);
int ikitaku_probe_box(rsvm_t* vm, const ikitaku_session_t* s,
                      int confusion_in, ikitaku_probe_t* out);
int ikitaku_behavior_tick(rsvm_t* vm, const ikitaku_session_t* s,
                          ikitaku_behavior_t* out);

int ikitaku_concept_drop(ikitaku_concept_chain_t* ch, int order, const char* as_var);
int ikitaku_concept_feed_stack(const ikitaku_concept_chain_t* ch,
                               char names[][32], int max_names);
int ikitaku_layered_reason_ex(const ikitaku_series_t* s,
                              ikitaku_concept_chain_t* chain,
                              ikitaku_stack_report_t* out);

int ikitaku_dropdown_loop(ikitaku_concept_chain_t* ch,
                          const char** candidates, int n_cand,
                          uint8_t depth);
int ikitaku_F_as_form(ikitaku_concept_chain_t* ch, const char* form_name);
int ikitaku_integrate(ikitaku_concept_chain_t* ch, uint8_t depth,
                      const char** candidates, int n_cand);

int ikitaku_layer_sure_eval(ikitaku_concept_chain_t* ch);
int ikitaku_layer_sure_report(const ikitaku_concept_chain_t* ch,
                              char* buf, int buf_len);

/* Facts are subjects to map — not English. A subject is a named bag
   under R5/R4 with a flat attr sheet. Atoms are info|atom, not a new R5. */
#define IKITAKU_SUBJ_MAX   64
#define IKITAKU_SUBJ_ATTR  16
#define IKITAKU_SUBJ_CAT   "/home/workdir/artifacts/rs_vm/ikitaku/ikitaku_subjects.cat"
#define IKITAKU_ATOM_CAT   "/home/workdir/artifacts/rs_vm/ikitaku/ikitaku_atom.cat"
/* Isolated cluster file for tests that must not touch permanent subjects. */
void ikitaku_subj_set_path(const char* path); /* NULL restores default */
const char* ikitaku_subj_path(void);

typedef struct {
    char key[24];
    char val[96];  /* was 32; paths/notes need more */
} ikitaku_fact_t;

typedef struct {
    char name[32];
    char r5[24];
    char r4[32];
    ikitaku_fact_t fact[IKITAKU_SUBJ_ATTR];
    int n;
    int conf_lo, conf_hi;
} ikitaku_subject_t;

typedef struct {
    ikitaku_subject_t s[IKITAKU_SUBJ_MAX];
    int n;
} ikitaku_subject_map_t;

int ikitaku_atom_sheet(char keys[][24], char roles[][32], int maxn);
int ikitaku_subject_load(ikitaku_subject_map_t* out);
int ikitaku_subject_teach(const char* name, const char* r5, const char* r4,
                          const char* attr, const char* val, int conf);
int ikitaku_subject_get(const char* name, ikitaku_subject_t* out);

/* English word = alias. Encyclopedia page = many deltas on one parent.
   Cost is the first node on a branch; siblings are attr-deltas. */
#define IKITAKU_ALIAS_MAX  64
#define IKITAKU_ALIAS_CAT  "/home/workdir/artifacts/rs_vm/ikitaku/ikitaku_alias.cat"
typedef struct {
    char word[32];
    char subject[32];
    char r5[24];
    char r4[32];
    int  conf;
} ikitaku_alias_t;

int ikitaku_alias_bind(const char* word, const char* subject, int conf);
int ikitaku_alias_resolve(const char* word, ikitaku_alias_t* out);
int ikitaku_teach_delta(const char* parent, const char* child,
                        const char* attr, const char* val, int conf,
                        char* english, int elen);
