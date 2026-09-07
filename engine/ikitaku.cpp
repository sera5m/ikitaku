#include "ikitaku_core.hpp"
void ikitaku_config_default(ikitaku_config_t* c) {
  if (!c) return;
  c->attention_desire = 24;
  c->allow_infeasible = 1;
  c->novelty_focus_boost = 2;
  c->min_accept_conf = 70;
  c->min_novel_conf = 50;
  c->cost_simple = 1; c->cost_stretch = 2; c->cost_novel = 4;
  c->cost_synthesize = 3; c->cost_dialectic = 2;
  c->exceptional_threshold = 25;
  c->enable_learn = 1; c->enable_synthesis = 1;
}
