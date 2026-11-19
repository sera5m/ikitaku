#pragma once
/* Manual thought input. Same role as the old paste-a-stimulus loop.

   A thought is a C string. Numbers become a series. Words that name
   a control form (if, loop, wait, print, sort, compare) become a
   short instruction image. Both go through the existing reasoner.

   iki_think "1 2 3 5 8"
   iki_think "loop if print"
   echo "who=- vanish" | iki_think
*/

#include "ikitaku_tp_bridge.hpp"
#include "ikitaku_core.hpp"
#include "ikitaku_series.hpp"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IKITAKU_THOUGHT_MAX  2048
#define IKITAKU_THOUGHT_NUMS 256

typedef struct {
    char   text[IKITAKU_THOUGHT_MAX];
    int    n_nums;
    double nums[IKITAKU_THOUGHT_NUMS];
    int    has_loop, has_if, has_wait, has_print, has_sort, has_cmp;
    int    used_series;
    int    used_image;
    ikitaku_session_t session;
    ikitaku_series_analysis_t series;
    char   english[512];
} ikitaku_thought_t;

/* Parse text, fill a synthetic image and/or a series, run analyze.
   Returns 0 on a usable reading, -1 on empty input. */
int ikitaku_thought_in(const char* text, const ikitaku_config_t* cfg,
                       ikitaku_thought_t* out);

/* Write a short human-readable dump of out into buf. */
int ikitaku_thought_dump(const ikitaku_thought_t* t, char* buf, int buflen);

#ifdef __cplusplus
}
#endif
