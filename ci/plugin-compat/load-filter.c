/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <obs.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
    if ((argc != 3 && argc != 5) || !obs_startup("en-US", NULL, NULL)) return 1;
    obs_module_t *module = NULL;
    int result = obs_open_module(&module, argv[1], argv[2]);
    if (result != MODULE_SUCCESS || !module || !obs_init_module(module)) {
        fprintf(stderr, "Module open/init failed: %d\n", result);
        obs_shutdown(); return 2;
    }
    const char *id = argc == 5 ? argv[3] : "simulcast_stock_sdk_probe";
    const char *expected = argc == 5 ? argv[4] : "Stock SDK probe";
    const char *name = obs_source_get_display_name(id);
    if (!name || strcmp(name, expected)) { obs_shutdown(); return 3; }
    /* The graphical filter needs a graphics context for instantiation. */
    if (argc == 5) { obs_shutdown(); puts("Native filter registered successfully"); return 0; }
    obs_source_t *source = obs_source_create_private("simulcast_stock_sdk_probe", "probe", NULL);
    if (!source) { obs_shutdown(); return 4; }
    obs_source_release(source);
    obs_shutdown();
    puts("Stock OBS SDK plugin load, registration, instance and shutdown passed");
    return 0;
}
