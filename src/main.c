/* SPDX-License-Identifier: GPL-2.0-or-later */
/* CiberRato Jr — app shell: input, fixed-tick stepping, rendering.
 * Drive the mouse with the keyboard until the Lua API lands (M3). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "sim/sim.h"
#include "sim/level.h"
#include "gfx/fx.h"
#include "gfx/hud.h"
#include "gfx/scene.h"

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
static float accum;
static int   speed = 1;

/* dev: --shot out.png renders N frames, saves, exits; --motors L R drives constantly */
static const char *shot_file;
static int    shot_frames = 90, frames;
static bool   auto_drive;
static double auto_l, auto_r;

static void load_level(const char *path)
{
    static Level tmp;
    char err[200];
    if (level_load_file(&tmp, path, err, sizeof err)) {
        level = tmp;
        snprintf(status_msg, sizeof status_msg, "Loaded %s", level.lab.name);
    } else {
        snprintf(status_msg, sizeof status_msg, "Error: %s", err);
        if (level.lab.width <= 0) /* nothing loaded yet: fall back to an empty box */
            level_load_json(&level, "{\"name\":\"Empty\",\"width\":14,\"height\":8}", err, sizeof err);
    }
    sim_init(&sim, &level, 1, 12345);
    scene_level(&scene, &sim);
    fx_reset(&fx);
    accum = 0;
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
    HudState hs = { paused, scene.overlays, scene.trail, speed, status_msg };
    hud_draw(&sim, panel, &hs);
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

static void frame(void)
{
    if (IsKeyPressed(KEY_SPACE)) paused = !paused;
    if (IsKeyPressed(KEY_R)) { sim_reset(&sim); scene_reset(&scene, &sim); fx_reset(&fx); accum = 0; }
    if (IsKeyPressed(KEY_L)) load_level(level_path);
    if (IsKeyPressed(KEY_O)) scene.overlays = !scene.overlays;
    if (IsKeyPressed(KEY_T)) scene.trail = !scene.trail;
    for (int k = 0; k < 4; k++) if (IsKeyPressed(KEY_ONE + k)) speed = 1 << k;

    if (auto_drive) {
        sim_set_motors(&sim, 0, auto_l, auto_r);
    } else {
        double fwd = (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) - (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S));
        double turn = (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) - (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D));
        sim_set_motors(&sim, 0, 0.1 * fwd - 0.05 * turn, 0.1 * fwd + 0.05 * turn);
    }

    float dt_frame = fminf(GetFrameTime(), 0.1f);
    if (shot_file) dt_frame = 1.0f / 60.0f; /* deterministic captures */
    const float dt = SIM_CYCLE_MS / 1000.0f;
    if (!paused && !sim.over) {
        accum += dt_frame * (float)speed;
        int steps = 0;
        while (accum >= dt && steps < 32) {
            sim_step(&sim);
            scene_after_step(&scene, &sim, &fx);
            accum -= dt;
            steps++;
        }
        if (steps == 32) accum = 0; /* fell behind: drop time rather than spiral */
    }
    float alpha = sim.over ? 1.0f : fminf(1.0f, accum / dt);
    fx_update(&fx, dt_frame);
    scene_update(&scene, dt_frame);

    View v = current_view();
    scene_prepare(&scene, &sim, v);

    BeginDrawing();
    draw_frame(v, alpha);
    EndDrawing();

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
        else snprintf(level_path, sizeof level_path, "%s", argv[i]);
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(win_w, win_h, "CiberRato Jr");
    SetWindowMinSize(640, 360);
    load_level(level_path);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(frame, 0, 1);
#else
    while (!WindowShouldClose() && frames >= 0) frame();
#endif
    scene_unload(&scene);
    CloseWindow();
    return 0;
}
