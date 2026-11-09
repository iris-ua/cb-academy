/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Headless tests for the Lua robot API (noise off). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "bot/bot.h"
#include "sim/level.h"

static int failures, checks;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define NEAR(a, b, eps) do { checks++; double _a = (a), _b = (b); \
    if (fabs(_a - _b) > (eps)) { failures++; \
    printf("  FAIL %s:%d: %s = %.6f, expected %.6f\n", __FILE__, __LINE__, #a, _a, _b); } } while (0)
#define HAS(str, sub) do { checks++; if (!strstr((str), (sub))) { failures++; \
    printf("  FAIL %s:%d: \"%s\" lacks \"%s\"\n", __FILE__, __LINE__, (str), (sub)); } } while (0)

static Level lv;
static Sim sim;
static Bot *bot;
static char said[512];

static void on_say(void *ud, const char *msg)
{
    (void)ud;
    size_t n = strlen(said);
    snprintf(said + n, sizeof said - n, "%s%s", n ? "|" : "", msg);
}

/* 20x20 box, noise off, robot at (5, 5) facing +x; run code up to max ticks */
static void run(const char *code, int max)
{
    char err[128];
    level_load_json(&lv, "{\"width\":20,\"height\":20,\"noise\":{\"motors\":0,\"ir\":0,\"compass\":0,\"beacon\":0},"
                         "\"start\":[{\"x\":5,\"y\":5,\"dir\":0}]}", err, sizeof err);
    sim_init(&sim, &lv, 1, 1);
    said[0] = '\0';
    bot_load(bot, code, "robot.lua");
    for (int i = 0; i < max && bot_state(bot) == BOT_RUNNING && !sim.over; i++) {
        bot_tick(bot, &sim, 0);
        sim_step(&sim);
    }
}

static void test_ticks(void)
{
    puts("ticks & yields");
    run("say(time()) wait() say(time()) wait(3) say(time()) local x = ir(0) + compass() say(time())", 10);
    CHECK(strcmp(said, "0|1|4|4") == 0);
    CHECK(bot_state(bot) == BOT_DONE);

    run("motors(0.1, 0.1)", 10); /* script ends: motors stop */
    CHECK(bot_state(bot) == BOT_DONE);
    NEAR(sim.robots[0].left.in, 0.0, 0);

    run("motors(0.1, 0.1) finish() say('after')", 10);
    CHECK(sim.robots[0].finished);
    CHECK(bot_state(bot) == BOT_DONE);
    CHECK(said[0] == '\0');
}

static void test_sandbox(void)
{
    puts("sandbox");
    run("assert(io == nil and os == nil and require == nil and load == nil and dofile == nil\n"
        "  and debug == nil and package == nil and string.dump == nil) say('ok')", 2);
    CHECK(strcmp(said, "ok") == 0);
    CHECK(bot_state(bot) == BOT_DONE);

    run("local n = 0\nwhile true do n = n + 1 end", 3);
    CHECK(bot_state(bot) == BOT_ERROR);
    CHECK(bot_error_line(bot) == 2);
    HAS(bot_error(bot), "wait()");

    run("while true do pcall(function() while true do end end) end", 3);
    CHECK(bot_state(bot) == BOT_ERROR);

    run("local s = string.rep('x', 1 << 26)", 3);
    CHECK(bot_state(bot) == BOT_ERROR);
    HAS(bot_error(bot), "memory");

    run("motors(0.1, 0.1)\nwhile true do end", 5); /* error stops the robot */
    CHECK(bot_state(bot) == BOT_ERROR);
    NEAR(sim.robots[0].left.in, 0.0, 0);
}

static void test_errors(void)
{
    puts("friendly errors");
    run("wait()\n\nmotor(1, 1)", 5);
    CHECK(bot_state(bot) == BOT_ERROR);
    CHECK(bot_error_line(bot) == 3);
    HAS(bot_error(bot), "Line 3:");
    HAS(bot_error(bot), "'motors'");

    run("if ir(0) > 1 then\n  wait()\n", 5);
    CHECK(bot_state(bot) == BOT_ERROR);
    HAS(bot_error(bot), "'end' expected");

    run("wait()\nforward('far')", 5); /* helper errors point at the caller */
    CHECK(bot_error_line(bot) == 2);
    HAS(bot_error(bot), "forward");

    run("wait()\nir(7)", 5);
    CHECK(bot_error_line(bot) == 2);

    run("for i = 1, 3 do\n  i = i + 1\nend", 5);
    CHECK(bot_error_line(bot) == 2);
    HAS(bot_error(bot), "'for' loop");
}

static void test_helpers(void)
{
    puts("helpers");
    run("forward(2)", 200);
    CHECK(bot_state(bot) == BOT_DONE);
    NEAR(sim.robots[0].pose.x, 7.0, 0.01);
    NEAR(sim.robots[0].pose.y, 5.0, 1e-9);

    run("forward(-1.5, 0.05)", 200);
    NEAR(sim.robots[0].pose.x, 3.5, 0.01);

    run("turn(90)", 200);
    NEAR(sim.robots[0].pose.dir, M_PI / 2, 0.01);
    NEAR(sim.robots[0].pose.x, 5.0, 1e-9);

    run("turn(-45) turn_to(180)", 300);
    NEAR(fabs(sim.robots[0].pose.dir), M_PI, 0.02);
}

int main(void)
{
    bot = bot_new(on_say, NULL);
    test_ticks();
    test_sandbox();
    test_errors();
    test_helpers();
    bot_free(bot);
    printf("%d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
