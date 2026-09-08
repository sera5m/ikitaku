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

Read-only phosphor viewer. Source in `ui/`.

- **thought graph** — bags, ranks, association
- **neurons** — matrix of dots. R5+ always lit as cells; R4 only when fired; R3–R0 appear when you select an R4. Connections follow parent/strategy/family. Field is black. Idle phosphor `#883400`.

Cells are not editable. Stimulus is a pattern bag or a file.

Engine C++ lives in `engine/`. Catalogs in `catalogs/`.
