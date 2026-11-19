# Platonic verb set (R5 work)

Practiced pipeline and what tests actually did: `IKITAKU_NOTES.md`.
Snapshot of this page: `artifacts/archive/ikitaku_theory_2026-08-25/`.

Locked 2026-08-25. Species and axes live *below* these.  
If it is `already-have(axis=?)` without lying, it is not a new R5.

Mimicry must not install a verb. A costume (`bubble_sort` sticker) may only *propose*.

---

## Rank reminder

| Rank | What |
|------|------|
| R5 | kind of work on information (this list) |
| R4 | family / axis (math vs table; iterator vs time vs order) |
| R3 | named algorithm / species |
| R2 | micro schema |
| R1 | region / CFG / purity |
| R0 | surface ops |
| R6 | **strategy of thinking** — which thought-model to run (scientific, Hegelian, Bayesian, Occam, desperation cascade), plus monitor / theorize / raise-lower |

`linear`, `sine`, `quadratic` are R3 of **map** (map-math).  
`loop` / `while` are R4 of **repeat**.  
`variable` as a noun is **info**. Binding it is **name**.

---

## R5 verbs (program / data work)

| # | Verb | Work | Example | Not this |
|---|------|------|---------|----------|
| 1 | **info** | A value/structure exists | struct, var, const | the *act* of binding (name) |
| 2 | **name** | Bind identity to a value | `let x = 5` | copy |
| 3 | **copy** | Duplicate without change | `a = b` | map |
| 4 | **map** | One→one, shape kept | `x*2` over array | R4: map-math / map-table |
| 5 | **reduce** | Many→one | sum, max, fold | map of a scalar |
| 6 | **generate** | One→many / unfold / sample | `rand()`, iter, unfold | repeat of a constant |
| 7 | **create** | Bring a structure into existence | new, alloc | **same purpose as generate**; keep as alias, do not split kingdom |
| 8 | **filter** | Many→fewer, same type | keep evens | decision (no extra control path) |
| 9 | **split** | One bag → two bags | partition by `> 5` | decision |
| 10 | **join** | Two bags → one | zip, concat | map of pairs |
| 11 | **order** | Rearrange by key | sort by name | repeat+compare as *method* |
| 12 | **seek** | Find location / witness | find, scan-until | repeat as *method* |
| 13 | **repeat** | Apply along an axis | loop, while | the axis is R4 (iter / time / order) |
| 14 | **sequence** | Do T then U | `T(); U();` | one fat map |
| 15 | **decision** | Compare → **which code path** | if, switch | producing a bool is not this |
| 16 | **wait** | Defer on time axis | delay(ms) | dirty repeat; T is still a var |
| 17 | **invert** | Undo a known transform | T⁻¹, peel | “try sine because it wiggles” |
| 18 | **bound** | Make state obey a constraint | clamp, satisfy, type | decision |
| 19 | **emit** | Cross world boundary **out** | print, host | map |
| 20 | **absorb** | Cross world boundary **in** | sensor, read | info that appeared from nowhere |
| 21 | **destroy** | Delete a structure / resource | free, close | invert of create/generate |

Emit and absorb are one platonic **boundary** with two directions.  
Create and generate share purpose; destroy is the inverse of that pair.

---

## Compare / classify / decide / evaluate

Yes: **relating two pieces of information is its own work.**  
No: that work is not the same as **decision**.

| Verb | Rank | What it *does* |
|------|------|----------------|
| **compare** | **R5** | Produce a relation: `<`, `=`, order key, distance. Output is a *value* (bool, sign, rank). No path change required. |
| **classify** | R4 of **map-table** or of **compare** | Emit a label from a table / predicate. Still a map unless it forks CFG. |
| **decision** | R5 | Use a relation to **choose a control path**. |
| **evaluate** | split | (a) apply a known f → **map**. (b) score “did this theory hold?” → **R6 theorize**. |

So:

- `cmp = a < b` → **compare** (maybe then **map** into a flag).  
- `if [a < b] { A } else { B }` → **compare** *used by* **decision**.  
- `label = table[x]` → **map-table**, not decision.  
- `keep if even` → **filter**, which *uses* compare, purpose is still filter.

Do not collapse compare into decision. That was the table/if mistake again.

Add **compare** as #22.

| # | Verb | Work | Example |
|---|------|------|---------|
| 22 | **compare** | Produce a relation across values | `<`, `==`, key-order, distance |

---

## #23 Raise / lower (concept as variable)

**Not R5 program work.** This is **R6**.

Raising/lowering the derivative order of a *concept* (`F` ↔ `f′` as a slot) is how iki *thinks about* verbs. It is not a verb the object program performs.

- Object-level: the program **maps** or **repeats**.  
- Meta-level: iki treats `repeat` as a variable and drops it to `repeat(axis=time)`.

If we stuffed this into R5, every program would look like metacognition. Keep it as:

`R6 apply` = raise/lower a concept slot (already: concept chains, peel, layer-sure).

---

## #24 Theorize (predict / evaluate a claim)

**Not R5.** Also **R6**.

A theory is a claim about an **event** (“this form will hold,” “I would recognize a tighter species”). Scoring it is calibration, not `a < b`.

It is *sorta* like compare only in the thin sense that both produce a relation.  
Compare: relation between *values*.  
Theorize: relation between a *claim* and a later *event*.

Existing hooks: EOL / JOL / FOK / RETRO in `ikitaku_mcog`. Do not add `theorize` as a program purpose.

---

## Locked R5 set (object work only)

```
info  name  copy
map   reduce  generate(=create)  destroy
filter  split  join
order  seek  compare
repeat  sequence
decision
wait  invert  bound
emit  absorb
```

R4 under **map**: `map-math` | `map-table`.  
R4 under **repeat**: axis = iterator | time | order | position.  
R4 under **generate**: lawful unfold | lawless sample (random).

R6 (not in the box): thought-models (scientific / Hegelian / Bayesian / Occam),
raise/lower, theorize, monitor, control, mimicry-as-probe.

---

## Rejected / composite (do not add as R5)

### Denoise
Subset of **filter** (drop residuals) or **reduce** (collapse to signal) or **bound**
(force onto a model). Not a new kingdom.

### Rearrange according to a schema
**order** if by a key; **map** if each cell moves by a known template; **join** if
two tracks lock to one schema. The schema itself is **info**.

### Schema extract (find the schema in the data)
Not L6 and not a 23rd verb.

It is an **L5 task**: the *job* of the analyzer. It is done *by composing*
existing verbs:

```
seek     find a repeating shape
compare  this instance vs that
reduce   many examples → one general form (LGG)
generate hypothesize an F
invert   peel a wrap to see the inner schema
map      write the found schema as a table or f(x)
```

How it *chooses among those moves* is R6 (scientific try, Bayesian score,
Hegelian collide, Occam).

So: schema extract = “uses everything else to try to do it.”  
The schema found is **info** (a form). Promoting “schema-extract” to R5 would
duplicate the whole layer.

---

## Plugin policy

Empty slots first (id + purpose + one-line general form).  
Write a matcher only when a dataset or Vulcan span needs that verb.  
Never promote linear/sine/loop/if to this list.

Runtime L5 map: `ikitaku_r5_verbs.cat`  
`ikitaku_r5_lookup` / `is_platonic` / `propose`.  
Novel L5 → `propose` writes locked=0. Not a new kingdom until a human/plugin lock.  
`concept_chain_init` marks platonic only for locked object verbs (not R6, not unknown).
