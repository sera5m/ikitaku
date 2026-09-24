# Mode network and the memory cut

2026-09-23. Notes from the organ audit. Not implemented.

Iki is not a whole brain, and it is not "the prefrontal lobe with more
features bolted on." Two corrections:

1. Iki should be the **frontoparietal task mode**: the current rule, a
   small scratch, and bias lines onto other organs. PFC is one node
   inside that mode, not the mode.
2. Memory is the first organ to remake. Association today remembers
   **table position**, not **links**. Every other section reads through
   that table, so renaming them does nothing until the map exists.

Older map (half-organ, "iki = PFC") stays in
[neurons/BRAIN_MAP.md](neurons/BRAIN_MAP.md). This file supersedes the
"this is iki" row.

## Modes, not more prefrontal

Large-scale networks are anti-correlated gain patterns.

| Network | Core | When it is up |
|---|---|---|
| Default mode (DMN) | medial PFC, posterior cingulate, angular gyrus, hippocampus | no external task. Episode, self, simulation. |
| Dorsal attention (DAN) | intraparietal sulcus, frontal eye fields | where to look. External space. |
| Frontoparietal control (FPN) | dorsolateral PFC, inferior parietal | the **task set**. Holds the rule and biases everyone else. |
| Salience (SN) | anterior insula, dorsal anterior cingulate | "this matters." Flips DMN ↔ task. |

The old name "task-positive network" mixed DAN and FPN. The piece Iki
should become is the FPN, not that whole blob. Attention-where and the
salience switch stay other organs.

A mode does not sense, remember, or move. It writes a temporary gain
onto those systems so only task-relevant channels drive the act.
Electrically that is biased competition: same cells, different \(g\) on
the task-relevant subset. Not a new neuron species.

```
salience  --sets mode-->  task (Iki / FPN)   XOR   default (replay, self, unsolved gaps)
                              |                         |
                              +---- bias / read --------+---- sensory, assoc, episode,
                                                              valence, selector, timing
```

XOR is required. Both modes may read the others. They must not both
drive at full gain. Boredom that only fidgets is a missing
salience→DMN route, not a missing prefrontal feature. Iki during a goal
and Iki during replay are the same graph at different gains.

## What task-mode keeps and what it drops

Keeps:

- current task set (goal, constraints, which verbs are legal)
- working-memory slots for that set only
- bias lines onto other organs
- halt when the same loop does not drop residual

Does not keep:

- sensing (R0 watch / world bus)
- episode store (diary)
- the association fabric
- valence
- the action brake
- timed loops (cerebellum / Vulcan)

## Memory failure

`g_affinity` is a 64×64 matrix indexed by a hardcoded string list
(`AFF_KEYS` in `ikitaku_assoc.inc.cpp`). `ikitaku_affinity()` looks up a
name, gets a row number, and reads `g_affinity[i][j]`. A name not in
those 64 strings collapses into slot 7 (`unknown`). Hebb only bumps
pairs that already occupy those rows, and it stops after
`IKITAKU_AFF_HEBB_CAP` (64) updates.

The embedding is the same mistake: 32 hand-assigned axes (repeat is
dimension 6, map is 11), not a code that travels with the concept.

So "near" means near in the table. Rename a form, insert a row, or
learn a name outside the list, and the link does not follow the thing.
It falls into the junk bucket or stays glued to the old index.

| Store | What it is | Why recall fails |
|---|---|---|
| `g_affinity[64][64]` | dense co-occurrence of a fixed vocab | position, not edge |
| `ikitaku_embed_t.vec[32]` | hand basis | same axes forever |
| session `hypos[32]` | `parent_a` / `parent_b` are array indices | synthesis breaks if the array shifts |
| `r3.slab` | records with `uint32_t id` and up to 8 links | the only real graph, and affinity does not query it. Cap 8. Path hardcoded. |
| diary | timestamped text | a log. Cannot follow "who points here" |
| `learned.cat` | append, max 16 | a notebook |
| home slots | 8 directories | place on disk |
| values | 16-row table | valence not on the link |

`ikitaku_id32` and `ikitaku_r3_rec_t` already point the right way.
They were never the store the rest of the system reads.

Target:

- Every form, episode, and act is an id.
- A link is `(src, dst, weight, kind, eligibility)`.
- Lookup is "start at these ids, spread." Row order does not matter.
- The 64 keys become seed nodes, not the address space. Unknown is a new id, not slot 7.
- Session hypos point at ids. `parent_a` as an array index goes away.
- Hebb writes an edge only if a modulator bit is set. The edge keeps an eligibility trace so a later error can still change it.

## Shortcomings

1. No content-addressed memory. See above.
2. No split between episode and semantic. Diary is prose. R3/catalog is forms. Nothing binds an episode to the cells it should strengthen, and nothing replays it back.
3. Task mode owns the other organs. Rank gates, Hebb, boredom, values, and world-watching share one process and one hypo list. No gain line.
4. No mode switch. `ikitaku_boredom_tick` picks a wired fidget. It does not inhibit the task set and hand the bus to replay.
5. Salience is a residual number on the hypo, not a faster organ that can veto the loop.
6. Selector is a score. R7 picks A or B. There is no tonic inhibit-all, release-one.
7. Valence does not multiply a link. `iki_cell_hdr_t.valence` and `ikitaku_values.cat` both exist and neither changes completion.
8. Sensors and effectors are stubs. No feature code arrives already separated from the task set.
9. Time is a file stamp (`ikitaku_home_time_span`), not an eligibility trace.
10. Plasticity is one-speed Hebb. No third factor. Co-occurrence writes the matrix if the names are in the 64. Surprise and confirmation do not gate the write.
11. Working memory is the session. 32 hypos, gone when the call returns.
12. Self-model is `diary/self_changes` text. No pointer from "I changed strategy X" back to the cells of X.
13. Priors are source code. `AFF_KEYS` and the `aff_set` block are the ontology. Learning cannot add a row.

## Twelve sections

Do memory first. Remaking the other eleven on top of the 64×64 just
adds more readers of the same table. Task-mode split is the second cut.

| Organ | Job | Build from | Do not |
|---|---|---|---|
| Semantic net | stable ids, edges with weight and type | promote `r3` links to the store | keep the 64×64 as authority |
| Episode index | cue → which nodes were co-active, when | diary as pointers into the net, plus a timestamp | prose as the only trace |
| Task mode (Iki) | current rule, scratch, bias onto others | R6–R9, hypo scratch | store the net or sense the world |
| Default mode | replay, sample an unchallenged high-confidence node | boredom path, only when task gain is low | a second hypothesizer |
| Salience | residual / surprise → which mode owns the output | hunger, confusion, world-delta | a field on the hypo |
| Thalamic gate | one bus; task bias scales what enters | new, thin | direct writes into hypos |
| Association fabric | completion along edges | cosine on link sets | hand 32-d basis as identity |
| Valence | tag on node and edge; multiplies completion | header `valence`, values catalog | an emotion neuron species |
| Selector | tonic brake, release one act, surround-suppress | R7 as inhibit-all | argmax inside the task list |
| Timing | eligibility, lead/lag, wait axis | Vulcan trap loops | fold into R6 |
| World bus | typed features in, typed acts out | env bags | teach task mode to be the eye |
| Modulators | write-enable and "plastic now" | budget + novelty as gates on the edge write | more ungated Hebb |

## Order

1. Id + edge map. Seed it with today's `AFF_KEYS`. Unknown allocates.
2. Point hypos at ids. Stop using array indices as parents.
3. Split task gain from default-mode gain. Salience owns the mutex.
4. Eligibility on the edge. Modulator bit required to commit weight.
5. Diary lines become episode records of node ids.
6. Selector as inhibit-all. Valence multiplies spread, it does not live in a side file.

Do not add verbs, and do not merge this into a single `neuron_t`.
