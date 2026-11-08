/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GFX_HUD_H
#define GFX_HUD_H

#include <stdbool.h>
#include "raylib.h"
#include "sim/sim.h"

typedef struct {
    bool        paused, overlays, trail;
    float       speed;      /* sim time multiplier */
    const char *status;
    /* robot script (script == NULL: keyboard drive) */
    const char *script, *script_state;
    const char *const *log;     /* say() lines, oldest first */
    int         nlog;
} HudState;

/* Split the screen into arena and HUD panel (side, or bottom on narrow screens). */
void hud_layout(int screen_w, int screen_h, double lab_w, double lab_h, Rectangle *arena, Rectangle *panel);
void hud_draw(const Sim *s, Rectangle panel, const HudState *st);
void hud_set_lang(bool portuguese); /* labels in Portuguese (else English) */
/* Script error banner along the bottom of the arena. */
void hud_draw_error(Rectangle arena, const char *msg);

#endif
