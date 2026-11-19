#pragma once
/* Typed cells. Not a homogeneous spike neuron.
   Organelle analog = these kinds. Sentiment is a field, not a cell type. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  IKI_CELL_HYPO = 0,     /* working reading — ikitaku_hypo_t */
  IKI_CELL_ASSOC = 1,    /* affinity / embed — unity check */
  IKI_CELL_CLUSTER = 2,  /* bag of hypos — one object */
  IKI_CELL_CONCEPT = 3,  /* ladder slot — derive / anti-derive */
  IKI_CELL_VERB = 4,     /* R5 catalog row */
  IKI_CELL_FAMILY = 5,   /* R4 method family */
  IKI_CELL_VALUE = 6     /* like/dislike table — valence, not "emotion neuron" */
} iki_cell_kind_t;

typedef enum {
  IKI_POLE_NEAR = 0,
  IKI_POLE_MID = 1,
  IKI_POLE_FAR = 2,
  IKI_POLE_INVERT = 3
} iki_pole_t;

/* Header every typed cell can report. Valence lives here so you do not
   spawn a second "sentiment neuron" species. */
typedef struct {
  uint8_t  kind;
  uint8_t  rank;       /* R0..R9 */
  int16_t  conf;       /* 0..100 */
  int16_t  valence;    /* -100..100 preference, default 0 */
  uint8_t  pole;       /* near/mid/far/invert of last associate */
  uint8_t  locked;
} iki_cell_hdr_t;

#ifdef __cplusplus
}
#endif
