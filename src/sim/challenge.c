/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "challenge.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>

static bool flag(const cJSON *o, const char *key)
{
    return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, key));
}

static int ticks(const cJSON *o, const char *key)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsNumber(it) && it->valuedouble > 0 ? (int)it->valuedouble : 0;
}

bool challenge_parse(WinRule *w, const char *json, char *err, size_t errlen)
{
    memset(w, 0, sizeof *w);
    w->reach_target = -2;
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        snprintf(err, errlen, "invalid JSON");
        return false;
    }
    bool ok = true;
    const cJSON *win = cJSON_GetObjectItemCaseSensitive(root, "win");
    if (win && !cJSON_IsObject(win)) {
        snprintf(err, errlen, "win must be an object");
        ok = false;
    } else if (win) {
        w->active = true;
        const cJSON *rt = cJSON_GetObjectItemCaseSensitive(win, "reach_target");
        if (cJSON_IsTrue(rt)) w->reach_target = -1;
        else if (cJSON_IsNumber(rt)) w->reach_target = (int)rt->valuedouble;
        w->visit_all = flag(win, "visit_all");
        w->return_home = flag(win, "return_home");
        w->stop_on_target = flag(win, "stop_on_target");
        w->no_collisions = flag(win, "no_collisions");
        w->under_time = ticks(win, "under_time");
        w->star_time = ticks(cJSON_GetObjectItemCaseSensitive(root, "stars"), "time");
    }
    cJSON_Delete(root);
    return ok;
}

static bool any_visited(const Sim *s, const Robot *rb)
{
    for (int t = 0; t < s->level.lab.ntargets; t++)
        if (rb->visited[t]) return true;
    return false;
}

static bool goals_met(const WinRule *w, const Sim *s, int r, bool ended)
{
    const Robot *rb = &s->robots[r];
    const Lab *lab = &s->level.lab;
    if (w->reach_target == -1 && !any_visited(s, rb)) return false;
    if (w->reach_target >= 0 && (w->reach_target >= lab->ntargets || !rb->visited[w->reach_target])) return false;
    if (w->stop_on_target) {
        int g = rb->ground;
        if (!ended || g < 0 || (w->reach_target >= 0 && g != w->reach_target)) return false;
    }
    if (w->visit_all)
        for (int t = 0; t < lab->ntargets; t++)
            if (!rb->visited[t]) return false;
    if (w->return_home) {
        if (!any_visited(s, rb)) return false; /* go somewhere first */
        Pose home = lab->nstarts > 0 ? lab->starts[r % lab->nstarts]
                                     : (Pose){ lab->width / 2.0, lab->height / 2.0, 0.0 };
        double dx = rb->pose.x - home.x, dy = rb->pose.y - home.y;
        double in = CH_HOME_RADIUS - SIM_ROBOT_RADIUS;
        if (dx * dx + dy * dy > in * in) return false;
    }
    return true;
}

Outcome challenge_check(const WinRule *w, const Sim *s, int r, bool ended, const char **why)
{
    const char *dummy;
    if (!why) why = &dummy;
    *why = "";
    if (!w->active || r < 0 || r >= s->nrobots) return CH_PENDING;
    const Robot *rb = &s->robots[r];
    if (w->no_collisions && rb->collisions > 0) { *why = "Bumped into something."; return CH_FAIL; }
    if (goals_met(w, s, r, ended)) {
        if (w->under_time && s->cycle > w->under_time) { *why = "Too slow."; return CH_FAIL; }
        return CH_WIN;
    }
    if (w->under_time && s->cycle >= w->under_time) { *why = "Too slow."; return CH_FAIL; }
    int limit = s->level.params.time_limit;
    if (limit > 0 && s->cycle >= limit) { *why = "Time's up."; return CH_FAIL; }
    if (ended) {
        *why = w->stop_on_target && any_visited(s, rb) ? "Stop on the cheese, not past it."
                                                       : "The program ended before the goal.";
        return CH_FAIL;
    }
    return CH_PENDING;
}

int challenge_stars(const WinRule *w, const Sim *s, int r)
{
    const Robot *rb = &s->robots[r];
    return 1 + (!w->star_time || s->cycle <= w->star_time) + (rb->collisions == 0);
}
