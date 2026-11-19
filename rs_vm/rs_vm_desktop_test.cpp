// =============================================================================
// Arch / desktop test harness for RS-VM (no ESP-IDF)
// Build (from artifacts/rs_vm):
//   g++ -std=c++17 -O2 -I. -Ivm -o /tmp/rsvm_test \
//       vm/rs_vm.cpp vm/rs_vm_parse.cpp rs_vm_desktop_test.cpp \
//       vm_modules/vm_mdl_immut/vm_mdl_immut.cpp \
//       vm_modules/vm_mdl_thread/vm_mdl_thread.cpp \
//       vm_modules/vm_mdl_property/vm_mdl_property.cpp \
//       vm_modules/vm_mdl_flowmap/vm_mdl_flowmap.cpp
// Run:
//   /tmp/rsvm_test
//   /tmp/rsvm_test examples/basic.vul examples/abracadabra.vul
// =============================================================================

#include "rs_vm.hpp"
#include "rs_vm_parse.hpp"
#include "debugger/rs_dbg.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void host_print_i32(int32_t v, void*) {
    printf("%ld\n", (long)v);
}
static void host_print_str(const char* s, uint8_t len, void*) {
    fwrite(s, 1, len, stdout);
    fputc('\n', stdout);
}
static void host_print_char(char c, void*) {
    fputc(c, stdout);
}
static void host_delay_ms(uint32_t ms, void*) {
    (void)ms; // no-op on desktop tests
}
static int host_file_read(const char* path, char* buf, int max_len, void*) {
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    int n = (int)fread(buf, 1, (size_t)max_len, f);
    fclose(f);
    return n;
}
static int host_file_write(const char* path, const char* data, int len, void*) {
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    int n = (int)fwrite(data, 1, (size_t)len, f);
    fclose(f);
    return n;
}


static char g_cwd[128] = ".";
static int host_sys_cmd(const char* cmd, char* out, int out_max, void*) {
    if (!cmd || !out || out_max <= 0) return -1;
    if (strncmp(cmd, "echo ", 5) == 0) {
        snprintf(out, out_max, "%s", cmd + 5);
        return (int)strlen(out);
    }
    if (strncmp(cmd, "mkdir ", 6) == 0) {
        snprintf(out, out_max, "mkdir-ok:%s", cmd + 6);
        return (int)strlen(out);
    }
    if (strncmp(cmd, "rm ", 3) == 0) {
        snprintf(out, out_max, "rm-ok:%s", cmd + 3);
        return (int)strlen(out);
    }
    if (strncmp(cmd, "nano ", 5) == 0 || strncmp(cmd, "edit ", 5) == 0) {
        // "nano" is a thin edit stub: report path ready for file_write
        snprintf(out, out_max, "edit:%s", cmd + 5);
        return (int)strlen(out);
    }
    if (strncmp(cmd, "clear", 5) == 0) {
        snprintf(out, out_max, "");
        return 0;
    }
    if (strcmp(cmd, "help") == 0) {
        snprintf(out, out_max, "sh ls cd pwd exec open env pool uart_send mw_text grep cat echo head wc sysconf mkdir rm nano");
        return (int)strlen(out);
    }
    snprintf(out, out_max, "ok:%s", cmd);
    return (int)strlen(out);
}
static int host_sys_exec(const char* path, void*) {
    (void)path; return 0; // nested exec handled by higher layer on device
}
static int host_sys_open_app(const char* name, void*) {
    printf("[open_app] %s\n", name ? name : "?");
    return 0;
}
static int host_sys_ls(const char* path, char* out, int out_max, void*) {
    if (!out || out_max <= 0) return -1;
    snprintf(out, out_max, ".\n..\n(demo)\npath=%s", path ? path : ".");
    return (int)strlen(out);
}
static int host_sys_cd(const char* path, void*) {
    if (!path) return -1;
    strncpy(g_cwd, path, sizeof g_cwd - 1);
    return 0;
}
static int host_sys_pwd(char* out, int out_max, void*) {
    if (!out || out_max <= 0) return -1;
    snprintf(out, out_max, "%s", g_cwd);
    return (int)strlen(out);
}
static int host_mw_set_text(int32_t win_id, const char* text, void*) {
    printf("[mw_text] win=%ld '%s'\n", (long)win_id, text ? text : "");
    return 0;
}
static int host_uart_send(uint8_t type, const uint8_t* data, uint16_t len, void*) {
    printf("[uart_send] type=0x%02X len=%u\n", type, (unsigned)len);
    (void)data;
    return (int)len;
}
static int host_pool_op(const char* name, int op, const char* data, int len,
                        char* out, int out_max, void*) {
    if (!out || out_max <= 0) return -1;
    snprintf(out, out_max, "pool:%s op=%d len=%d", name ? name : "?", op, len);
    (void)data;
    return (int)strlen(out);
}
static int host_sys_env(const char* key, char* out, int out_max, void*) {
    if (!out || out_max <= 0) return -1;
    if (key && strcmp(key, "ROLE") == 0) {
        snprintf(out, out_max, "SOLO");
    } else {
        snprintf(out, out_max, "");
    }
    return (int)strlen(out);
}


/* mock v_env for desktop */
static struct {
    int brightness;
    int fpsTarget;
    int headless;
    int debugMode;
    int screen_dim_w;
    int screen_dim_h;
    int batteryPercent;
    char role[16];
} g_venv = { 80, 30, 0, 1, 240, 240, 100, "SOLO" };

static int host_sysconf_get(const char* key, char* out, int out_max, void*) {
    if (!key || !out || out_max <= 0) return -1;
    int v = -999999;
    if (strcmp(key, "brightness") == 0) v = g_venv.brightness;
    else if (strcmp(key, "fpsTarget") == 0) v = g_venv.fpsTarget;
    else if (strcmp(key, "headless") == 0) v = g_venv.headless;
    else if (strcmp(key, "debugMode") == 0) v = g_venv.debugMode;
    else if (strcmp(key, "screen_dim_w") == 0) v = g_venv.screen_dim_w;
    else if (strcmp(key, "screen_dim_h") == 0) v = g_venv.screen_dim_h;
    else if (strcmp(key, "batteryPercent") == 0) v = g_venv.batteryPercent;
    else if (strcmp(key, "ROLE") == 0 || strcmp(key, "role") == 0) {
        snprintf(out, out_max, "%s", g_venv.role);
        return (int)strlen(out);
    } else return -1;
    snprintf(out, out_max, "%d", v);
    return (int)strlen(out);
}
static int host_sysconf_set(const char* key, const char* value, void*) {
    if (!key || !value) return -1;
    int v = atoi(value);
    if (strcmp(key, "brightness") == 0) g_venv.brightness = v;
    else if (strcmp(key, "fpsTarget") == 0) g_venv.fpsTarget = v;
    else if (strcmp(key, "headless") == 0) g_venv.headless = v;
    else if (strcmp(key, "debugMode") == 0) g_venv.debugMode = v;
    else if (strcmp(key, "screen_dim_w") == 0) g_venv.screen_dim_w = v;
    else if (strcmp(key, "screen_dim_h") == 0) g_venv.screen_dim_h = v;
    else if (strcmp(key, "batteryPercent") == 0) g_venv.batteryPercent = v;
    else return -1;
    printf("[sysconf] %s = %s\n", key, value);
    return 0;
}

static int host_native_call(const char* name, const int32_t* args, int nargs,
                            int32_t* out, void*) {
    if (!name || !out) return -1;
    *out = 0;
    if (strcmp(name, "c_add") == 0 && nargs >= 2) { *out = args[0] + args[1]; return 0; }
    if (strcmp(name, "c_mul") == 0 && nargs >= 2) { *out = args[0] * args[1]; return 0; }
    if (strcmp(name, "c_abs") == 0 && nargs >= 1) {
        *out = args[0] < 0 ? -args[0] : args[0]; return 0;
    }
    if (strcmp(name, "c_hash") == 0) {
        int32_t h = 5381;
        for (int i = 0; i < nargs; ++i) h = ((h << 5) + h) + args[i];
        *out = h; return 0;
    }
    return -1;
}
static void install_stdio_host(rsvm_t* vm) {
    rsvm_host_t h;
    memset(&h, 0, sizeof h);
    h.print_i32 = host_print_i32;
    h.print_str = host_print_str;
    h.print_char = host_print_char;
    h.delay_ms = host_delay_ms;
    h.file_read = host_file_read;
    h.file_write = host_file_write;
    h.sys_cmd = host_sys_cmd;
    h.sys_exec = host_sys_exec;
    h.sys_open_app = host_sys_open_app;
    h.sys_ls = host_sys_ls;
    h.sys_cd = host_sys_cd;
    h.sys_pwd = host_sys_pwd;
    h.mw_set_text = host_mw_set_text;
    h.uart_send = host_uart_send;
    h.pool_op = host_pool_op;
    h.sys_env = host_sys_env;
    h.sysconf_get = host_sysconf_get;
    h.sysconf_set = host_sysconf_set;
    h.native_call = host_native_call;
    rsvm_set_host(vm, &h);
}

static int run_source(const char* src, const char* label) {
    printf("=== %s ===\n", label);
    rsvm_t vm;
    rsvm_init(&vm);
    install_stdio_host(&vm);
    rsvm_parse_err_t err;
    rsvm_status_t st = rsvm_eval(&vm, src, &err);
    if (st != RSVM_OK)
        rsvm_dbg_error(&vm, err.line, err.column, err.message);
    rsvm_dbg_stats(&vm, stdout);
    rsvm_dbg_monitors(&vm, stdout);
    rsvm_dbg_refs(&vm, stdout);
    printf("\n");
    return st == RSVM_OK ? 0 : 1;
}

static const char* SAMPLE_BASIC =
    "i32 x = 3;\n"
    "i32 y;\n"
    "x * 2 + 1 = y;\n"
    "print(y);\n"
    "if[y > 5]{ print(\"big\"); } else { print(\"small\"); }\n"
    "do n[3]{ x = x + 1; print(x); }\n";

static const char* SAMPLE_FN =
    "fn double in[i32 a] out[i32 r] {\n"
    "  a * 2 = r;\n"
    "  return r;\n"
    "}\n"
    "fn main in[] out[] {\n"
    "  i32 v = 21;\n"
    "  i32 w;\n"
    "  // call via expression\n"
    "  print(\"double:\");\n"
    "  print(double(v));\n"
    "  return;\n"
    "}\n";

static const char* SAMPLE_STRUCT =
    "struct Point {\n"
    "  i32 x;\n"
    "  i32 y;\n"
    "}\n"
    "fn main in[] out[] {\n"
    "  Point p = new Point;\n"
    "  p.x = 10;\n"
    "  p.y = 20;\n"
    "  print(p.x);\n"
    "  print(p.y);\n"
    "  print(p.x + p.y);\n"
    "  return;\n"
    "}\n";

static const char* SAMPLE_ENUM =
    "enum Color { RED, GREEN, BLUE }\n"
    "fn main in[] out[] {\n"
    "  i32 c = GREEN@Color;\n"
    "  print(c);\n"
    "  if[c == 1]{ print(\"green\"); }\n"
    "  return;\n"
    "}\n";

static const char* SAMPLE_PROPS =
    "FlowMap Route { a = b; }\n"
    "struct Vec2 ~packed { i32 x; i32 y; }\n"
    "fn pure_add in[i32 a, i32 b] out[i32 r] {\n"
    "  a + b = r;\n"
    "  return r;\n"
    "}@immut @property;\n"
    "fn thread_example in[i32 n] out[] {\n"
    "  ```ifsingle=true```\n"
    "  `thread_0 { print(\"seq0\"); }\n"
    "  ``par { print(\"par\"); }``\n"
    "  return;\n"
    "}@threaded;\n"
    "fn main in[] out[] {\n"
    "  @shared i32 s = 10;\n"
    "  @mut i32 m = 3;\n"
    "  print(pure_add(s, m));\n"
    "  thread_example(2);\n"
    "  return;\n"
    "}\n";

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)n, f);
    buf[n] = 0;
    fclose(f);
    return buf;
}

int main(int argc, char** argv) {
    int failed = 0;
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            const char* path = argv[i];
            size_t plen = strlen(path);
            int is_bvul = (plen > 5 && strcmp(path + plen - 5, ".bvul") == 0);
            if (is_bvul) {
                printf("=== %s (bytecode) ===\n", path);
                FILE* f = fopen(path, "rb");
                if (!f) { fprintf(stderr, "cannot read %s\n", path); failed++; continue; }
                fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
                uint8_t* buf = (uint8_t*)malloc((size_t)n);
                if (!buf) { fclose(f); failed++; continue; }
                fread(buf, 1, (size_t)n, f); fclose(f);
                rsvm_t vm; rsvm_init(&vm); install_stdio_host(&vm);
                rsvm_status_t st = rsvm_load_bvul(&vm, buf, (size_t)n);
                free(buf);
                if (st == RSVM_OK) st = rsvm_run(&vm);
                if (st != RSVM_OK) rsvm_dbg_error(&vm, 0, 0, "bvul run");
                rsvm_dbg_stats(&vm, stdout);
                rsvm_dbg_monitors(&vm, stdout);
                failed += (st != RSVM_OK);
                printf("\n");
            } else {
                printf("=== %s ===\n", path);
                rsvm_t vm; rsvm_init(&vm); install_stdio_host(&vm);
                rsvm_parse_err_t err;
                rsvm_status_t st = rsvm_eval_file(&vm, path, &err);
                if (st != RSVM_OK)
                    rsvm_dbg_error(&vm, err.line, err.column, err.message);
                if (vm.want_bvul && vm.output_bvul[0])
                    printf("-- wrote bytecode: %s\n", vm.output_bvul);
                rsvm_dbg_stats(&vm, stdout);
                rsvm_dbg_monitors(&vm, stdout);
                rsvm_dbg_refs(&vm, stdout);
                failed += (st != RSVM_OK);
                printf("\n");
            }
        }
        return failed ? 1 : 0;
    }

    failed += run_source(SAMPLE_BASIC, "basic arithmetic / control");
    failed += run_source(SAMPLE_FN, "functions + main entry");
    failed += run_source(SAMPLE_STRUCT, "struct + new + fields");
    failed += run_source(SAMPLE_ENUM, "enum members");
    failed += run_source(SAMPLE_PROPS, "properties + FlowMap + thread sugar");
    if (failed)
        printf("%d sample(s) failed\n", failed);
    else
        printf("all samples OK\n");
    return failed ? 1 : 0;
}
