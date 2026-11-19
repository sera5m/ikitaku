# Vulcan Structure Analysis Layer — Research Notes

**Focus (2026-08-24):** Code → structural analysis → identified structures / general algorithm forms.\
Lambda/e-graph/IPC chain-deposit and novel-algorithm naming are **future** features under this umbrella.

---

## 1. Goal

Given Vulcan (or bytecode) input, produce a **structure report**:

```
regions, CF shape, dataflow skeleton, matched schemas,
known algorithm class (if any), general form, confidence,
rewrite / deposit opportunities
```

Not “optimize first” — **recognize first**, then decide rewrites (existing algo-rewrite, strength, trap, IPC deposit).

Pipeline:

```
source / bytecode
    → AST + regions + DFG/CFG skeleton
    → structure features + schema match
    → algorithm class / general form (catalog)
    → optional: e-graph equality class, IPC chain deposit
    → opt passes only on pure / matched forms
```

---

## 2. Algorithm recognition (CS literature)

### 2.1 Schema + beacon (classic program comprehension)

- Algorithms stored as **schemas** (canonical control/data patterns).
- **Beacons**: distinctive constructs (swap, mid-index, partition, heapify).
- Taherkhani (Aalto): schema detection + C4.5 classification on beacons/metrics → \~97–98% on sorting/search/graph teaching algorithms.
- Combined method: extract schema fragments, then classify only those.

**Vulcan fit:** Our `rsvm_region_t` (LOOP/TRAP/IF/JT) + pure-math body patterns are already lightweight schemas. Extend with a **schema catalog** (YAML/table of opcode-shape fingerprints).

### 2.2 AST pattern DSL (2026)

- arXiv 2605.06098: DSL over AST features; F1 \~0.74 on Fibonacci / BubbleSort / BinarySearch; beats CodeLlama (0.35) and clone detectors on recall.
- Patterns name-independent when keyed on **shape**, not identifiers.

**Vulcan fit:** Match on bytecode region graphs + opcode sequences, not variable names (aligns with type-keyword ban / mangling).

### 2.3 Learning approaches

- GraphCodeBERT + scrambled identifiers: 96–99% algorithm ID among known sets (sort/search/shortest path).
- Feature counts (loops, vars, constructs) + classifier (ICMLA 2017) — weaker but cheap.
- Hybrid static filter + LLM (arXiv 2604.03048): static analysis cuts LLM calls 72–97% and improves F1.

**Vulcan fit (near term):** Rule/schema catalog + confidence, **not** onboard GNN. Optional offline catalog builder later.

### 2.4 Formal / binary

- Horn-clause normalization of bytecode → common clause set ⇒ same algorithm (Dalvik study).
- Useful long-term for AOT-C / multi-backend equivalence.

---

## 3. Intermediate representations (what to build)

| IR | Role | Priority |
| --- | --- | --- |
| **AST / parse tree** | Syntax shapes, fn boundaries, `in`/`out` chains | High (parser already close) |
| **Region graph** | LOOP/TRAP/IF/JT/FN spans (exists) | High — export as structure API |
| **CFG** | Block successors from JMP/JZ/JT | High |
| **DFG / slot def-use** | Single-producer chains → IPC deposit | High |
| **CPG-lite** | AST + control + data edges (Joern-style, subset) | Medium |
| **E-graph** | Equality saturation for pure expr (egg/egglog) | Future |
| **Schema match table** | Fingerprint → algorithm id + general form | High |

**Primary API target:**

```c
typedef struct {
    const char* kind;          // "counted_inc", "affine_update", "phase_wrap",
                               // "bubble_sort_like", "unknown"
    const char* general_form;  // "x := x + c  (n times) → x += n*c"
    const char* big_o;         // "O(n)", "O(1)", "O(n^2)"
    int         confidence;    // 0–100
    uint16_t    region_start, region_end;
    uint8_t     pure;
} rsvm_structure_hit_t;

int rsvm_analyze_structures(const rsvm_t* vm,
                            rsvm_structure_hit_t* out, int max_out);
```

---

## 4. Lisp / Haskell parsing & evaluation (for lambda path)

### 4.1 Evaluation strategies (lambda calculus)

| Strategy | Order | Strict? | Complete for NF? | Language analog |
| --- | --- | --- | --- | --- |
| **Normal order** | leftmost-outermost | No | Yes | Theoretical; call-by-name family |
| **Applicative order** | leftmost-innermost | Yes | No | C, ML, Vulcan (eager) |
| **Call-by-value** | eval args, then β | Yes | Weak NF | Most imperative VMs |
| **Call-by-need** | lazy + share | No | Yes (with care) | Haskell |
| **Full β** | any redex | — | Confluent (CR) | Theorem provers |

Church–Rosser: if two reduction paths reach normal forms, they are the same.\
Normalization theorem: normal order finds NF if one exists.

**Abstract machines:** SECD (Landin), Krivine (call-by-name), CEK, KAM/KN (full reducing).

**Vulcan near term:** Stay call-by-value / applicative on the VM. Lambda/e-graph layer is for **analysis and rewrite of pure fragments**, not changing default evaluation.

### 4.2 Haskell-style analysis we can steal

- **Purity / effect tracking** — we have pure/mut and impure opcode gate.
- **Rewrite RULES** — GHC RULES ≈ our algo rule table (match/apply/confidence).
- **Deforestation / stream fusion** — future feed-chain generalizations.
- **Equality saturation (egg, egglog, hegg)** — non-destructive rewrite space; extract by cost (steps, size). Bidirectional in the sense that all equal forms coexist until extraction.

### 4.3 Lisp-style structural code

- Code as data: S-expressions ↔ our explicit `in`/`out` / region markers.
- Macros ≈ `@compose` / property-driven codegen.
- Homoiiconicity advantage: Vulcan’s explicit format is already closer to analyzable IR than C++.

---

## 5. IPC / interprocedural chain deposit (expand significantly)

**Current:** intra-region LOAD/STORE noop elim; parse-time monomorph/inline of small fns.

**Target IPC structure analysis:**

1. Build **slot def-use** across function boundaries via `in`/`out` edges.
2. Detect **single-producer → single-consumer** chains:
   - `foo out[a]` and only `bar in[a]` reads `a` → deposit `foo`’s result into `bar`’s arg register / accumulator; delete intermediate slot traffic.
3. **Setter-getter collapse:** property get/set pairs that only shuttle values → same as deposit.
4. **HERE / stack leave:** `out[here]` already expresses “leave on stack”; structural pass should prefer HERE over named temps when safe.
5. Call-graph SCC: pure leaf clusters can be inlined or e-graph’d as one unit.

Data needed: per-func `writes[]`, `reads[]`, purity bit, region spans — extend `rsvm_func_t` metadata at compile end.

---

## 6. General forms catalog (seed)

| Id | Shape (schema) | General form | Rewrite |
| --- | --- | --- | --- |
| `counted_inc` | trap body = add const to one slot | (x \\leftarrow x + c) × n | (x \\mathrel{+}= n\\cdot c) |
| `affine_update` | (x \\leftarrow a x + b) | affine iteration | closed form (a^n) |
| `feed_sum` | (s \\mathrel{+}= t;\\ t\\mathrel{+}+) | triangular accumulate | (s \\mathrel{+}= n t_0 + n(n-1)/2) |
| `phase_wrap` | (p \\leftarrow (p+d) \\bmod 2^k) | ring phase | `BAND` mask; Q15 LUT index |
| `strength_mul_pow2` | `* 2^k` | strength reduction | `SHL k` |
| `invsqrt_bits` | float bits → invsqrt | Quake form | `ISQRT` |
| `bubble_like` | nested loop + adjacent swap | O(n²) sort | **recognize only** (future suggest) |
| `linear_search` | loop + eq break | O(n) search | recognize |
| `unknown_pure` | pure trap, no rule | O(n) generic | leave trapdoor |
| `impure` | I/O/sensor/wait | — | never superopt |

**Novel algorithm (future):** if structure matches no catalog entry but has clean general form (e.g. single affine system), emit:

```
[structure] novel_affine_system conf=70 form="x' = A x + b" big_o=O(n)
```

Do **not** auto-rewrite novels until verified (same trial/revert discipline as algo rules).

---

## 7. Implementation roadmap (structure-first)

### Phase A — Structure API (next engineering)

1. `rsvm_analyze_structures()` walking regions + opcode schemas.
2. Emit hits for existing rules (`counted_inc`, `affine`, `feed`, `phase_wrap`, `strength_*`).
3. Debug dump: `set_structure_report 1` → print forms + confidence.

### Phase B — IPC expand

1. Def-use + purity per function.
2. Single-producer/consumer deposit across `in`/`out`.
3. Prefer `out[here]` when intermediate slot dies.

### Phase C — Catalog growth

1. Sort/search/scan schemas (recognize-only).
2. Graph-ish patterns when arrays exist (recognize-only).

### Phase D — Future (saved)

1. E-graph on pure expression DAGs (egg-style rebuild + extract).
2. Lambda intermediate for pure functional subsets.
3. Novel-form naming + optional human/trial confirm.
4. Optional offline GNN/GraphCodeBERT catalog trainer.

---

## 8. Design principles

1. **Structure before rewrite** — identify general form; only then apply cost-reducing transform.
2. **Name-independent shapes** — opcodes + region topology, not identifiers.
3. **Purity is law** — impure/tainted never enter superopt or e-graph.
4. **Trial/revert** — any rewrite must verify or snap back (already in algo pass).
5. **Confidence always logged** — same style as `[algo] conf=92`.
6. **Catalog is data** — new algorithms = new schema rows, not new VM opcodes.

---

## 9. Key references (research)

- Taherkhani — algorithm recognition via schemas/beacons + ML (Aalto thesis).
- AST pattern DSL for algorithm recognition — arXiv:2605.06098 (F1≈0.74).
- GraphCodeBERT algorithm identification — 96–99% on known classes.
- Static + LLM hybrid recognition — arXiv:2604.03048.
- egg / equality saturation — Willsey et al., CACM; egglog successor.
- Lambda strategy survey — normal vs applicative order; KN/KAM machines.
- Code property graphs (Joern) — AST+CFG+DFG query model.
- Strength reduction / OSR — classic compiler; already partially in Vulcan.

---

## 10. Relation to current Vulcan opt

| Existing | Maps to structure layer |
| --- | --- |
| `body_is_pure_math` | purity feature |
| `match_pure_inc` / affine / feed | schema entries |
| `@superoptimize_enable` | gate on structure-driven rewrites |
| regions LOOP/TRAP/IF/JT | CFG skeleton |
| `slot_props` mut/immut | effect / taint seeds |
| strength `*2^k`, `%2^k→&` | local schema rewrites |
| ISQRT | magic-number schema |

**Shift:** treat these as **instances of structure hits**, not one-off passes. New #1 work is the analyzer that **returns the forms**; optimizers consume that report.

---

## 11. Taxonomy refinement — pattern derivation trees (2026-08-24)

User proposal evaluated and sharpened into implementable ranks. Optimization stays a **thinking/evaluation layer** beside IR, not mixed into parse loops. Layers are strict: extract → classify → compare → (optional) rewrite.

### 11.1 Verdict on the proposal

**Strong / keep**

- Macro purpose vs micro mutation split (bubble vs merge: same “sort”, different micro).
- Early structure trees before optimization.
- Separation of concerns into ordered layers.
- Catalog of known base patterns + similarity / combination matching.
- Scientific-method loop: hypothesize form → test against IR features → accept/reject with confidence.
- Stacked structures (patterns composed of patterns).

**Vague / needs formalization**

- “Hyperset / meta-cognitive why” — useful as *optional human annotation*, not automatic rank-0 analysis.
- “Derivatives / anti-derivatives of function-transforms” — metaphor only until defined as:
  - derivative ≈ strengthen/specialize (add constraint, fix order, add swap beacon)
  - anti-derivative ≈ abstract (drop detail → parent purpose node)
- Set theory “transformers as axioms” — adopt lightly: each catalog entry is an **axiom schema** (pattern ⇒ form + complexity); matching is **instance of** schema, not full theorem proving in v1.

**Incomplete (expected)**

- No similarity metric yet (edit distance on region graphs, beacon Jaccard, DFG isomorphism cost).
- No multi-label / stacked hit merge rules.
- Sort catalog is illustrative only until schemas are bytecode-shaped.

### 11.2 Ranked derivation tree (inverted triangle)

Ranks are **partial order**, not biology. One region can attach to several branches (stacked).

```
R5  PURPOSE        sort | search | reduce | map | scan | io | unknown
R4  METHOD FAMILY  comparison-sort | divide-conquer-sort | linear-scan | ...
R3  ALGORITHM      bubble | insertion | merge | quick | counted_inc | affine | ...
R2  MICRO SCHEMA   adjacent-compare-swap | split-merge | trap-add-const | ...
R1  REGION/CFG     this LOOP/TRAP/IF body, spans, purity, slot def-use
R0  SURFACE        opcodes, immediates, slot ids (name-independent shape)
```

- **R5** answers *what transformation on data* (macro attention).
- **R2–R3** answer *how* (micro → named algo).
- **R0–R1** are always computable from current VM (regions + bytecode).
- Climbing R1→R5 is **abstraction**; descending is **specialization**.

Example:

| Rank | Bubble | Merge |
| --- | --- | --- |
| R5 | sort | sort |
| R4 | comparison-sort | divide-conquer-sort |
| R3 | bubble | merge |
| R2 | adjacent swap × nested loops | split + recursive merge |
| R1 | two nested LOOPs, swap beacon | recursion or explicit stack + merge loop |
| complexity | T: O(n²) all cases; S: O(1) | T: O(n log n); S: O(n) |

Same R5 → different R2–R4. Structure report must **not** collapse them early.

### 11.3 Layered pipeline (separation of concerns)

```
L0  Parse / lower          → AST, bytecode, regions
L1  Feature extraction     → CFG, DFG, beacons, purity, complexities local
L2  Schema match           → R2 hits (micro patterns), confidence
L3  Algorithm assign       → R3–R4 from combinations of R2 + topology
L4  Purpose assign         → R5 from R3–R4 catalog edges (or unknown)
L5  Thinking / evaluation  → compare alts, big-O, rewrite candidates, IPC deposit
L6  Apply (optional)       → existing opt passes only on accepted pure hits
```

No mixing L2 matching inside L0 parse loops. Report is available after L4; L5–L6 are the “thinking” layer near IR.

### 11.4 Formal logic / sets / lambda (lightweight stance)

- **Set:** each schema (S) is a set of IR instances; match = (ir \\in S) approximated by features.
- **Logic:** Horn-like rules already match practice:
  - `pure_trap ∧ body_is_inc(slot,c) ⇒ form(counted_inc) ∧ O(1)_rewrite`
  - Same shape as current algo rule table; grow into explicit rule base.
- **Lambda:** pure fragments as terms; β-equivalent terms share an e-class **later** (egg). Evaluation strategy remains CBV on the VM; analysis may consider normal-order equivalents only inside pure e-graphs.
- **Intersection:** feature extraction = interpretation of IR into a fact base; matching = resolution/query against axiom schemas; rewrite = proven instance under purity assumptions.

v1 does **not** ship a theorem prover — it ships **facts + rules + confidence**, which is the implementable fragment of that intersection.

### 11.5 Similarity and combinations

For “similar but not identical” and stacked structures:

1. **Beacon vector** Jaccard / weighted overlap.
2. **Region-graph edit distance** (insert/delete LOOP, swap body schema).
3. **DFG motif** match (producer-consumer chains).
4. **Combination rules:** e.g. `nested(adjacent_swap, counted_outer) ⇒ bubble_like conf=…`

Stacked: outer hit R5=sort, inner hit R2=compare-swap → report both + link parent/child in the structure tree.

### 11.6 Complexity attributes (retain always)

Per hit, when known:

- time: best / avg / worst
- space: extra
- logical: micro-step count vs macro effect (qualitative tag: `single_pattern_accumulative` | `multi_phase`)

### 11.7 What we implement next under this model

1. **L1–L2 only:** export structure tree nodes for *already known* micro schemas (counted_inc, affine, feed, phase_wrap, strength_pow2) with ranks R0–R3 filled, R4–R5 = `reduce`/`map`/`unknown` as appropriate.
2. **Structure report API** after compile (no rewrite required).
3. **L5** consumes report for existing rewrites (unchanged safety: pure + trial).
4. Sort R3 catalog = recognize-only stubs until array DFG is strong enough.

### 11.8 Meta ranks (explicitly deferred)

- **Why / hyperset** (self-eval of analysis quality, task appropriateness): operator/tooling concern, not automatic IR rank.
- Record as future `R6_meta` only if we add explicit goal annotations in source (`@purpose sort`).

---

## 12. Modules + rule intake (2026-08-24)

Specialization and abstraction are **derivative transforms** between ranks:

- specialize: R5→R2 (add constraints / beacons)
- abstract: R2→R5 (drop detail → shared purpose)

Mutations that share an upward abstract form (bubble vs merge → sort) are siblings under the same R5 node.

### 12.1 Separate layer modules

| Module | Layer | File | Role |
| --- | --- | --- | --- |
| Parse/lower | L0 | `rs_vm_parse.cpp` | code → bytecode + regions |
| Structure | L1–L4 | `rs_vm_struct.cpp` | facts → schema match → ranks → tree |
| Opt thinking/apply | L5–L6 | `rs_vm_opt.cpp` | consume hits; pure rewrite |

Shared libraries allowed (opcode helpers, purity); **no** mixed control loops across modules.

Conceptual stack the analyzer understands:

```
abstract logic   (purpose R5, family R4)
    ↓ specialize / ↑ abstract
mathematical logic  (general form, big-O, purity)
    ↓
code structure tree  (regions, DFG, beacons)
    ↓
instructions  (bytecode R0)
```

### 12.2 Rule intake (axiom schemas)

Rules are **data** the parametric matcher runs on:

```c
rsvm_struct_rule_t {
  id, general_form, purpose, family, big_o,
  min_confidence, rank, match(vm, span) → conf
}
```

Built-in catalog via `rsvm_struct_rules()` / `rsvm_struct_rule_count()`. New examples = new rows (or future load from table/file) — analyzer “learns” by **catalog growth**, not by mutating parse.

### 12.3 Structure trees API

```c
int rsvm_analyze_structures(vm, hits, max);
int rsvm_structure_report(vm, buf, len);
```

Each hit: kind, general_form, purpose, family, big_o, confidence, pure, span, parent_hit.

Example report:

```
[structure] hits=… rules=4 regions=2
  [0] R3 counted_inc  purpose=reduce family=affine_reduce  O(n)→O(1)  conf=92
       form: x := x + (1) → x += n*(1)
```

### 12.4 Lambda / set / logic (how it maps)

- **Rule shape derivation** = specializing or abstracting a schema (new sibling or parent form).
- **Set:** schema = set of IR instances; match ≈ membership by features.
- **Lambda:** pure terms; equal forms under β only in future e-graph module.
- **Formal logic:** Horn-like `features ⇒ form ∧ conf` — the match functions.

---

## 13. Example structure trees — flow control & variables (R0–R5)

Three examples each for the basic R4/R5 concepts.\
R5 = purpose (why in the program). R4 = method family (how class).\
Lower ranks specialize; climbing abstracts to the same concept.

### 13.1 Purpose: **repeat** — “do something until a condition is met”

R5 `repeat` · R4 families: `counted_loop` | `cond_loop` | `trap_loop`

#### Example A — `do n` / trap counted loop

```
R5  PURPOSE     repeat
R4  FAMILY      counted_loop
R3  ALGORITHM   trap_counted  (or counted_inc if body is pure +c)
R2  MICRO       OP_TRAP_LOOP + fixed body; count on stack
R1  REGION      RSVM_REG_TRAP  pc[start..end)
R0  SURFACE     TRAP_LOOP, u16 body_len, body ops…
form:  repeat body exactly n times
big_O: O(n) body-cost  (→ O(1) if pure counted_inc)
```

#### Example B — `while [cond]`

```
R5  PURPOSE     repeat
R4  FAMILY      cond_loop
R3  ALGORITHM   while_cond
R2  MICRO       test → JZ exit; body; JMP test
R1  REGION      RSVM_REG_WHILE
R0  SURFACE     LOAD/ops, JZ, body, JMP
form:  while cond { body }  — repeat until cond false
big_O: O(iterations × body)  unknown bound if cond dynamic
```

#### Example C — `for [i=0; i<n; i=i+1]`

```
R5  PURPOSE     repeat
R4  FAMILY      counted_loop   (affine induction)
R3  ALGORITHM   for_affine
R2  MICRO       init; test; body; step; JMP test
R1  REGION      RSVM_REG_FOR
R0  SURFACE     LDI/STORE init, CMP/JZ, body, ADD_IMM step
form:  for i in [lo, hi) by step { body }
big_O: O(((hi-lo)/step) × body)
```

All three abstract to **repeat**. Micro differs; R5 stays the same.

---

### 13.2 Purpose: **decision** — “compare something; act on the option”

R5 `decision` · R4 families: `binary_branch` | `multi_branch`

#### Example A — `if [cond] { } else { }`

```
R5  PURPOSE     decision
R4  FAMILY      binary_branch
R3  ALGORITHM   if_else
R2  MICRO       cond → JZ else; then; JMP end; else; end
R1  REGION      RSVM_REG_IF
R0  SURFACE     CMP/JZ/JMP + two bodies
form:  if cond then A else B
big_O: O(1) control + max(cost A, cost B)  (both pure)
```

#### Example B — `if [cond] { }` (no else)

```
R5  PURPOSE     decision
R4  FAMILY      binary_branch
R3  ALGORITHM   if_then
R2  MICRO       cond → JZ end; then; end
R1  REGION      RSVM_REG_IF
R0  SURFACE     CMP/JZ + one body
form:  if cond then A
big_O: O(1) + cost A if taken
```

#### Example C — `switch` / jump table

```
R5  PURPOSE     decision
R4  FAMILY      multi_branch
R3  ALGORITHM   switch_jt   (dense)  |  switch_chain (sparse)
R2  MICRO       OP_SWITCH_JT min,count,targets[]  or cascaded JZ
R1  REGION      RSVM_REG_JT or RSVM_REG_SWITCH
R0  SURFACE     SWITCH_JT encoding or if-else chain
form:  select body by discrete value v
big_O: O(1) JT  /  O(cases) chain
```

All three abstract to **decision**. Binary vs multi is R4; same R5.

---

### 13.3 Purpose: **info** — “information of miscelaneous or arbitrary kind” (variables / values)

R5 `info` · R4 families: `slot_bind` | `literal` | `derived`

#### Example A — named slot binding

```
R5  PURPOSE     info
R4  FAMILY      slot_bind
R3  ALGORITHM   var_decl
R2  MICRO       STORE slot / LDI slot, imm
R1  REGION      (function or top-level span)
R0  SURFACE     LDI/STORE, slot index, optional name in names[]
form:  name ↔ slot holds typed value
big_O: O(1) access
```

#### Example B — literal / immediate

```
R5  PURPOSE     info
R4  FAMILY      literal
R3  ALGORITHM   push_const
R2  MICRO       PUSHI imm
R1  REGION      expression span
R0  SURFACE     PUSHI i32 bits
form:  constant value on stack
big_O: O(1)
```

#### Example C — derived value (expression result)

```
R5  PURPOSE     info
R4  FAMILY      derived
R3  ALGORITHM   expr_result
R2  MICRO       LOAD/ops/STORE or leave on stack (HERE)
R1  REGION      assignment or call out[]
R0  SURFACE     arithmetic / CALL / superops
form:  value computed from other info
big_O: O(expr)
```

All three abstract to **info**. Binding vs literal vs derived is R4.

---

### 13.4 Shared abstract cores (mutations of each other)

| R5 concept | Shared meaning | R4 mutations (examples) |
| --- | --- | --- |
| **repeat** | continue body until stop condition | counted_loop, cond_loop, trap_loop |
| **decision** | choose path from a test | binary_branch, multi_branch |
| **info** | hold or produce a value | slot_bind, literal, derived |

Specialization = pick R4/R3/R2 under the same R5.\
Abstraction = climb any of the three example trees back to that R5 label.

### 13.5 Catalog seed ids (recognize)

```
repeat/counted_loop/trap_counted
repeat/counted_loop/for_affine
repeat/cond_loop/while_cond
decision/binary_branch/if_else
decision/binary_branch/if_then
decision/multi_branch/switch_jt
info/slot_bind/var_decl
info/literal/push_const
info/derived/expr_result
```

---

## 14. Purpose: **wait** — suspend progress until a condition in time/events

R5 `wait` · R4 families: `delay_block` | `async_wait` | `event_listen` | `poll`

### 14.1 Subtypes (R4)

| R4 family | Meaning | Vulcan / IR today |
| --- | --- | --- |
| **delay_block** | do nothing for N ms, then resume | `delay_ms(N)` → `RSVM_OP_DELAY_MS` |
| **async_wait** | do other work while waiting, then resume | not first-class yet |
| **event_listen** | resume when event/notify fires | host/notify hooks future |
| **poll** | repeatedly check condition until true | while + sensor/read loop |

### 14.2 Lower layer (R2–R3 implementation concerns)

- timer precision / host clock (`MILLIS`)
- mutex / critical section if shared state across waiters
- scheduler yield vs busy-spin (poll)
- blocking vs non-blocking host API

### 14.3 Example tree — blocking delay

```
R5  PURPOSE     wait
R4  FAMILY      delay_block
R3  ALGORITHM   wait_delay
R2  MICRO       RSVM_OP_DELAY_MS + i32 ms
R1  REGION      expression / loop body span
R0  SURFACE     DELAY_MS encoding
form: blocking wait N ms then resume (no concurrent work)
pure: 0  (side effect on wall clock / progress)
```

**Important:** any region containing `wait` is **impure** for superopt / closed-form rewrite.

---

## 15. Hypothesis lock — transforms, ontology, attributes (2026-08-24)

### Claim (supported by the wait/loop/if case)

All “work” is a **transform of information under a rule set** (lambda-like).\
Concepts live on an **ontology tree** (ranks R5→R0).\
Moving on that tree is **derivative / anti-derivative of concepts**:

| Direction | Calculus metaphor | Ontology move | Effect |
| --- | --- | --- | --- |
| **Specialize** | derivative | R5 → R4 → … → R0 | gain context, beacons, IR shape |
| **Abstract** | anti-derivative | R0 → … → R5 | drop circumstance; recover platonic form F |

- **F** = platonic / highest form (R5 purpose: `repeat`, `decision`, `wait`, `info`, …).
- **f + c** = specialized instance; **c** is circumstantial data (bounds, ms, slot ids, string text) that is derived away when abstracting.

The stacked report on `do n` + `if` + `delay_ms` is exactly one F-family (`repeat`) with children `decision` and `wait`, and impurity from wait blocking closed-form rewrite — i.e. the tree + transform attributes did the right thing.

### Transform attributes (next axis — required)

Every transform / hit should carry (at least):

| Attribute | Values (v1) | Role |
| --- | --- | --- |
| **purity** | pure | impure | side effects / wall clock / IO |
| **predictability** | predictable | partial | unpredictable | can we closed-form or only trap? |
| **progress** | finite | data-dependent | external | loop bounds, wait, sensors |
| **concurrency** | sequential | async | contended | wait subtypes, mutex later |

Rules of thumb already implied by the case:

- `delay_block` → impure, predictable duration, external progress, sequential
- pure `counted_inc` → pure, predictable, finite, sequential → O(1) rewrite OK
- sensor poll → impure, partial/unpredictable, external → never superopt

Ontology without these attributes is classification only; **with** them, L5 can decide rewrite vs leave vs deposit.

### Status

- Ontology tree + rule catalog + structure report: **on track**
- Derivative/anti-derivative as rank climb/descend: **on track**
- Transform attributes as first-class fields on hits: **next engineering step**
- Full lambda/e-graph equality of pure terms: **later, under pure+predictable only**

---

## 16. Discovery consolidation — forms, plugins, scientific method (2026-08-24)

### 16.1 Formal logic layers

| Layer | Role |
| --- | --- |
| **Wide context** | whole-program / function region graph; parent/child hits |
| **Focus** | single region or expression span; micro schema match |
| **Theory** | Horn-like rules: features ⇒ form ∧ attributes ∧ conf |

The algorithm “theorizes” by asserting facts (L1), querying plugins (L2–L4), stacking hits into a tree, then L5 decides actions from **attributes**, not names alone.

### 16.2 How forms are determined

1. **Region topology** → candidate R5/R4 (repeat/decision/wait/info).
2. **Opcode/beacon facts** → R2/R3 plugins fire.
3. **Highest-confidence non-dup** hits retained; parents link stacks.
4. **Attributes** filled from purity scan + plugin defaults.
5. Form string = general_form of winning plugin (F or specialized F+c text).

No LLM required for v1: forms = **matched axiom plugins**.

### 16.3 Plugins = R5-anchored form packs

Each catalog row is a **plugin**:

```
purpose (R5 platonic F)
  family (R4)
  id / general_form
  default attributes
  match(IR span) → confidence
```

New understanding = new plugin, not new VM opcode.

### 16.4 f vs F and anti-derivatives (detail)

- **F** — platonic purpose at R5 (`wait`, `repeat`, …).
- **f** — specialized method/algorithm (R4–R3).
- **c** — circumstance (ms=3, n=8, slot names, string text) at R0–R1.

**Anti-derivative (abstract):** drop c and method detail → recover F.\
**Derivative (specialize):** attach method + c → concrete IR.

Two-way walk:

```
IR  --match-->  f+c  --abstract-->  F
F   --plugin-->  candidate f  --bind c-->  rewrite / reject
```

### 16.5 Attributes first-class

On every hit:

- `pure`, `predictable`, `progress`, `concurrency`
- `rsvm_hit_rewrite_allowed(h)` ⇔ pure ∧ predictable ∧ progress≠external

L5 must read these; never rewrite on external progress (wait).

### 16.6 Autonomous “scientific method” (bounded)

Not open-ended AGI. Bounded loop:

```
hypothesize: plugin match on span (theory)
predict:     attributes + big_O
test:        trial rewrite OR attribute consistency check
accept/reject: confidence threshold + trial/revert
recurse:     on child spans / parent stack only
halt:        max depth, max hits, no new facts, or rewrite fixed-point
```

**Recursion** = analyze children of regions, then parents (tree walk).\
**Infinite loop detection** = visited-set on (span, plugin_id); max_out; step_limit already on VM.\
**“How do I break out?”** = future goal note (meta), not required for classification.

Form derivation test: feed only highly specific IR (f+c); expect abstract F in `purpose` and rewrite_allowed consistent with attributes.

---

## 17. ikitaku — living-math layer (2026-08-24)

**ikitaku** (生きた数学): parametric multi-hypothesis thinking over structure plugins.

### Config

- `attention_desire` — try/depth budget
- `allow_infeasible` — stretch hypos to escape local maxima
- `novelty_focus_boost` — extra tries on novel spans
- `min_accept_conf` / `min_novel_conf`

### Thinking methods

abductive | deductive | inductive | analogical | search

### Pipeline

1. Structure hits (L1–L4)
2. Deductive hypos from hits
3. Search stretch hypos (infeasible branch)
4. Novelty detection (boost attention)
5. Specialize F → f+c from span context
6. Accept best feasible conf ≥ threshold

### Files

`vm/rs_vm_ikitaku.hpp`, `vm/rs_vm_ikitaku.cpp`

### Tests observed

- Known loop+if+wait: F=decision/repeat/wait; specialize → `repeat_impure_timed`, `wait_delay`; rewrite=no on wait
- Pure counted: accepts `counted_inc` conf=92 rewrite=yes
- Sparse no regions: **NOVEL** program-level hypo (breakthrough path)

---

## 18. Evidence clustering + metacog superloop (2026-08-24)

**Priority feature:** `ikitaku_cluster_*` + `ikitaku_analyze_ex`.

### Superloop phases

OBSERVE → COMPARE → CLUSTER → DIALECTIC → SYNTH → SPECIALIZE → PROMOTE → ACCEPT

### Subloops (per cluster)

- Jaccard pairwise (sim min/avg/max)
- Hegelian pure vs impure → resolved child
- Ambiguous mark when 40≤sim_avg&lt;70
- Promote when members≥2 → R5 candidate with conf margins + evidence indices

### Debug

Each cluster carries `debug[]` trace: members, sim stats, dialectic, promotion. `ikitaku_cluster_report` dumps full state for inspection.

---

## 19. Experiment variables, undo, agenda, query stub (2026-08-24)

- Variable roles: determinant / co-determinant / irrelevant via |corr| thresholds.
- Inverse-transform probes (`asin`, artanh, |y|, y²) always **unsure**.
- Think cost: wall seconds + op count + crude big-O guess.
- Agenda: idle “nothing on schedule” vs extreme-priority vs budget vs desperation/confusion.
- Query origin: stub entities (self/human/any/webhook/online); English NL later.
- Debug post: `ikitaku_debug_post(url, text)` — pass webhook at call site, do not commit secrets.