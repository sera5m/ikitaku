#pragma once
#define IKITAKU_MAX_HYPOS 32
#define IKITAKU_NOTE_LEN 120
typedef struct {
  int attention_desire;
  int allow_infeasible;
  int novelty_focus_boost;
  int min_accept_conf;
  int min_novel_conf;
  int cost_simple, cost_stretch, cost_novel, cost_synthesize, cost_dialectic;
  int exceptional_threshold, enable_learn, enable_synthesis;
} ikitaku_config_t;
void ikitaku_config_default(ikitaku_config_t* c);
