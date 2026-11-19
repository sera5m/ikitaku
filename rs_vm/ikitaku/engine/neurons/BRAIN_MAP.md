# Half an organ

We built a prefrontal stack and called it a mind.

That is not a failure of the ladder. It is a map error. Iki is **one
section**. A brain is several sections talking over thin buses. Missing
the other cortices is what made iki look “almost” — it could plan, score,
and wonder, and then stall because there was no V1 to give it features,
no motor to close a loop in the world, no cheap valence bus, no
hippocampal index that actually time-stamps an episode.

Funny. Also the next work.

## What iki *is*

Closest analog: **prefrontal + a thin hippocampal index**.

Plans, working memory, “why”, gate, remainder, budgeted hypothesis.
Not V1. Not motor. Not a whole brain.

Work is still `F(x)+c`. Abstract = strip `+c`. De-abstract = add
circumstance. Association is attribute overlap (Jaccard + cosine +
affinity), not a neural net.

```
R9 remainder     R8 is this useful     R7 right task
R6 strategy      R5 verb               R4 method family
R3 genus/attrs   R2 micro schema       R1 region       R0 surface
```

## Cortices we have vs do not

| Brain piece | Job | Iki analog | Status |
|---|---|---|---|
| V1 / sensory | sensors → features | R0–R2 / watch slots | thin |
| Association cortex | bind, analogize | `g_affinity` + Jaccard + embed | **real** |
| Hippocampus | episode index, pattern completion | diary / experience bags | stub |
| Basal ganglia | pick an action, inhibit the rest | R7 gate + hunt poles | real-ish |
| **Prefrontal** | plans, WM, why | **R6–R9 + hypo stack + budget** | **this is iki** |
| Amygdala / valence | cheap value on a percept | `valence` on `iki_cell_hdr_t` | table, not a species |
| Cerebellum | timed precise loops | wait / trap / VM | Vulcan, not iki |
| Brainstem | keep the lights on | idle / boredom tick | thin |
| Motor / speech | act in the world | emit / env / watch_run | delegated bags |

Sections work together by **buses**, not by becoming one cell type.
Sensory writes a bag. Association scores it. PFC keeps a hypo stack and a
budget. Basal-ganglia-ish gate picks. Hippocampus stores a compressed
trace. Valence tags the bag so later associate is biased. Motor is
someone else's job (Vulcan / env).

That split is already the files: `core` / `assoc` / `cluster` /
`concept` / `mcog` / `life`. Those *are* the organelles. `neurons/`
names them so instances share a header without merging kinds.

## What not to do

- One TensorFlow neuron for everything. Erases the ladder.
- “Associative neurons that detect values.” Values are **valence**.
  Assoc detects **near/far on an axis**.
- Sentiment as a specialized cell type. Put `valence` on the header.
- Pretend PFC-without-V1 is a person. It is a planner with almost no
  senses. Fill cortices as **modules**, not as spike shapes.

## Next (parts, not the whole organ)

1. Sensory bus that is not a costume of English (R0–R2 actually fired
   by world/watch, not by a prompt).
2. Valence on the header, cheap, mutating Hebb only when pole is local.
3. Hippocampal index that is time + novelty, not a folder of txt.
4. Motor as a bag-delegate: PFC watches, lower layer runs.
5. Instances of iki = **sessions with the same kinds**, not new neuron
   geometries (teacher bag, world bag, another self).

Do the parts. Do not wait for a complete homunculus.
