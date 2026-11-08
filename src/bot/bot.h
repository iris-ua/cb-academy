/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef BOT_BOT_H
#define BOT_BOT_H

/* Student robot scripts: sandboxed Lua 5.4 (no io/os/package/debug/load)
 * running in a coroutine. bot_tick resumes the script until its next action
 * (motors/wait/finish), which yields; the caller then runs one sim_step.
 * Sensor reads never yield. No graphics dependency.
 *
 * Lua API (robot r):
 *   motors(l, r)   set wheel power [-0.15, 0.15], wait one tick
 *   wait([n])      wait n ticks (default 1)
 *   ir(i)          IR reading, i = 0 front, 1 left, 2 right, 3 back
 *   compass()      heading in degrees
 *   beacon([i])    beacon angle relative to heading (degrees), nil if hidden
 *   ground()       target index under the robot, -1 if none
 *   bump()         true if the robot hit something last tick
 *   time()         ticks since start (1 tick = 50 ms)
 *   led(name, on)  name "visiting" or "returning"
 *   finish()       end the run (scores, stops the robot)
 *   say(...)       message to the console (print does the same)
 *   forward(n [, speed]), turn(deg [, speed]), turn_to(deg [, speed])
 *                  helpers on a noise-free motor model; turn is CCW-positive */

#include <stdbool.h>
#include "sim/sim.h"

#define BOT_TICK_BUDGET 200000      /* Lua instructions per tick */
#define BOT_MEM_LIMIT   (16u << 20) /* bytes */
#define BOT_ERR_MAX     512

typedef enum { BOT_NONE, BOT_RUNNING, BOT_DONE, BOT_ERROR } BotState;

typedef void (*BotSayFn)(void *ud, const char *msg);
typedef struct Bot Bot;

Bot        *bot_new(BotSayFn say, void *ud);
void        bot_free(Bot *b);
/* Keep a copy of code and start it fresh; false on a compile error (see bot_error). */
bool        bot_load(Bot *b, const char *code, const char *name);
bool        bot_restart(Bot *b);             /* rerun the loaded code from the top */
void        bot_tick(Bot *b, Sim *s, int r); /* call before each sim_step */
BotState    bot_state(const Bot *b);
const char *bot_error(const Bot *b);         /* friendly "Line N: ..." or "" */
int         bot_error_line(const Bot *b);    /* 0 if unknown */

#endif
