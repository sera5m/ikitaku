#pragma once
// =============================================================================
// lang/objects — heap objects, smart/raw pointers, struct field ops
// Future home for:
//   - richer NEW / retain / release
//   - method tables / vtables (optional)
//   - object identity & equality
//   - optional GC sweep hooks
// Core opcodes (NEW, FIELD_LD/ST, RETAIN, RELEASE, PTR_*) live in vm/ for now.
// =============================================================================
#include "rs_vm.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// Placeholder — will absorb heap helpers from rs_vm.cpp when split further
// int16_t rs_obj_alloc(rsvm_t* vm, uint8_t struct_id);
// void    rs_obj_retain(rsvm_t* vm, int32_t off);
// void    rs_obj_release(rsvm_t* vm, int32_t off);

#ifdef __cplusplus
}
#endif
