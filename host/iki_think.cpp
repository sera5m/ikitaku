/* Manual thought host.
   iki_think "1 2 4 8 16"
   iki_think "loop if print wait"
   echo "who=- vanish" | iki_think
*/
#include "../rs_vm/ikitaku/ikitaku_thought.hpp"
#include <stdio.h>
#include <string.h>

static void read_all(char* buf, int cap) {
    int n = 0;
    while (n + 1 < cap) {
        int c = getchar();
        if (c == EOF) break;
        buf[n++] = (char)c;
    }
    buf[n] = 0;
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = 0;
}

int main(int argc, char** argv) {
    char text[IKITAKU_THOUGHT_MAX];
    text[0] = 0;
    if (argc > 1) {
        int used = 0;
        for (int i = 1; i < argc && used + 2 < (int)sizeof text; ++i) {
            int k = snprintf(text + used, sizeof text - (size_t)used,
                             i == 1 ? "%s" : " %s", argv[i]);
            if (k < 0) break;
            used += k;
        }
    } else {
        read_all(text, (int)sizeof text);
    }
    if (!text[0]) {
        fprintf(stderr, "usage: iki_think <thought>   or pipe text on stdin\n");
        return 2;
    }

    ikitaku_config_t cfg;
    ikitaku_config_default(&cfg);
    ikitaku_thought_t th;
    if (ikitaku_thought_in(text, &cfg, &th) != 0) {
        fprintf(stderr, "empty thought\n");
        return 1;
    }
    char dump[4096];
    ikitaku_thought_dump(&th, dump, (int)sizeof dump);
    fputs(dump, stdout);
    if (dump[strlen(dump) - 1] != '\n') fputc('\n', stdout);
    return 0;
}
