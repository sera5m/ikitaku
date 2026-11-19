# Ikitaku goals & capability card (2026-08-26)

## 1. Is iki an effective AI?

**Effective as a budgeted hypothesizer / structure reasoner — not as a general agent.**

It reliably: scores competing readings, extracts rules from labeled candidate
families, runs Hegel/Kant/bool16 on charges, gates goals through R9→R6, and
stops on irreducible remainder instead of inventing answers.

It is **not** effective at open-ended world autonomy, English dialogue, or
inventing algebraic forms that are not in its prior candidate set.

Honest rank (from ARCHITECTURE_SMART): budgeted hypothesizer, not an animal.

## 2. What iki *can* do

- Rank ladder R9→R0; platonic R5 verbs; R4 families
- Series fit / rule_scan / prior match (add/mul/pow/square once candidates exist)
- Fuzzy Hegel × 16 Boolean ops; Kant TAS; Lie near–far
- Desire/template on numeric bags (have→want delta)
- Ladder + objective_run (split/assemble/approach under budget)
- Self-improve on methods using the **same** consider pathway
- Info-gap, XOR cascade, remainder (weird irreducible)
- Solver-gap naming; ask/need_context when methods missing
- Permanent subjects when path is not isolated (`subject_teach`)

## 3. What iki *cannot* do

- Derive operators that are not among scored candidates (no free invention of forms)
- Full knight/knave or multi-door isolation without enough bits / world enum
- English NL in or out (deliberately outside)
- Permanent learn **lock** without human/plugin
- Treat “bag of same function, many (x,y)” unless that structure is submitted
- Open-ended AGI or self-as-person modeling

## 4. Later goal — fully autonomous problem solving

Iki should reason on its own and **ask for clarification or help** when it is
very unsure and budget/tokens run out (`need_context`, solver-gap iterate).

**Note only for now — implement later.** Do not expand into open AGI.

## 5. Active build order (this pass)

1. Wire **desire → ladder → objective_run** (have/want delta becomes R8/R7/R6)
2. Close **self-mod**: invent → optional persist → residual re-measure on same bag
3. Light **method-from-context**: residual / pure-impure → thought-plan lead
4. Problem-solving batteries become **glue checks**, not hole discovery

Focus: goal-oriented spine — not more small-list math derivation tests.

## 6. Next for the spine (active)

Ladder **self-improvement into thinking**, not only as a side residual drop:

1. Pass 1: method-from-context → ladder → objective_run
2. If residual still hot or objective incomplete → self_mod_closed
3. Adopt `best_method` as thinking lead
4. Pass 2: **re-ladder** + optional second objective_run at reduced residual

Goal system stays R9→R8→R7→R6; self-mod is how R6 revises its own strategy bag.

## 7. Later — attribute change transforms

Iki should treat **attribute change** as a first-class transform family, e.g.:

- int → float (widen representation)
- float → int (quantize / bound)
- bool ↔ charge (±1)
- label → numeric rank

Capability is likely already latent via map/bound/prior candidates, but **has not been demonstrated** as an explicit “attribute transform” reading.

**Address later.** Do not block goal-spine work on this.

## 8. Circle metacog (2026-08-26)

Per super-loop: if logic fingerprint matches prior loop **and residual did not
improve**, macro_conf drops. Prevents extended identical reasoning.

Acts (`ikitaku_circle_observe` → spine):
- **warn** — repeated without closing gap
- **self-improve** — meta-metacog: better way to think this over
- **seek-priors** — consider how to find missing priors
- **halt-missing-priors** — stop spinning; understanding incomplete

Wired into `ikitaku_goal_spine` after each think pass.

## 9. Thought-loop ≈ while-loop (2026-08-26)

Platonic parent: **R5 `repeat` / R4 `cycle`** (return to same state class along an axis).

| Form | Context |
|------|---------|
| `while-axis` | control: condition → body → retest |
| `thought-loop` | metacog: re-evaluate methods without residual drop |
| `circular-eval` (R4 under theorize) | same fingerprint, no progress |

Analogy: `thought-loop : cycle :: while-loop : cycle` (sim high).
`ikitaku_circle_map_concept` deposits this when streak ≥ 1; spine surfaces the line.

## 10. Thought-state metacog (2026-08-26) — the missing piece

Track **thinking as data**, not only execute it.

| Piece | Role |
|-------|------|
| **Micro** (`ikitaku_trace_push`) | each pass: lead, form, residual, conf, R7/R6, outcome |
| **Macro** (`ikitaku_trace_macro`) | Δ residual, stall vs progress, pattern cycle/cascade/flat/mixed, macro_conf, **goal_rel** |
| **Layer drop** (`ikitaku_layer_drop`) | any form, from layer N down K steps → broader pattern (generic, not per-case) |
| **Reflect** (`ikitaku_reflect_chain`) | after fail/succeed/end: micro→macro→goal + drop; cycle → thought-loop concept |

Spine records micro each pass and reflects at end. This is self-state tracking so iki can think about thinking after the chain, not only while running policy counters.

## 11. Bidirectional layer flow (metacog abstraction)

Why the old heuristic drop was weak for metacog: it only renamed downward with
a fixed table and never fed **subjects / thought-chains** as bags to abstract.

Now:

| Direction | Meaning | API |
|-----------|---------|-----|
| **DOWN** | derivative / abstract / macro pattern | `ikitaku_layer_flow(..., FLOW_DOWN)` / `abstract_subject` |
| **UP** | anti-derivative / concrete / micro | `FLOW_UP` / `concretize_form` |

Reflect abstracts the chain pattern DOWN and records platonic `abstract_r5`.
Same operator works on any form or subject, not only circle.

## 12. Expression bags F(x)+c (2026-08-26)

Theory check: `ikitaku/EXPR_BAG_THEORY.md`.

- **F** form tip, **x** data, **c** context
- **OPEN** = derive into interior (micro focus)
- **CLOSE/wrap** = anti-derive, treat nest as unit (macro focus)
- **Subject** = bag that may hold verb + descriptor
- Nested maps reasoned as bags, not only single-layer names

## 13. Bag slots + tree map recursion (2026-08-26)

| Axis | Meaning | API |
|------|---------|-----|
| **Horizontal** | peer slots in one bag; unity via Lie | `expr_add_slot`, `expr_slot_unity`, `expr_map_horizontal` |
| **Vertical** | parent↔child derive/anti-derive | `expr_map_vertical` |
| **Tree** | both, bounded DFS | `expr_map_tree` |

Subject/expression bags hold multiple F(x)+c slots; recursion walks depth and peers.

## 14. Abstraction / un-abstraction patterning (2026-08-26)

Unified path on expression bags (not only name walks):

| Step | Direction | Does |
|------|-----------|------|
| **abstract** | derive / DOWN | open nest, remember F(x)+c, flow F → platonic |
| **unabstract** | anti-derive / UP | close unit, keep/recompose +c, flow toward micro, expand props |
| **pattern_run** | both | classify leaf/nested/cycle, full round-trip |

API: `ikitaku_pattern_abstract`, `_unabstract`, `_run` → `ikitaku_abs_pattern_t`.

## 15. Spine glue (2026-08-26)

- **Control stack** (R9→R7 ladder, R6_ready, objective_run): unchanged role.
- **Goal determination**: after ladder, build R5 expr `want(have)+c[residual|lead]` → `pattern_run` → `abstract_r5` + pattern tag; may bias lead (cycle→hegelian).
- **Self-mod substance**: method as bag `F=lead(x=want)+c[...]` + pattern, not name-only.

## 16. Goal-delta endpoints (2026-08-26)

Desire → delta bag → segment have (leaf windows) → abstraction tips → candidates:

| Kind | Source |
|------|--------|
| direct | want as endpoint |
| linear | have + mean(delta) |
| seg-slope | last/mid segment leaf slopes |
| hole-fill | extrapolate flat delta positions |
| phase | periodic continue |
| abstract | macro-blended want via layer_flow |

Top ≤10 by parametric `score_pct` (filter &lt;25% unless few remain).
API: `ikitaku_goal_delta_endpoints`, `ikitaku_goal_delta_from_desire`.

## 17. Fuzzy delta + layer confidence (2026-08-26)

- **Fuzzy on delta:** near / mid / far (relative magnitude), stable / noisy (variance).
- Endpoint scores blended by fuzzy weights (not hard thresholds only).
- Per-endpoint **layer_conf** + **layer_meta_conf**; aggregate on report.
- **unsure_layer / escalate_metacog** when meta low or layer scores disagree →
  note: examine in metacog / macro-confidence.
