/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <obs-module.h>
OBS_DECLARE_MODULE()
static const char *name(void *unused) { (void)unused; return "Stock SDK probe"; }
static void *create(obs_data_t *settings, obs_source_t *source) { (void)settings; return source; }
static void destroy(void *data) { (void)data; }
static struct obs_source_frame *filter_video(void *data, struct obs_source_frame *frame) { (void)data; return frame; }
static struct obs_source_info filter = {
    .id = "simulcast_stock_sdk_probe", .type = OBS_SOURCE_TYPE_FILTER,
    .output_flags = OBS_SOURCE_ASYNC_VIDEO, .get_name = name,
    .create = create, .destroy = destroy, .filter_video = filter_video,
};
bool obs_module_load(void) { obs_register_source(&filter); return true; }
