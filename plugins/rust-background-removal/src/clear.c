/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <obs.h>
#include <graphics/vec4.h>
void simulcast_clear_transparent(void)
{
    struct vec4 color;
    vec4_zero(&color);
    gs_clear(GS_CLEAR_COLOR, &color, 0.0f, 0);
}
