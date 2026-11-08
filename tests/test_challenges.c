/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Every challenge must be winnable: tests/solutions/<id>.lua wins it on
 * several seeds, judged like main.c does. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bot/bot.h"
#include "sim/challenge.h"
#include "sim/level.h"

#define SEEDS 12

static char *slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

static void quiet(void *ud, const char *msg) { (void)ud; (void)msg; }

static int cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* returns the ticks to win, or -1 (prints why) */
static int play(const char *id, const char *json, const char *code, uint64_t seed)
{
    static Level lv;
    static Sim sim;
    char err[200];
    WinRule w;
    if (!level_load_json(&lv, json, err, sizeof err) || !challenge_parse(&w, json, err, sizeof err) || !w.active) {
        printf("  FAIL %s: bad level: %s\n", id, w.active ? err : "no win rule");
        return 1;
    }
    Bot *bot = bot_new(quiet, NULL);
    int rc = -1;
    if (!bot_load(bot, code, "solution.lua")) {
        printf("  FAIL %s: %s\n", id, bot_error(bot));
        goto out;
    }
    sim_init(&sim, &lv, 1, seed);
    for (int i = 0; i < 100000; i++) {
        if (bot_state(bot) == BOT_ERROR) { printf("  FAIL %s seed %llu: %s\n", id, (unsigned long long)seed, bot_error(bot)); goto out; }
        bool ended = sim.over || sim.robots[0].finished || bot_state(bot) != BOT_RUNNING;
        const char *why;
        Outcome o = challenge_check(&w, &sim, 0, ended, &why);
        if (o == CH_WIN) { rc = sim.cycle; goto out; }
        if (o == CH_FAIL) {
            printf("  FAIL %s seed %llu: %s (tick %d, at %.2f, %.2f)\n", id, (unsigned long long)seed, why, sim.cycle,
                   sim.robots[0].pose.x, sim.robots[0].pose.y);
            goto out;
        }
        bot_tick(bot, &sim, 0);
        sim_step(&sim);
    }
    printf("  FAIL %s: never judged\n", id);
out:
    bot_free(bot);
    return rc;
}

/* args (for tuning): [challenges dir] [id prefix] */
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : CHALLENGES_DIR, *only = argc > 2 ? argv[2] : "";
    DIR *d = opendir(dir);
    if (!d) { printf("cannot open %s\n", dir); return 1; }
    char *names[64];
    int n = 0, failures = 0, runs = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < 64) {
        size_t len = strlen(e->d_name);
        if (len > 5 && strcmp(e->d_name + len - 5, ".json") == 0 && strncmp(e->d_name, only, strlen(only)) == 0)
            names[n++] = strdup(e->d_name);
    }
    closedir(d);
    qsort(names, (size_t)n, sizeof *names, cmp);
    for (int i = 0; i < n; i++) {
        char id[128], path[512];
        snprintf(id, sizeof id, "%.*s", (int)(strlen(names[i]) - 5), names[i]);
        snprintf(path, sizeof path, "%s/%s", dir, names[i]);
        char *json = slurp(path);
        snprintf(path, sizeof path, "%s/%s.lua", SOLUTIONS_DIR, id);
        char *code = slurp(path);
        if (!json || !code) { printf("  FAIL %s: missing %s\n", id, json ? path : "json"); failures++; }
        else {
            int lo = 1 << 30, hi = 0, won = 0;
            for (uint64_t s = 1; s <= SEEDS; s++, runs++) {
                int t = play(id, json, code, s * 7919);
                if (t < 0) { failures++; continue; }
                won++;
                lo = t < lo ? t : lo;
                hi = t > hi ? t : hi;
            }
            if (won) printf("  %-14s won %2d/%d in %d..%d ticks\n", id, won, SEEDS, lo, hi);
        }
        free(json);
        free(code);
        free(names[i]);
    }
    printf("%d/%d challenge runs won\n", runs - failures, runs);
    return failures || n == 0;
}
