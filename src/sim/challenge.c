/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "challenge.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
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
        w->signal = flag(win, "signal");
        w->under_time = ticks(win, "under_time");
        w->variants = ticks(win, "variants");
    }
    cJSON_Delete(root);
    return ok;
}

/* a cheese counts once visited (with the visiting LED on, under "signal") */
static bool counted(const WinRule *w, const Robot *rb, int t)
{
    return w->signal ? rb->signaled[t] : rb->visited[t];
}

static bool any_counted(const WinRule *w, const Sim *s, const Robot *rb)
{
    for (int t = 0; t < s->level.lab.ntargets; t++)
        if (counted(w, rb, t)) return true;
    return false;
}

static bool goals_met(const WinRule *w, const Sim *s, int r, bool ended)
{
    const Robot *rb = &s->robots[r];
    const Lab *lab = &s->level.lab;
    if (w->reach_target == -1 && !any_counted(w, s, rb)) return false;
    if (w->reach_target >= 0 && (w->reach_target >= lab->ntargets || !counted(w, rb, w->reach_target))) return false;
    if (w->stop_on_target) {
        int g = rb->ground;
        if (!ended || g < 0 || (w->reach_target >= 0 && g != w->reach_target)) return false;
    }
    if (w->visit_all)
        for (int t = 0; t < lab->ntargets; t++)
            if (!counted(w, rb, t)) return false;
    if (w->return_home) {
        if (!any_counted(w, s, rb)) return false; /* go somewhere first */
        if (w->signal && !rb->led_returning) return false;
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
    if (w->signal && rb->led_visiting && rb->ground < 0) { *why = "The visiting LED is on away from the cheese."; return CH_FAIL; }
    if (goals_met(w, s, r, ended)) {
        if (w->under_time && s->cycle > w->under_time) { *why = "Too slow."; return CH_FAIL; }
        return CH_WIN;
    }
    if (w->under_time && s->cycle >= w->under_time) { *why = "Too slow."; return CH_FAIL; }
    int limit = s->level.params.time_limit;
    if (limit > 0 && s->cycle >= limit) { *why = "Time's up."; return CH_FAIL; }
    if (ended) {
        *why = w->stop_on_target && any_counted(w, s, rb) ? "Stop on the cheese, not past it."
                                                       : "The program ended before the goal.";
        return CH_FAIL;
    }
    return CH_PENDING;
}

/* ---- stages and variants ------------------------------------------------ */

static int list_count(const char *json, const char *key)
{
    cJSON *root = cJSON_Parse(json);
    int n = cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(root, key));
    cJSON_Delete(root);
    return n;
}

int challenge_stage_count(const char *json) { return list_count(json, "stages"); }
int challenge_variant_count(const char *json) { return list_count(json, "variants"); }

/* json without key, with key[i]'s members (but not skip's) laid over it */
static char *merge(const char *json, const char *key, int i, const char *const *skip)
{
    cJSON *root = cJSON_Parse(json);
    cJSON *list = cJSON_DetachItemFromObjectCaseSensitive(root, key);
    cJSON *over = cJSON_GetArrayItem(list, i);
    char *out = NULL;
    if (cJSON_IsObject(root) && cJSON_IsObject(over)) {
        cJSON *it;
        cJSON_ArrayForEach(it, over) {
            bool keep = true;
            for (const char *const *k = skip; *k; k++) keep &= strcmp(it->string, *k) != 0;
            if (!keep) continue;
            cJSON *dup = cJSON_Duplicate(it, true);
            if (cJSON_GetObjectItemCaseSensitive(root, it->string)) cJSON_ReplaceItemInObjectCaseSensitive(root, it->string, dup);
            else cJSON_AddItemToObject(root, it->string, dup);
        }
        out = cJSON_PrintUnformatted(root);
    }
    cJSON_Delete(list);
    cJSON_Delete(root);
    return out;
}

char *challenge_stage(const char *json, int i)
{
    static const char *const page_only[] = { "goal", "pt", NULL };
    return merge(json, "stages", i, page_only);
}

char *challenge_variant(const char *json, int i)
{
    static const char *const none[] = { NULL };
    return merge(json, "variants", i, none);
}
