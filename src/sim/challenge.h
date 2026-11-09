/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIM_CHALLENGE_H
#define SIM_CHALLENGE_H

/* Challenge win rules, read from a level JSON that also has (all optional):
 *   "win": { "reach_target": true | <index>, "visit_all": true,
 *            "return_home": true, "stop_on_target": true,
 *            "no_collisions": true, "under_time": <ticks>,
 *            "signal": true, "variants": <n> }
 * Goals (reach_target, visit_all, return_home) must all be met; constraints
 * (no_collisions, under_time, the level time_limit) fail the run as soon as
 * they break. stop_on_target: judged only when the run ends, with the robot
 * standing on the reach_target target (any if none given). Home is the robot's
 * start pose. signal: a cheese counts only once the robot stands on it with the
 * visiting LED on, that LED on anywhere else fails the run, and home needs the
 * returning LED on. variants: the script must also win n of the level's hidden
 * "variants" (judged by the caller, see bot/trial.h).
 *
 * A challenge file holds "stages": [ {...}, ... ], each merged key by key over
 * the rest of the file to make that stage's level (a stage's page-only "goal"
 * and "pt" stay out). A level's "variants": [ {...}, ... ] are merged the same
 * way to make its hidden variants. chapter/title/goal/starter/hints/pt are
 * page-only and ignored here. Pure C, no graphics. */

#include <stdbool.h>
#include <stddef.h>
#include "sim.h"

#define CH_HOME_RADIUS 1.0 /* home counts like a target of this radius */

typedef struct {
    bool active;         /* the JSON had a "win" object */
    int  reach_target;   /* target index, -1 any, -2 not required */
    bool visit_all, return_home, stop_on_target, no_collisions, signal;
    int  under_time;     /* ticks, 0 = none */
    int  variants;       /* hidden variants to win too, 0 = none */
} WinRule;

typedef enum { CH_PENDING, CH_WIN, CH_FAIL } Outcome;

bool challenge_parse(WinRule *w, const char *json, char *err, size_t errlen);
/* ended: the robot can no longer act (script done, finish(), or sim over).
 * why gets a short reason on CH_FAIL. */
Outcome challenge_check(const WinRule *w, const Sim *s, int r, bool ended, const char **why);

/* Number of "stages" / "variants" in a JSON (0 if none or not valid JSON). */
int   challenge_stage_count(const char *json);
int   challenge_variant_count(const char *json);
/* Level JSON of stage i / hidden variant i (malloc'd, caller frees); NULL if out of range. */
char *challenge_stage(const char *json, int i);
char *challenge_variant(const char *json, int i);

#endif
