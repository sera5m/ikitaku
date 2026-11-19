// bench_super_ops — naive vs optimized ops/s for Vulcan superopt stack
// Build:
//   g++ -std=c++17 -O2 -I. -Ivm -Idebugger -Ilang/codegen -o /tmp/bench_super_ops \
//     bench_super_ops.cpp vm/rs_vm.cpp vm/rs_vm_parse.cpp vm/rs_vm_opt.cpp \
//     debugger/rs_dbg.cpp lang/codegen/rs_cgen.cpp vm_modules/vm_mdl_*/vm_mdl_*.cpp
#include "rs_vm.hpp"
#include "rs_vm_parse.hpp"
#include "rs_vm_opcodes.h"
#include "rs_vm_opt.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
static void host_print_i32(int32_t, void*) {}
static void host_print_str(const char*, uint8_t, void*) {}
static void host_print_char(char, void*) {}
static void quiet(rsvm_t* vm) {
    rsvm_host_t h; memset(&h, 0, sizeof h);
    h.print_i32 = host_print_i32; h.print_str = host_print_str; h.print_char = host_print_char;
    rsvm_set_host(vm, &h);
}

struct result {
    const char* name;
    uint32_t steps;
    double ms;
    uint32_t code_len;
    int status_ok;
    char log[256];
};

static result run_src(const char* name, const char* src, uint64_t logical_ops) {
    result r; memset(&r, 0, sizeof r); r.name = name;
    rsvm_t vm; rsvm_init(&vm); quiet(&vm);
    rsvm_parse_err_t err;
    if (rsvm_compile(&vm, src, &err) != RSVM_OK) {
        printf("FAIL %s: %s\n", name, err.message);
        return r;
    }
    r.code_len = vm.code_len;
    strncpy(r.log, vm.opt_log, sizeof r.log - 1);
    double t0 = now_s();
    rsvm_status_t st = rsvm_run(&vm);
    double t1 = now_s();
    r.ms = (t1 - t0) * 1000.0;
    r.steps = vm.steps;
    r.status_ok = (st == RSVM_OK);
    (void)logical_ops;
    return r;
}

static void report(const result* r, uint64_t logical_ops) {
    double ops_s = (r->ms > 0.0) ? (logical_ops / (r->ms / 1000.0)) : 0.0;
    double step_s = (r->ms > 0.0) ? (r->steps / (r->ms / 1000.0)) : 0.0;
    printf("%-28s  code=%4u  steps=%8u  time=%8.3f ms  "
           "logical_ops/s=%10.0f  bytecode_steps/s=%10.0f  %s\n",
           r->name, r->code_len, r->steps, r->ms, ops_s, step_s,
           r->status_ok ? "OK" : "ERR");
    if (r->log[0]) {
        // print first line of log
        const char* p = r->log;
        while (*p && *p != '\n') p++;
        printf("    log: %.*s\n", (int)(p - r->log), r->log);
    }
}

int main() {
    const uint64_t N = 500000; // iterations / logical ops baseline
    char src[1024];

    printf("=== Vulcan superopt ops/s (N=%llu logical increments) ===\n\n",
           (unsigned long long)N);

    // 1) Naive classic (no trapdoor)
    snprintf(src, sizeof src,
        "set_step_depth 50000000;\n"
        "set_loops_trapdoor 0;\n"
        "set_limitpassoptimize 0;\n"
        "i32 i = 0; i32 n = %llu;\n"
        "do n[n] { i = i + 1; }\n"
        "print(i);\n", (unsigned long long)N);
    result classic = run_src("naive classic JMP", src, N);
    report(&classic, N);

    // 2) Trapdoor only (default)
    snprintf(src, sizeof src,
        "set_step_depth 50000000;\n"
        "set_limitpassoptimize 0;\n"
        "i32 i = 0; i32 n = %llu;\n"
        "do n[n] { i = i + 1; }\n"
        "print(i);\n", (unsigned long long)N);
    result trap = run_src("trapdoor only", src, N);
    report(&trap, N);

    // 3) Trap + superops residue (parse-time INC_SLOT) — still opt_passes 0 so no algo
    // parse-time already emits INC_SLOT

    // 4) Full algo rewrite O(1)
    snprintf(src, sizeof src,
        "set_step_depth 50000000;\n"
        "set_limitpassoptimize 4;\n"
        "set_enable_algo_rewrite 1;\n"
        "set_superoptimize_enable 1;\n"
        "i32 i = 0; i32 n = %llu;\n"
        "do n[n] { i = i + 1; }\n"
        "print(i);\n", (unsigned long long)N);
    result algo = run_src("algo+super O(1)", src, N);
    report(&algo, N);

    // 5) Strength: many *8 in a loop (should become SHL)
    snprintf(src, sizeof src,
        "set_step_depth 50000000;\n"
        "set_limitpassoptimize 2;\n"
        "set_superoptimize_enable 1;\n"
        "i32 x = 1; i32 n = %llu;\n"
        "do n[n] { x = x * 8; x = x / 8; }\n"
        "print(x);\n", (unsigned long long)(N / 10));
    result strength = run_src("strength *8/8 x N/10", src, N / 10 * 2);
    report(&strength, N / 10 * 2);

    // 6) Phase wrap: x = (x + 1) % 256  — MOD→BAND under superopt, trap body
    snprintf(src, sizeof src,
        "set_step_depth 50000000;\n"
        "set_limitpassoptimize 2;\n"
        "set_superoptimize_enable 1;\n"
        "i32 ph = 0; i32 n = %llu;\n"
        "do n[n] { ph = ph + 1; ph = ph %% 256; }\n"
        "print(ph);\n", (unsigned long long)N);
    result wrap = run_src("phase wrap %%256", src, N * 2);
    report(&wrap, N * 2);

    // 7) Naive vs opt summary
    printf("\n--- Summary ---\n");
    printf("naive steps/logical_op : %.2f\n", classic.steps / (double)N);
    printf("trap  steps/logical_op : %.2f\n", trap.steps / (double)N);
    printf("algo  steps/logical_op : %.4f\n", algo.steps / (double)N);
    if (classic.ms > 0 && algo.ms > 0)
        printf("speedup algo vs classic: %.1fx\n", classic.ms / algo.ms);
    if (trap.ms > 0 && algo.ms > 0)
        printf("speedup algo vs trap   : %.1fx\n", trap.ms / algo.ms);
    printf("\nBytecode ops ratio (classic/algo steps): %.1fx fewer steps after rewrite\n",
           classic.steps / (double)(algo.steps ? algo.steps : 1));

    // ISQRT micro
    {
        rsvm_t vm; rsvm_init(&vm); quiet(&vm);
        float f = 4.0f; int32_t bits; memcpy(&bits, &f, 4);
        uint8_t code[16]; int n = 0;
        code[n++] = RSVM_OP_PUSHI; memcpy(code + n, &bits, 4); n += 4;
        code[n++] = RSVM_OP_ISQRT;
        code[n++] = RSVM_OP_HALT;
        rsvm_load_code(&vm, code, (uint16_t)n);
        const int iters = 200000;
        double t0 = now_s();
        for (int i = 0; i < iters; i++) {
            vm.pc = 0; vm.sp = 0; vm.steps = 0; vm.running = true;
            rsvm_run(&vm);
        }
        double t1 = now_s();
        double ms = (t1 - t0) * 1000.0;
        printf("ISQRT micro: %d calls in %.3f ms → %.0f invsqrt/s\n",
               iters, ms, iters / (ms / 1000.0));
    }
    return 0;
}
