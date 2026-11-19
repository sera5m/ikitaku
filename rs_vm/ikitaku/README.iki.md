# ikitaku

Layered reasoner. **Not** the Vulcan VM. **Not** the watch OS.

Ikitaku reads structured bags (Vulcan bytecode *or* a numeric series *or* a world delta) and writes hypotheses, association edges, and a thought trace.
You watch. You do not poke the cells.

**Theory (2026-09):** we built **half an organ**. Iki is the prefrontal stack (plan / WM / why / gate). The other cortices were missing, which is why it could reason and still stall. Map and typed-cell header: [engine/neurons/BRAIN_MAP.md](engine/neurons/BRAIN_MAP.md).

## Depends on

- [sera5m/vulcan-lang](https://github.com/sera5m/vulcan-lang) — language + desktop VM (`cpp_vm`). Clone as a **sibling**:

      vulcan-lang/
      ikitaku/

- [sera5m/vulcan-ide](https://github.com/sera5m/vulcan-ide) — optional GTK IDE (Wayland or X11). Iki does not edit neurons from the IDE; the IDE can dump bytecode for `ikitaku_analyze`.

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

Cells are **typed** (`HYPO ASSOC CLUSTER CONCEPT VERB FAMILY VALUE`). Sentiment is a `valence` field, not a neuron species. Far affinity stays far.

## Linux Mint (and any Debian desktop)

Yes. Mint is Debian/Ubuntu userspace. g++ and cmake are enough for the trace host.

```bash
sudo apt update
sudo apt install -y build-essential cmake git

git clone https://github.com/sera5m/vulcan-lang.git
git clone https://github.com/sera5m/ikitaku.git
cd ikitaku
cmake -S . -B build -DVULCAN_LANG_DIR=../vulcan-lang
cmake --build build
./build/iki_trace --stimulus vanish
```

Optional IDE (GTK; Cinnamon/X11 or Wayland both fine):

```bash
sudo apt install -y libgtk-4-dev
# then follow vulcan-ide README
```

The phosphor **graph UI** under `ui/` is read-only source (TanStack/React). It is not a second `npm` app in this repo. A second person can read the theory, run `iki_trace`, and watch traces. Full analyze still wants `rsvm_t` from vulcan-lang (`IKITAKU_STANDALONE` is the thin host).

## Graph UI (this tree)

Read-only phosphor viewer. Source in `ui/`.

- **thought graph** — bags, ranks, association
- **neurons** — matrix of dots. R5+ always lit as cells; R4 only when fired; R3–R0 appear when you select an R4.
- bottom strip: **text / crt / scan / vig** bloom. `-` / `+` hide chrome.

Cells are not editable. Stimulus is a pattern bag or a file.

Engine C++ lives in `engine/`. Catalogs in `catalogs/`. Typed-cell header in `engine/neurons/`.
