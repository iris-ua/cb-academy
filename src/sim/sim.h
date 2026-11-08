/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIM_SIM_H
#define SIM_SIM_H

/* CiberRato Academy simulation core. Pure C, no graphics.
 * Model follows the original ciberRatoTools simulator. */

#include <stdbool.h>
#include <stdint.h>
#include "geom.h"
#include "rng.h"

#define SIM_ROBOT_RADIUS 0.5
#define SIM_MAX_POWER    0.15
#define SIM_CYCLE_MS     50
#define SIM_NUM_IR       4
#define SIM_MAX_ROBOTS   4
#define SIM_MAX_WALLS    512
#define SIM_MAX_BEACONS  8
#define SIM_MAX_TARGETS  8
#define SIM_SOLID_HEIGHT 1e-9  /* "any wall taller than 0" */
#define SIM_IR_HALF_CONE (M_PI / 6.0) /* IR sensor cone half-angle */

typedef struct { double x, y, dir; } Pose; /* dir in radians, 0 = +x, CCW */

typedef struct { Vec2 pos; double height; } Beacon;
typedef struct { Vec2 pos; double radius; } Target;

typedef struct {
    char   name[64];
    double width, height;
    Wall   walls[SIM_MAX_WALLS]; /* walls[0] is the outer border */
    int    nwalls;
    Beacon beacons[SIM_MAX_BEACONS];
    int    nbeacons;
    Target targets[SIM_MAX_TARGETS];
    int    ntargets;
    Pose   starts[SIM_MAX_ROBOTS];
    int    nstarts;
} Lab;

typedef struct {
    double motors;      /* % std-dev, multiplicative */
    double ir;          /* absolute std-dev on 1/d */
    double compass_deg;
    double beacon_deg;
} Noise;

typedef struct {
    int    time_limit;              /* cycles; <=0 means unlimited */
    Noise  noise;
    double ir_angles[SIM_NUM_IR];   /* radians, relative to heading */
    double beacon_aperture;         /* radians, half-angle */
    int    collision_penalty;
    int    target_reward;
} SimParams;

typedef struct {
    Lab       lab;
    SimParams params;
} Level;

typedef struct { double in, out; } Motor;

typedef struct {
    Pose   pose, next;
    Motor  left, right;
    double vel;                         /* last linear velocity */
    bool   collision, collision_wall, collision_robot, collision_prev;
    bool   led_visiting, led_returning;
    bool   finished;

    /* sensors (updated every cycle) */
    double ir[SIM_NUM_IR];              /* ~1/distance, 0.1 resolution */
    double compass;                     /* degrees, integer-rounded */
    bool   beacon_visible[SIM_MAX_BEACONS];
    double beacon_dir[SIM_MAX_BEACONS]; /* degrees relative to heading */
    int    ground;                      /* target index under robot or -1 */
    bool   bumper;

    /* stats */
    bool   visited[SIM_MAX_TARGETS];
    int    visit_cycle[SIM_MAX_TARGETS];
    int    collisions;
    int    score;
} Robot;

typedef struct {
    Level    level;
    Robot    robots[SIM_MAX_ROBOTS];
    int      nrobots;
    int      cycle;
    bool     over;
    Rng      rng;
    uint64_t seed;
} Sim;

void level_default_params(SimParams *p);

/* Robots are placed on lab.starts (cycled if fewer starts than robots). */
void sim_init(Sim *s, const Level *lv, int nrobots, uint64_t seed);
void sim_reset(Sim *s);                 /* same level, same seed */
void sim_step(Sim *s);                  /* advance one 50 ms cycle */

/* robot actions (persist until changed) */
void sim_set_motors(Sim *s, int r, double left, double right);
void sim_set_led(Sim *s, int r, bool visiting, bool returning);
void sim_finish(Sim *s, int r);

/* queries (noiseless; used by debug overlays) */
double lab_ray_distance(const Lab *lab, Vec2 p, double dir, double min_h); /* -1 if no hit */
/* IR distance for robot r as if it stood at pose (NULL = its current pose); -1 if nothing seen */
double sim_ir_distance(const Sim *s, int r, const Pose *pose, int k);

#endif
