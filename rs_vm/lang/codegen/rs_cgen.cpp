// rs_cgen.cpp — AOT: Vulcan bytecode → standalone C (gcc -O2)
// Supports superops, TRAP_LOOP as real C for, SWITCH_JT, PACK2.
#include "rs_cgen.hpp"
#include "rs_vm_opcodes.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

typedef struct {
    char*  buf;
    size_t cap;
    size_t len;
    int    overflow;
} abuf;

static void ab_init(abuf* a, char* b, size_t c) {
    a->buf = b; a->cap = c; a->len = 0; a->overflow = 0;
    if (c && b) b[0] = 0;
}
static void ab_printf(abuf* a, const char* fmt, ...) {
    if (!a || a->overflow || !a->buf) return;
    va_list ap;
    va_start(ap, fmt);
    size_t avail = a->cap > a->len ? a->cap - a->len : 0;
    int n = vsnprintf(a->buf + a->len, avail, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= avail) { a->overflow = 1; return; }
    a->len += (size_t)n;
}

static uint8_t rd8(const uint8_t* code, uint16_t* pc) {
    return code[(*pc)++];
}
static int16_t rd16(const uint8_t* code, uint16_t* pc) {
    uint8_t lo = rd8(code, pc), hi = rd8(code, pc);
    return (int16_t)((uint16_t)lo | ((uint16_t)hi << 8));
}
static int32_t rd32(const uint8_t* code, uint16_t* pc) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= ((uint32_t)rd8(code, pc)) << (8 * i);
    return (int32_t)v;
}

// Emit one instruction at *pc; advances pc. labels=1 uses L_pc gotos for jumps.
// When labels=0 (trap body), relative jumps become local C control (best-effort).
static void emit_one(abuf* a, const rsvm_t* vm, uint16_t* pc, int labels) {
    const uint8_t* code = vm->code;
    uint16_t here = *pc;
    uint8_t op = rd8(code, pc);
    switch (op) {
    case RSVM_OP_HALT:
    case RSVM_OP_END:
        ab_printf(a, "  return 0;\n"); break;
    case RSVM_OP_NOP:
        break;
    case RSVM_OP_PUSHI: {
        int32_t v = rd32(code, pc);
        ab_printf(a, "  PUSH(%d);\n", (int)v); break;
    }
    case RSVM_OP_POP:
        ab_printf(a, "  (void)POP();\n"); break;
    case RSVM_OP_DUP:
        ab_printf(a, "  { int32_t t=S[sp-1]; PUSH(t); }\n"); break;
    case RSVM_OP_LDI: {
        uint8_t s = rd8(code, pc); int32_t v = rd32(code, pc);
        ab_printf(a, "  slots[%u]=%d;\n", (unsigned)s, (int)v); break;
    }
    case RSVM_OP_LOAD: {
        uint8_t s = rd8(code, pc);
        ab_printf(a, "  PUSH(slots[%u]);\n", (unsigned)s); break;
    }
    case RSVM_OP_STORE: {
        uint8_t s = rd8(code, pc);
        ab_printf(a, "  slots[%u]=POP();\n", (unsigned)s); break;
    }
    case RSVM_OP_MOV: {
        uint8_t d = rd8(code, pc), s = rd8(code, pc);
        ab_printf(a, "  slots[%u]=slots[%u];\n", (unsigned)d, (unsigned)s); break;
    }
    case RSVM_OP_ADD:
        ab_printf(a, "  { int32_t b=POP(),a=POP(); PUSH(a+b); }\n"); break;
    case RSVM_OP_SUB:
        ab_printf(a, "  { int32_t b=POP(),a=POP(); PUSH(a-b); }\n"); break;
    case RSVM_OP_MUL:
        ab_printf(a, "  { int32_t b=POP(),a=POP(); PUSH(a*b); }\n"); break;
    case RSVM_OP_DIV:
        ab_printf(a, "  { int32_t b=POP(),a=POP(); PUSH(b?a/b:0); }\n"); break;
    case RSVM_OP_MOD:
        ab_printf(a, "  { int32_t b=POP(),a=POP(); PUSH(b?a%%b:0); }\n"); break;
    case RSVM_OP_NEG:
        ab_printf(a, "  { int32_t a=POP(); PUSH(-a); }\n"); break;
    case RSVM_OP_NOT:
        ab_printf(a, "  { int32_t a=POP(); PUSH(!a); }\n"); break;
    case RSVM_OP_EQ:
        ab_printf(a, "  { int32_t b=POP(),a=POP(); PUSH(a==b); }\n"); break;
    case RSVM_OP_NE:
        ab_printf(a, "  { int32_t b=POP(),a=POP(); PUSH(a!=b); }\n"); break;
    case RSVM_OP_LT:
        ab_printf(a, "  { int32_t b=POP(),a=POP(); PUSH(a<b); }\n"); break;
    case RSVM_OP_LE:
        ab_printf(a, "  { int32_t b=POP(),a=POP(); PUSH(a<=b); }\n"); break;
    case RSVM_OP_GT:
        ab_printf(a, "  { int32_t b=POP(),a=POP(); PUSH(a>b); }\n"); break;
    case RSVM_OP_GE:
        ab_printf(a, "  { int32_t b=POP(),a=POP(); PUSH(a>=b); }\n"); break;
    case RSVM_OP_JMP: {
        int16_t rel = rd16(code, pc);
        uint16_t tgt = (uint16_t)((int32_t)(*pc) + rel);
        if (labels) ab_printf(a, "  goto L_%u;\n", (unsigned)tgt);
        else ab_printf(a, "  /* jmp %u in trap body */\n", (unsigned)tgt);
        break;
    }
    case RSVM_OP_JZ: {
        int16_t rel = rd16(code, pc);
        uint16_t tgt = (uint16_t)((int32_t)(*pc) + rel);
        if (labels) ab_printf(a, "  if(!POP()) goto L_%u;\n", (unsigned)tgt);
        else ab_printf(a, "  if(!POP()) { /* jz */ }\n");
        break;
    }
    case RSVM_OP_JNZ: {
        int16_t rel = rd16(code, pc);
        uint16_t tgt = (uint16_t)((int32_t)(*pc) + rel);
        if (labels) ab_printf(a, "  if(POP()) goto L_%u;\n", (unsigned)tgt);
        else ab_printf(a, "  if(POP()) { /* jnz */ }\n");
        break;
    }
    case RSVM_OP_PRINT_I: {
        int32_t v = rd32(code, pc);
        ab_printf(a, "  printf(\"%%d\\n\", %d);\n", (int)v); break;
    }
    case RSVM_OP_PRINT_V:
        ab_printf(a, "  printf(\"%%d\\n\", (int)POP());\n"); break;
    case RSVM_OP_PRINT_STR: {
        uint8_t n = rd8(code, pc);
        ab_printf(a, "  printf(\"");
        for (uint8_t i = 0; i < n; ++i) {
            char ch = (char)rd8(code, pc);
            if (ch == '\\' || ch == '"') ab_printf(a, "\\%c", ch);
            else if (ch == '\n') ab_printf(a, "\\n");
            else ab_printf(a, "%c", ch);
        }
        ab_printf(a, "\\n\");\n"); break;
    }
    case RSVM_OP_PRINT_S: {
        uint8_t s = rd8(code, pc);
        ab_printf(a, "  printf(\"%%d\\n\", (int)slots[%u]);\n", (unsigned)s); break;
    }
    case RSVM_OP_PRINT_C:
        ab_printf(a, "  { char c=(char)POP(); fputc(c, stdout); }\n"); break;
    case RSVM_OP_PRINT_NL:
        ab_printf(a, "  fputc('\\n', stdout);\n"); break;
    case RSVM_OP_CALL: {
        uint8_t fid = rd8(code, pc);
        if (fid < vm->func_count) {
            uint16_t ep = vm->funcs[fid].entry_pc;
            ab_printf(a, "  if(rsp<16){ rstack[rsp++]=&&LR_%u; goto L_%u; } LR_%u: ;\n",
                      (unsigned)(*pc), (unsigned)ep, (unsigned)(*pc));
        } else {
            ab_printf(a, "  return 1;\n");
        }
        break;
    }
    case RSVM_OP_RET: {
        (void)rd8(code, pc);
        ab_printf(a, "  if(rsp>0){ void*ra=rstack[--rsp]; goto *ra; } return 0;\n");
        break;
    }
    case RSVM_OP_RET0:
        ab_printf(a, "  if(rsp>0){ void*ra=rstack[--rsp]; goto *ra; } return 0;\n");
        break;
    case RSVM_OP_IGNORE:
        ab_printf(a, "  if(sp>0) (void)POP();\n"); break;
    case RSVM_OP_STR_LIT: {
        uint8_t n = rd8(code, pc);
        ab_printf(a, "  PUSH(0); /* str */\n");
        *pc = (uint16_t)(*pc + n);
        break;
    }
    case RSVM_OP_STR_LEN:
        ab_printf(a, "  (void)POP(); PUSH(0);\n"); break;
    case RSVM_OP_STR_CHAR:
        ab_printf(a, "  { (void)POP(); (void)POP(); PUSH(0); }\n"); break;

    // ---- superops ----
    case RSVM_OP_INC_SLOT: {
        uint8_t s = rd8(code, pc);
        ab_printf(a, "  slots[%u]++;\n", (unsigned)s); break;
    }
    case RSVM_OP_ADD_IMM_SLOT: {
        uint8_t s = rd8(code, pc);
        int32_t imm = rd32(code, pc);
        ab_printf(a, "  slots[%u]+=%d;\n", (unsigned)s, (int)imm); break;
    }
    case RSVM_OP_LOAD_ADD_STORE: {
        uint8_t d = rd8(code, pc), x = rd8(code, pc), y = rd8(code, pc);
        ab_printf(a, "  slots[%u]=slots[%u]+slots[%u];\n",
                  (unsigned)d, (unsigned)x, (unsigned)y); break;
    }
    case RSVM_OP_LOAD_PUSHI: {
        uint8_t s = rd8(code, pc);
        int32_t imm = rd32(code, pc);
        ab_printf(a, "  PUSH(slots[%u]); PUSH(%d);\n", (unsigned)s, (int)imm); break;
    }
    case RSVM_OP_PACK2: {
        uint8_t o0 = rd8(code, pc), o1 = rd8(code, pc);
        // Expand dual-op as sequential C
        uint8_t ops[2] = { o0, o1 };
        for (int i = 0; i < 2; ++i) {
            switch (ops[i]) {
            case RSVM_OP_NOP: break;
            case RSVM_OP_POP: ab_printf(a, "  (void)POP();\n"); break;
            case RSVM_OP_DUP: ab_printf(a, "  { int32_t t=S[sp-1]; PUSH(t); }\n"); break;
            case RSVM_OP_ADD: ab_printf(a, "  { int32_t b=POP(),a=POP(); PUSH(a+b); }\n"); break;
            case RSVM_OP_SUB: ab_printf(a, "  { int32_t b=POP(),a=POP(); PUSH(a-b); }\n"); break;
            case RSVM_OP_MUL: ab_printf(a, "  { int32_t b=POP(),a=POP(); PUSH(a*b); }\n"); break;
            case RSVM_OP_NEG: ab_printf(a, "  { int32_t a=POP(); PUSH(-a); }\n"); break;
            case RSVM_OP_NOT: ab_printf(a, "  { int32_t a=POP(); PUSH(!a); }\n"); break;
            default: ab_printf(a, "  /* pack2 unknown 0x%02X */\n", ops[i]); break;
            }
        }
        break;
    }
    case RSVM_OP_SWITCH_JT: {
        int32_t min = rd32(code, pc);
        uint16_t count = (uint16_t)rd8(code, pc) | ((uint16_t)rd8(code, pc) << 8);
        uint16_t base = *pc;
        uint16_t def_off = (uint16_t)(base + count * 2);
        uint16_t default_pc = (uint16_t)code[def_off] | ((uint16_t)code[def_off+1] << 8);
        ab_printf(a, "  { int32_t _v=POP();\n");
        ab_printf(a, "    if(_v>=%d && _v<%d){\n", (int)min, (int)(min + count));
        ab_printf(a, "      switch(_v-(%d)){\n", (int)min);
        for (uint16_t i = 0; i < count; ++i) {
            uint16_t to = (uint16_t)code[base + i*2] | ((uint16_t)code[base + i*2 + 1] << 8);
            ab_printf(a, "      case %u: goto L_%u;\n", (unsigned)i, (unsigned)to);
        }
        ab_printf(a, "      } } else goto L_%u; }\n", (unsigned)default_pc);
        *pc = (uint16_t)(def_off + 2);
        break;
    }
    case RSVM_OP_TRAP_LOOP: {
        uint8_t lo = rd8(code, pc), hi = rd8(code, pc);
        uint16_t blen = (uint16_t)lo | ((uint16_t)hi << 8);
        uint16_t body = *pc;
        uint16_t after = (uint16_t)(body + blen);
        // Real C for-loop: inline body opcodes without outer labels
        ab_printf(a, "  { int32_t _n=POP(); for(int32_t _i=0;_i<_n;++_i){\n");
        uint16_t bpc = body;
        while (bpc < after && !a->overflow) {
            emit_one(a, vm, &bpc, 0);
        }
        ab_printf(a, "  } }\n");
        // Skip body in outer linear scan
        *pc = after;
        break;
    }
    case RSVM_OP_BREAK:
        ab_printf(a, "  break;\n"); break;
    case RSVM_OP_CONTINUE:
        ab_printf(a, "  continue;\n"); break;
    default:
        ab_printf(a, "  /* unknown op 0x%02X at %u */\n", op, (unsigned)here);
        ab_printf(a, "  return 1;\n");
        break;
    }
}

size_t rsvm_cgen_emit(const rsvm_t* vm, char* out, size_t out_cap) {
    if (!vm || !out || out_cap < 64) return 0;
    abuf a; ab_init(&a, out, out_cap);

    ab_printf(&a,
        "/* generated by rsvm_cgen — Vulcan AOT (superops+trap) */\n"
        "#include <stdio.h>\n"
        "#include <stdint.h>\n"
        "#include <string.h>\n"
        "static int32_t S[64]; static int sp=0;\n"
        "static int32_t slots[64];\n"
        "static void* rstack[16]; static int rsp=0;\n"
        "#define PUSH(x) do{ if(sp<64) S[sp++]=(x);}while(0)\n"
        "#define POP()   (sp>0?S[--sp]:0)\n"
        "int main(void){\n"
        "  memset(slots,0,sizeof slots);\n"
    );

    const uint8_t* code = vm->code;
    uint16_t len = vm->code_len;
    uint16_t entry = vm->entry;

    ab_printf(&a, "  goto L_%u;\n", (unsigned)entry);

    uint16_t pc = 0;
    // Track trap body ranges already inlined so outer scan skips them
    while (pc < len && !a.overflow) {
        uint16_t here = pc;
        // If this PC is inside a trap body that was already emitted, still need labels
        // for any external jumps — emit labels always, but TRAP handler advances pc past body.
        ab_printf(&a, "L_%u: ;\n", (unsigned)here);
        // Peek: if TRAP_LOOP, emit_one will skip body
        emit_one(&a, vm, &pc, 1);
        (void)code;
    }
    ab_printf(&a, "  return 0;\n}\n");
    if (a.overflow) return 0;
    return a.len;
}

int rsvm_cgen_write_file(const rsvm_t* vm, const char* path) {
    if (!vm || !path) return -1;
    char* buf = (char*)malloc(512 * 1024);
    if (!buf) return -1;
    size_t n = rsvm_cgen_emit(vm, buf, 512 * 1024);
    if (!n) { free(buf); return -1; }
    FILE* f = fopen(path, "w");
    if (!f) { free(buf); return -1; }
    fwrite(buf, 1, n, f);
    fclose(f);
    free(buf);
    return 0;
}
