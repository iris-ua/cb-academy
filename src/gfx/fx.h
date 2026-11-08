/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GFX_FX_H
#define GFX_FX_H

/* Juice: screen shake, flash, particles and floating score popups.
 * Runs on real (frame) time, independent of the sim tick. */

#include "view.h"

#define FX_MAX_PARTICLES 512
#define FX_MAX_POPUPS    16

typedef struct {
    Vec2  pos, vel;          /* world units, units/s */
    float life, max, size, spin, angle;
    Color color;
} Particle;

typedef struct {
    Vec2  pos;
    char  text[16];
    float life;
    Color color;
} Popup;

typedef struct {
    Particle p[FX_MAX_PARTICLES];
    int      np;
    Popup    pop[FX_MAX_POPUPS];
    float    shake, flash, time;
} Fx;

void    fx_reset(Fx *fx);
void    fx_update(Fx *fx, float dt);
void    fx_collision(Fx *fx, Vec2 at, double dir, int penalty);
void    fx_target(Fx *fx, Vec2 at, int reward);
Vector2 fx_shake_offset(const Fx *fx);     /* pixels */
void    fx_draw_world(const Fx *fx, const View *v);  /* particles + popups */
void    fx_draw_flash(const Fx *fx, Rectangle arena);

#endif
