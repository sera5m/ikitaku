# How smart is ikitaku, and what would actually make it smarter

Honest rank: **budgeted hypothesizer**, not an animal. It can split a goal,
flatten Lie curvature, propose a leftover genus, and watch a black box.
It cannot speak, lock its own priors, or know that a proposal is *true*.

## What it already does well

| layer | practiced |
|-------|-----------|
| R0–R2 | opcode/watch/probe micros |
| R4–R5 | 22 verbs + families, catalog match |
| R6 | compose micros→macro, self/meta, watch |
| R7–R9 | useful / right-task / unknown remainder |
| curve | fractional TAS + Lie gap |
| create | unlocked leftover → R3/discovery |
| autonomy | split working micros vs gap joints |

That is already more structure than a classifier. It is **not** yet a
method-selector that treats every bag as `F'+c` and picks the transform
family from metacog context. That theory is **parked** (see below).

## Architectural improvements (method, not more verbs)

1. **Budget = remaining.** Tick decrements. A solved sub **adds** remaining.
   Zero remaining = give up. Open-ended only while it is actually closing gaps.
   Hard cap 64 is a runaway brake, not a philosophy.

2. **Local vs far association.** Hebbian was gluing anything that co-occurred
   (emit sitting next to absorb in one file). Humans get data from mimicry
   then **transform** it; they do not raise antonym affinity.
   - **Local** (affinity ≥ 0.40): Hebb as now — same cluster.
   - **Mid** (0.20–0.40): slow bump.
   - **Far** (< 0.20): skip. Far is for invert/mimicry **probe**, not synonym.
   Poles already existed (`POLE_NEAR/FAR`). Learning now obeys them.

3. **Mimicry is a dataset pump, not identity.** Probe I/O → xp bag → attrs.
   Do not copy the box's costume as the form (old rule, keep it).

4. **Bags / delegate.** R6 should not execute R0. A bag is a box: lower
   layer runs (watch, rule_scan, probe); R6 only watches and scores.
   `ikitaku_bag_delegate`. Thin on purpose.

5. **Metacognition (careful).** What we have is *strategy about a reading*
   (EOL/JOL, r6_compose, theories-about-events). What we do **not** have:
   a model of *whether this method is the right method for this bag*
   beyond desperation and Lie unity. Next real gain is: bind **method
   choice** to **context of the bag** (pure/impure, local/far, residual).
   That is still R6, as a self-on-the-ladder (not a person).

## Advanced transform theory — consider, do not apply yet

Everything is a transform plus context: `F → F'+c`. Metacog would pick
*which* transform family (map-math, map-table, invert, filter, wait-axis)
from the bag's attributes, then compose. Powerful if the selector is
grounded in the ladder we already have.

**Do not apply yet.** Reason: method selection without a calibrated
"this context → this family" table will invent wraps (pareidolia). We
just gated far-Hebb and fractional TAS. Let those settle. The theory
stays a note until leftover-create + local cluster are boringly reliable.

## How much smarter can architecture get (order)

1. Budget-on-success + give-up (this pass)
2. Local/far Hebb split (this pass)
3. Bags so R6 is experimenter not executor (stub this pass)
4. Method-from-context table (not yet)
5. Transform-as-default ontology (not yet)
6. English / world / lock-own-priors — outside iki
