/* SPDX-License-Identifier: GPL-2.0-or-later */
/* CiberRato Academy — app shell: input, fixed-tick stepping, rendering.
 * With a .lua argument the robot runs that script (hot-reloaded on save);
 * otherwise drive it with the keyboard. On the web the page's editor drives
 * it through the web_* exports and gets Module.onEvent(kind, text, n) back. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "sim/sim.h"
#include "sim/level.h"
#include "sim/challenge.h"
#include "bot/bot.h"
#include "bot/trial.h"
#include "gfx/fx.h"
#include "gfx/hud.h"
#include "gfx/scene.h"
#include "gfx/text.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static Level level;
static Sim   sim;
static Scene scene;
static Fx    fx;
static char  level_path[512] = "levels/default.json";
static char  status_msg[256];
static bool  paused;
static bool  step_req; /* paused: advance one tick on the next frame */
static float accum;
static float speed = 1;  /* sim time multiplier: 0.5, 1, 2, 4, 8 */
static WinRule rule;     /* challenge win rule (rule.active: the level is a challenge) */
static char *level_src;  /* the loaded level JSON (its hidden variants) */
static bool  judged;     /* this run got a result; the sim stops */
static bool  lang_pt;    /* web page language: Portuguese */

/* robot script */
#define LOG_N 32
static Bot  *bot;
static bool  script_on;          /* the script drives the robot (else keyboard) */
static char  script_path[512];   /* desktop: file to hot-reload */
static long  script_mtime;
static float script_poll;
static char  log_buf[LOG_N][160];
static const char *log_lines[LOG_N];
static int   log_n;

/* dev: --shot out.png renders N frames, saves, exits; --motors L R drives constantly */
static const char *shot_file;
static int    shot_frames = 90, frames;
static bool   auto_drive;
static double auto_l, auto_r;

#ifdef __EMSCRIPTEN__
EM_JS(void, js_emit, (const char *kind, const char *text, int n), {
    if (Module.onEvent) Module.onEvent(UTF8ToString(kind), UTF8ToString(text), n);
});
#else
static void js_emit(const char *kind, const char *text, int n) { (void)kind, (void)text, (void)n; }
#endif

static void on_say(void *ud, const char *msg)
{
    (void)ud;
#ifdef __EMSCRIPTEN__
    js_emit("print", msg, 0);
    return; /* the page has its own console */
#endif
    printf("[robot] %s\n", msg);
    if (log_n == LOG_N) { /* drop oldest */
        memmove(log_buf[0], log_buf[1], sizeof log_buf[0] * (LOG_N - 1));
        log_n--;
    }
    snprintf(log_buf[log_n++], sizeof log_buf[0], "%s", msg);
}

/* a fresh GPS origin every run, so scripts can't rely on a fixed one */
static void new_gps_offset(void)
{
    if (sim.level.params.gps) sim_set_gps_offset(&sim, (Vec2){ GetRandomValue(0, 9999) / 10.0, GetRandomValue(0, 9999) / 10.0 });
}

static void reset_world(void)
{
    sim.seed = (uint64_t)GetRandomValue(1, 1 << 30); /* fresh noise every run, as on a real robot */
    sim_reset(&sim);
    new_gps_offset();
    scene_reset(&scene, &sim);
    fx_reset(&fx);
    accum = 0;
    log_n = 0;
    judged = false;
}

static void restart(void)
{
    reset_world();
    if (script_on) bot_restart(bot);
}

static void load_script(void)
{
    script_mtime = GetFileModTime(script_path);
    char *code = LoadFileText(script_path);
    if (!code) {
        snprintf(status_msg, sizeof status_msg, "Error: cannot read %s", GetFileName(script_path));
        return;
    }
    reset_world();
    script_on = true;
    if (!bot_load(bot, code, GetFileName(script_path))) printf("[robot] %s\n", bot_error(bot));
    UnloadFileText(code);
    snprintf(status_msg, sizeof status_msg, "Loaded %s", GetFileName(script_path));
}

static const char *script_state(void)
{
    if (!script_on) return "idle";
    switch (bot_state(bot)) {
    case BOT_RUNNING: return judged ? "done" : "running"; /* a judged run stops for good */
    case BOT_DONE:    return "done";
    case BOT_ERROR:   return "error";
    default:          return "no code";
    }
}

/* Page language for the few messages made here (challenge reasons come in English). */
static const char *tr(const char *en)
{
    static const char *pt[][2] = {
        { "Bumped into something.", "Bateste em alguma coisa." },
        { "Too slow.", "Demasiado lento." },
        { "Time's up.", "Acabou o tempo." },
        { "Stop on the cheese, not past it.", "Para em cima do queijo, não depois dele." },
        { "The program ended before the goal.", "O programa acabou antes do objetivo." },
        { "Loaded %s", "Carregado: %s" },
        { "The visiting LED is on away from the cheese.", "O LED de visita está aceso longe do queijo." },
        { "Works here, but won only %d of %d hidden versions of this level (needs %d).",
          "Funciona aqui, mas só venceu %d de %d versões escondidas deste nível (precisa de %d)." },
        { "Stage complete!", "Etapa concluída!" },
        { "Not yet: %s", "Ainda não: %s" },
    };
    if (lang_pt)
        for (size_t k = 0; k < sizeof pt / sizeof pt[0]; k++)
            if (strcmp(en, pt[k][0]) == 0) return pt[k][1];
    return en;
}

/* json == NULL: read the file at path. */
static bool load_level(const char *path, const char *json)
{
    static Level tmp;
    WinRule wr;
    char err[200];
    char *text = json ? NULL : LoadFileText(path);
    const char *src = json ? json : text;
    bool ok = src && level_load_json(&tmp, src, err, sizeof err) && challenge_parse(&wr, src, err, sizeof err);
    if (!src) snprintf(err, sizeof err, "cannot open %.150s", path);
    if (ok) {
        level = tmp;
        rule = wr;
        free(level_src);
        if ((level_src = malloc(strlen(src) + 1))) strcpy(level_src, src);
        snprintf(status_msg, sizeof status_msg, tr("Loaded %s"), level.lab.name);
        js_emit("level", level.lab.name, 0);
    } else {
        snprintf(status_msg, sizeof status_msg, "Error: %s", err);
        js_emit("error", TextFormat("Level: %s", err), 0);
        if (level.lab.width <= 0) /* nothing loaded yet: fall back to an empty box */
            level_load_json(&level, "{\"name\":\"Empty\",\"width\":14,\"height\":8}", err, sizeof err);
        else if (json) return false; /* keep the current level running */
    }
    if (text) UnloadFileText(text);
    sim_init(&sim, &level, 1, 12345);
    new_gps_offset();
    scene_level(&scene, &sim);
    fx_reset(&fx);
    accum = 0;
    log_n = 0;
    judged = false;
    if (script_on) bot_restart(bot);
    return ok;
}

/* Sound cues for the page: one event per bump / cheese reached this tick. */
static void sound_cues(const Robot *rb, int collisions_before, int visited_before)
{
    int visited = 0;
    for (int t = 0; t < sim.level.lab.ntargets; t++) visited += rb->visited[t];
    if (rb->collisions > collisions_before) js_emit("sound", "bump", 0);
    if (visited > visited_before) js_emit("sound", "squeak", 0);
}

/* Challenge result, once per run: stops the sim and tells the page. A win on
 * a level with hidden variants counts only if the same code wins enough of them. */
static void judge(void)
{
    if (!rule.active || judged) return;
    BotState bs = bot_state(bot);
    if (script_on && bs == BOT_ERROR) return; /* the error banner says enough */
    bool ended = sim.over || sim.robots[0].finished || (script_on && bs != BOT_RUNNING);
    const char *why;
    Outcome o = challenge_check(&rule, &sim, 0, ended, &why);
    if (o == CH_PENDING) return;
    judged = true;
    const Robot *rb = &sim.robots[0];
    int n = 0, solved = 0;
    static char vwhy[200];
    if (o == CH_WIN && rule.variants && level_src) {
        solved = script_on && bot_code(bot) ? trial_variants(level_src, bot_code(bot), sim.seed, &n, NULL, 0) : 0;
        if (solved < rule.variants) {
            o = CH_FAIL;
            snprintf(vwhy, sizeof vwhy, tr("Works here, but won only %d of %d hidden versions of this level (needs %d)."),
                     solved, n, rule.variants);
            why = vwhy;
        }
    } else if (o == CH_FAIL) why = tr(why);
    if (o == CH_WIN) snprintf(status_msg, sizeof status_msg, "%s", tr("Stage complete!"));
    else snprintf(status_msg, sizeof status_msg, tr("Not yet: %s"), why);
    printf("%s\n", status_msg);
    js_emit("result", TextFormat("{\"win\":%s,\"ticks\":%d,\"collisions\":%d,\"score\":%d,"
                                 "\"variants\":%d,\"solved\":%d,\"need\":%d,\"why\":\"%s\"}",
                                 o == CH_WIN ? "true" : "false", sim.cycle, rb->collisions, rb->score,
                                 n, solved, rule.variants, why), o == CH_WIN);
}

static void layout(Rectangle *arena, Rectangle *panel)
{
    hud_layout(GetScreenWidth(), GetScreenHeight(), sim.level.lab.width, sim.level.lab.height, arena, panel);
}

static View current_view(void)
{
    Rectangle arena, panel;
    layout(&arena, &panel);
    float pad = fmaxf(16, fminf(arena.width, arena.height) * 0.04f);
    return view_fit(arena, sim.level.lab.width, sim.level.lab.height, pad);
}

static void draw_frame(View v, float alpha)
{
    ClearBackground((Color){ 18, 20, 26, 255 });
    Vector2 shake = fx_shake_offset(&fx);
    scene_draw(&scene, &sim, v, shake, alpha);
    View vs = v;
    vs.ox += shake.x;
    vs.oy += shake.y;
    fx_draw_world(&fx, &vs);
    Vector2 tl = view_pt(&vs, 0, sim.level.lab.height);
    fx_draw_flash(&fx, (Rectangle){ tl.x, tl.y, (float)sim.level.lab.width * v.scale, (float)sim.level.lab.height * v.scale });

    Rectangle arena, panel;
    layout(&arena, &panel);
    for (int i = 0; i < log_n; i++) log_lines[i] = log_buf[i];
    const char *name = script_path[0] ? GetFileName(script_path) : bot_state(bot) != BOT_NONE ? "editor" : NULL;
    HudState hs = { paused, scene.overlays, scene.trail, speed, status_msg,
                    name, name ? script_state() : NULL, log_lines, log_n };
    hud_draw(&sim, panel, &hs);
    if (script_on && bot_state(bot) == BOT_ERROR) hud_draw_error(arena, bot_error(bot));
}

static void save_shot(View v, float alpha)
{
    /* offscreen capture: TakeScreenshot double-scales on HiDPI in raylib 5.5 */
    RenderTexture2D rt = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    BeginTextureMode(rt);
    draw_frame(v, alpha);
    EndTextureMode();
    Image img = LoadImageFromTexture(rt.texture);
    ImageFlipVertical(&img);
    ExportImage(img, shot_file);
    UnloadImage(img);
    UnloadRenderTexture(rt);
}

/* Tell the page about state it shows in its own UI. */
static const char *last_state; /* NULL: report again */

static void report_changes(void)
{
    static float last_speed;
    static bool last_over, last_paused;
    const char *st = script_state();
    if (st != last_state) {
        if (bot_state(bot) == BOT_ERROR && script_on) js_emit("error", bot_error(bot), bot_error_line(bot));
        js_emit("state", st, 0);
        last_state = st;
    }
    if (speed != last_speed) js_emit("speed", TextFormat("%g", last_speed = speed), 0);
    if (paused != last_paused) js_emit("paused", "", last_paused = paused); /* button or Space */
    if (sim.over != last_over && (last_over = sim.over)) js_emit("finished", "", sim.robots[0].score);
}

#ifdef __EMSCRIPTEN__
/* ---- page bridge (Module.ccall) ------------------------------------------ */
EMSCRIPTEN_KEEPALIVE int web_load_level(const char *json) { return load_level(NULL, json); }

/* Level JSON of a challenge's stage (see challenge.h), "" if it has none. */
EMSCRIPTEN_KEEPALIVE const char *web_stage_json(const char *json, int stage)
{
    static char *out;
    free(out);
    out = challenge_stage(json, stage);
    return out ? out : "";
}

/* Start code from the top on a fresh world; compile errors show as state "error". */
EMSCRIPTEN_KEEPALIVE void web_run(const char *code)
{
    reset_world();
    script_on = true;
    paused = false;
    last_state = NULL; /* a rerun that fails the same way still reports */
    bot_load(bot, code, "robot");
}

EMSCRIPTEN_KEEPALIVE void web_pause(int on) { paused = on != 0; }

/* Pause and advance exactly one tick. */
EMSCRIPTEN_KEEPALIVE void web_step(void) { paused = step_req = true; }

EMSCRIPTEN_KEEPALIVE void web_reset(void)
{
    script_on = false;
    paused = false;
    reset_world();
}

EMSCRIPTEN_KEEPALIVE void web_set_speed(double x) { speed = x < 0.25 ? 0.25f : x > 8 ? 8.0f : (float)x; }

EMSCRIPTEN_KEEPALIVE void web_set_lang(const char *code)
{
    lang_pt = strcmp(code, "pt") == 0;
    hud_set_lang(lang_pt);
}

/* Shown name of the level (the page translates it); keeps the run going. */
EMSCRIPTEN_KEEPALIVE void web_set_level_name(const char *name)
{
    snprintf(level.lab.name, sizeof level.lab.name, "%s", name);
    snprintf(sim.level.lab.name, sizeof sim.level.lab.name, "%s", name);
}
#endif

/* One sim cycle: script, physics, effects, judge. */
static void tick(bool scripted)
{
    if (scripted) bot_tick(bot, &sim, 0);
    const Robot *rb = &sim.robots[0];
    int hits = rb->collisions, got = 0;
    for (int t = 0; t < sim.level.lab.ntargets; t++) got += rb->visited[t];
    sim_step(&sim);
    sound_cues(rb, hits, got);
    scene_after_step(&scene, &sim, &fx);
    judge();
}

static void frame(void)
{
    if (IsKeyPressed(KEY_SPACE)) paused = !paused;
    if (IsKeyPressed(KEY_N)) paused = step_req = true;
    if (IsKeyPressed(KEY_R)) restart();
#ifndef __EMSCRIPTEN__
    if (IsKeyPressed(KEY_L)) load_level(level_path, NULL);
#endif
    if (IsKeyPressed(KEY_O)) scene.overlays = !scene.overlays;
    if (IsKeyPressed(KEY_T)) scene.trail = !scene.trail;
    if (IsKeyPressed(KEY_ZERO)) speed = 0.5f;
    for (int k = 0; k < 4; k++) if (IsKeyPressed(KEY_ONE + k)) speed = (float)(1 << k);

    bool scripted = script_on && !auto_drive;
    if (script_path[0] && (script_poll += GetFrameTime()) > 0.5f) { /* hot reload */
        script_poll = 0;
        if (GetFileModTime(script_path) != script_mtime) load_script();
    }

    if (auto_drive) {
        sim_set_motors(&sim, 0, auto_l, auto_r);
    } else if (!scripted) {
        double fwd = (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) - (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S));
        double turn = (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) - (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D));
        sim_set_motors(&sim, 0, 0.1 * fwd - 0.05 * turn, 0.1 * fwd + 0.05 * turn);
    }

    float dt_frame = fminf(GetFrameTime(), 0.1f);
    if (shot_file) dt_frame = 1.0f / 60.0f; /* deterministic captures */
    const float dt = SIM_CYCLE_MS / 1000.0f;
    bool frozen = scripted && bot_state(bot) == BOT_ERROR; /* keep the scene where it broke */
#ifdef __EMSCRIPTEN__
    frozen = !scripted || bot_state(bot) != BOT_RUNNING; /* the clock only runs after Run */
#endif
    judge(); /* script ended between ticks */
    if (!paused && !sim.over && !frozen && !judged) {
        accum += dt_frame * speed;
        int steps = 0;
        while (accum >= dt && steps < 32) {
            tick(scripted);
            accum -= dt;
            steps++;
            if (judged) break;
        }
        if (steps == 32 || judged) accum = 0; /* fell behind: drop time rather than spiral */
    } else if (step_req && !sim.over && !frozen && !judged) {
        tick(scripted);
        accum = dt; /* show the new pose; Resume interpolates on from it */
    }
    step_req = false;
    float alpha = sim.over || judged ? 1.0f : fminf(1.0f, accum / dt);
    fx_update(&fx, dt_frame);
    scene_update(&scene, dt_frame);

    View v = current_view();
    scene_prepare(&scene, &sim, v);

    BeginDrawing();
    draw_frame(v, alpha);
    EndDrawing();

    report_changes();

    if (shot_file && ++frames == shot_frames) {
        save_shot(v, alpha);
        shot_file = NULL;
        frames = -1;
    }
}

int main(int argc, char **argv)
{
    int win_w = 1280, win_h = 720;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc) shot_file = argv[++i];
        else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) shot_frames = atoi(argv[++i]);
        else if (strcmp(argv[i], "--size") == 0 && i + 2 < argc) { win_w = atoi(argv[++i]); win_h = atoi(argv[++i]); }
        else if (strcmp(argv[i], "--motors") == 0 && i + 2 < argc) { auto_drive = true; auto_l = atof(argv[++i]); auto_r = atof(argv[++i]); }
        else if (strcmp(argv[i], "--overlays") == 0) scene.overlays = scene.trail = true;
        else if (IsFileExtension(argv[i], ".lua")) snprintf(script_path, sizeof script_path, "%s", argv[i]);
        else snprintf(level_path, sizeof level_path, "%s", argv[i]);
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(win_w, win_h, "CiberRato Academy");
#ifdef __EMSCRIPTEN__
    SetWindowMinSize(320, 240); /* the page's canvas pane can be small */
#else
    SetWindowMinSize(640, 360);
#endif
    bot = bot_new(on_say, NULL);
    load_level(level_path, NULL);
    if (script_path[0]) load_script();
    js_emit("ready", "", 0);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(frame, 0, 1);
#else
    while (!WindowShouldClose() && frames >= 0) frame();
#endif
    bot_free(bot);
    scene_unload(&scene);
    text_unload();
    CloseWindow();
    return 0;
}
