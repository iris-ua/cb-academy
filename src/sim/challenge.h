/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIM_CHALLENGE_H
#define SIM_CHALLENGE_H

/* Challenge win rules, read from a level JSON that also has (all optional):
 *   "win":   { "reach_target": true | <index>, "visit_all": true,
 *              "return_home": true, "stop_on_target": true,
 *              "no_collisions": true, "under_time": <ticks> },
 *   "stars": { "time": <ticks> }      // 2nd star: done within this time
 * Goals (reach_target, visit_all, return_home) must all be met; constraints
 * (no_collisions, under_time, the level time_limit) fail the run as soon as
 * they break. stop_on_target: judged only when the run ends, with the robot
 * standing on the reach_target target (any if none given). Home is the robot's start pose. title/goal/starter/hints are
 * page-only and ignored here. Pure C, no graphics. */

#include <stdbool.h>
#include <stddef.h>
#include "sim.h"

#define CH_HOME_RADIUS 1.0 /* home counts like a target of this radius */

typedef struct {
    bool active;         /* the JSON had a "win" object */
    int  reach_target;   /* target index, -1 any, -2 not required */
    bool visit_all, return_home, stop_on_target, no_collisions;
    int  under_time;     /* ticks, 0 = none */
    int  star_time;      /* ticks, 0 = time star always given */
} WinRule;

typedef enum { CH_PENDING, CH_WIN, CH_FAIL } Outcome;

bool challenge_parse(WinRule *w, const char *json, char *err, size_t errlen);
/* ended: the robot can no longer act (script done, finish(), or sim over).
 * why gets a short reason on CH_FAIL. */
Outcome challenge_check(const WinRule *w, const Sim *s, int r, bool ended, const char **why);
/* 1..3: done, +1 within stars.time, +1 no collisions. */
int challenge_stars(const WinRule *w, const Sim *s, int r);

#endif
