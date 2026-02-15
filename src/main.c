#include "config.h"
#include "server.h"

int main(void) {
    awg_config cfg;

    awg_config_init_runtime(&cfg);

    return awg_server_run(&cfg);
}
