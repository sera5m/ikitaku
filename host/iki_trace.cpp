#include <stdio.h>
#include <string.h>
#include "ikitaku_rsvm_min.hpp"
#include "ikitaku_core.hpp"
int main(int argc, char** argv) {
    const char* stim = argc > 1 ? argv[1] : "vanish";
    if (!strcmp(stim, "--stimulus") && argc > 2) stim = argv[2];
    ikitaku_config_t cfg; ikitaku_config_default(&cfg);
    printf("{\"repo\":\"ikitaku\",\"stimulus\":\"%s\",\"read_only\":true,\"attention_desire\":%d}\n", stim, cfg.attention_desire);
    return 0;
}
