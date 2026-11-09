/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GFX_SCENE_H
#define GFX_SCENE_H

/* Arena rendering: floor, walls (shadow + bevel), cheese, beacons, mice,
 * debug overlays and trails. Reads the sim; never changes it. */

#include <stdbool.h>
#include "sim/sim.h"
#include "fx.h"
#include "view.h"

#define SCENE_TRAIL_N  1024
#define SCENE_WALL_PTS (SIM_MAX_CORNERS * 5)

typedef struct {
    Pose   prev, cur;            /* committed poses before / after the last tick */
    double wheel[2], wheel_prev[2]; /* distance rolled by left/right wheel */
    Vec2   trail[SCENE_TRAIL_N];
    int    trail_head, trail_len;
    float  hit;                  /* collision flash, 1 -> 0 */
    float  speed, speed_goal;    /* smoothed |vel| / max, for the tail wag */
    double wag;                  /* tail wag phase, integrated so speed changes don't jump it */
    int    collisions;
    bool   visited[SIM_MAX_TARGETS];
} RobotVis;

typedef struct {
    RobotVis rv[SIM_MAX_ROBOTS];
    Vec2     outline[SIM_MAX_WALLS][SCENE_WALL_PTS]; /* rounded wall outlines (world) */
    int      nout[SIM_MAX_WALLS];
    double   tall_h;             /* walls at least this tall get the "tall" colour */

    RenderTexture2D shadow;      /* low-res wall shadow mask */
    View     shadow_view;
    bool     shadow_dirty;

    Texture2D logo;              /* University of Aveiro watermark (id 0: missing) */

    bool     overlays, trail;
    float    time;
} Scene;

void scene_level(Scene *sc, const Sim *s);   /* new level: rebuild caches, reset */
void scene_reset(Scene *sc, const Sim *s);   /* after sim_reset */
void scene_after_step(Scene *sc, const Sim *s, Fx *fx);
void scene_update(Scene *sc, float dt);
void scene_prepare(Scene *sc, const Sim *s, View v); /* call outside BeginDrawing */
void scene_draw(const Scene *sc, const Sim *s, View v, Vector2 shake, float alpha);
Pose scene_pose(const Scene *sc, int r, float alpha); /* interpolated pose */
void scene_unload(Scene *sc);

#endif
