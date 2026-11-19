# Port: September ikitaku → Toplethos 1.2 tripar

The reasoner under `rs_vm/ikitaku/` is the September engine. It still
analyzes an `rsvm_t` image (`code[]`, `RSVM_OP_*`, regions). Current
Toplethos does not produce that image.

## What current Toplethos is

Release **1.2 tripar**, spec 1.0, system UMMES. Spec is `USE.md` in
`sera5m/toplethos-lang`. There is no 1.3 in that tree.

- Source is `.tpth_s`. Call form is `name in[args]`. Types are
  `i4`–`i64` / `ui*` / `f64` / `bool` / `object`. No `string`, no `struct`
  keyword, no namespace.
- One front end lowers to three-address bytecode:
  `tp::Program` / `BcFunc` / `Insn` in `compiler/include/tp/bytecode.hpp`.
- `interpret()` is the oracle (`tpcc --run`). `tpcc --asm` is the amd64
  listing. `--jit` compiles an `@inlined` pure function while the
  interpreter is still running.

## What this port does

`ikitaku_tp_bridge` projects that bytecode onto the old image so the
existing `ikitaku_analyze(const rsvm_t*)` still runs. The higher layers
(session, association, series, metacog, rank gates) are unchanged.

```c
iki_tp_insn_t ins[] = { /* filled from tp::Insn, op = (uint16_t)in.op */ };
rsvm_t vm;
ikitaku_tp_fill(&vm, ins, n);
ikitaku_analyze(&vm, &cfg, &session);
```

With `IKITAKU_HAVE_TP` and `tp/bytecode.hpp` on the include path:

```cpp
ikitaku_tp_fill_program(&vm, prog);
```

## Opcode map (lossy)

| tp::Op | RSVM_OP |
|---|---|
| Add Sub Mul Div Rem | ADD SUB MUL DIV MOD |
| Eq Ne Lt Le Gt Ge | EQ NE LT LE GT GE |
| Print | PRINT_V |
| Call | CALL |
| Ret | RET0 |
| Load Store Addr | LOAD STORE |
| Jmp with back edge | TRAP_LOOP + region |
| Br / BrRange | JZ (loop region) |
| Clock Watch | DELAY_MS |
| LoadBmp LoadBytes | FREAD |
| SaveBmp | FWRITE |
| Lock Unlock | PROP_CHK |
| SlotAdd | ADD_IMM_SLOT |

Host ops the old walker never looked for (Fork, Start, Threads, Freemem)
become NOP. That is enough for the catalog matchers; it is not a second
interpreter.

## Build

The September tree still needs its own `rs_vm/vm` headers. Point the
include path at `rs_vm` and link `ikitaku_tp_bridge.cpp` plus the
existing `ikitaku.cpp`. To consume a live `tp::Program`, also pass
`-DIKITAKU_HAVE_TP` and the toplethos-lang `compiler/include` path.
