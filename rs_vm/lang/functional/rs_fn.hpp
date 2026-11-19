#pragma once
// =============================================================================
// lang/functional — function tables, call frames, property bits
// Future home for:
//   - richer CALL / RET framing
//   - @property / @immut / @threaded dispatch helpers
//   - closures / partial apply (if ever needed)
// Core func_ent + CALL/RET stay in vm/ opcodes for now.
// =============================================================================
#include "rs_vm.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// Placeholder
// int  rs_fn_lookup(const rsvm_t* vm, const char* name);
// void rs_fn_enter_hooks(rsvm_t* vm, uint8_t func_id);
// void rs_fn_leave_hooks(rsvm_t* vm, uint8_t func_id);

#ifdef __cplusplus
}
#endif
