#pragma once
// lang/variables — slots, arrays, strings
// Vulcan:
//   i32 a[5];                 static-size heap array (zeros)
//   i32 a[5] = [1,2,3,4,5];   static size + literal fill
//   arr a = arr_new(n);       dynamic 1D (n runtime)
//   arr a = arr_new(r,c);     2D
//   arr a = arr_of(x,y,z);    pack current var values
//   a[i]  a[i] = v;           load/store
//   arr_len(a)  a = arr_push(a, v);
#include "rs_vm.hpp"
#ifdef __cplusplus
extern "C" {
#endif
#ifdef __cplusplus
}
#endif
