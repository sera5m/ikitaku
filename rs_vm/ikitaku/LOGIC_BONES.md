# Formal-logic bones vs iki (addition table test)

`=` here is a **comparator** or a **push-result**, depending on context.
In the table it is push-result: `c` is the bag that `add` deposits.

## Map

| school name | iki already | missing |
|-------------|-------------|---------|
| Commutativity | Lie unity / `forms_commute` on *verbs*; two series identical | no axiom “+ commutes” as a rule that fires on unseen pairs |
| Associativity | cluster/compose grouping; property extract | no `(a+b)+c = a+(b+c)` engine |
| Distributivity | GCF/factor *plugin* if quad-like | not known as “you can push an op through a group” |
| Idempotence | — | \(P\land P\equiv P\); \(+\ ) is *not* idempotent (\(1+1\neq1\)) |
| Reflexivity | tautology / same-purpose | \(x=x\) not named |
| Symmetry | associate both ways; Gentner A:B::C:D | math-tier symmetry of \(=\) not separate |
| Transitivity | ABC chain, peel-parent | HS on equations not implemented |
| Intransitive | `useless-gap` | same bone, different costume |

**Mathematical unity of magnitudes** = R4 of `join`/`reduce` on the magnitude axis
(`small`/`big` leftover is `magnitude`). `add` = unify two magnitudes into one
bag. Taught as a fact this run; not yet a locked plugin.

Idempotence in one line: doing the op with the same thing again **does not
grow**. `and`/`or` are idempotent. `add` is not.

## Dataset

`files/ikitaku_datasets/add_table_1_25.csv` — 625 rows `a,b,c` with \(c=a+b\),
\(a,b\in[1,25]\).

## What first principles actually did (2026-08-25)

Rows \(a+b\) as \(b=1..25\):

- R5 `map`, R4 `linear_scan`, slope \(=1\), regularity \(1.0\)
- fit \(y=1\cdot x + (a+1)\) conf 100 (index origin)
- discrete deriv **all 1s** → successor / “unity of +1”
- row \(4\) − row \(3\) mean \(=1\) → same unity between *rows*
- \(5+b\) values \(=\) \(b+5\) (harness identity; iki saw the same linear map twice)

Flattened 25×25 raster (first 256): **irregular / tan-like**. It does **not**
recover a 2-argument `add` from a 1-D walk across the table. Needs the pair
\((a,b)\) as a bag, not a raster.

## Verdict

Bones: yes. Preschool theorem from axioms: not yet. Exposure to the table
makes it see **+1 as a linear map of magnitude**. It does not invent
associativity/distributivity as named theorems from the raster alone.
Feed it *rows and row-deltas*, not the zigzag of the full grid.

## Taught 2026-08-25 (magnitude unity / disunity)

Platonic framing (not “it’s math” costume):

| subject | R5 | R4 | meaning |
|---------|----|----|---------|
| magnitude | info | axis | size-of-bag; poles small–big |
| unity_mag / add | join | magnitude-unify | two magnitudes → one |
| disunity_mag / sub | split | magnitude-split | magnitude minus magnitude; inverse of add |
| plus_n | map | plus-n | \(x\mapsto x+n\) |
| minus_n | map | minus-n | \(x\mapsto x-n\) |

R4 families locked: `join|magnitude-unify`, `split|magnitude-split`, `map|plus-n`, `map|minus-n`.

Series checks: ordered \(1..16\) then \(+5\) / \(-2\) → map-linear slope 1; row-delta constant \(n\); random \(+3\) then \(-3\) recovers base (inverse).

## Prior-match + self-teach (library, 2026-08-25)

API in `ikitaku_series.hpp`:

- `ikitaku_match_priors_binop(a,b,c)` — residual vs unity/disunity/scale/inv/max/min
- `ikitaku_reason_binop` — pseudo-English chain (debug only)
- `ikitaku_reason_unknown_value` — inverse deposit / underdetermined / impossible
- `ikitaku_self_teach_prior` — if EXACT and not ambiguous, `subject_teach` with source=self-teach

Ambiguous exact hits (e.g. 1?2=2 → mul and max) refuse self-teach until second probe.

## Information bags + XOR cascade (2026-08-25)

Why the guards puzzle failed: no charge algebra, no info-gap, no nested emit,
no XOR-lock. Dialectic sat on door0 vs door1 because the speaker was still in
the equation.

Added (general, not a puzzle solver):

- `ikitaku_info_gap` — other bag may hold extractable priori self lacks; nested emit legal
- `ikitaku_xor_lock` — \(S = A \oplus P\) with \(A\) unknown ⇒ \(P\) not isolable
- `ikitaku_charge_compose` — unity \(+1\), disunity \(-1\), compose by multiply
- `ikitaku_state_cascade` — \(R = S_\text{agent} \oplus (S_\text{other} \oplus S_\text{path})\);
  if agents are XOR-opposed, speaker dissolves and \(R = \ominus S_\text{path}\)

## Solver-gap tier (R6 vs R7 vs R9)

Named missing method → **R6**: cognition insufficient, fix the catalog.
Tools exist but failure unnamed → **R7**: cannot tell if this is still the right task.
Cannot form the gap at all → **R9**.

## 16 Boolean ops on charge ±1 (`ikitaku_bool16`)

`+1` unity, `-1` disunity. `A ⊕ B` = XNOR = multiply. `A ⊖ B` = XOR.
AND/OR/→ are real Boolean (the multiply rewrite of AND is not AND).
R4 `decision|boolean-tt` locked.

