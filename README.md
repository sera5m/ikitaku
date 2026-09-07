# ikitaku

Layered reasoner. **Not** the Vulcan VM. **Not** the watch OS.

Ikitaku reads structured bags (Vulcan bytecode *or* a numeric series *or* a world delta) and writes hypotheses, association edges, and a thought trace.
You watch. You do not poke the cells.

## Depends on

- [sera5m/vulcan-lang](https://github.com/sera5m/vulcan-lang) — language + desktop VM (`cpp_vm`). Clone as a **sibling**:

      vulcan-lang/
      ikitaku/

- [sera5m/vulcan-ide](https://github.com/sera5m/vulcan-ide) — optional GTK IDE (Wayland). Iki does not edit neurons from the IDE; the IDE can dump bytecode for `ikitaku_analyze`.

Watch firmware / ESP host is **out of this repo**.

## What it is

Work is `F(x)+c`: platonic verb, data, circumstance.

```
R9 remainder     R8 is this useful     R7 right task
R6 strategy      R5 verb               R4 method family
R3 genus/attrs   R2 micro schema       R1 region       R0 surface
```

Abstract = strip `+c` (zoom out). De-abstract = add circumstance (zoom in).
Association is attribute overlap, not a neural net.

## Build (desktop, Linux)

```bash
cmake -S . -B build -DVULCAN_LANG_DIR=../vulcan-lang
cmake --build build
./build/iki_trace --stimulus vanish
```

Standalone (`IKITAKU_STANDALONE`) compiles a trace host. Full `ikitaku_analyze` still needs `rsvm_t` from vulcan-lang.

## Graph UI

Read-only. Nodes are bags (rank + verb + conf). Edges are parent, assoc, cause, or the live think-trace. No weight sliders.

Engine C++ lives in `engine/` (copy from the split tree). Catalogs in `catalogs/`.
