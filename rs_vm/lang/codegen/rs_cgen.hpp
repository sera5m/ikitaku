#pragma once
// Vulcan bytecode → C source (AOT "cheat" backend)
// Compile the emitted C with gcc -O2 for native speed.
#include "rs_vm.hpp"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Emit a self-contained C program implementing the loaded VM image.
// Returns bytes written (excluding NUL), or 0 on failure.
size_t rsvm_cgen_emit(const rsvm_t* vm, char* out, size_t out_cap);

// Convenience: write to a file path. Returns 0 on success.
int rsvm_cgen_write_file(const rsvm_t* vm, const char* path);

#ifdef __cplusplus
}
#endif
