# ikitaku — practiced theory (2026-08-25)

Older drafts: `artifacts/archive/ikitaku_theory_2026-08-25/`.
This file is what the **code actually does**. Speculative items are marked *experimental*.

Ikitaku is not the VM. It reads `rsvm_t` (ops, slots, heap, host) and writes
hypotheses / cards / discoveries. Vulcan JIT stays in `vm/`.

---

## Rank ladder (locked)

```
R9  unknown     remainder / incomplete priori
R8  useful      is the objective a known verb (useful to try)
R7  right-task  does this bag match that objective
R6  strategy    micro compose + macro; call self/meta
R5  purpose     22 work verbs
R4  family      32-bit id space
R3  genus       wide weight+ptr into R4 (slab)
R2  species     micro schema
R1  region      CFG / purity / def-use
R0  surface     opcodes
```

Do not collapse:

- **map-math** `y=f(x)` — no branch
- **map-table** period / alphabet / labels — still a map
- **decision** — compare then take a *control path*
- **repeat** — advance an axis (iterator or time)

A table is not an if. Create = generate (alias). Compare ≠ decision.

---

## What runs (practiced pipeline)

```
compile Vulcan
  → ikitaku_analyze_ex        hypos, clusters, Hegelian collide, F⊗F
  → r5_identify + r4_attach   verbs + families from ops / watch
  → complexity_est            surface + hidden prior (boxes look cheap)
  → probe_box                 step + host.on_slot STORE trace → increment | cycle-k | opaque
  → watch_run                 start/end + heap bag monotone → lift order/seek
  → confusion                 C unexpected (order/split/bound) bumps it
  → origin_trace              if confusion ≥ 35: B→C, invert A if B frequent
  → cause_audit               R4 species: directed-chain | post-hoc | cum-hoc | common-cause
  → discovery_consider        write discovery/<r5>/<r4>/<form>.form
```

On a **numeric / event array** (no program needed):

```
series_from_i32
  → rule_scan / schema_extract     map-math vs map-table
  → seq_cause                      infrequent asymmetric bigrams → directed rules
```

---

## Theories that survived contact with tests

| theory | practiced result |
|--------|------------------|
| f(x) → f'+c ontology | useful as *rank talk*; drop-down exists; not what sorts get identified by |
| watch + lift | qsort/bubble: bag monotone → R5 **order**, R4 key-sort |
| origin | `repeat → decision → order` when C = order lift |
| hidden prior + probe | 0→1→2→0 looked surface-thin; STORE trace → `slot0 cycle-3`; confusion 35→0 |
| causal audit | sorts: **directed-chain** stand=1; quiet loop does not invent a cause |
| seq-cause | planted `1→2` (11 vs 1) extracted; top rank was setup `0→1` — chain `0→1→2→0` |
| mimicry | probe / propose only. Not how a form is built |
| Occam vs origin | calm plan still leads Occam; origin weight ~70, fires on confusion not on every print |

## What failed or is still thin

| item | status |
|------|--------|
| R3 species "quicksort" | English alias only. Concept form is `partition-then-order` |
| heap partition as cycle | probe sees opaque STORE soup; origin+watch carry the reading |
| black-box SM from start/end snapshot | failed until STORE hook |
| R7 / "is this the right problem" | **built**: `rank_gate` (one goal) + `ladder_plan` (two goals / conflict) |
| token-merge of two platonic F | **built**: `ikitaku_F_merge_lower` → R3 genus slab |
| permanent learn from novel | detect yes; catalog write is propose-file, human/plugin locks |
| Lie algebra | discrete commutator + Kant unity leftover |
| boolean algebra | **built**: 16 ops on charge ±1, `decision\|boolean-tt` |
| XOR cascade / nested emit | **built**: info-gap, xor-lock, state_cascade (guards) |
| 2x2 bimatrix | **built**: dominant / Nash / pareto_conflict (PD shape) |
| competing goals | **built**: dialectic `goal_conflict`; ladder R7_pick=3 blocks R6 |

---

## R6 strategies (thought_plan)

| name | when | practiced move |
|------|------|----------------|
| occam | calm, strong shallow fit | reject extra wraps unless desperation ≥ 8 |
| scientific | default | fit → peel → rule-scan under budget |
| hegelian | math-conf and rule-conf both mid/high | collide map-math vs map-table |
| bayesian | both confs exist | compete, don't pick a costume |
| origin | rule-ish / late C | associate B→C, invert A, mental-test synth |
| ladder | always available | walk R9→R8→R7→R6; do not skip to strategy |

---

## Solver-gap tiers (practiced)

| tier | meaning | action |
|------|---------|--------|
| **R6** | cognition insufficient; **named** missing method | propose R4 stub, need_context, don't hang |
| **R7** | right-task not singular (two useful goals, pareto clash, or unnamed application fail) | `ladder_plan` R7_pick=3; missing=`goal-priority`; **do not run R6** |
| **R8** | is this objective even useful (known verb) | skip if unknown costume |
| **R9** | cannot form the gap / residual high | unknown remainder |

Desire/template/hunt were built earlier. They were **not** wired through this ladder, so PD's two goals fell into “no solver” instead of “R7 pick a metric.” That was the bug, not missing desire.

---

## Boolean + cascade (2026-08-26)

Charge `+1` unity / `-1` disunity. `A ⊕ B` = XNOR = multiply. `A ⊖ B` = XOR.
AND/OR/→ are real Boolean. Nested emit = quote another bag's map.
Guards: `R = speaker ⊕ (other ⊕ path)` with xor-agents → speaker dissolves, utterance = ⊖ path.

Bimatrix worlds `++ +- -+ --`. Dominant = same pref in both other-worlds.
PD payoffs `3,0,5,1` / `3,5,0,1` → A_dom=B_dom=`-1`, Nash=`--`, joint max=`++`, pareto_conflict=1.

---

## Causal R4 species (audit)

- **post-hoc** — B after A, weak assoc or test open
- **cum-hoc** — commutator ≈ 0 (same span)
- **common-cause** — frequent hidden D, A–B assoc weak
- **directed-chain** — order-asymmetric + test hold + no better D → **stand**

Association (Jaccard/cosine/affinity) is the *unity check*, not the cause.

---

## Discoveries

Path: `ikitaku/discovery/<r5>/<r4>/<form>.form`  
`alias=` is a human handle (`qsort`). Iki's object is the form name.

Locked verbs/families stay in `ikitaku_r5_verbs.cat` / `ikitaku_r4_families.cat`.

---

## Files

| File | Concern |
|------|---------|
| `ikitaku.hpp` | facade |
| `ikitaku_core.*` | session, hypos, dialectic, F⊗F |
| `ikitaku_cluster.*` | clusters, analyze_ex superloop |
| `ikitaku_assoc.*` | Jaccard, affinity, gaps |
| `ikitaku_series.*` | fit, rule_scan, **seq_cause** |
| `ikitaku_meta.*` | desperation, thought_plan, undo, agenda |
| `ikitaku_concept.*` | R5/R4, watch, origin, probe, audit, discovery |
| `ikitaku_mcog.*` | theories about events (EOL/JOL/FOK) |
| `vm/rs_vm_ikitaku.*` | shim — compile one TU only |

Headers carry `PURPOSE` / `WHY` so the theory is next to the API.

Desire (`ikitaku_desire_*`): delta on an ordered set. Gate = not-impossible
(length / amplitude / regularity). Then platonic tries: filter, map, invert
as circular shift. Successful mse drop raises focus and budget. Vague note OK.
Noisy sin→cos (dx=0.05): possible 93, `circ-shift 31` mse 49→1 HOLD.
Good enough; not a reconstruction of the generating formula.

**Template** (R4 of seek): a shorter bag is a *witness* inside a longer one.
Offset + sample-ratio are the context `c` on `f'`. Partial cos @ 0.08 against
noisy sin @ 0.05 matched by sliding the template — full-bag phase shift is
redundant once the want is only a section. `ikitaku_template_match`.

---

## Kant map (Critique of Pure Reason → practiced layers)

Kant is used here as a **naming scheme for axioms**, not as German Idealism
cosplay. The hunt mutex is the antinomy engine. Critique classifies a claim
so iki does not smuggle a definition as a discovery.

| Kant | Ikitaku already | Where |
|------|-----------------|-------|
| **Analytic** judgment — predicate inside the subject; no new world-fact | tautology strip; R5 verb list ("repeat means advance an axis"); `form_id` that restates `purpose` | `extract_card` tautology flag; platonic verb catalog |
| **Synthetic** judgment — adds information | watch lift, STORE probe, series fit, origin B→C | `watch_run`, `probe_box`, `rule_scan` |
| **A priori** — independent of *this* run's senses | R5/R4 catalogs, quadratic formula, unit-circle-as-repeat, hunt rule "invert the farther pole" | catalogs + `ikitaku_critique` source=priori |
| **A posteriori** — from this appearance | heap bag, opcode span, xp cases, taught facts with conf interval | series, watch, `teach_context` |
| **Synthetic a priori** — new universal without this program's sensors | "a counted loop is repeat on an iterator axis"; "cos is sin shifted by π/2"; math fit families | flagged `axiom=1` when synthetic+priori |
| **Categories** (unity, plurality, causality, limitation…) | R5 verbs + cause_audit species + bound/filter | thin: no explicit category table |
| **Schematism** — category applied to an intuition | R5→R4 attach; template window = schema of "seek" on a bag | `r4_attach`, `template_match` |
| **Antinomy** — both poles can be argued | Hegelian thesis/antithesis; hunt pos/neg mutex | `dialectic_hunt`, `associate_hunt` |
| **Thing-in-itself** vs appearance | black box / opaque STORE soup vs start-end snapshot | `thing_in_self` on critique; probe |
| **Condition of possible experience** | time-as-transform; purity/mut flags; "not-impossible" desire gate | meta time + desire_possible |
| **Fallacy / illusion** | post-hoc, cum-hoc, common-cause, moderation fallacy | `cause_audit`, `assoc_poles.mid_is_fallacy` |
| **Gap / unknown** | find_gaps + need_context | aec `NO_CONTEXT` |

### Gaps (ours vs Kant)

1. **No category table.** Unity/plurality/causality are scattered across R5
   and cause_audit. Kant's point is that these are *conditions*, not findings.
2. **Schematism is implicit.** R4 attach is the schema; it is not named as
   "this is how a category touches a series."
3. **Space/time as forms of intuition.** Time-as-transform exists. Space
   (index axis of a bag) is used but not declared as the form that makes a
   template possible.
4. **Thing-in-itself was a bug, not a concept.** Opaque partition looked
   cheap until STORE. Critique now *labels* opaque so hunt does not treat
   the box as the appearance.
5. **Analytic was mixed with synthetic.** Saying "this loop is repeat"
   from the verb catalog is analytic. Saying "this loop orders the bag"
   after watch is synthetic a posteriori. Those used to share one confidence.
6. **Antinomy was averaged.** Old dialectic mid-conf of thesis+antithesis.
   Hunt waits for both, then inverts the more extreme miss from 50.

### What changed

- Cluster dialectic calls `ikitaku_dialectic_hunt` (mutex both confidences).
- `ikitaku_associate_hunt` scores two hypos as goal/avoid poles.
- `ikitaku_critique(claim, has_run_evidence, definitional, both_poles, opaque)`
  names analytic/synthetic × priori/posteriori and whether this is an axiom,
  an antinomy, or a thing-in-itself.

Iki can now say: this is an axiom I am *using*, not a fact I *found*.

---

## Experimental next (not claimed done)

- Rank seq-cause by *planted rarity*, not only one-way count
- Intermediate heap traces so partition is not "opaque"
- R3 plugin pack for known algos (still propose-only)
- Controlled F⊗F synthesis experiment (token-merge)
- R7 task-level metacognition

**Inexperience / experience:** L4 metaphor without L3 cases is not a missing
thought-form. `ikitaku_xp_record` writes HOLD/unwrap when a rule is *applied*.
A gap in a series is a template window (`ikitaku_gap_window`), not a void.
This is case-memory + reward, not policy-gradient RL.

No context → `need_context` (aec `NO_CONTEXT` / `WEAK_ASSOC`) and an ask.
Teach with `teach_context(fact, lo, hi)`.

Metaphor is overlap of *some* F attributes, not the costume. Circle vs
figure-8: shared closed/repeat/path/energy-loss (j=66), R4 one-loop vs
two-loop. Circle vs accumulator: only repeat+energy-loss (j=28). Same R5
repeat, different R4. Several R4 may hold; `alts_next` tries falling conf.

Poles: high assoc = synonym, low = antonym/inverse (`ikitaku_assoc_poles`).
Naive mid = Δ/2. Educated mid is rejected when the middle is worse than either
pole (fallacy of moderation). Assoc ≤15 → apply invert. Recombine is
desire-gated: far pole or low possible-conf = no merge.

Hunt: goal vs avoid (`ikitaku_hunt_*`). Mutex wait for both scores. Bias is
|score-50|. Invert the more extreme miss (neg 20 → next 80). Pos 70 / neg 20
HOLD. Slots, not OS threads.

Finite-c / partial want (`ikitaku_template_strategy`): vague desire without
experience is insufficient priori, not independent thought and not a high
lie. Several witnesses (prefix / thirds / whole). Whole-bag slice often
unwraps (mse 47) while short slices hold (mse 1-3). Micro score = hunt
(Hegelian). Macro label = critique (Kant). Thin xp: mild dissonance ~25
+ confusion ~35 + ask later. No self-model.

Presuppositions are challengeable (`ikitaku_critique_challenge`). Synthetic
claims stay open. New evidence + residual >= 25 or a weak hidden d drops
axiom status to challenged-priori. Action: keep (analytic, small residual),
ask (hidden rules / thin priori), extract (black box / high residual).
Test: repeat-def KEEP; cos-phase ASK hidden_d; partition EXTRACT.

Place (`ikitaku_place_report`): depth = layers used this reading.
incomplete_priori if xp thin or residual high. Always: self is structured
data evaluating a given bag; external reality is a larger unused superset.
Not a self-model and not a claim about the world outside the session.

Mechanism priori (`ikitaku_mechanism_priori`): every bag is structured data.
Evaluator and the code it sees are data that act on data — cycle
info+order+repeat. Kant: synthetic a priori, open. Recursion is the form.
Not a soul.

R6 can learn new types (`ikitaku_strategy_propose`). Micro = hunt/Kant/scientific.
Macro = iterative (revisit) vs waterfall (lock stage). Thinking is a **concept**
(methods as attrs, r5=theorize), not a self. `think_about_thinking` inspects
the method bag. Test: pick iterative vs waterfall; propose spiral; self=0.

Assoc memory: DIM 32, VOCAB 64, Hebb observe cap 64. Concept integrate is
uint8 depth 0..8 (not a bool), chain 24, `ikitaku_integrate`. Depth 20 clamps
to 8. Teaching real-world: priori then examples — `TEACH_REAL_WORLD.md`.

## Fuzzy Hegel × bool16 (2026-08-26)

`ikitaku_hegel_bool16` scores all 16 Boolean ops on thesis/antithesis charges
(conf ≥50 → +1, else −1). Directional bias: more-wrong pole → prefer invert
ops (NOT_A / XOR / NAND). Both high → AND/XNOR. Near-far Lie (`bind_near_far`)
biases unity vs disunity ops when mag is mid-range.

Isolated memory: `ikitaku_subj_set_path("/tmp/...")` redirects subject catalog
so test batteries do not write permanent subjects. `NULL` restores default.
`allow_persist=0` skips subject_teach even on the active path.

Permanent memory: `enable_learn=1` by default; `ikitaku_subject_teach` writes
`ikitaku_subjects.cat` and reloads via `ikitaku_subject_load`.

## Self-improve recursion (2026-08-26)

What was weak despite existing pieces (`self_reflect`, `r6 call_self/meta`,
`strategy_propose`, Hegel/TAS, bool16, F-merge):

1. **No outcome-closed loop** — pick method, never score residual delta
2. **Method-from-context thin** — desperation not binding bag residual tightly
3. **No progressive invent on methods** — Hegel/bool16 only on data hypos
4. **Stuck picks** — same method repeated without forced alternate / compose

`ikitaku_self_improve(residual, math, rule, depth, persist, out)`:
- each depth: method_hegel_pick → hegel_bool16(prev, pick) → kant_tas
- residual drops from synth charge + conf gain + determinate-negation ops
- invent = method change under residual; optional `strategy_propose` + F-merge
- stuck → force next-best or `prev_x_alt` compose
