# Ikitaku objectives (2026-08-25)

Status: `[x]` practiced + tested this pass. `[~]` partial. `[ ]` still out.

## From the unfinished inventory
- [x] Merge R6 planners (`thought_plan` micros + `strategy_pick` macros) into `r6_compose`
- [x] R6 can call **self** and **meta** (bounded uint8 depth)
- [x] R7: is this the right **task** for the objective
- [x] Meta-confidence on layer-sure (no longer STUB)
- [x] Token-merge of two platonic F → associated **lower** layer (antiderivative merge → R3)
- [x] Kant categories table (including movement → repeat)
- [x] Fuzzy thesis–antithesis–synthesis as Kant **sub-layer**
- [ ] Origin query / English NL (still stub — English stays outside)
- [ ] Webhook NL in
- [ ] Permanent learn lock without human
- [~] R3 species/genus thickness (R3 store live; catalogs still thin)
- [x] Lie algebra beyond commutator (bracket as Kant unity / Hegel leftover; scan joints)
- [x] Goal extract + compose (delta have→want → verb chain, lie-checked)
- [~] noisy-sin fit vs stack (unchanged this pass)

## New (this ask)
- [x] Write this checklist
- [x] Strategy via **method micro-composition** (synthetic a priori; ↑ antiderivation of components)
- [x] Thinking-about-thinking stays **R6**
- [x] R3 **wide**: weight+ptr into 32-bit R4 id space; file slab (no extra lib)
- [x] R9 unknown remainder
- [x] R8 is the objective **useful**
- [x] R7 right task for that objective
- [x] Recursive objective-solve loop (budgeted, existing superloop shape)
- [x] Genus = R3, species ≈ R2 (notes corrected)
- [x] Basic tests
- [x] 16 boolean ops + `decision|boolean-tt`
- [x] Info-gap / XOR-lock / nested cascade
- [x] Solver-gap R6 vs R7 vs R9
- [x] Ladder plan R9→R8→R7→R6 (two-goal conflict blocks R6)
- [x] Dialectic names `goal_conflict` (seek vs join)
- [x] 2x2 bimatrix (dominant / Nash / pareto)


## Deliberately not
- English parser inside iki
- Open-ended AGI
- Mimicry as construction

## Goal spine (2026-08-26)
- [x] GOALS.md capability card (effective? can/cannot; autonomous ask-later)
- [x] `ikitaku_method_from_context` (residual/pure → lead)
- [x] `ikitaku_goal_spine` / `_from_desire` (desire → ladder → objective_run → self-mod)
- [x] `ikitaku_self_mod_closed` (improve → re-measure residual)
- [ ] Fully autonomous problem solving + ask when budget dies (noted only)
- [x] Spine pass-2: self-mod feeds method lead → re-ladder thinking
- [ ] Attribute change transforms (int↔float etc.) — noted later only


## Constraints, make-path, symbolic delta (2026-08-26)

Theory:
- Constraints are **not** the antithesis itself; they are **second-order +c** of the antithesis
  (antithesis context for *execution*).
- Tools / known methods are the **inverse of constraint**; micro costume/indirection may be
  required so macro effect holds without violating +c.
- Solution-method synthesis = **fuzzy Hegel** + memory of prior paths to try first.
- Constraints **shape the form** of the solution (not only residual size).
- Identify the problem (detail extract) **before** solve — symbolic delta, not only numeric.

### Forgetting list (from make vs peel test) — TODO
- [x] Wire **desire / top endpoint → MAKE** work bag (`desire_make`, `endpoint_make`, spine_from_desire)
- [x] Thicken `synthesize_F` rule table + generic antiderivative merge
- [x] Dialectic hypos structured (`hypo_fill` + synthesize_F fallback)
- [x] R5 **generate** method_pick when want=generate/create
- [x] Auto path: `endpoint_make` → desire_make under pos/neg

### Constraint / method inverse
- [x] Tag constraint as +c antithesis-context on expr bags
- [x] Tag method as inverse-of-constraint (costume/micro ok)
- [x] Symbolic delta extract (problem ID before solve)
- [x] Memory-ranked path via strategy + xp in construct (lived-ok path)

## Construction under pos/neg constraints (2026-08-26)

- **Pos** constraints: must-hold (+c inclusion / thesis pressure on method set)
- **Neg** constraints: must-not (antithesis context; forbid paths)
- Solution space **narrows** as constraints established (`space_before→after`, `narrow_pct`)
- **Construct** = inverse of symbolic extraction: known micro methods → ordered assemble
- Earlier analog: `objective_split` / `assemble` / `goal_approach` curvature
- API: `ikitaku_construct_*`

## Construction: algorithm vs experience (2026-08-26)

Both:
- **Algorithm incomplete:** construct was catalog filter + assemble without deep metacog loop
  (pattern_run / layer conf / reflect) on the built chain.
- **Inexperience:** `xp` cases thin → meta note literally "L4 without L3 cases is inexperience,
  not a missing R6." Construct now reports `INEXPERIENCED` and drops conf when xp=0.

Bottom-up needs **lived micro patterns** (xp_record outcomes). Upward needs metacog on the
assembled chain. Neither alone is enough.

## Implemented: healthy solve stack (2026-08-26)

Order (deps first):
1. [x] Named cascade/collapse dual — `ikitaku_solve_mode_pick`
2. [x] Construct metacog + xp fuel — `ikitaku_construct_metacog`
3. [x] Thicken `synthesize_F` (pairs + generic antiderivative merge)
4. [x] Desire → MAKE under constraints — `ikitaku_desire_make`
5. [x] `goal_spine_from_desire` calls MAKE then control spine

Policy: Occam COLLAPSE under calm residual; CASCADE under desperation/unsure layer;
construct narrowing collapses method set before build; alternate in the mid band.

## Continuation pass (2026-08-26 pm)
- [x] `objective_run` mode-aware (COLLAPSE Occam vs CASCADE up-ladder per tick)
- [x] `ikitaku_hypo_fill` + dialectic structured non-empty synthesis
- [x] `ikitaku_endpoint_make` top goal-delta endpoint → MAKE

## Vertical association (2026-08-26)

- **Horizontal**: same-layer affinity (moderate–weak co-occurrence)
- **Vertical**: deliberate cross-layer link via weak Jaccard on abstract tips
  (`layer_flow` DOWN), diluted by `|layer_a - layer_b|`
- **Bag tactic**: OPEN = extract/derive down; CLOSE = build/anti-derive up
- API: `form_layer_guess`, `associate_axis`, `associate_vertical`, `assoc_bag_tactic`

## Novel strategy scoring (2026-08-26)

- Basial R6 (scientific/hegelian/occam/…) = `locked=1`, **not scored**
- Developed strategies = `novel=1`, score from trials/wins/drop_sum
- API: `strategy_score`, `strategy_rank_novel`, `strategy_pick_novel`, `strategy_save`
- self_improve calls `strategy_score` on invent/use residual drop

## Metaphor/analogy → strategy (2026-08-26)

Strategy: extract first, then map *shape* to R6 — do not invent English.

1. Attr-bag metaphor (partial F overlap / Jaccard)
2. Analogy A:parent :: B:parent (Lie + affinity)
3. Parent climb (`_x_` / R5) + vertical assoc
4. Shape → lead: cycle→iterative, poles→hegelian, high-j→occam,
   climb/weak→origin, else compose `A_x_B` (novel only)
5. Persist+score novel strategies only (basial locked)
6. `cluster_gap_hint` now carries `strategy` / `origin_lead` / `meta_jaccard`

API: `ikitaku_form_attrbag`, `ikitaku_metaphor_plan`


## Reason-as-transform (2026-08-28)

- [x] Archive `archive/rs_vm_pre_reason_as_transform_2026-08-28.zip`
- [x] `ikitaku_reason_pipeline`: compare→split→join|invert→bound→repeat(S vs anti-S)
- [x] Same verbs for DATA / METHOD / SYNTHESIS; `exempted` must stay 0
- [x] `think_about_thinking` runs pipeline on method bags
- Hegel recurse already tests synthesis against anti-S

- [x] Layer-shift ANY bag: `reason_shift_any` / `reason_shift_hypo` DOWN+UP
- [x] Methods (occam/hegelian) abstract to theorize; thought-loop/while to repeat
- [x] Synthesis product and think_about_thinking also shift


- [x] know-condition ~ bound: shared attrs bound/theorize/condition/evaluate, Jaccard 50, strat=occam hold
- [x] theorize parent taught; teach_delta works
- [x] Stack test: method pipe, shift, objective_run solved, self_mod 60→10, place evaluator+superset


## Self-mod under ignorance battery (2026-08-28)
- [x] 10/10: name gap, iterate, invent/switch method, residual drop, novel score, basial lock, persist fringe, think pipe, incomplete priori, spine after mod
- Caveat: invent often selects hegelian (basial) rather than minting a new form; residual drop is internal score


## Approach-shift (2026-08-28)
- [x] Failed leftover = antithesis (`anti_synthesis`)
- [x] Inhibit last lead on loss; lazy next vs anti; recomb if confused
- [x] Novelty-seek when desperation high; Occam when calm or surprise+
- [x] Parallel Hegel recurse on last vs next; DOWN+UP layer walk
- [x] Wired into self_improve (no longer Hegel-only under ignorance)
- [x] 14/14 approach battery


- [x] Live hooks: thought_plan, method_hegel_pick, circle_observe, solver_iterate, upward_cascade, self_improve
- [x] Utilization test 6/6: plan→origin, pick→origin, circle lead=origin_x_bound, iterate chain, objective solved with origin_x_bound

- [x] patience_declare_stuck + understand_new_abilities; 9/9
- [x] prior_revise: upper wrong walks parent-pointers; context-local does not wipe parent; 10/10 meta-metacog
- [x] TMS weighted (keep both worlds + covar time); ontology_self_write R5/R4/subject; 10/10
- no delete from iki_home yet; alarms/wake later
- values.cat locked base life/learn/tools/novelty/threshold/not-death; feel pain=anti-joy; world universe bag; death_possible=0
