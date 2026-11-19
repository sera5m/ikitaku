#pragma once
/* Project a Toplethos 1.2 tripar program (three-address bytecode)
   onto the September rsvm_t image that ikitaku_analyze walks.

   Current Toplethos (USE.md, compiler/include/tp/bytecode.hpp):
     tp::Program / BcFunc / Insn, Op enum, interpret() is the oracle.
   September ikitaku:
     ikitaku_analyze(const rsvm_t*) scans code[] for RSVM_OP_*.

   This header is the only new surface. The reasoner is unchanged.
   Call ikitaku_tp_fill() then ikitaku_analyze().

   Optional: define IKITAKU_HAVE_TP and include tp/bytecode.hpp
   before this header to use the typed overload. */

#include "../vm/rs_vm.hpp"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Flat instruction, matching tp::Op order in bytecode.hpp (1.2 tripar). */
enum {
    IKI_TP_Imm = 0, IKI_TP_Mov, IKI_TP_Add, IKI_TP_Sub, IKI_TP_Mul, IKI_TP_Div,
    IKI_TP_Rem, IKI_TP_And, IKI_TP_Or, IKI_TP_Xor, IKI_TP_Shl, IKI_TP_Shr,
    IKI_TP_Eq, IKI_TP_Ne, IKI_TP_Lt, IKI_TP_Le, IKI_TP_Gt, IKI_TP_Ge,
    IKI_TP_Not, IKI_TP_Neg, IKI_TP_Jmp, IKI_TP_Br, IKI_TP_BrRange, IKI_TP_Label,
    IKI_TP_Ret, IKI_TP_Call, IKI_TP_Threads, IKI_TP_Freemem, IKI_TP_Addr,
    IKI_TP_Load, IKI_TP_Store, IKI_TP_Pow, IKI_TP_FloorDiv, IKI_TP_BitNot,
    IKI_TP_Print, IKI_TP_Trap, IKI_TP_StaticInit, IKI_TP_Fork, IKI_TP_Start,
    IKI_TP_Lock, IKI_TP_Unlock, IKI_TP_Cast, IKI_TP_State, IKI_TP_SlotAdd,
    IKI_TP_Clock, IKI_TP_Watch, IKI_TP_SaveBmp, IKI_TP_LoadBytes, IKI_TP_LoadBmp
};

typedef struct {
    uint16_t op;       /* IKI_TP_* */
    uint32_t dst, lhs, rhs;
    uint64_t imm;
    int      line;
} iki_tp_insn_t;

/* Fill vm->code[] / regions[] from a flat instruction list.
   Returns bytes written, or -1 if the image would overflow. */
int ikitaku_tp_fill(rsvm_t* vm, const iki_tp_insn_t* ins, int n);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#ifdef IKITAKU_HAVE_TP
#include "tp/bytecode.hpp"
int ikitaku_tp_fill_program(rsvm_t* vm, const tp::Program& prog);
#endif
#endif
