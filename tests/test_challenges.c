/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Every challenge must be winnable: tests/solutions/<id>.lua wins each of its
 * STAGES stages on several seeds (and their hidden variants), judged like
 * main.c does. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bot/trial.h"
#include "sim/challenge.h"
#include "sim/level.h"

#define SEEDS  12
#define STAGES 3

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

static int cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* stage json, seed: the ticks to win, or -1 (prints why) */
static int play(const char *id, int stage, const char *json, const char *code, uint64_t seed)
{
    static Level lv;
    char err[200] = "";
    WinRule w = { 0 };
    if (!json || !level_load_json(&lv, json, err, sizeof err) || !challenge_parse(&w, json, err, sizeof err) || !w.active) {
        printf("  FAIL %s stage %d: bad level: %s\n", id, stage, w.active ? err : "no win rule");
        return -1;
    }
    Trial t = trial_run(&lv, &w, code, seed);
    if (t.outcome == CH_WIN && w.variants) {
        int n, won = trial_variants(json, code, seed, &n, t.why, sizeof t.why);
        if (won < w.variants) {
            printf("  FAIL %s stage %d seed %llu: won %d/%d variants, needs %d: %s\n", id, stage,
                   (unsigned long long)seed, won, n, w.variants, t.why);
            return -1;
        }
    }
    if (t.outcome == CH_WIN) return t.ticks;
    printf("  FAIL %s stage %d seed %llu: %s (tick %d)\n", id, stage, (unsigned long long)seed, t.why, t.ticks);
    return -1;
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
        if (len > 5 && strcmp(e->d_name + len - 5, ".json") == 0 && strcmp(e->d_name, "chapters.json") != 0 &&
            strncmp(e->d_name, only, strlen(only)) == 0)
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
        else if (challenge_stage_count(json) != STAGES) { printf("  FAIL %s: needs %d stages\n", id, STAGES); failures++; }
        else {
            for (int st = 0; st < STAGES; st++) {
                char *sj = challenge_stage(json, st);
                int lo = 1 << 30, hi = 0, won = 0;
                for (uint64_t s = 1; s <= SEEDS; s++, runs++) {
                    int t = play(id, st + 1, sj, code, s * 7919);
                    if (t < 0) { failures++; continue; }
                    won++;
                    lo = t < lo ? t : lo;
                    hi = t > hi ? t : hi;
                }
                if (won) printf("  %-14s %d  won %2d/%d in %d..%d ticks\n", st ? "" : id, st + 1, won, SEEDS, lo, hi);
                free(sj);
            }
        }
        free(json);
        free(code);
        free(names[i]);
    }
    printf("%d/%d challenge runs won\n", runs - failures, runs);
    return failures || n == 0;
}
