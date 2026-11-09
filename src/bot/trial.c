/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "trial.h"
#include <stdio.h>
#include <stdlib.h>
#include "sim/level.h"

#define MAX_TICKS 100000 /* for levels without a time limit */

Trial trial_run(const Level *lv, const WinRule *w, const char *code, uint64_t seed)
{
    static Sim sim;
    Trial t = { CH_FAIL, 0, "" };
    Bot *bot = bot_new(NULL, NULL);
    if (!bot || !bot_load(bot, code, "robot")) {
        snprintf(t.why, sizeof t.why, "%s", bot ? bot_error(bot) : "out of memory");
        bot_free(bot);
        return t;
    }
    sim_init(&sim, lv, 1, seed);
    for (int i = 0; i < MAX_TICKS; i++) {
        if (bot_state(bot) == BOT_ERROR) { snprintf(t.why, sizeof t.why, "%s", bot_error(bot)); break; }
        bool ended = sim.over || sim.robots[0].finished || bot_state(bot) != BOT_RUNNING;
        const char *why;
        Outcome o = challenge_check(w, &sim, 0, ended, &why);
        if (o != CH_PENDING) {
            t.outcome = o;
            snprintf(t.why, sizeof t.why, "%s", why);
            break;
        }
        bot_tick(bot, &sim, 0);
        sim_step(&sim);
    }
    t.ticks = sim.cycle;
    bot_free(bot);
    return t;
}

int trial_variants(const char *json, const char *code, uint64_t seed, int *n, char *why, size_t len)
{
    static Level lv;
    int won = 0;
    *n = challenge_variant_count(json);
    if (why && len) why[0] = '\0';
    for (int i = 0; i < *n; i++) {
        char *src = challenge_variant(json, i), err[200] = "";
        WinRule w;
        Trial t = { CH_FAIL, 0, "" };
        if (src && level_load_json(&lv, src, err, sizeof err) && challenge_parse(&w, src, err, sizeof err))
            t = trial_run(&lv, &w, code, seed + 1000003u * (uint64_t)(i + 1));
        else snprintf(t.why, sizeof t.why, "variant %d: %s", i + 1, err);
        free(src);
        if (t.outcome == CH_WIN) won++;
        else if (why && len && !why[0]) snprintf(why, len, "%s", t.why);
    }
    return won;
}
