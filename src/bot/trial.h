/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef BOT_TRIAL_H
#define BOT_TRIAL_H

/* Headless challenge runs (tests, hidden variants): a script drives robot 0
 * of a fresh sim until the win rule judges it, as main.c does on screen.
 * No graphics dependency. */

#include "bot.h"
#include "sim/challenge.h"

typedef struct {
    Outcome outcome;     /* CH_WIN or CH_FAIL */
    int     ticks;
    char    why[BOT_ERR_MAX];
} Trial;

Trial trial_run(const Level *lv, const WinRule *w, const char *code, uint64_t seed);
/* Run code on every hidden variant of a level JSON; returns how many it wins.
 * *n = how many there are; why = the first failure (may be NULL). */
int   trial_variants(const char *json, const char *code, uint64_t seed, int *n, char *why, size_t len);

#endif
