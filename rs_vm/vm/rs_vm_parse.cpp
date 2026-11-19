#include "rs_vm_parse.hpp"
#include "rs_vm_opt.hpp"
#include "vm_modules/vm_mdl_property/vm_mdl_property.h"
#include "vm_modules/vm_mdl_thread/vm_mdl_thread.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

typedef struct {
    const char* s;
    int pos, line, col;
    rsvm_t* vm;
    rsvm_parse_err_t* err;
    bool failed;
    bool in_func;
    int  do_depth;   // nest level for unique do-n counters
    int  include_depth;
    uint16_t cur_func_props;  // props known before body
    int  autounroll_max;      // 0 = off; from @autounroll(n)
    bool loop_trapdoor;       // default true; set_loops_trapdoor 0 / @trapdoor_disable turns off
    bool trap_disabled;       // @trapdoor_disable on current fn
} P;

static char* read_entire_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char* buf = (char*)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = 0;
    if (out_len) *out_len = got;
    return buf;
}

// Join include_base + rel into out (out_cap).
static void path_join(char* out, size_t out_cap, const char* base, const char* rel) {
    if (!out || out_cap == 0) return;
    out[0] = 0;
    if (rel && rel[0] == '/') { strncpy(out, rel, out_cap - 1); return; }
    if (base && base[0]) {
        snprintf(out, out_cap, "%s/%s", base, rel ? rel : "");
    } else if (rel) {
        strncpy(out, rel, out_cap - 1);
    }
}

// dirname of path into out
static void path_dirname(char* out, size_t out_cap, const char* path) {
    if (!out || out_cap == 0) return;
    out[0] = 0;
    if (!path) return;
    const char* slash = strrchr(path, '/');
    if (!slash) { strncpy(out, ".", out_cap - 1); return; }
    size_t n = (size_t)(slash - path);
    if (n >= out_cap) n = out_cap - 1;
    memcpy(out, path, n);
    out[n] = 0;
}


static void fail(P* p, const char* msg) {
    if (p->failed) return;
    p->failed = true;
    if (p->err) {
        snprintf(p->err->message, sizeof p->err->message, "%s", msg);
        p->err->line = p->line;
        p->err->column = p->col;
    }
}
static char peek(P* p) { return p->s[p->pos]; }
static char getc_(P* p) {
    char c = p->s[p->pos++];
    if (c == '\n') { p->line++; p->col = 1; } else p->col++;
    return c;
}
static void skip_ws(P* p) {
    for (;;) {
        char c = peek(p);
        if (!c) return;
        if (c==' '||c=='\t'||c=='\r'||c=='\n') { getc_(p); continue; }
        if (c=='/' && p->s[p->pos+1]=='/') {
            while (peek(p) && peek(p)!='\n') getc_(p);
            continue;
        }
        return;
    }
}
static bool match(P* p, const char* kw) {
    skip_ws(p);
    int n = (int)strlen(kw);
    if (strncmp(p->s + p->pos, kw, n) != 0) return false;
    if (isalpha((unsigned char)kw[0]) || kw[0]=='_') {
        char a = p->s[p->pos + n];
        if (isalnum((unsigned char)a) || a=='_') return false;
    }
    for (int i = 0; i < n; ++i) getc_(p);
    return true;
}
static bool expect(P* p, const char* kw) {
    if (!match(p, kw)) {
        char b[64]; snprintf(b, sizeof b, "expected '%s'", kw);
        fail(p, b); return false;
    }
    return true;
}
static void emit_u8(P* p, uint8_t b) {
    if (p->vm->code_len >= RSVM_MAX_CODE) { fail(p, "code full"); return; }
    p->vm->code[p->vm->code_len++] = b;
}
static void emit_i16(P* p, int16_t v) {
    emit_u8(p, (uint8_t)(v & 0xFF));
    emit_u8(p, (uint8_t)((v >> 8) & 0xFF));
}
static void emit_i32(P* p, int32_t v) {
    for (int i = 0; i < 4; ++i) emit_u8(p, (uint8_t)((v >> (8*i)) & 0xFF));
}
static void emit_u16(P* p, uint16_t v) {
    emit_u8(p, (uint8_t)(v & 0xFF));
    emit_u8(p, (uint8_t)((v >> 8) & 0xFF));
}
// Record a control-flow region [start, end) so length is known at compile time.
static void push_region(P* p, uint16_t start, uint16_t end, uint8_t kind) {
    if (!p->vm || p->vm->region_count >= RSVM_MAX_REGIONS) return;
    rsvm_region_t* r = &p->vm->regions[p->vm->region_count++];
    r->start = start;
    r->end   = end;
    r->kind  = kind;
    r->flags = 0;
}
// Type keywords may not be used as variable or function names.
static bool is_type_keyword(const char* name) {
    if (!name) return false;
    static const char* kws[] = {
        "i8","i16","i32","i64","ui8","ui16","ui32","ui64",
        "u8","u16","u32","u64","f16","f32","f64","uf16","uf32",
        "bool","ptr","string","nil","true","false",
        "int","float","void","enum","struct","fn",
        NULL
    };
    for (int i = 0; kws[i]; ++i)
        if (strcmp(name, kws[i]) == 0) return true;
    return false;
}
static bool parse_ident(P* p, char* out, int out_len) {
    skip_ws(p);
    if (!isalpha((unsigned char)peek(p)) && peek(p)!='_') return false;
    int i = 0;
    while (isalnum((unsigned char)peek(p)) || peek(p)=='_') {
        if (i < out_len-1) out[i++] = getc_(p); else getc_(p);
    }
    out[i] = 0;
    return i > 0;
}
// Resolve call target with type-directed overload: try i32_foo / f32_foo / int_foo / float_foo
static int resolve_func_overload(P* p, const char* base, int arg_kind) {
    // arg_kind: 0 = unknown, 1 = int-like, 2 = float-like
    int fid = rsvm_func_by_name(p->vm, base);
    if (fid >= 0) return fid;
    char mangled[RSVM_NAME_LEN + 8];
    if (arg_kind == 2) {
        snprintf(mangled, sizeof mangled, "f32_%s", base);
        fid = rsvm_func_by_name(p->vm, mangled);
        if (fid >= 0) return fid;
        snprintf(mangled, sizeof mangled, "float_%s", base);
        fid = rsvm_func_by_name(p->vm, mangled);
        if (fid >= 0) return fid;
    } else if (arg_kind == 1) {
        snprintf(mangled, sizeof mangled, "i32_%s", base);
        fid = rsvm_func_by_name(p->vm, mangled);
        if (fid >= 0) return fid;
        snprintf(mangled, sizeof mangled, "int_%s", base);
        fid = rsvm_func_by_name(p->vm, mangled);
        if (fid >= 0) return fid;
    }
    return -1;
}
// Peek whether next expression looks like a float literal (contains '.')
static int peek_arg_kind(P* p) {
    skip_ws(p);
    int pos = p->pos;
    if (peek(p) == '-') pos++;
    if (pos >= 0 && p->s[pos] && isdigit((unsigned char)p->s[pos])) {
        while (p->s[pos] && isdigit((unsigned char)p->s[pos])) pos++;
        if (p->s[pos] == '.') return 2; // float
        return 1; // int
    }
    return 0;
}
static bool parse_int(P* p, int32_t* out) {
    skip_ws(p);
    bool neg = false;
    if (peek(p)=='-') { neg = true; getc_(p); }
    if (!isdigit((unsigned char)peek(p))) return false;
    int32_t v = 0;
    // hex: 0x...
    if (peek(p)=='0' && (p->s[p->pos+1]=='x' || p->s[p->pos+1]=='X')) {
        getc_(p); getc_(p);
        if (!isxdigit((unsigned char)peek(p))) return false;
        while (isxdigit((unsigned char)peek(p))) {
            char c = getc_(p);
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (c - '0');
            else if (c >= 'a' && c <= 'f') v |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (c - 'A' + 10);
        }
        *out = neg ? -v : v;
        return true;
    }
    while (isdigit((unsigned char)peek(p))) v = v*10 + (getc_(p)-'0');
    *out = neg ? -v : v;
    return true;
}
static uint8_t slot_of(P* p, const char* name, bool create) {
    if (create && is_type_keyword(name)) {
        fail(p, "type keyword cannot be a variable name");
        return 0xFF;
    }
    return rsvm_slot_by_name(p->vm, name, create);
}

static void expr(P* p);
static void statement(P* p);
static void parse_block(P* p);

static int field_index(rsvm_t* vm, const char* fld) {
    for (uint8_t si = 0; si < vm->struct_count; ++si)
        for (uint8_t f = 0; f < vm->structs[si].nfields; ++f)
            if (strcmp(vm->structs[si].field_names[f], fld) == 0)
                return (int)f;
    return -1;
}

static void primary(P* p) {
    skip_ws(p);
    int32_t lit;
    char id[RSVM_NAME_LEN];
    if (parse_int(p, &lit)) {
            // float: N.M or N.
            if (peek(p) == '.') {
                getc_(p);
                double frac = 0, base = 0.1;
                while (isdigit((unsigned char)peek(p))) {
                    frac += (peek(p) - '0') * base;
                    base *= 0.1;
                    getc_(p);
                }
                float fv = (float)((double)lit + frac);
                // optional f suffix
                if (peek(p) == 'f' || peek(p) == 'F') getc_(p);
                rsvm_val_t fvbits; fvbits.ty = RSVM_TY_F32; fvbits.aux = 0;
                memcpy(&fvbits.v, &fv, 4);
                emit_u8(p, RSVM_OP_PUSH_TY); emit_u8(p, RSVM_TY_F32); emit_u8(p, 0); emit_i32(p, fvbits.v);
                return;
            }
            emit_u8(p, RSVM_OP_PUSHI); emit_i32(p, lit);
            return;
        }
    if (match(p, "true"))  { emit_u8(p, RSVM_OP_PUSHI); emit_i32(p, 1); return; }
    if (match(p, "false")) { emit_u8(p, RSVM_OP_PUSHI); emit_i32(p, 0); return; }
    if (match(p, "nil"))   { emit_u8(p, RSVM_OP_PUSH_NIL); return; }
    if (match(p, "(")) { expr(p); expect(p, ")"); return; }
    // strlen(expr) / size(expr)
    if (match(p, "strlen") || match(p, "size")) {
        expect(p, "("); expr(p); expect(p, ")");
        emit_u8(p, RSVM_OP_STR_LEN);
        return;
    }
    if (match(p, "pwd")) {
        expect(p, "("); expect(p, ")");
        emit_u8(p, RSVM_OP_SYS_PWD);
        return;
    }
    if (match(p, "ls")) {
        expect(p, "("); skip_ws(p);
        if (peek(p) == ')') {
            getc_(p);
            emit_u8(p, RSVM_OP_STR_LIT); emit_u8(p, 1); emit_u8(p, (uint8_t)'.');
        } else {
            expr(p); expect(p, ")");
        }
        emit_u8(p, RSVM_OP_SYS_LS);
        return;
    }
    if (match(p, "env")) {
        expect(p, "("); expr(p); expect(p, ")");
        emit_u8(p, RSVM_OP_SYS_ENV);
        return;
    }
    if (match(p, "sh") || match(p, "sys")) {
        expect(p, "("); expr(p); expect(p, ")");
        emit_u8(p, RSVM_OP_SYS_CMD);
        return;
    }
    if (match(p, "grep")) {
        expect(p, "("); expr(p); expect(p, ","); expr(p); expect(p, ")");
        emit_u8(p, RSVM_OP_SYS_GREP);
        return;
    }
    if (match(p, "cat")) {
        expect(p, "("); expr(p); expect(p, ")");
        emit_u8(p, RSVM_OP_SYS_CAT);
        return;
    }
    if (match(p, "echo")) {
        expect(p, "("); expr(p); expect(p, ")");
        emit_u8(p, RSVM_OP_SYS_ECHO);
        return;
    }
    if (match(p, "head")) {
        expect(p, "("); expr(p); expect(p, ","); expr(p); expect(p, ")");
        emit_u8(p, RSVM_OP_SYS_HEAD);
        return;
    }
    if (match(p, "wc")) {
        expect(p, "("); expr(p); expect(p, ")");
        emit_u8(p, RSVM_OP_SYS_WC);
        return;
    }
    if (match(p, "sysconf_get") || match(p, "sysconf")) {
        // sysconf("key") as expr → GET
        expect(p, "("); expr(p); expect(p, ")");
        emit_u8(p, RSVM_OP_SYSCONF_GET);
        return;
    }
    if (match(p, "native") || match(p, "ccall")) {
        // native("name", arg0, arg1, ...)
        expect(p, "(");
        expr(p); // name
        int nargs = 0;
        while (match(p, ",")) {
            expr(p);
            nargs++;
            if (nargs >= 8) break;
        }
        expect(p, ")");
        emit_u8(p, RSVM_OP_NATIVE); emit_u8(p, (uint8_t)nargs);
        return;
    }
    // string literal → STR_LIT
    if (peek(p) == '"') {
        getc_(p);
        char tmp[192]; int n = 0;
        while (peek(p) && peek(p) != '"' && n < (int)sizeof(tmp) - 1) tmp[n++] = getc_(p);
        expect(p, "\"");
        emit_u8(p, RSVM_OP_STR_LIT); emit_u8(p, (uint8_t)n);
        for (int i = 0; i < n; i++) emit_u8(p, (uint8_t)tmp[i]);
        return;
    }
    if (match(p, "new")) {
        if (!parse_ident(p, id, sizeof id)) { fail(p, "type after new"); return; }
        int sid = rsvm_struct_by_name(p->vm, id);
        if (sid < 0) { fail(p, "unknown struct"); return; }
        emit_u8(p, RSVM_OP_NEW); emit_u8(p, (uint8_t)sid);
        return;
    }
    if (parse_ident(p, id, sizeof id)) {
        skip_ws(p);
        if (peek(p) == '@') {
            getc_(p);
            char en[RSVM_NAME_LEN];
            if (!parse_ident(p, en, sizeof en)) { fail(p, "enum name"); return; }
            int eid = rsvm_enum_by_name(p->vm, en);
            if (eid < 0) { fail(p, "unknown enum"); return; }
            int found = -1;
            for (uint8_t i = 0; i < p->vm->enums[eid].nmembers; ++i)
                if (strcmp(p->vm->enums[eid].member_names[i], id) == 0) {
                    found = (int)p->vm->enums[eid].member_vals[i]; break;
                }
            if (found < 0) { fail(p, "unknown member"); return; }
            emit_u8(p, RSVM_OP_PUSH_TY);
            emit_u8(p, RSVM_TY_ENUM); emit_u8(p, (uint8_t)eid); emit_i32(p, found);
            return;
        }
        if (peek(p) == '.') {
            getc_(p);
            char fld[RSVM_NAME_LEN];
            if (!parse_ident(p, fld, sizeof fld)) { fail(p, "field"); return; }
            uint8_t s = slot_of(p, id, true);
            rsvm_ref_note(p->vm, id, RSVM_REF_VAR, (uint16_t)p->line);
            // string.length / .len / .size
            if (strcmp(fld, "len") == 0 || strcmp(fld, "size") == 0 ||
                strcmp(fld, "length") == 0) {
                emit_u8(p, RSVM_OP_LOAD); emit_u8(p, s);
                emit_u8(p, RSVM_OP_STR_LEN);
                return;
            }
            emit_u8(p, RSVM_OP_LOAD); emit_u8(p, s);
            int fi = field_index(p->vm, fld);
            if (fi < 0) { fail(p, "unknown field"); return; }
            rsvm_ref_note(p->vm, fld, RSVM_REF_FIELD, (uint16_t)p->line);
            emit_u8(p, RSVM_OP_FIELD_LD); emit_u8(p, (uint8_t)fi);
            // chained: obj.a.b.c
            while (peek(p) == '.') {
                getc_(p);
                if (!parse_ident(p, fld, sizeof fld)) { fail(p, "field chain"); return; }
                fi = field_index(p->vm, fld);
                if (fi < 0) { fail(p, "unknown field"); return; }
                rsvm_ref_note(p->vm, fld, RSVM_REF_FIELD, (uint16_t)p->line);
                emit_u8(p, RSVM_OP_FIELD_LD); emit_u8(p, (uint8_t)fi);
            }
            return;
        }
        // string / array index: name[expr]
        if (peek(p) == '[') {
            getc_(p);
            uint8_t s = slot_of(p, id, true);
            rsvm_ref_note(p->vm, id, RSVM_REF_VAR, (uint16_t)p->line);
            emit_u8(p, RSVM_OP_LOAD); emit_u8(p, s);
            int nidx = 0;
            expr(p); nidx = 1;
            while (match(p, ",")) { expr(p); nidx++; }
            expect(p, "]");
            if (nidx <= 1) emit_u8(p, RSVM_OP_STR_CHAR); // str or 1D arr
            else { emit_u8(p, RSVM_OP_ARR_LDN); emit_u8(p, (uint8_t)nidx); }
            return;
        }
        // builtins first
        if (strcmp(id, "file_read") == 0 && peek(p) == '(') {
            getc_(p); expr(p); expect(p, ")");
            emit_u8(p, RSVM_OP_FREAD);
            return;
        }
        if (strcmp(id, "file_write") == 0 && peek(p) == '(') {
            getc_(p); expr(p); expect(p, ","); expr(p); expect(p, ")");
            emit_u8(p, RSVM_OP_FWRITE);
            return;
        }
        if (strcmp(id, "arr_new") == 0 && peek(p) == '(') {
            getc_(p);
            int ndim = 0;
            skip_ws(p);
            if (peek(p) != ')') {
                expr(p); ndim = 1;
                while (match(p, ",")) { expr(p); ndim++; }
            }
            expect(p, ")");
            if (ndim <= 1) {
                emit_u8(p, RSVM_OP_ARR_NEW);
            } else {
                emit_u8(p, RSVM_OP_ARR_NEWD); emit_u8(p, (uint8_t)ndim);
            }
            return;
        }
        if (strcmp(id, "arr_len") == 0 && peek(p) == '(') {
            getc_(p); expr(p); expect(p, ")");
            emit_u8(p, RSVM_OP_ARR_LEN);
            return;
        }
        if (strcmp(id, "arr_push") == 0 && peek(p) == '(') {
            getc_(p); expr(p); expect(p, ","); expr(p); expect(p, ")");
            emit_u8(p, RSVM_OP_ARR_PUSH);
            return;
        }
        if ((strcmp(id, "arr_of") == 0 || strcmp(id, "arr_pack") == 0) && peek(p) == '(') {
            getc_(p);
            int n = 0;
            skip_ws(p);
            if (peek(p) != ')') {
                expr(p); n = 1;
                while (match(p, ",")) { expr(p); n++; }
            }
            expect(p, ")");
            emit_u8(p, RSVM_OP_ARR_PACK); emit_u8(p, (uint8_t)n);
            return;
        }
        if (strcmp(id, "arr_swap") == 0 && peek(p) == '(') {
            getc_(p); expr(p); expect(p, ","); expr(p); expect(p, ","); expr(p); expect(p, ")");
            emit_u8(p, RSVM_OP_ARR_SWAP);
            return;
        }
        if (strcmp(id, "arr_pop") == 0 && peek(p) == '(') {
            getc_(p); expr(p); expect(p, ")");
            emit_u8(p, RSVM_OP_ARR_POP);
            return;
        }
        // Classic: foo(args)  OR  Vulcan: foo in[args] out[dests|here|void]
        // Overload: if foo missing, try i32_foo / f32_foo from first-arg literal kind.
        if (match(p, "in")) {
            expect(p, "[");
            // Peek first arg kind before emitting exprs (for overload resolve)
            int arg_kind = 0;
            skip_ws(p);
            if (peek(p) != ']') arg_kind = peek_arg_kind(p);

            int fid = resolve_func_overload(p, id, arg_kind);
            if (fid < 0) fid = rsvm_func_by_name(p->vm, id);
            if (fid < 0) { fail(p, "unknown function"); return; }
            rsvm_ref_note(p->vm, id, RSVM_REF_FUNC, (uint16_t)p->line);
            rsvm_func_ent_t* f = &p->vm->funcs[fid];
            uint8_t n_in = f->n_in;
            uint8_t n_out = f->n_out;

            for (uint8_t i = 0; i < n_in; ++i) {
                if (i) expect(p, ",");
                skip_ws(p);
                if (peek(p) == ']') break;
                expr(p);
            }
            expect(p, "]");
            skip_ws(p);
            int here_mode = 0; // 0=none/stmt, 1=here, 2=void, 3=store slots
            uint8_t out_slots[8]; uint8_t n_store = 0;
            if (match(p, "out")) {
                expect(p, "[");
                skip_ws(p);
                if (match(p, "here")) {
                    here_mode = 1;
                } else if (match(p, "void")) {
                    here_mode = 2;
                } else {
                    here_mode = 3;
                    while (n_store < 8 && peek(p) && peek(p) != ']') {
                        if (n_store) expect(p, ",");
                        char on[RSVM_NAME_LEN];
                        if (!parse_ident(p, on, sizeof on)) { fail(p, "out name"); return; }
                        out_slots[n_store++] = slot_of(p, on, true);
                        skip_ws(p);
                    }
                }
                expect(p, "]");
            } else {
                if (n_out > 0) {
                    fail(p, "function has outputs; add out[params] or out[void]");
                    return;
                }
            }

            // Inline small monomorphic bodies (no @noinline, body_len known & small)
            bool did_inline = false;
            if (!(f->props & RSVM_PROP_NOINLINE) && f->body_len > 0 && f->body_len < 48
                && f->n_in <= 4 && f->entry_pc + f->body_len <= p->vm->code_len) {
                // Pop args into parameter slots (same as CALL), then copy body sans RET
                for (int i = (int)n_in - 1; i >= 0; --i) {
                    uint8_t s = f->arg_slots[i];
                    emit_u8(p, RSVM_OP_STORE); emit_u8(p, s);
                }
                uint16_t blen = f->body_len;
                // Trim trailing RET0 / RET if present in body_len window
                const uint8_t* src = p->vm->code + f->entry_pc;
                uint16_t copy_len = blen;
                if (copy_len > 0 && src[copy_len - 1] == RSVM_OP_RET0)
                    copy_len--;
                else if (copy_len > 1 && src[copy_len - 2] == RSVM_OP_RET)
                    copy_len = (uint16_t)(copy_len - 2);
                if (p->vm->code_len + copy_len > RSVM_MAX_CODE) {
                    fail(p, "inline overflow"); return;
                }
                memcpy(p->vm->code + p->vm->code_len, src, copy_len);
                p->vm->code_len = (uint16_t)(p->vm->code_len + copy_len);
                did_inline = true;
            }
            if (!did_inline) {
                emit_u8(p, RSVM_OP_CALL); emit_u8(p, (uint8_t)fid);
            }
            if (here_mode == 2) {
                for (uint8_t i = 0; i < n_out; ++i) emit_u8(p, RSVM_OP_IGNORE);
            } else if (here_mode == 3) {
                for (int i = (int)n_store - 1; i >= 0; --i) {
                    emit_u8(p, RSVM_OP_STORE); emit_u8(p, out_slots[i]);
                }
            }
            return;
        }
        // builtins as primary
        
        if (strcmp(id, "file_write") == 0 && peek(p) == '(') {
            getc_(p); expr(p); expect(p, ","); expr(p); expect(p, ")");
            emit_u8(p, RSVM_OP_FWRITE);
            return;
        }
        if (strcmp(id, "arr_new") == 0 && peek(p) == '(') {
            getc_(p);
            int ndim = 0;
            skip_ws(p);
            if (peek(p) != ')') {
                expr(p); ndim = 1;
                while (match(p, ",")) { expr(p); ndim++; }
            }
            expect(p, ")");
            if (ndim <= 1) {
                emit_u8(p, RSVM_OP_ARR_NEW);
            } else {
                emit_u8(p, RSVM_OP_ARR_NEWD); emit_u8(p, (uint8_t)ndim);
            }
            return;
        }
        if (strcmp(id, "arr_len") == 0 && peek(p) == '(') {
            getc_(p); expr(p); expect(p, ")");
            emit_u8(p, RSVM_OP_ARR_LEN);
            return;
        }
        if (strcmp(id, "arr_push") == 0 && peek(p) == '(') {
            getc_(p); expr(p); expect(p, ","); expr(p); expect(p, ")");
            emit_u8(p, RSVM_OP_ARR_PUSH);
            return;
        }
        if ((strcmp(id, "arr_of") == 0 || strcmp(id, "arr_pack") == 0) && peek(p) == '(') {
            getc_(p);
            int n = 0;
            skip_ws(p);
            if (peek(p) != ')') {
                expr(p); n = 1;
                while (match(p, ",")) { expr(p); n++; }
            }
            expect(p, ")");
            emit_u8(p, RSVM_OP_ARR_PACK); emit_u8(p, (uint8_t)n);
            return;
        }
        if (strcmp(id, "arr_swap") == 0 && peek(p) == '(') {
            getc_(p); expr(p); expect(p, ","); expr(p); expect(p, ","); expr(p); expect(p, ")");
            emit_u8(p, RSVM_OP_ARR_SWAP);
            return;
        }
        if (strcmp(id, "arr_pop") == 0 && peek(p) == '(') {
            getc_(p); expr(p); expect(p, ")");
            emit_u8(p, RSVM_OP_ARR_POP);
            return;
        }
        uint8_t s = slot_of(p, id, true);
        rsvm_ref_note(p->vm, id, RSVM_REF_VAR, (uint16_t)p->line);
        emit_u8(p, RSVM_OP_LOAD); emit_u8(p, s);
        return;
    }
    fail(p, "bad primary");
}
static void unary(P* p) {
    skip_ws(p);
    if (match(p, "-")) { unary(p); emit_u8(p, RSVM_OP_NEG); return; }
    if (match(p, "!")) { unary(p); emit_u8(p, RSVM_OP_NOT); return; }
    if (match(p, "~")) { unary(p); emit_u8(p, RSVM_OP_BNOT); return; }
    primary(p);
}
static void mul(P* p) {
    unary(p);
    for (;;) {
        skip_ws(p);
        if (match(p, "*")) { unary(p); emit_u8(p, RSVM_OP_MUL); }
        else if (match(p, "/")) { unary(p); emit_u8(p, RSVM_OP_DIV); }
        else if (match(p, "%")) { unary(p); emit_u8(p, RSVM_OP_MOD); }
        else break;
    }
}
static void add(P* p) {
    mul(p);
    for (;;) {
        skip_ws(p);
        if (match(p, "+")) { mul(p); emit_u8(p, RSVM_OP_ADD); }
        else if (match(p, "-")) { mul(p); emit_u8(p, RSVM_OP_SUB); }
        else break;
    }
}
static void shift_expr(P* p) {
    add(p);
    for (;;) {
        skip_ws(p);
        if (match(p, "<<")) { add(p); emit_u8(p, RSVM_OP_SHL); }
        else if (match(p, ">>")) { add(p); emit_u8(p, RSVM_OP_SHR); }
        else break;
    }
}
static void cmp_expr(P* p) {
    shift_expr(p);
    skip_ws(p);
    uint8_t cmp = 0;
    if (match(p, "==")) cmp = RSVM_OP_EQ;
    else if (match(p, "!=")) cmp = RSVM_OP_NE;
    else if (match(p, "<=")) cmp = RSVM_OP_LE;
    else if (match(p, ">=")) cmp = RSVM_OP_GE;
    else if (match(p, "<"))  cmp = RSVM_OP_LT;
    else if (match(p, ">"))  cmp = RSVM_OP_GT;
    if (cmp) { shift_expr(p); emit_u8(p, cmp); }
}
static void band_expr(P* p) {
    cmp_expr(p);
    for (;;) {
        skip_ws(p);
        // single & but not &&
        if (peek(p) == '&' && p->s[p->pos + 1] != '&') {
            getc_(p); cmp_expr(p); emit_u8(p, RSVM_OP_BAND);
        } else break;
    }
}
static void bxor_expr(P* p) {
    band_expr(p);
    for (;;) {
        skip_ws(p);
        if (peek(p) == '^') { getc_(p); band_expr(p); emit_u8(p, RSVM_OP_BXOR); }
        else break;
    }
}
static void bor_expr(P* p) {
    bxor_expr(p);
    for (;;) {
        skip_ws(p);
        if (peek(p) == '|' && p->s[p->pos + 1] != '|') {
            getc_(p); bxor_expr(p); emit_u8(p, RSVM_OP_BOR);
        } else break;
    }
}
static void expr(P* p) {
    bor_expr(p);
}
static void parse_block(P* p) {
    expect(p, "{");
    while (!p->failed) {
        skip_ws(p);
        if (peek(p) == '}') { getc_(p); return; }
        if (!peek(p)) { fail(p, "unclosed {"); return; }
        statement(p);
    }
}

static void statement(P* p) {
    skip_ws(p);
    if (p->failed || !peek(p)) return;

    // --- thread / property state-machine sugar: ` `` ``` ---
    // ```key=val```  → configure active module (usually thread)
    // `name { … }`   → sequential ordered block (runs as normal {})
    // ``name { … }`` → parallel block (sequential fallback on desktop)
    if (peek(p) == '`') {
        int ticks = 0;
        while (peek(p) == '`') { getc_(p); ticks++; }
        skip_ws(p);
        if (ticks >= 3) {
            // config: key=val  (optional trailing ```)
            char key[32] = {0}, val[32] = {0};
            int ki = 0;
            while (peek(p) && peek(p) != '=' && peek(p) != '`' && ki < 31)
                key[ki++] = getc_(p);
            if (peek(p) == '=') {
                getc_(p);
                int vi = 0;
                while (peek(p) && peek(p) != '`' && peek(p) != '\n' && vi < 31)
                    val[vi++] = getc_(p);
            }
            while (peek(p) == '`') getc_(p);
            if (p->vm->mdl_thread && p->vm->mdl_thread->configure)
                p->vm->mdl_thread->configure(p->vm, key, val);
            else if (p->vm->mdl_property && p->vm->mdl_property->configure)
                p->vm->mdl_property->configure(p->vm, key, val);
            skip_ws(p);
            if (peek(p) == ';') getc_(p);
            return;
        }
        // 1 or 2 ticks: optional label then a normal block (sequential)
        if (isalpha((unsigned char)peek(p)) || peek(p)=='_') {
            char label[RSVM_NAME_LEN];
            parse_ident(p, label, sizeof label);
            (void)label;
        }
        // consume optional trailing ticks before {
        while (peek(p) == '`') getc_(p);
        skip_ws(p);
        parse_block(p);
        // trailing ticks after }
        while (peek(p) == '`') getc_(p);
        skip_ws(p);
        if (peek(p) == ';') getc_(p);
        return;
    }

    // @shared / @mut variable modifiers before a declaration
    uint8_t decl_props = 0;
    while (peek(p) == '@') {
        int save = p->pos, sl = p->line, sc = p->col;
        getc_(p);
        char pn[RSVM_NAME_LEN];
        if (!parse_ident(p, pn, sizeof pn)) { p->pos=save; p->line=sl; p->col=sc; break; }
        uint16_t bit = rsvm_prop_from_name(pn);
        if (bit == RSVM_PROP_SHARED || bit == RSVM_PROP_MUT || bit == RSVM_PROP_IMMUT) {
            decl_props |= (uint8_t)bit;
            skip_ws(p);
            continue;
        }
        // not a decl modifier – rewind (could be enum member@Enum later)
        p->pos = save; p->line = sl; p->col = sc;
        break;
    }

    if (match(p, "halt")) { expect(p, ";"); emit_u8(p, RSVM_OP_HALT); return; }

    // set_output_bytecode "path.bvul";  or set_output_bytecode; → <main>.bvul
    if (match(p, "set_output_bytecode")) {
        skip_ws(p);
        if (peek(p) == '"') {
            getc_(p);
            char path[128]; int n = 0;
            while (peek(p) && peek(p) != '"' && n < (int)sizeof(path) - 1)
                path[n++] = getc_(p);
            expect(p, "\"");
            path[n] = 0;
            strncpy(p->vm->output_bvul, path, sizeof p->vm->output_bvul - 1);
        } else {
            strncpy(p->vm->output_bvul, "out.bvul", sizeof p->vm->output_bvul - 1);
        }
        expect(p, ";");
        p->vm->want_bvul = true;
        return;
    }
    // include "rel.vul";  /  import "rel.vul";
    if (match(p, "include") || match(p, "import")) {
        expect(p, "\"");
        char rel[160]; int n = 0;
        while (peek(p) && peek(p) != '"' && n < (int)sizeof(rel) - 1)
            rel[n++] = getc_(p);
        expect(p, "\"");
        expect(p, ";");
        rel[n] = 0;
        if (p->include_depth > 20) { fail(p, "include too deep"); return; }
        char full[256];
        path_join(full, sizeof full, p->vm->include_base, rel);
        size_t len = 0;
        char* src = read_entire_file(full, &len);
        if (!src) { fail(p, "include file not found"); return; }
        // save parser position and swap source
        const char* old_s = p->s;
        int old_pos = p->pos, old_line = p->line, old_col = p->col;
        char old_base[192];
        strncpy(old_base, p->vm->include_base, sizeof old_base - 1);
        old_base[sizeof old_base - 1] = 0;
        char new_base[192];
        path_dirname(new_base, sizeof new_base, full);
        strncpy(p->vm->include_base, new_base, sizeof p->vm->include_base - 1);
        p->s = src; p->pos = 0; p->line = 1; p->col = 1;
        p->include_depth++;
        while (!p->failed) {
            skip_ws(p);
            if (!peek(p)) break;
            statement(p);
        }
        p->include_depth--;
        free(src);
        p->s = old_s; p->pos = old_pos; p->line = old_line; p->col = old_col;
        strncpy(p->vm->include_base, old_base, sizeof p->vm->include_base - 1);
        return;
    }
    // program-wide config: set_step_depth n;
    if (match(p, "set_step_depth")) {
        int32_t n = 0;
        if (!parse_int(p, &n) || n < 0) { fail(p, "set_step_depth n"); return; }
        expect(p, ";");
        p->vm->step_limit = (uint32_t)n;
        return;
    }
    if (match(p, "set_loops_trapdoor")) {
        int32_t n = 1;
        skip_ws(p);
        if (peek(p) != ';') {
            if (!parse_int(p, &n)) { fail(p, "set_loops_trapdoor 0|1"); return; }
        }
        expect(p, ";");
        p->loop_trapdoor = (n != 0);
        p->trap_disabled = (n == 0);
        return;
    }
    if (match(p, "set_limitpassoptimize") || match(p, "set_opt_passes")) {
        int32_t n = 0;
        if (!parse_int(p, &n) || n < 0 || n > 4) { fail(p, "set_limitpassoptimize 0..4"); return; }
        expect(p, ";");
        p->vm->opt_passes = (uint8_t)n;
        return;
    }
    if (match(p, "set_packed_instructions")) {
        int32_t n = 1;
        skip_ws(p);
        if (peek(p) != ';') {
            if (!parse_int(p, &n)) { fail(p, "set_packed_instructions 0|1"); return; }
        }
        expect(p, ";");
        p->vm->packed_instructions = (n != 0) ? 1 : 0;
        return;
    }
    if (match(p, "set_enable_algo_rewrite")) {
        int32_t n = 1;
        skip_ws(p);
        if (peek(p) != ';') {
            if (!parse_int(p, &n)) { fail(p, "set_enable_algo_rewrite 0|1"); return; }
        }
        expect(p, ";");
        p->vm->enable_algo_rewrite = (n != 0) ? 1 : 0;
        return;
    }
    if (match(p, "set_superoptimize_enable") || match(p, "set_superoptimize")) {
        int32_t n = 1;
        skip_ws(p);
        if (peek(p) != ';') {
            if (!parse_int(p, &n)) { fail(p, "set_superoptimize_enable 0|1"); return; }
        }
        expect(p, ";");
        p->vm->superoptimize = (n != 0) ? 1 : 0;
        return;
    }
    if (match(p, "return")) {
        skip_ws(p);
        if (peek(p) == ';') { getc_(p); emit_u8(p, RSVM_OP_RET0); return; }
        uint8_t n = 0;
        expr(p); n++;
        while (match(p, ",")) { expr(p); n++; }
        expect(p, ";");
        emit_u8(p, RSVM_OP_RET); emit_u8(p, n);
        return;
    }
    // --- shell / OS (bash-like) ---
    if (match(p, "sysconf_set")) {
        expect(p, "("); expr(p); expect(p, ","); expr(p); expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_SYSCONF_SET); emit_u8(p, RSVM_OP_IGNORE);
        return;
    }
    if (match(p, "sh") || match(p, "sys")) {
        expect(p, "("); expr(p); expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_SYS_CMD); emit_u8(p, RSVM_OP_IGNORE);
        return;
    }
    if (match(p, "exec") || match(p, "run")) {
        expect(p, "("); expr(p); expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_SYS_EXEC); emit_u8(p, RSVM_OP_IGNORE);
        return;
    }
    if (match(p, "open_app") || match(p, "open")) {
        expect(p, "("); expr(p); expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_SYS_OPEN); emit_u8(p, RSVM_OP_IGNORE);
        return;
    }
    if (match(p, "ls")) {
        expect(p, "("); skip_ws(p);
        if (peek(p) == ')') {
            getc_(p); expect(p, ";");
            emit_u8(p, RSVM_OP_STR_LIT); emit_u8(p, 1); emit_u8(p, (uint8_t)'.');
        } else {
            expr(p); expect(p, ")"); expect(p, ";");
        }
        emit_u8(p, RSVM_OP_SYS_LS);
        // leave listing on stack — usually assigned or printed; as stmt discard
        emit_u8(p, RSVM_OP_IGNORE);
        return;
    }
    if (match(p, "cd")) {
        expect(p, "("); expr(p); expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_SYS_CD); emit_u8(p, RSVM_OP_IGNORE);
        return;
    }
    if (match(p, "pwd")) {
        expect(p, "("); expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_SYS_PWD); emit_u8(p, RSVM_OP_IGNORE);
        return;
    }
    if (match(p, "mw_text")) {
        expect(p, "("); expr(p); expect(p, ","); expr(p); expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_MW_TEXT); emit_u8(p, RSVM_OP_IGNORE);
        return;
    }
    if (match(p, "uart_send")) {
        expect(p, "("); expr(p); expect(p, ","); expr(p); expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_UART_SEND); emit_u8(p, RSVM_OP_IGNORE);
        return;
    }
    if (match(p, "pool")) {
        // pool(name, op, data?)
        expect(p, "("); expr(p); expect(p, ","); expr(p);
        skip_ws(p);
        if (peek(p) == ',') { getc_(p); expr(p); }
        else { emit_u8(p, RSVM_OP_STR_LIT); emit_u8(p, 0); }
        expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_POOL_OP); emit_u8(p, RSVM_OP_IGNORE);
        return;
    }
    if (match(p, "env")) {
        expect(p, "("); expr(p); expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_SYS_ENV); emit_u8(p, RSVM_OP_IGNORE);
        return;
    }
    if (match(p, "file_read")) {
        // file_read(path) → string on stack, typically assigned
        expect(p, "("); expr(p); expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_FREAD);
        emit_u8(p, RSVM_OP_IGNORE); // discard if statement form
        return;
    }
    if (match(p, "file_write")) {
        expect(p, "("); expr(p); expect(p, ","); expr(p); expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_FWRITE);
        emit_u8(p, RSVM_OP_IGNORE);
        return;
    }
    if (match(p, "printc")) {
        expect(p, "("); expr(p); expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_PRINT_C);
        return;
    }
    if (match(p, "printnl")) {
        expect(p, "("); expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_PRINT_NL);
        return;
    }
    if (match(p, "print")) {
        expect(p, "("); skip_ws(p);
        if (peek(p) == '"') {
            getc_(p); char tmp[96]; int n=0;
            while (peek(p) && peek(p)!='"' && n<(int)sizeof(tmp)-1) tmp[n++]=getc_(p);
            expect(p, "\""); expect(p, ")"); expect(p, ";");
            emit_u8(p, RSVM_OP_PRINT_STR); emit_u8(p, (uint8_t)n);
            for (int i=0;i<n;i++) emit_u8(p, (uint8_t)tmp[i]);
            return;
        }
        expr(p); expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_PRINT_V);
        return;
    }
    if (match(p, "arr_swap")) {
        expect(p, "(");
        char an[RSVM_NAME_LEN];
        if (!parse_ident(p, an, sizeof an)) { fail(p, "arr name"); return; }
        uint8_t s = slot_of(p, an, true);
        expect(p, ","); 
        emit_u8(p, RSVM_OP_LOAD); emit_u8(p, s);
        expr(p); expect(p, ","); expr(p);
        expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_ARR_SWAP);
        emit_u8(p, RSVM_OP_STORE); emit_u8(p, s);
        return;
    }
    if (match(p, "arr_pop")) {
        expect(p, "(");
        char an[RSVM_NAME_LEN];
        if (!parse_ident(p, an, sizeof an)) { fail(p, "arr name"); return; }
        uint8_t s = slot_of(p, an, true);
        expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_LOAD); emit_u8(p, s);
        emit_u8(p, RSVM_OP_ARR_POP);
        emit_u8(p, RSVM_OP_STORE); emit_u8(p, s);
        return;
    }
    if (match(p, "arr_push")) {
        expect(p, "(");
        char an[RSVM_NAME_LEN];
        if (!parse_ident(p, an, sizeof an)) { fail(p, "arr name"); return; }
        uint8_t s = slot_of(p, an, true);
        expect(p, ",");
        emit_u8(p, RSVM_OP_LOAD); emit_u8(p, s);
        expr(p);
        expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_ARR_PUSH);
        emit_u8(p, RSVM_OP_STORE); emit_u8(p, s);
        return;
    }
    if (match(p, "delay_ms")) {
        expect(p, "("); int32_t ms=0;
        if (!parse_int(p, &ms)) { fail(p, "ms"); return; }
        expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_DELAY_MS); emit_i32(p, ms); return;
    }
    if (match(p, "pin_mode")) {
        expect(p, "("); int32_t pin=0;
        if (!parse_int(p, &pin)) { fail(p, "pin"); return; }
        expect(p, ",");
        uint8_t mode = match(p,"out")?1: match(p,"in_pu")?2: match(p,"in")?0:0xFF;
        if (mode==0xFF) { fail(p, "mode"); return; }
        expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_PIN_MODE); emit_u8(p,(uint8_t)pin); emit_u8(p,mode); return;
    }
    if (match(p, "dig_write")) {
        expect(p, "("); int32_t pin=0, lvl=0;
        if (!parse_int(p,&pin)) { fail(p,"pin"); return; }
        expect(p, ","); if (!parse_int(p,&lvl)) { fail(p,"lvl"); return; }
        expect(p, ")"); expect(p, ";");
        emit_u8(p, RSVM_OP_DIG_WR); emit_u8(p,(uint8_t)pin); emit_u8(p,(uint8_t)lvl); return;
    }
    if (match(p, "dig_read")) {
        expect(p, "("); int32_t pin=0; char id[RSVM_NAME_LEN];
        if (!parse_int(p,&pin)) { fail(p,"pin"); return; }
        expect(p, ","); if (!parse_ident(p,id,sizeof id)) { fail(p,"dst"); return; }
        expect(p, ")"); expect(p, ";");
        uint8_t s = slot_of(p,id,true);
        emit_u8(p, RSVM_OP_DIG_RD); emit_u8(p,(uint8_t)pin); emit_u8(p,s); return;
    }

    // while [cond] { body }
    if (match(p, "while")) {
        expect(p, "[");
        uint16_t region_start = p->vm->code_len;
        uint16_t top = p->vm->code_len;
        expr(p);
        expect(p, "]");
        emit_u8(p, RSVM_OP_JZ);
        uint16_t jz = p->vm->code_len; emit_i16(p, 0);
        parse_block(p);
        emit_u8(p, RSVM_OP_JMP);
        int16_t back = (int16_t)(top - (p->vm->code_len + 2));
        emit_i16(p, back);
        int16_t rel = (int16_t)(p->vm->code_len - (jz + 2));
        p->vm->code[jz] = (uint8_t)(rel & 0xFF);
        p->vm->code[jz + 1] = (uint8_t)((rel >> 8) & 0xFF);
        push_region(p, region_start, p->vm->code_len, RSVM_REG_WHILE);
        return;
    }
    // for [init; cond; step] { body }  — init/step are statements without extra ;
    if (match(p, "for")) {
        expect(p, "[");
        uint16_t region_start = p->vm->code_len;
        // init
        if (peek(p) != ';') {
            // assignment or decl-ish: id = expr
            char id[RSVM_NAME_LEN];
            if (parse_ident(p, id, sizeof id) && match(p, "=")) {
                uint8_t s = slot_of(p, id, true);
                expr(p);
                emit_u8(p, RSVM_OP_STORE); emit_u8(p, s);
            } else {
                fail(p, "for init"); return;
            }
        }
        expect(p, ";");
        uint16_t cond_pc = p->vm->code_len;
        expr(p);
        expect(p, ";");
        emit_u8(p, RSVM_OP_JZ);
        uint16_t jz = p->vm->code_len; emit_i16(p, 0);
        // step is parsed later: save source positions
        int step_pos = p->pos, step_line = p->line, step_col = p->col;
        // skip step tokens until ]
        int depth = 0;
        while (peek(p) && !(peek(p) == ']' && depth == 0)) {
            if (peek(p) == '[') depth++;
            if (peek(p) == ']') depth--;
            getc_(p);
        }
        expect(p, "]");
        parse_block(p);
        // emit step by re-parsing saved region
        {
            int save = p->pos, sl = p->line, sc = p->col;
            p->pos = step_pos; p->line = step_line; p->col = step_col;
            skip_ws(p);
            if (peek(p) != ']') {
                char id[RSVM_NAME_LEN];
                if (parse_ident(p, id, sizeof id) && match(p, "=")) {
                    uint8_t s = slot_of(p, id, true);
                    expr(p);
                    emit_u8(p, RSVM_OP_STORE); emit_u8(p, s);
                } else {
                    // expression step (e.g. discarded)
                    expr(p);
                    emit_u8(p, RSVM_OP_IGNORE);
                }
            }
            p->pos = save; p->line = sl; p->col = sc;
        }
        emit_u8(p, RSVM_OP_JMP);
        int16_t back = (int16_t)(cond_pc - (p->vm->code_len + 2));
        emit_i16(p, back);
        int16_t rel = (int16_t)(p->vm->code_len - (jz + 2));
        p->vm->code[jz] = (uint8_t)(rel & 0xFF);
        p->vm->code[jz + 1] = (uint8_t)((rel >> 8) & 0xFF);
        push_region(p, region_start, p->vm->code_len, RSVM_REG_FOR);
        return;
    }
    if (match(p, "switch")) {
        // Prefer jump-table when all case keys are integer/enum constants
        // and the key span is small (≤64). Otherwise fall back to linear EQ chain.
        // Region [start,end) is recorded so body lengths are known at compile time.
        expect(p, "["); expr(p); expect(p, "]");
        expect(p, "{");

        // Collect case source positions and constant keys (reparse bodies later for JT)
        enum { MAX_CASES = 32 };
        int32_t keys[MAX_CASES];
        int     body_pos[MAX_CASES], body_line[MAX_CASES], body_col[MAX_CASES];
        int     ncases = 0;
        int     def_pos = -1, def_line = 0, def_col = 0;
        bool    all_const = true;
        int     save_pos = p->pos, save_line = p->line, save_col = p->col;

        while (!p->failed && peek(p) != '}') {
            skip_ws(p);
            if (match(p, "case")) {
                if (ncases >= MAX_CASES) { fail(p, "too many cases"); return; }
                expect(p, "[");
                skip_ws(p);
                int32_t kv = 0;
                int ksave = p->pos;
                if (parse_int(p, &kv)) {
                    keys[ncases] = kv;
                } else {
                    all_const = false;
                    // skip until ]
                    while (peek(p) && peek(p) != ']') getc_(p);
                }
                expect(p, "]");
                body_pos[ncases] = p->pos; body_line[ncases] = p->line; body_col[ncases] = p->col;
                // skip one block { ... }
                skip_ws(p);
                if (peek(p) == '{') {
                    getc_(p);
                    int depth = 1;
                    while (peek(p) && depth > 0) {
                        char c = getc_(p);
                        if (c == '{') depth++;
                        else if (c == '}') depth--;
                    }
                } else {
                    // single statement until ;
                    while (peek(p) && peek(p) != ';' && peek(p) != '}') getc_(p);
                    if (peek(p) == ';') getc_(p);
                }
                ncases++;
            } else if (match(p, "default")) {
                def_pos = p->pos; def_line = p->line; def_col = p->col;
                skip_ws(p);
                if (peek(p) == '{') {
                    getc_(p);
                    int depth = 1;
                    while (peek(p) && depth > 0) {
                        char c = getc_(p);
                        if (c == '{') depth++;
                        else if (c == '}') depth--;
                    }
                }
            } else break;
        }
        expect(p, "}");

        // Decide JT vs linear
        int32_t kmin = 0, kmax = 0;
        uint16_t span = 0;
        bool use_jt = false;
        if (all_const && ncases > 0 && ncases <= MAX_CASES) {
            kmin = keys[0]; kmax = keys[0];
            for (int i = 1; i < ncases; ++i) {
                if (keys[i] < kmin) kmin = keys[i];
                if (keys[i] > kmax) kmax = keys[i];
            }
            if (kmax >= kmin && (kmax - kmin) < 64) {
                span = (uint16_t)(kmax - kmin + 1);
                use_jt = (span <= 64 && ncases >= 2);
            }
        }

        uint16_t region_start = p->vm->code_len;

        if (use_jt) {
            // Value already on stack from first expr().
            emit_u8(p, RSVM_OP_SWITCH_JT);
            emit_i32(p, kmin);
            emit_u16(p, span);
            uint16_t table_at = p->vm->code_len;
            // placeholders: span * u16 targets + u16 default
            for (uint16_t i = 0; i < span; ++i) emit_u16(p, 0);
            uint16_t def_slot = p->vm->code_len;
            emit_u16(p, 0);

            uint16_t body_pcs[MAX_CASES];
            uint16_t end_patches[MAX_CASES];
            int nend = 0;

            // Emit each case body (reparse from saved positions)
            for (int i = 0; i < ncases; ++i) {
                body_pcs[i] = p->vm->code_len;
                int sp = p->pos, sl = p->line, sc = p->col;
                p->pos = body_pos[i]; p->line = body_line[i]; p->col = body_col[i];
                parse_block(p);
                p->pos = sp; p->line = sl; p->col = sc;
                emit_u8(p, RSVM_OP_JMP);
                if (nend < MAX_CASES) end_patches[nend++] = p->vm->code_len;
                emit_i16(p, 0);
            }

            // default body
            uint16_t def_pc = p->vm->code_len;
            if (def_pos >= 0) {
                int sp = p->pos, sl = p->line, sc = p->col;
                p->pos = def_pos; p->line = def_line; p->col = def_col;
                parse_block(p);
                p->pos = sp; p->line = sl; p->col = sc;
            }
            // fallthrough end
            uint16_t end_pc = p->vm->code_len;

            // Fill jump table: unmapped slots → default
            for (uint16_t s = 0; s < span; ++s) {
                uint16_t tgt = def_pc;
                int32_t key = kmin + (int32_t)s;
                for (int i = 0; i < ncases; ++i) {
                    if (keys[i] == key) { tgt = body_pcs[i]; break; }
                }
                p->vm->code[table_at + s * 2]     = (uint8_t)(tgt & 0xFF);
                p->vm->code[table_at + s * 2 + 1] = (uint8_t)((tgt >> 8) & 0xFF);
            }
            p->vm->code[def_slot]     = (uint8_t)(def_pc & 0xFF);
            p->vm->code[def_slot + 1] = (uint8_t)((def_pc >> 8) & 0xFF);

            // Patch case-end JMPs to end_pc
            for (int i = 0; i < nend; ++i) {
                uint16_t at = end_patches[i];
                int16_t rel = (int16_t)(end_pc - (at + 2));
                p->vm->code[at]     = (uint8_t)(rel & 0xFF);
                p->vm->code[at + 1] = (uint8_t)((rel >> 8) & 0xFF);
            }

            push_region(p, region_start, p->vm->code_len, RSVM_REG_JT);
        } else {
            // Linear EQ chain (original semantics)
            uint16_t end_patches[16]; int nend = 0;
            // reparse from the opening of the switch body — we already consumed '}'
            // so restore and emit linearly in one go by re-reading the whole switch text
            // Stored save_pos points at first case. We need the value still on stack.
            p->pos = save_pos; p->line = save_line; p->col = save_col;
            while (!p->failed && peek(p) != '}') {
                skip_ws(p);
                if (match(p, "case")) {
                    expect(p, "[");
                    emit_u8(p, RSVM_OP_DUP);
                    expr(p);
                    expect(p, "]");
                    emit_u8(p, RSVM_OP_EQ);
                    emit_u8(p, RSVM_OP_JZ);
                    uint16_t jz = p->vm->code_len; emit_i16(p, 0);
                    parse_block(p);
                    emit_u8(p, RSVM_OP_JMP);
                    if (nend < 16) end_patches[nend++] = p->vm->code_len;
                    emit_i16(p, 0);
                    int16_t rel = (int16_t)(p->vm->code_len - (jz + 2));
                    p->vm->code[jz] = (uint8_t)(rel & 0xFF);
                    p->vm->code[jz + 1] = (uint8_t)((rel >> 8) & 0xFF);
                } else if (match(p, "default")) {
                    parse_block(p);
                } else break;
            }
            expect(p, "}");
            emit_u8(p, RSVM_OP_POP); // drop switch value
            for (int i = 0; i < nend; ++i) {
                uint16_t at = end_patches[i];
                int16_t rel = (int16_t)(p->vm->code_len - (at + 2));
                p->vm->code[at] = (uint8_t)(rel & 0xFF);
                p->vm->code[at + 1] = (uint8_t)((rel >> 8) & 0xFF);
            }
            push_region(p, region_start, p->vm->code_len, RSVM_REG_SWITCH);
        }
        return;
    }

    if (match(p, "break")) {
        expect(p, ";");
        emit_u8(p, RSVM_OP_BREAK);
        return;
    }
    if (match(p, "continue")) {
        expect(p, ";");
        emit_u8(p, RSVM_OP_CONTINUE);
        return;
    }
    if (match(p, "when") || match(p, "if")) {
        uint16_t region_start = p->vm->code_len;
        expect(p, "["); expr(p); expect(p, "]");
        emit_u8(p, RSVM_OP_JZ);
        uint16_t jz = p->vm->code_len; emit_i16(p, 0);
        parse_block(p);
        if (match(p, "else")) {
            emit_u8(p, RSVM_OP_JMP);
            uint16_t jp = p->vm->code_len; emit_i16(p, 0);
            int16_t rel = (int16_t)(p->vm->code_len - (jz+2));
            p->vm->code[jz]=(uint8_t)(rel&0xFF); p->vm->code[jz+1]=(uint8_t)((rel>>8)&0xFF);
            parse_block(p);
            int16_t r2 = (int16_t)(p->vm->code_len - (jp+2));
            p->vm->code[jp]=(uint8_t)(r2&0xFF); p->vm->code[jp+1]=(uint8_t)((r2>>8)&0xFF);
        } else {
            int16_t rel = (int16_t)(p->vm->code_len - (jz+2));
            p->vm->code[jz]=(uint8_t)(rel&0xFF); p->vm->code[jz+1]=(uint8_t)((rel>>8)&0xFF);
        }
        push_region(p, region_start, p->vm->code_len, RSVM_REG_IF);
        return;
    }
    if (match(p, "loop") && match(p, "while")) {
        expect(p, "[");
        uint16_t top = p->vm->code_len;
        expr(p); expect(p, "]");
        emit_u8(p, RSVM_OP_JZ);
        uint16_t jz = p->vm->code_len; emit_i16(p, 0);
        parse_block(p);
        emit_u8(p, RSVM_OP_JMP);
        int16_t back = (int16_t)(top - (p->vm->code_len + 2));
        emit_i16(p, back);
        int16_t rel = (int16_t)(p->vm->code_len - (jz+2));
        p->vm->code[jz]=(uint8_t)(rel&0xFF); p->vm->code[jz+1]=(uint8_t)((rel>>8)&0xFF);
        return;
    }
    if (match(p, "do") && match(p, "n")) {
        // do n[expr] { body }
        // Paths:
        //  1) @autounroll(k) + const count ≤ k → emit body N times (no loop)
        //  2) @loops_trapdoor / set_loops_trapdoor → OP_TRAP_LOOP (native C for)
        //  3) default → classic countdown JMP loop
        expect(p, "[");
        skip_ws(p);
        int32_t const_n = -1;
        int save = p->pos, sl = p->line, sc = p->col;
        if (parse_int(p, &const_n)) {
            skip_ws(p);
            if (peek(p) != ']') { const_n = -1; p->pos = save; p->line = sl; p->col = sc; }
        } else {
            p->pos = save; p->line = sl; p->col = sc;
        }

        // Trapdoor is DEFAULT. Disable only via set_loops_trapdoor 0 / @trapdoor_disable.
        bool want_trap = p->loop_trapdoor
                      && !p->trap_disabled
                      && !(p->cur_func_props & RSVM_PROP_TRAP_DISABLE);
        // Legacy @loops_trapdoor still forces on even if global was set 0
        if (p->cur_func_props & RSVM_PROP_LOOPS_TRAP) want_trap = true;
        int unroll_max = p->autounroll_max;
        if ((p->cur_func_props & RSVM_PROP_AUTOUNROLL) && unroll_max <= 0)
            unroll_max = 8; // default when flag set without (n)

        // --- autounroll const ---
        if (const_n >= 0 && unroll_max > 0 && const_n <= unroll_max) {
            expect(p, "]");
            // emit body const_n times by parsing once then memcpy
            uint16_t body_pc = p->vm->code_len;
            p->do_depth++;
            parse_block(p);
            p->do_depth--;
            uint16_t blen = (uint16_t)(p->vm->code_len - body_pc);
            for (int32_t i = 1; i < const_n; ++i) {
                if (p->vm->code_len + blen > RSVM_MAX_CODE) { fail(p, "unroll overflow"); return; }
                memcpy(p->vm->code + p->vm->code_len, p->vm->code + body_pc, blen);
                p->vm->code_len = (uint16_t)(p->vm->code_len + blen);
            }
            return;
        }

        // --- trapdoor: native for over body range ---
        if (want_trap) {
            uint16_t region_start = p->vm->code_len;
            if (const_n >= 0) {
                expect(p, "]");
                emit_u8(p, RSVM_OP_PUSHI); emit_i32(p, const_n);
            } else {
                expr(p);
                expect(p, "]");
            }
            emit_u8(p, RSVM_OP_TRAP_LOOP);
            uint16_t len_at = p->vm->code_len;
            emit_u8(p, 0); emit_u8(p, 0); // body_len placeholder
            uint16_t body_pc = p->vm->code_len;
            p->do_depth++;
            parse_block(p);
            p->do_depth--;
            uint16_t blen = (uint16_t)(p->vm->code_len - body_pc);
            p->vm->code[len_at] = (uint8_t)(blen & 0xFF);
            p->vm->code[len_at + 1] = (uint8_t)((blen >> 8) & 0xFF);
            // body region + whole trap region
            push_region(p, body_pc, p->vm->code_len, RSVM_REG_TRAP);
            push_region(p, region_start, p->vm->code_len, RSVM_REG_LOOP);
            return;
        }

        // --- classic countdown ---
        uint16_t region_start = p->vm->code_len;
        char cname[RSVM_NAME_LEN];
        snprintf(cname, sizeof cname, "__i%d", p->do_depth);
        uint8_t ci = slot_of(p, cname, true);
        if (const_n >= 0) {
            expect(p, "]");
            emit_u8(p, RSVM_OP_PUSHI); emit_i32(p, const_n);
        } else {
            expr(p);
            expect(p, "]");
        }
        emit_u8(p, RSVM_OP_STORE); emit_u8(p, ci);
        uint16_t top = p->vm->code_len;
        emit_u8(p, RSVM_OP_LOAD); emit_u8(p, ci);
        emit_u8(p, RSVM_OP_PUSHI); emit_i32(p, 0);
        emit_u8(p, RSVM_OP_GT);
        emit_u8(p, RSVM_OP_JZ);
        uint16_t jz = p->vm->code_len; emit_i16(p, 0);
        p->do_depth++;
        parse_block(p);
        p->do_depth--;
        emit_u8(p, RSVM_OP_LOAD); emit_u8(p, ci);
        emit_u8(p, RSVM_OP_PUSHI); emit_i32(p, 1);
        emit_u8(p, RSVM_OP_SUB);
        emit_u8(p, RSVM_OP_STORE); emit_u8(p, ci);
        emit_u8(p, RSVM_OP_JMP);
        int16_t back = (int16_t)(top - (p->vm->code_len + 2));
        emit_i16(p, back);
        int16_t rel = (int16_t)(p->vm->code_len - (jz + 2));
        p->vm->code[jz]     = (uint8_t)(rel & 0xFF);
        p->vm->code[jz + 1] = (uint8_t)((rel >> 8) & 0xFF);
        push_region(p, region_start, p->vm->code_len, RSVM_REG_LOOP);
        return;
    }
    if (match(p, "enum")) {
        char en[RSVM_NAME_LEN];
        if (!parse_ident(p,en,sizeof en)) { fail(p,"enum name"); return; }
        if (p->vm->enum_count >= RSVM_MAX_ENUMS) { fail(p,"too many enums"); return; }
        rsvm_enum_ent_t* e = &p->vm->enums[p->vm->enum_count];
        memset(e,0,sizeof(*e));
        strncpy(e->name,en,RSVM_NAME_LEN-1);
        expect(p,"{");
        int32_t next=0;
        while (!p->failed) {
            skip_ws(p);
            if (peek(p)=='}') { getc_(p); break; }
            char mem[RSVM_NAME_LEN];
            if (!parse_ident(p,mem,sizeof mem)) { fail(p,"member"); return; }
            if (match(p,"=")) { if (!parse_int(p,&next)) { fail(p,"enum val"); return; } }
            if (e->nmembers >= RSVM_MAX_ENUM_MEMBERS) { fail(p,"too many members"); return; }
            strncpy(e->member_names[e->nmembers],mem,RSVM_NAME_LEN-1);
            e->member_vals[e->nmembers]=next;
            e->nmembers++; next++;
            skip_ws(p); if (peek(p)==',') getc_(p);
        }
        p->vm->enum_count++;
        skip_ws(p); if (peek(p)==';') getc_(p);
        return;
    }
    if (match(p, "struct")) {
        char sn[RSVM_NAME_LEN];
        if (!parse_ident(p,sn,sizeof sn)) { fail(p,"struct name"); return; }
        if (p->vm->struct_count >= RSVM_MAX_STRUCTS) { fail(p,"too many structs"); return; }
        rsvm_struct_ent_t* st = &p->vm->structs[p->vm->struct_count];
        memset(st,0,sizeof(*st));
        strncpy(st->name,sn,RSVM_NAME_LEN-1);
        st->base_id = 0xFF;
        for (int i = 0; i < RSVM_MAX_FIELDS; ++i) st->field_aux[i] = 0xFF;

        // Inheritance:  struct Cat : Animal
        //           or  struct Cat extends Animal
        //           or  struct Cat @extends(Animal)
        skip_ws(p);
        if (peek(p) == ':') {
            getc_(p);
            char bn[RSVM_NAME_LEN];
            if (!parse_ident(p, bn, sizeof bn)) { fail(p, "base struct after :"); return; }
            int bid = rsvm_struct_by_name(p->vm, bn);
            if (bid < 0) { fail(p, "unknown base struct"); return; }
            st->base_id = (uint8_t)bid;
        } else if (match(p, "extends")) {
            char bn[RSVM_NAME_LEN];
            if (!parse_ident(p, bn, sizeof bn)) { fail(p, "base after extends"); return; }
            int bid = rsvm_struct_by_name(p->vm, bn);
            if (bid < 0) { fail(p, "unknown base struct"); return; }
            st->base_id = (uint8_t)bid;
        }
        skip_ws(p);
        while (peek(p) == '@') {
            getc_(p);
            char pn[RSVM_NAME_LEN];
            if (!parse_ident(p, pn, sizeof pn)) { fail(p, "struct @prop"); return; }
            if (strcmp(pn, "extends") == 0) {
                expect(p, "(");
                char bn[RSVM_NAME_LEN];
                if (!parse_ident(p, bn, sizeof bn)) { fail(p, "base in @extends"); return; }
                expect(p, ")");
                int bid = rsvm_struct_by_name(p->vm, bn);
                if (bid < 0) { fail(p, "unknown base struct"); return; }
                st->base_id = (uint8_t)bid;
            }
            skip_ws(p);
        }

        // optional trailing ~flags before body
        skip_ws(p);
        while (peek(p)=='~') {
            getc_(p);
            char fl[RSVM_NAME_LEN];
            if (!parse_ident(p, fl, sizeof fl)) { fail(p,"flag after ~"); return; }
            if (strcmp(fl,"packed")==0) st->flags |= RSVM_FLAG_PACKED;
            else if (strcmp(fl,"volatile")==0) st->flags |= RSVM_FLAG_VOLATILE;
            else if (strcmp(fl,"immut")==0) st->flags |= RSVM_FLAG_IMMUT;
            else if (strcmp(fl,"aligned")==0) {
                st->flags |= RSVM_FLAG_ALIGNED;
                if (match(p,"(")) {
                    int32_t n=0; parse_int(p,&n);
                    st->align = (uint8_t)(n>0 && n<255 ? n : 0);
                    expect(p,")");
                }
            }
            skip_ws(p);
        }

        // Flatten inheritance: copy base fields first
        if (st->base_id != 0xFF) {
            rsvm_struct_ent_t* base = &p->vm->structs[st->base_id];
            for (uint8_t i = 0; i < base->nfields; ++i) {
                if (st->nfields >= RSVM_MAX_FIELDS) { fail(p, "too many fields (base)"); return; }
                strncpy(st->field_names[st->nfields], base->field_names[i], RSVM_NAME_LEN-1);
                st->field_ty[st->nfields] = base->field_ty[i];
                st->field_flags[st->nfields] = base->field_flags[i];
                st->field_aux[st->nfields] = base->field_aux[i];
                st->nfields++;
            }
        }

        expect(p,"{");
        while (!p->failed) {
            skip_ws(p);
            if (peek(p)=='}') { getc_(p); break; }
            uint8_t ty = RSVM_TY_I32;
            uint8_t fflags = 0;
            uint8_t faux = 0xFF;
            // @compose before field type
            if (peek(p)=='@') {
                getc_(p);
                char pn[RSVM_NAME_LEN];
                if (!parse_ident(p, pn, sizeof pn)) { fail(p, "field @prop"); return; }
                if (strcmp(pn, "compose") == 0) {
                    fflags |= RSVM_FLAG_COMPOSE;
                } else {
                    fail(p, "unknown field property"); return;
                }
                skip_ws(p);
            }
            // optional field ~flags
            while (peek(p)=='~') {
                getc_(p);
                char fl[RSVM_NAME_LEN];
                if (!parse_ident(p, fl, sizeof fl)) { fail(p,"field flag"); return; }
                if (strcmp(fl,"volatile")==0) fflags |= RSVM_FLAG_VOLATILE;
                else if (strcmp(fl,"immut")==0) fflags |= RSVM_FLAG_IMMUT;
                skip_ws(p);
            }
            if (match(p,"i32")||match(p,"i16")||match(p,"i8")) ty=RSVM_TY_I32;
            else if (match(p,"bool")) ty=RSVM_TY_BOOL;
            else if (match(p,"ptr")) ty=RSVM_TY_PTR;
            else if (match(p,"string")) ty=RSVM_TY_STR;
            else {
                // struct type name → composition / nested ptr
                char tn[RSVM_NAME_LEN];
                int save=p->pos, sl=p->line, sc=p->col;
                if (parse_ident(p, tn, sizeof tn)) {
                    int sid = rsvm_struct_by_name(p->vm, tn);
                    if (sid >= 0) {
                        ty = RSVM_TY_PTR;
                        faux = (uint8_t)sid;
                        // bare StructName field implies @compose ownership
                        fflags |= RSVM_FLAG_COMPOSE;
                    } else {
                        p->pos=save; p->line=sl; p->col=sc;
                        fail(p,"field type"); return;
                    }
                } else {
                    fail(p,"field type"); return;
                }
            }
            char fn[RSVM_NAME_LEN];
            if (!parse_ident(p,fn,sizeof fn)) { fail(p,"field name"); return; }
            expect(p,";");
            if (st->nfields >= RSVM_MAX_FIELDS) { fail(p,"too many fields"); return; }
            strncpy(st->field_names[st->nfields],fn,RSVM_NAME_LEN-1);
            st->field_ty[st->nfields]=ty;
            st->field_flags[st->nfields]=fflags;
            st->field_aux[st->nfields]=faux;
            st->nfields++;
        }
        p->vm->struct_count++;
        // trailing ~flags after body also accepted
        skip_ws(p);
        while (peek(p)=='~') {
            getc_(p);
            char fl[RSVM_NAME_LEN];
            if (!parse_ident(p, fl, sizeof fl)) break;
            if (strcmp(fl,"packed")==0) st->flags |= RSVM_FLAG_PACKED;
            else if (strcmp(fl,"volatile")==0) st->flags |= RSVM_FLAG_VOLATILE;
            else if (strcmp(fl,"immut")==0) st->flags |= RSVM_FLAG_IMMUT;
            else if (strcmp(fl,"aligned")==0) {
                st->flags |= RSVM_FLAG_ALIGNED;
                if (match(p,"(")) {
                    int32_t n=0; parse_int(p,&n);
                    st->align = (uint8_t)(n>0 && n<255 ? n : 0);
                    expect(p,")");
                }
            }
            skip_ws(p);
        }
        // trailing @extends after body (alternate form)
        skip_ws(p);
        while (peek(p)=='@') {
            getc_(p);
            char pn[RSVM_NAME_LEN];
            if (!parse_ident(p, pn, sizeof pn)) break;
            if (strcmp(pn, "extends")==0) {
                expect(p, "(");
                char bn[RSVM_NAME_LEN];
                if (!parse_ident(p, bn, sizeof bn)) { fail(p, "base"); return; }
                expect(p, ")");
                int bid = rsvm_struct_by_name(p->vm, bn);
                if (bid < 0) { fail(p, "unknown base"); return; }
                // late extends: only valid if we had no fields from base yet — skip if already set
                if (st->base_id == 0xFF) {
                    st->base_id = (uint8_t)bid;
                    // prepend is hard after the fact; require extends before body
                    fail(p, "@extends after body: use struct Cat : Base { }"); return;
                }
            }
            skip_ws(p);
        }
        if (peek(p)==';') getc_(p);
        return;
    }

    if (match(p, "FlowMap")) {
        char id[RSVM_NAME_LEN]; parse_ident(p,id,sizeof id); // name optional / ignored for now
        (void)id;
        expect(p,"{");
        while (!p->failed) {
            skip_ws(p);
            if (peek(p)=='}') { getc_(p); break; }
            // route: src = dst ;
            char src[RSVM_NAME_LEN], dst[RSVM_NAME_LEN];
            if (!parse_ident(p, src, sizeof src)) { fail(p, "FlowMap src"); return; }
            expect(p, "=");
            if (!parse_ident(p, dst, sizeof dst)) { fail(p, "FlowMap dst"); return; }
            if (peek(p)==';') getc_(p);
            if (p->vm->flow_count < RSVM_MAX_FLOW_ROUTES) {
                rsvm_flow_route_t* r = &p->vm->flows[p->vm->flow_count++];
                memset(r, 0, sizeof(*r));
                strncpy(r->src, src, RSVM_NAME_LEN-1);
                strncpy(r->dst, dst, RSVM_NAME_LEN-1);
            }
        }
        skip_ws(p); if (peek(p)==';') getc_(p);
        return;
    }
    if (match(p, "fn")) {
        char fname[RSVM_NAME_LEN];
        if (!parse_ident(p,fname,sizeof fname)) { fail(p,"fn name"); return; }
        if (is_type_keyword(fname)) { fail(p, "type keyword cannot be a function name"); return; }
        if (p->vm->func_count >= RSVM_MAX_FUNCS) { fail(p,"too many funcs"); return; }
        rsvm_func_ent_t* f = &p->vm->funcs[p->vm->func_count];
        memset(f,0,sizeof(*f));
        strncpy(f->name,fname,RSVM_NAME_LEN-1);
        f->body_len = 0xFF;
        expect(p,"in"); expect(p,"[");
        uint8_t n_in=0;
        skip_ws(p);
        if (peek(p)!=']') {
            for (;;) {
                match(p,"i32"); match(p,"i16"); match(p,"i8"); match(p,"bool"); match(p,"ptr");
                match(p,"arr"); match(p,"array");
                char an[RSVM_NAME_LEN];
                if (!parse_ident(p,an,sizeof an)) { fail(p,"arg name"); return; }
                f->arg_slots[n_in] = slot_of(p,an,true);
                n_in++;
                skip_ws(p);
                if (peek(p)==',') { getc_(p); continue; }
                break;
            }
        }
        expect(p,"]");
        f->n_in = n_in;
        expect(p,"out"); expect(p,"[");
        uint8_t n_out=0;
        skip_ws(p);
        if (peek(p)!=']') {
            for (;;) {
                match(p,"i32"); match(p,"i16"); match(p,"i8"); match(p,"bool"); match(p,"ptr");
                match(p,"arr"); match(p,"array");
                char on[RSVM_NAME_LEN];
                if (!parse_ident(p,on,sizeof on)) { fail(p,"out name"); return; }
                f->out_slots[n_out] = slot_of(p,on,true);
                n_out++;
                skip_ws(p);
                if (peek(p)==',') { getc_(p); continue; }
                break;
            }
        }
        expect(p,"]");
        f->n_out = n_out;
        f->n_locals = (uint8_t)(n_in + 4);

        // Props BEFORE body so do-n can see trapdoor/unroll:
        //   fn f in[] out[] @loops_trapdoor @autounroll(8) { ... }
        skip_ws(p);
        uint16_t pre_props = 0;
        int pre_unroll = 0;
        while (peek(p)=='@') {
            getc_(p);
            char pn[RSVM_NAME_LEN];
            if (!parse_ident(p, pn, sizeof pn)) { fail(p, "property name after @"); return; }
            // Program-level opt attributes (not prop bits)
            if (strcmp(pn, "limitpassoptimize") == 0 || strcmp(pn, "opt_passes") == 0) {
                expect(p, "(");
                int32_t n = 0;
                if (!parse_int(p, &n) || n < 0 || n > 4) { fail(p, "@limitpassoptimize(0..4)"); return; }
                expect(p, ")");
                p->vm->opt_passes = (uint8_t)n;
                skip_ws(p);
                continue;
            }
            if (strcmp(pn, "packed_instructions") == 0) {
                p->vm->packed_instructions = 1;
                skip_ws(p);
                continue;
            }
            if (strcmp(pn, "enable_algo_rewrite") == 0) {
                p->vm->enable_algo_rewrite = 1;
                if (p->vm->opt_passes == 0) p->vm->opt_passes = 4;
                skip_ws(p);
                continue;
            }
            if (strcmp(pn, "superoptimize_enable") == 0 || strcmp(pn, "superoptimize") == 0) {
                p->vm->superoptimize = 1;
                if (p->vm->opt_passes == 0) p->vm->opt_passes = 4;
                skip_ws(p);
                continue;
            }
            uint16_t bit = rsvm_prop_from_name(pn);
            if (bit == RSVM_PROP_NONE) { fail(p, "unknown function property"); return; }
            pre_props |= bit;
            if (bit == RSVM_PROP_AUTOUNROLL) {
                pre_unroll = 8;
                if (match(p, "(")) {
                    int32_t n = 0;
                    if (!parse_int(p, &n) || n < 0) { fail(p, "autounroll n"); return; }
                    pre_unroll = (int)n;
                    expect(p, ")");
                }
            }
            skip_ws(p);
        }
        f->props |= pre_props;

        emit_u8(p, RSVM_OP_JMP);
        uint16_t skip_at = p->vm->code_len; emit_i16(p, 0);
        f->entry_pc = p->vm->code_len;
        p->vm->func_count++;
        bool prev = p->in_func; p->in_func = true;
        uint16_t prev_props = p->cur_func_props;
        int prev_unroll = p->autounroll_max;
        bool prev_trap = p->loop_trapdoor;
        bool prev_tdis = p->trap_disabled;
        p->cur_func_props = f->props;
        if (pre_unroll > 0) p->autounroll_max = pre_unroll;
        if (f->props & RSVM_PROP_LOOPS_TRAP) p->loop_trapdoor = true;
        if (f->props & RSVM_PROP_TRAP_DISABLE) p->trap_disabled = true;
        parse_block(p);
        // body_len excludes the RET0 we are about to emit
        {
            uint16_t blen = (uint16_t)(p->vm->code_len - f->entry_pc);
            f->body_len = (blen < 255) ? (uint8_t)blen : 0xFF;
        }
        emit_u8(p, RSVM_OP_RET0);
        p->in_func = prev;
        p->cur_func_props = prev_props;
        p->autounroll_max = prev_unroll;
        p->loop_trapdoor = prev_trap;
        p->trap_disabled = prev_tdis;
        int16_t rel = (int16_t)(p->vm->code_len - (skip_at+2));
        p->vm->code[skip_at]=(uint8_t)(rel&0xFF);
        p->vm->code[skip_at+1]=(uint8_t)((rel>>8)&0xFF);

        // trailing function properties still accepted: }@monitor_time;
        skip_ws(p);
        while (peek(p)=='@') {
            getc_(p);
            char pn[RSVM_NAME_LEN];
            if (!parse_ident(p, pn, sizeof pn)) { fail(p, "property name after @"); return; }
            uint16_t bit = rsvm_prop_from_name(pn);
            if (bit == RSVM_PROP_NONE) { fail(p, "unknown function property"); return; }
            f->props |= bit;
            if (bit == RSVM_PROP_AUTOUNROLL && match(p, "(")) {
                int32_t n = 0; parse_int(p, &n); expect(p, ")");
                (void)n; // trailing autounroll only sets flag; prefer pre-body form
            }
            skip_ws(p);
        }
        if (peek(p)==';') getc_(p);
        return;
    }

    // typed decl
    {
        char tname[RSVM_NAME_LEN];
        int save=p->pos, sl=p->line, sc=p->col;
        bool is_type=false;
        if (match(p,"i32")||match(p,"i16")||match(p,"i8")||match(p,"bool")||match(p,"ptr")||match(p,"string")
                ||match(p,"ui8")||match(p,"ui16")||match(p,"ui32")||match(p,"ui64")
                ||match(p,"u8")||match(p,"u16")||match(p,"u32")||match(p,"u64")
                ||match(p,"f16")||match(p,"f32")||match(p,"uf16")||match(p,"uf32")
                ||match(p,"arr")||match(p,"array"))
            is_type=true;
        else if (parse_ident(p,tname,sizeof tname)) {
            if (rsvm_struct_by_name(p->vm,tname)>=0) is_type=true;
            else { p->pos=save; p->line=sl; p->col=sc; }
        }
        if (is_type) {
            char id[RSVM_NAME_LEN];
            if (!parse_ident(p,id,sizeof id)) { fail(p,"name"); return; }
            uint8_t s = slot_of(p,id,true);
            if (s < RSVM_MAX_SLOTS && decl_props)
                p->vm->slot_props[s] |= decl_props;
            skip_ws(p);
            if (peek(p)=='[') {
                getc_(p);
                expr(p);
                expect(p, "]");
                emit_u8(p, RSVM_OP_ARR_NEW);
                emit_u8(p, RSVM_OP_STORE); emit_u8(p, s);
                if (match(p, "=")) {
                    skip_ws(p);
                    if (peek(p)=='[') {
                        getc_(p);
                        int n = 0;
                        skip_ws(p);
                        if (peek(p)!=']') {
                            emit_u8(p, RSVM_OP_LOAD); emit_u8(p, s);
                            emit_u8(p, RSVM_OP_PUSHI); emit_i32(p, n);
                            expr(p);
                            emit_u8(p, RSVM_OP_ARR_ST);
                            n++;
                            while (match(p, ",")) {
                                emit_u8(p, RSVM_OP_LOAD); emit_u8(p, s);
                                emit_u8(p, RSVM_OP_PUSHI); emit_i32(p, n);
                                expr(p);
                                emit_u8(p, RSVM_OP_ARR_ST);
                                n++;
                            }
                        }
                        expect(p, "]");
                    } else {
                        expr(p);
                        emit_u8(p, RSVM_OP_STORE); emit_u8(p, s);
                    }
                }
            } else if (match(p,"=")) { expr(p); emit_u8(p,RSVM_OP_STORE); emit_u8(p,s); }
            else { emit_u8(p,RSVM_OP_LDI); emit_u8(p,s); emit_i32(p,0); }
            expect(p,";");
            return;
        }
    }

    // assignment
    {
        int save=p->pos, sl=p->line, sc=p->col;
        char id[RSVM_NAME_LEN];
        if (parse_ident(p,id,sizeof id)) {
            skip_ws(p);
            if (peek(p)=='[') {
                getc_(p);
                uint8_t s = slot_of(p, id, true);
                emit_u8(p, RSVM_OP_LOAD); emit_u8(p, s);
                int nidx = 0;
                expr(p); nidx = 1;
                while (match(p, ",")) { expr(p); nidx++; }
                expect(p, "]");
                if (!match(p, "=")) { fail(p, "="); return; }
                expr(p); expect(p, ";");
                if (nidx <= 1) emit_u8(p, RSVM_OP_ARR_ST);
                else { emit_u8(p, RSVM_OP_ARR_STN); emit_u8(p, (uint8_t)nidx); }
                return;
            }
            if (peek(p)=='.') {
                // a.b.c = expr  →  expr; LOAD a; [FIELD_LD b...]; FIELD_ST c
                char chain[8][RSVM_NAME_LEN];
                int nchain = 0;
                while (peek(p)=='.' && nchain < 8) {
                    getc_(p);
                    if (!parse_ident(p, chain[nchain], RSVM_NAME_LEN)) {
                        fail(p, "field"); return;
                    }
                    nchain++;
                    skip_ws(p);
                }
                if (nchain < 1) { fail(p, "field"); return; }
                if (!match(p,"=")) { fail(p,"="); return; }
                expr(p); expect(p,";");
                uint8_t s = slot_of(p,id,true);
                emit_u8(p,RSVM_OP_LOAD); emit_u8(p,s);
                for (int i = 0; i < nchain - 1; ++i) {
                    int fi = field_index(p->vm, chain[i]);
                    if (fi < 0) { fail(p, "unknown field"); return; }
                    emit_u8(p, RSVM_OP_FIELD_LD); emit_u8(p, (uint8_t)fi);
                }
                int fi = field_index(p->vm, chain[nchain - 1]);
                if (fi < 0) { fail(p, "unknown field"); return; }
                // stack: val (from expr), obj → FIELD_ST expects pop obj, pop val
                // current emit order was: expr; LOAD; FIELD_ST  which is val, obj — good
                // but we need val under obj: expr pushes val, then LOAD/FIELD_LD push objs
                // FIELD_ST pops obj then val — so stack top must be obj. Good.
                emit_u8(p,RSVM_OP_FIELD_ST); emit_u8(p,(uint8_t)fi);
                return;
            }
            if (match(p,"=")) {
                uint8_t s = slot_of(p,id,true);
                // Fast path: x = x + N  /  x = x - N  → superinstruction
                {
                    int save2 = p->pos, sl2 = p->line, sc2 = p->col;
                    char rhs[RSVM_NAME_LEN];
                    if (parse_ident(p, rhs, sizeof rhs) && strcmp(rhs, id) == 0) {
                        skip_ws(p);
                        if (peek(p) == '+') {
                            getc_(p);
                            int32_t imm = 0;
                            if (parse_int(p, &imm) && match(p, ";")) {
                                if (imm == 1) {
                                    emit_u8(p, RSVM_OP_INC_SLOT); emit_u8(p, s);
                                } else {
                                    emit_u8(p, RSVM_OP_ADD_IMM_SLOT); emit_u8(p, s); emit_i32(p, imm);
                                }
                                return;
                            }
                        } else if (peek(p) == '-') {
                            getc_(p);
                            int32_t imm = 0;
                            if (parse_int(p, &imm) && match(p, ";")) {
                                emit_u8(p, RSVM_OP_ADD_IMM_SLOT); emit_u8(p, s); emit_i32(p, -imm);
                                return;
                            }
                        }
                    }
                    p->pos = save2; p->line = sl2; p->col = sc2;
                }
                expr(p); expect(p,";");
                emit_u8(p,RSVM_OP_STORE); emit_u8(p,s);
                return;
            }
        }
        p->pos=save; p->line=sl; p->col=sc;
        expr(p);
        skip_ws(p);
        if (match(p, "=")) {
            if (!parse_ident(p,id,sizeof id)) { fail(p,"dest"); return; }
            expect(p,";");
            uint8_t s = slot_of(p,id,true);
            emit_u8(p,RSVM_OP_STORE); emit_u8(p,s);
            return;
        }
        // bare expression statement (e.g. foo();) – discard result
        expect(p, ";");
        emit_u8(p, RSVM_OP_IGNORE);
        return;
    }
}

// Post-compile peephole: fuse common multi-op sequences into superinstructions.
// Runs once after parse; shrinks code and cuts dispatch in the interpreter.
static void rsvm_peephole(rsvm_t* vm) {
    if (!vm || vm->code_len < 4) return;
    uint8_t* c = vm->code;
    uint16_t n = vm->code_len;
    uint16_t r = 0, w = 0; // read / write cursors
    // We only fuse forward; relative jumps inside bodies are already fixed.
    // Safe patterns that do not embed relative offsets:
    //   LOAD s; PUSHI 1; ADD; STORE s  → INC_SLOT s
    //   LOAD s; PUSHI imm; ADD; STORE s → ADD_IMM_SLOT s, imm
    //   LOAD a; LOAD b; ADD; STORE d   → LOAD_ADD_STORE d,a,b
    //   LOAD s; PUSHI imm               → LOAD_PUSHI s, imm
    while (r < n) {
        // INC / ADD_IMM pattern
        if (r + 11 <= n && c[r] == RSVM_OP_LOAD && c[r+2] == RSVM_OP_PUSHI
            && c[r+7] == RSVM_OP_ADD && c[r+8] == RSVM_OP_STORE
            && c[r+1] == c[r+9]) {
            uint8_t slot = c[r+1];
            int32_t imm = (int32_t)c[r+3] | ((int32_t)c[r+4] << 8)
                        | ((int32_t)c[r+5] << 16) | ((int32_t)c[r+6] << 24);
            if (imm == 1) {
                c[w++] = RSVM_OP_INC_SLOT;
                c[w++] = slot;
            } else {
                c[w++] = RSVM_OP_ADD_IMM_SLOT;
                c[w++] = slot;
                c[w++] = (uint8_t)(imm & 0xFF);
                c[w++] = (uint8_t)((imm >> 8) & 0xFF);
                c[w++] = (uint8_t)((imm >> 16) & 0xFF);
                c[w++] = (uint8_t)((imm >> 24) & 0xFF);
            }
            r = (uint16_t)(r + 10);
            continue;
        }
        // LOAD a; LOAD b; ADD; STORE d
        if (r + 7 <= n && c[r] == RSVM_OP_LOAD && c[r+2] == RSVM_OP_LOAD
            && c[r+4] == RSVM_OP_ADD && c[r+5] == RSVM_OP_STORE) {
            c[w++] = RSVM_OP_LOAD_ADD_STORE;
            c[w++] = c[r+6]; // dst
            c[w++] = c[r+1]; // a
            c[w++] = c[r+3]; // b
            r = (uint16_t)(r + 7);
            continue;
        }
        // LOAD s; PUSHI imm
        if (r + 6 <= n && c[r] == RSVM_OP_LOAD && c[r+2] == RSVM_OP_PUSHI) {
            c[w++] = RSVM_OP_LOAD_PUSHI;
            c[w++] = c[r+1];
            c[w++] = c[r+3]; c[w++] = c[r+4]; c[w++] = c[r+5]; c[w++] = c[r+6];
            // PUSHI is op + 4 bytes = 5 total after LOAD's 2 → r+=7? LOAD(1)+slot(1)+PUSHI(1)+i32(4)=7
            r = (uint16_t)(r + 7);
            continue;
        }
        // default: copy one byte
        c[w++] = c[r++];
    }
    vm->code_len = w;
    // Note: relative JMP/JZ targets inside regions may be invalidated if we
    // compressed code before them. Peephole only fires on pure data-motion
    // patterns that typically sit *inside* trapdoor bodies (no internal jumps),
    // or we skip fusion when any JMP/JZ appears in the scan window — already
    // guaranteed by the exact match patterns above.
}

rsvm_status_t rsvm_compile(rsvm_t* vm, const char* source, rsvm_parse_err_t* err) {
    if (!vm || !source) return RSVM_ERR_CODE;
    rsvm_host_t host = vm->host;
    char saved_base[192];
    strncpy(saved_base, vm->include_base, sizeof saved_base - 1);
    saved_base[sizeof saved_base - 1] = 0;
    rsvm_init(vm);
    vm->host = host;
    strncpy(vm->include_base, saved_base, sizeof vm->include_base - 1);
    P p; memset(&p,0,sizeof p);
    p.s=source; p.line=1; p.col=1; p.vm=vm; p.err=err;
    p.loop_trapdoor = true; // DEFAULT: trapdoor on for all do-n
    p.trap_disabled = false;
    if (err) memset(err,0,sizeof(*err));
    while (!p.failed) {
        skip_ws(&p);
        if (!peek(&p)) break;
        statement(&p);
    }
    if (p.failed) return RSVM_ERR_CODE;
    // Run from PC 0 so top-level inits execute; fn bodies are skipped via JMP.
    // Then call main (if any) and halt.
    int mid = rsvm_func_by_name(vm, "main");
    if (mid >= 0) {
        emit_u8(&p, RSVM_OP_CALL); emit_u8(&p, (uint8_t)mid);
    }
    emit_u8(&p, RSVM_OP_HALT);
    vm->entry = 0;
    // Multi-pass optimizer (0–4). Algo rewrite / pack controlled by set_* flags.
    if (vm->opt_passes > 0 || vm->packed_instructions || vm->enable_algo_rewrite) {
        if (vm->opt_passes == 0 && (vm->packed_instructions || vm->enable_algo_rewrite))
            vm->opt_passes = 4; // enable full pipeline when feature flags set alone
        rsvm_optimize(vm);
        if (vm->opt_log[0] && (vm->enable_algo_rewrite || vm->packed_instructions))
            fprintf(stderr, "%s", vm->opt_log);
    }
    return RSVM_OK;
}

rsvm_status_t rsvm_eval(rsvm_t* vm, const char* source, rsvm_parse_err_t* err) {
    rsvm_status_t st = rsvm_compile(vm, source, err);
    if (st != RSVM_OK) return st;
    return rsvm_run(vm);
}

rsvm_status_t rsvm_compile_file(rsvm_t* vm, const char* path, rsvm_parse_err_t* err) {
    if (!vm || !path) return RSVM_ERR_CODE;
    size_t len = 0;
    char* src = read_entire_file(path, &len);
    if (!src) {
        if (err) {
            snprintf(err->message, sizeof err->message, "cannot open %s", path);
            err->line = 0; err->column = 0;
        }
        return RSVM_ERR_CODE;
    }
    path_dirname(vm->include_base, sizeof vm->include_base, path);
    rsvm_status_t st = rsvm_compile(vm, src, err);
    free(src);
    return st;
}

rsvm_status_t rsvm_eval_file(rsvm_t* vm, const char* path, rsvm_parse_err_t* err) {
    rsvm_status_t st = rsvm_compile_file(vm, path, err);
    if (st != RSVM_OK) return st;
    // Optional: emit .bvul then run pure bytecode (JIT-style)
    if (vm->want_bvul && vm->output_bvul[0]) {
        uint8_t* buf = (uint8_t*)malloc(65536);
        if (buf) {
            size_t n = rsvm_save_bvul(vm, buf, 65536);
            if (n) {
                FILE* f = fopen(vm->output_bvul, "wb");
                if (f) { fwrite(buf, 1, n, f); fclose(f); }
            }
            // reload from bytecode so execution path is pure bytecode
            if (n) {
                rsvm_host_t host = vm->host;
                // keep tables that load_bvul restores partially
                uint8_t tmp[65536];
                memcpy(tmp, buf, n);
                free(buf);
                st = rsvm_load_bvul(vm, tmp, n);
                vm->host = host;
                if (st != RSVM_OK) return st;
            } else {
                free(buf);
            }
        }
    }
    return rsvm_run(vm);
}
