#pragma once
// L1–L4 structure analysis — separate from optimize (L5–L6 thinking/apply).
// Ranks R0..R5; rules are axiom schema plugins against IR facts.
// Transform attributes (pure/predictable/progress/concurrency) are first-class.
#include "rs_vm.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#define RSVM_STRUCT_MAX_HITS   32
#define RSVM_STRUCT_MAX_RULES  48
#define RSVM_FORM_LEN          96
#define RSVM_KIND_LEN          32

// Abstraction ranks (partial order; climb = abstract, descend = specialize)
typedef enum {
    RSVM_RANK_SURFACE = 0,  // R0 opcodes
    RSVM_RANK_REGION  = 1,  // R1 CFG/region
    RSVM_RANK_MICRO   = 2,  // R2 micro schema
    RSVM_RANK_ALGO    = 3,  // R3 algorithm
    RSVM_RANK_FAMILY  = 4,  // R4 method family
    RSVM_RANK_PURPOSE = 5   // R5 purpose (platonic F)
} rsvm_rank_t;

// Transform attributes — first-class; L5 reads these, does not re-derive
typedef enum {
    RSVM_PRED_PREDICTABLE = 0,  // closed form / fixed bound possible
    RSVM_PRED_PARTIAL     = 1,  // some paths/bounds known
    RSVM_PRED_UNPREDICTABLE = 2 // external / sensor / unknown
} rsvm_predict_t;

typedef enum {
    RSVM_PROG_FINITE     = 0,  // known iteration count
    RSVM_PROG_DATA       = 1,  // data-dependent bound
    RSVM_PROG_EXTERNAL   = 2   // wait / host / sensor
} rsvm_progress_t;

typedef enum {
    RSVM_CONC_SEQUENTIAL = 0,
    RSVM_CONC_ASYNC      = 1,
    RSVM_CONC_CONTENDED  = 2
} rsvm_conc_t;

// One node in the structure tree / one match hit
typedef struct {
    char     kind[RSVM_KIND_LEN];       // e.g. counted_inc, wait_delay
    char     general_form[RSVM_FORM_LEN]; // F or specialized form string
    char     purpose[RSVM_KIND_LEN];    // R5 platonic: repeat|decision|wait|info|…
    char     family[RSVM_KIND_LEN];     // R4
    char     big_o[24];
    int      confidence;                // 0–100
    uint8_t  rank;                      // highest rank filled
    uint8_t  pure;
    uint8_t  micro_schema;              // 1 = R2 micro (sub-association graph)
    uint8_t  predictable;               // rsvm_predict_t
    uint8_t  progress;                  // rsvm_progress_t
    uint8_t  concurrency;               // rsvm_conc_t
    uint16_t region_start;
    uint16_t region_end;
    uint8_t  region_kind;               // RSVM_REG_*
    int16_t  parent_hit;                // index of parent hit, or -1
} rsvm_structure_hit_t;

// Axiom schema plugin: R5-anchored form the analyzer matches against IR
typedef struct {
    const char* id;
    const char* general_form;
    const char* purpose;        // R5 plugin anchor
    const char* family;
    const char* big_o;
    int         min_confidence;
    uint8_t     rank_algo;
    uint8_t     pure_default;
    uint8_t     predictable;    // rsvm_predict_t
    uint8_t     progress;       // rsvm_progress_t
    uint8_t     concurrency;    // rsvm_conc_t
    int (*match)(const rsvm_t* vm, uint16_t start, uint16_t end,
                 rsvm_structure_hit_t* out);
} rsvm_struct_rule_t;

const rsvm_struct_rule_t* rsvm_struct_rules(void);
int                       rsvm_struct_rule_count(void);

int rsvm_analyze_structures(const rsvm_t* vm,
                            rsvm_structure_hit_t* out, int max_out);

int rsvm_structure_report(const rsvm_t* vm, char* buf, int buf_len);

// True if L5 may attempt closed-form rewrite on this hit
int rsvm_hit_rewrite_allowed(const rsvm_structure_hit_t* h);

#ifdef __cplusplus
}
#endif
