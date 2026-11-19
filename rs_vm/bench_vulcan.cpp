// bench_vulcan — compare parse+interp, bvul interp, AOT-C native
// Build:
//   g++ -std=c++17 -O2 -I. -Ivm -Idebugger -Ilang/codegen -o /tmp/bench_vulcan \
//     bench_vulcan.cpp vm/rs_vm.cpp vm/rs_vm_parse.cpp vm/rs_vm_opt.cpp debugger/rs_dbg.cpp \
//     lang/codegen/rs_cgen.cpp vm_modules/vm_mdl_*/vm_mdl_*.cpp
#include "rs_vm.hpp"
#include "rs_vm_parse.hpp"
#include "rs_cgen.hpp"
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

static const char* BENCH_SRC =
    "set_step_depth 5000000;\n"
    "i32 i = 0;\n"
    "i32 n = 200000;\n"
    "do n[n] {\n"
    "  i = i + 1;\n"
    "}\n"
    "print(i);\n";

static void host_print_i32(int32_t v, void*) { (void)v; /* silence for bench */ }
static void host_print_str(const char*, uint8_t, void*) {}
static void host_print_char(char, void*) {}

static void install_quiet(rsvm_t* vm) {
    rsvm_host_t h; memset(&h, 0, sizeof h);
    h.print_i32 = host_print_i32;
    h.print_str = host_print_str;
    h.print_char = host_print_char;
    rsvm_set_host(vm, &h);
}

int main(void) {
    printf("=== Vulcan backend benchmark (n=200000 increments) ===\n\n");

    // --- 1) Parse + interpret ---
    {
        rsvm_t vm; rsvm_init(&vm); install_quiet(&vm);
        rsvm_parse_err_t err;
        double t0 = now_s();
        rsvm_status_t st = rsvm_compile(&vm, BENCH_SRC, &err);
        double t1 = now_s();
        if (st != RSVM_OK) {
            printf("compile failed: %s\n", err.message);
            return 1;
        }
        double t2 = now_s();
        st = rsvm_run(&vm);
        double t3 = now_s();
        printf("[interp parse+run]\n");
        printf("  compile : %.3f ms\n", (t1 - t0) * 1000.0);
        printf("  run     : %.3f ms  (steps=%u status=%s)\n",
               (t3 - t2) * 1000.0, vm.steps, rsvm_status_str(st));
        printf("  total   : %.3f ms\n\n", (t3 - t0) * 1000.0);

        // --- 2) Save bvul, reload, run ---
        uint8_t* blob = (uint8_t*)malloc(65536);
        size_t bn = rsvm_save_bvul(&vm, blob, 65536);
        rsvm_t vm2; rsvm_init(&vm2); install_quiet(&vm2);
        double t4 = now_s();
        st = rsvm_load_bvul(&vm2, blob, bn);
        double t5 = now_s();
        st = rsvm_run(&vm2);
        double t6 = now_s();
        printf("[bvul load+run]\n");
        printf("  load    : %.3f ms (%zu bytes)\n", (t5 - t4) * 1000.0, bn);
        printf("  run     : %.3f ms  (steps=%u)\n", (t6 - t5) * 1000.0, vm2.steps);
        printf("  total   : %.3f ms\n\n", (t6 - t4) * 1000.0);

        // --- 3) AOT C backend ---
        if (rsvm_cgen_write_file(&vm, "/tmp/vulcan_aot.c") != 0) {
            printf("[aot-c] emit failed\n");
        } else {
            double t7 = now_s();
            int rc = system("gcc -O2 -pipe -o /tmp/vulcan_aot /tmp/vulcan_aot.c 2>/dev/null");
            double t8 = now_s();
            if (rc != 0) {
                printf("[aot-c] gcc failed\n");
            } else {
                double t9 = now_s();
                rc = system("/tmp/vulcan_aot >/dev/null");
                double t10 = now_s();
                printf("[aot-c gcc -O2 native]\n");
                printf("  compile : %.3f ms\n", (t8 - t7) * 1000.0);
                printf("  run     : %.3f ms  (exit=%d)\n", (t10 - t9) * 1000.0, rc);
                printf("  total   : %.3f ms\n\n", (t10 - t7) * 1000.0);
            }
        }
        free(blob);
    }


    // --- trapdoor vs classic (same workload) ---
    {
        // Trapdoor is now DEFAULT; classic requires explicit disable.
        const char* classic =
            "set_step_depth 5000000;\n"
            "set_loops_trapdoor 0;\n"
            "i32 i = 0; i32 n = 200000;\n"
            "do n[n] { i = i + 1; }\n"
            "print(i);\n";
        const char* trap =
            "set_step_depth 5000000;\n"
            "i32 i = 0; i32 n = 200000;\n"
            "do n[n] { i = i + 1; }\n"
            "print(i);\n";
        for (int k = 0; k < 2; k++) {
            const char* src = k ? trap : classic;
            const char* name = k ? "trapdoor do-n (default)" : "classic do-n (@trapdoor_disable)";
            rsvm_t vm; rsvm_init(&vm); install_quiet(&vm);
            rsvm_parse_err_t err;
            if (rsvm_compile(&vm, src, &err) != RSVM_OK) {
                printf("[%s] compile fail: %s\n", name, err.message);
                continue;
            }
            double t0 = now_s();
            rsvm_status_t st = rsvm_run(&vm);
            double t1 = now_s();
            printf("[%s]\n", name);
            printf("  run     : %.3f ms  (steps=%u status=%s)\n\n",
                   (t1 - t0) * 1000.0, vm.steps, rsvm_status_str(st));
        }
    }

    // --- 4) Pure C reference (same work) ---
    {
        double t0 = now_s();
        volatile int32_t i = 0;
        for (int32_t k = 0; k < 200000; ++k) i = i + 1;
        double t1 = now_s();
        printf("[native C for-loop reference]\n");
        printf("  run     : %.3f ms  (i=%d)\n\n", (t1 - t0) * 1000.0, (int)i);
    }

    // --- 5) switch jump-table vs linear (dense 0..7 enum-like) ---
    {
        // 50k iterations of switch on rotating 0..7 → accumulate
        const char* sw_src =
            "set_step_depth 5000000;\n"
            "i32 i = 0;\n"
            "i32 s = 0;\n"
            "i32 n = 50000;\n"
            "do n[n] {\n"
            "  switch [i] {\n"
            "    case [0] { s = s + 1; }\n"
            "    case [1] { s = s + 2; }\n"
            "    case [2] { s = s + 3; }\n"
            "    case [3] { s = s + 4; }\n"
            "    case [4] { s = s + 5; }\n"
            "    case [5] { s = s + 6; }\n"
            "    case [6] { s = s + 7; }\n"
            "    case [7] { s = s + 8; }\n"
            "    default { s = s + 0; }\n"
            "  }\n"
            "  i = i + 1;\n"
            "  if [i > 7] { i = 0; }\n"
            "}\n"
            "print(s);\n";
        rsvm_t vm; rsvm_init(&vm); install_quiet(&vm);
        rsvm_parse_err_t err;
        if (rsvm_compile(&vm, sw_src, &err) != RSVM_OK) {
            printf("[switch JT] compile fail: %s\n", err.message);
        } else {
            printf("[switch JT dense 0..7 x 50k]\n");
            printf("  regions : %u\n", (unsigned)vm.region_count);
            for (uint8_t ri = 0; ri < vm.region_count && ri < 12; ++ri) {
                const rsvm_region_t* r = &vm.regions[ri];
                const char* kn = "?";
                switch (r->kind) {
                    case RSVM_REG_LOOP: kn = "LOOP"; break;
                    case RSVM_REG_WHILE: kn = "WHILE"; break;
                    case RSVM_REG_FOR: kn = "FOR"; break;
                    case RSVM_REG_IF: kn = "IF"; break;
                    case RSVM_REG_SWITCH: kn = "SWITCH"; break;
                    case RSVM_REG_TRAP: kn = "TRAP"; break;
                    case RSVM_REG_JT: kn = "JT"; break;
                    case RSVM_REG_FN: kn = "FN"; break;
                    default: break;
                }
                printf("    [%u] %s  pc[%u..%u) len=%u\n",
                       (unsigned)ri, kn, (unsigned)r->start, (unsigned)r->end,
                       (unsigned)(r->end - r->start));
            }
            double t0 = now_s();
            rsvm_status_t st = rsvm_run(&vm);
            double t1 = now_s();
            printf("  run     : %.3f ms  (steps=%u status=%s)\n\n",
                   (t1 - t0) * 1000.0, vm.steps, rsvm_status_str(st));
        }
    }

    // --- default trapdoor region dump + superop presence ---
    {
        rsvm_t vm; rsvm_init(&vm); install_quiet(&vm);
        rsvm_parse_err_t err;
        const char* src =
            "set_step_depth 5000000;\n"
            "i32 i = 0; i32 n = 200000;\n"
            "do n[n] { i = i + 1; }\n"
            "print(i);\n";
        if (rsvm_compile(&vm, src, &err) == RSVM_OK) {
            printf("[default trapdoor do-n regions]\n");
            printf("  regions : %u  code_len=%u\n",
                   (unsigned)vm.region_count, (unsigned)vm.code_len);
            for (uint8_t ri = 0; ri < vm.region_count; ++ri) {
                const rsvm_region_t* r = &vm.regions[ri];
                printf("    pc[%u..%u) kind=%u len=%u\n",
                       (unsigned)r->start, (unsigned)r->end,
                       (unsigned)r->kind, (unsigned)(r->end - r->start));
            }
            // Scan for superops
            int n_inc = 0, n_trap = 0;
            for (uint16_t pc = 0; pc < vm.code_len; ++pc) {
                if (vm.code[pc] == RSVM_OP_INC_SLOT) n_inc++;
                if (vm.code[pc] == RSVM_OP_TRAP_LOOP) n_trap++;
            }
            printf("  superops: INC_SLOT=%d TRAP_LOOP=%d\n\n", n_inc, n_trap);
        } else {
            printf("[default trapdoor] compile fail: %s\n\n", err.message);
        }
    }

    // --- inline + overload smoke ---
    {
        const char* src =
            "set_step_depth 100000;\n"
            "fn i32_bump in[x] out[y] {\n"
            "  y = x + 1;\n"
            "  return y;\n"
            "}\n"
            "i32 a = 0;\n"
            "bump in[5] out[a];\n"
            "print(a);\n";
        rsvm_t vm; rsvm_init(&vm); install_quiet(&vm);
        rsvm_parse_err_t err;
        if (rsvm_compile(&vm, src, &err) != RSVM_OK) {
            printf("[overload/inline] compile: %s\n\n", err.message);
        } else {
            double t0 = now_s();
            rsvm_status_t st = rsvm_run(&vm);
            double t1 = now_s();
            printf("[overload i32_bump via bump in[5]]\n");
            printf("  run     : %.3f ms  status=%s steps=%u\n\n",
                   (t1 - t0) * 1000.0, rsvm_status_str(st), vm.steps);
        }
    }

    // --- 6) opt passes + algo rewrite + packed + fixed AOT ---
    {
        const char* src_algo =
            "set_step_depth 5000000;\n"
            "set_limitpassoptimize 4;\n"
            "set_enable_algo_rewrite 1;\n"
            "set_packed_instructions 1;\n"
            "i32 i = 0;\n"
            "i32 n = 200000;\n"
            "do n[n] { i = i + 1; }\n"
            "print(i);\n";
        rsvm_t vm; rsvm_init(&vm); install_quiet(&vm);
        rsvm_parse_err_t err;
        double t0 = now_s();
        rsvm_status_t st = rsvm_compile(&vm, src_algo, &err);
        double t1 = now_s();
        if (st != RSVM_OK) {
            printf("[opt+algo+pack] compile fail: %s\n\n", err.message);
        } else {
            printf("[opt passes=%u applied=%u algo=%u pack=%u]\n",
                   (unsigned)vm.opt_passes, (unsigned)vm.opt_applied,
                   (unsigned)vm.enable_algo_rewrite, (unsigned)vm.packed_instructions);
            if (vm.opt_log[0]) printf("  log:\n%s", vm.opt_log);
            int n_inc = 0, n_trap = 0, n_addimm = 0, n_pack = 0;
            for (uint16_t pc = 0; pc < vm.code_len; ++pc) {
                if (vm.code[pc] == RSVM_OP_INC_SLOT) n_inc++;
                if (vm.code[pc] == RSVM_OP_TRAP_LOOP) n_trap++;
                if (vm.code[pc] == RSVM_OP_ADD_IMM_SLOT) n_addimm++;
                if (vm.code[pc] == RSVM_OP_PACK2) n_pack++;
            }
            printf("  code_len=%u  INC=%d TRAP=%d ADD_IMM=%d PACK2=%d\n",
                   (unsigned)vm.code_len, n_inc, n_trap, n_addimm, n_pack);
            double t2 = now_s();
            st = rsvm_run(&vm);
            double t3 = now_s();
            printf("  compile : %.3f ms\n", (t1 - t0) * 1000.0);
            printf("  run     : %.3f ms  (steps=%u status=%s)\n",
                   (t3 - t2) * 1000.0, vm.steps, rsvm_status_str(st));

            // AOT with superops
            if (rsvm_cgen_write_file(&vm, "/tmp/vulcan_aot2.c") == 0) {
                double t7 = now_s();
                int rc = system("gcc -O2 -pipe -o /tmp/vulcan_aot2 /tmp/vulcan_aot2.c 2>/dev/null");
                double t8 = now_s();
                if (rc == 0) {
                    double t9 = now_s();
                    rc = system("/tmp/vulcan_aot2 >/dev/null");
                    double t10 = now_s();
                    printf("  aot-c   : compile %.3f ms  run %.3f ms  exit=%d\n\n",
                           (t8 - t7) * 1000.0, (t10 - t9) * 1000.0, rc);
                } else {
                    printf("  aot-c   : gcc failed\n\n");
                }
            }
        }
    }

    // Baseline without algo rewrite for comparison
    {
        const char* src =
            "set_step_depth 5000000;\n"
            "set_limitpassoptimize 2;\n"
            "i32 i = 0; i32 n = 200000;\n"
            "do n[n] { i = i + 1; }\n"
            "print(i);\n";
        rsvm_t vm; rsvm_init(&vm); install_quiet(&vm);
        rsvm_parse_err_t err;
        if (rsvm_compile(&vm, src, &err) == RSVM_OK) {
            double t0 = now_s();
            rsvm_status_t st = rsvm_run(&vm);
            double t1 = now_s();
            printf("[opt_passes=2 no algo]\n");
            printf("  run     : %.3f ms  steps=%u status=%s\n\n",
                   (t1 - t0) * 1000.0, vm.steps, rsvm_status_str(st));
        }
    }

    printf("Notes:\n");
    printf("  - interp: stack-machine bytecode interpreter (rsvm_step)\n");
    printf("  - aot-c:  lowered C with superops + real for-trapdoor\n");
    printf("  - @limitpassoptimize(n) / set_limitpassoptimize n  (0..4 passes)\n");
    printf("  - @enable_algo_rewrite / set_enable_algo_rewrite 1\n");
    printf("  - @packed_instructions / set_packed_instructions 1\n");
    return 0;
}
