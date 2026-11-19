# RS-VM v2.1 — Vulcan light interpreter

## Layout

```
rs_vm/
  vm/                 # core runtime + parser + opcodes + host
  lang/               # variables / objects / functional stubs
  vm_modules/         # @property runtime hooks
  debugger/           # stats, error messenger, ref table dump
  examples/           # .vul programs
  rs_vm_desktop_test.cpp
```

## Desktop build

```bash
cd artifacts/rs_vm
g++ -std=c++17 -O2 -I. -Ivm -Idebugger -o /tmp/rsvm_test \
    vm/rs_vm.cpp vm/rs_vm_parse.cpp rs_vm_desktop_test.cpp debugger/rs_dbg.cpp \
    vm_modules/vm_mdl_*/vm_mdl_*.cpp
/tmp/rsvm_test examples/abracadabra.vul
```

## New surface (this pass)

| Feature | Syntax / API |
|---------|----------------|
| step budget | `set_step_depth 2000000;` |
| string length | `strlen(s)` / `size(s)` / `s.len` / `s.size` / `s.length` |
| char print | `printc(c);` `printnl();` |
| monitors | `}@monitor_time @monitor_ram @monitor_execs;` |
| stats dump | automatic after each run (debugger/) |
| ref table | compile-time use counts (debugger/) |
| .bvul | `rsvm_save_bvul` / `rsvm_load_bvul` |

## Monitors

On function return:
- `@monitor_time` → `[monitor_time] name: N ms`
- `@monitor_ram` → `[monitor_ram] name: +N bytes (heap+str)`
- `@monitor_execs` → `[monitor_execs] call K of name`

## Notes

- Values are i32; practical loop size limited by `set_step_depth` (default 200000).
- Nested `do n` uses distinct counters (`__i0`, `__i1`, …).
- Top-level inits run before `main`.

## Watch / ESP port

See `ESP_PORT_NOTES.md` and `MS_vulcanapp.*` (AppBase console test).

## Inheritance & composition

```vulcan
struct Animal { i32 legs; }
struct Cat : Animal { i32 lives; }   // also: extends Animal  /  @extends(Animal)

struct Engine { i32 hp; }
struct Car {
  @compose Engine engine;  // or just: Engine engine;
  i32 wheels;
}
// car.engine.hp = 120;  // chained field access
```

## Performance backends

| Path | What | Typical use |
|------|------|-------------|
| Interpreter | `rsvm_compile` + `rsvm_run` | interactive, ESP watch |
| Bytecode (.bvul) | skip parse, same interp | cached scripts |
| AOT-C | `rsvm_cgen_emit` → `gcc -O2` | desktop speed boost |

Bench (200k increments, desktop -O2):

```
interp run  ~13.5 ms
bvul run    ~13.2 ms   (parse was only ~0.02 ms)
aot-c run   ~3.3 ms    (~4× interp; gcc cost ~300 ms once)
native C    ~0.08 ms   (ceiling)
```

```bash
g++ -std=c++17 -O2 -I. -Ivm -Idebugger -Ilang/codegen -o /tmp/bench_vulcan \
  bench_vulcan.cpp vm/rs_vm.cpp vm/rs_vm_parse.cpp debugger/rs_dbg.cpp \
  lang/codegen/rs_cgen.cpp vm_modules/vm_mdl_*/vm_mdl_*.cpp
/tmp/bench_vulcan
```

AOT entry points: `lang/codegen/rs_cgen.hpp` — `rsvm_cgen_write_file(vm, "out.c")`.

## Loop trapdoor & autounroll (v2.3+)

**Trapdoor is DEFAULT.** Classic JMP only with `set_loops_trapdoor 0` or `@trapdoor_disable`.

| Technique | Mechanism |
|-----------|-----------|
| Classic `do n` | LOAD/GT/JZ/body/SUB/JMP (`@trapdoor_disable`) |
| Trapdoor (default) | `OP_TRAP_LOOP`: C `for` over body range |
| `@autounroll(n)` | Const count ≤ n → memcpy body n times |
| Superops | `INC_SLOT` / `ADD_IMM_SLOT` for `x = x + N` |

```vulcan
do n[1000] { i = i + 1; }          // trapdoor by default
set_loops_trapdoor 0;              // or @trapdoor_disable on fn
do n[1000] { i = i + 1; }          // classic
```

Bench (200k increments, -O2): classic ~10 ms; **trapdoor+INC_SLOT ~1.0 ms**; native ~0.07 ms.

## Superops, regions, overload, inline

- Opcodes `0xBA–0xBF`: INC_SLOT, ADD_IMM_SLOT, LOAD_ADD_STORE, PACK2, LOAD_PUSHI
- Regions: start/end PC for LOOP/TRAP/IF/SWITCH/JT at compile
- Dense switch → OP_SWITCH_JT
- Call `foo in[6]` resolves `i32_foo` / `int_foo` if needed (float → `f32_foo`)
- Type keywords banned as var/fn names
- Small fns (`body_len < 48`, no `@noinline`) inlined at call site

## Language surface (v2.2+)

**Calls** (declaration-style; `()` still works):
```vulcan
add in[3, 4] out[s];
bump in[add in[10,20] out[here]] out[t];  // HERE = leave on stack
work in[];                                  // only if fn has no outs
work in[x] out[void];                       // discard outs
```

**Flow:** `if`/`when [cond] { } else { }`, `break;` / `continue;` inside trapdoor `do n`, `@unlimited` on fn.

**Bitwise:** `& | ^ ~ << >>` (hex `0xFF` ok)

**Files (host):** `file_read(path)`, `file_write(path, data)` — desktop fopen; ESP → wire to DataPool/SD.

**Arrays:** `a = arr_new(n); a[i] = v; print(a[i]);` (1D i32/heap). Multi-dim `@dimensions(n)` still TODO.

**Types (aliases → i32 storage for now):** `ui8/ui16/ui32/ui64`, `u8…`, `f16/f32`, `uf16/uf32`.

## v2.2 — types, multi-dim arrays, C flow, call syntax

**Calls:** only `name in[...] out[...]` / `out[here]` / `out[void]` / `in[]` if no outs. `name()` removed.

**Types (first-class tags):** `i8/i16/i32`, `ui8/ui16/ui32`, `f16` (IEEE binary16 bits), `f32` (IEEE binary32). Arithmetic promotes float operands to f32 ops.

**Arrays:**
```vulcan
a = arr_new(10);       // 1D
m = arr_new(2, 3);     // 2D row-major
m[1, 2] = 42;
print(m[1, 2]);
```

**Flow (C-like + Vulcan):**
- `if` / `when` / `else`
- `while [cond] { }`
- `for [i=0; i<n; i=i+1] { }`
- `switch [v] { case [k] { } default { } }`
- `do n[...]` (+ trapdoor) with `break` / `continue`
- `@unlimited` on functions

## v2.4+ — superops, algo rewrite, superoptimize

**Flags**
- `set_limitpassoptimize n` / `@limitpassoptimize(n)` — 0..4 passes
- `set_packed_instructions 1` / `@packed_instructions`
- `set_enable_algo_rewrite 1` / `@enable_algo_rewrite`
- `set_superoptimize_enable 1` / `@superoptimize_enable`

**Passes**
1. Superop residue  
2. Const fold + **strength reduction** (`* 2^k` → `<< k`, `/ 2^k` → `>> k`)  
3. PACK2 (when packed)  
4. Algo rewrite (pure trap bodies) + superoptimize sweep  

**Algo rules** (pure math only — no I/O/GPIO/calls/jumps)
| Rule | Pattern | Result |
|------|---------|--------|
| pure_counted_inc | `x = x + c` in trap | `x += n*c` O(1) |
| affine_fx | `x = a*x + b` | closed form `a^n` (const n≤40) |
| feed_chain_sum | `s += t; t++` | triangular sum O(1) |

**Magic / experimental**
- `RSVM_OP_ISQRT` (0xBF) — Quake `0x5F3759DF` invsqrt on f32 bits in i32
- Unit-circle Q15 LUTs in `precomputed_math/` for trig range reduction (next: auto-wrap before LUT)
- Chain-deposit / LOAD-STORE noop elim under superoptimize

**Bench (200k pure inc)**
- classic trapdoor: ~1 ms  
- algo rewrite: ~0.001 ms (steps≈13)  
- strength `*8` → one SHL, MUL gone  
