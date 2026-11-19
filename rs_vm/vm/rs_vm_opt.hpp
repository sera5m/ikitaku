#pragma once
// Multi-pass bytecode optimizer + algo-rewrite rule table
#include "rs_vm.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// Run 0..4 optimization passes on vm->code (in place).
// Passes: 1=superop residue, 2=const fold, 3=PACK2 if enabled, 4=algo rewrite if enabled.
// Writes suggestions into vm->opt_log when algo rewrite is on.
void rsvm_optimize(rsvm_t* vm);

// Classify a simple loop body's asymptotic cost (heuristic).
// Returns a short string like "O(n)", "O(1)", "O(n^2)", "unknown".
const char* rsvm_estimate_big_o(const rsvm_t* vm, uint16_t body_start, uint16_t body_end);

#ifdef __cplusplus
}
#endif
