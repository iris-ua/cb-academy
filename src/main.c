/* SPDX-License-Identifier: GPL-2.0-or-later */
/* CiberRato Jr — app shell.
 * M0/M1: draws the arena with placeholder graphics and lets you drive the
 * mouse with the keyboard to exercise the simulation. Visuals come in M2. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "raylib.h"
#include "sim/sim.h"
#include "sim/level.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#define HUD_W  260
#define MARGIN 24

static Level level;
static Sim   sim;
static char  level_path[512] = "levels/default.json";
static char  status_msg[256];
static bool  paused;
static float accum;
static int   speed = 1;
static const char *shot_file; /* dev: --shot out.png renders a few frames, saves, exits */
static int   frames;

/* ---- view transform (sim y up -> screen y down) ------------------------ */
typedef struct { float ox, oy, scale; } View;

static View fit_view(void)
{
    float w = (float)GetScreenWidth() - HUD_W - 2 * MARGIN;
    float h = (float)GetScreenHeight() - 2 * MARGIN;
    const Lab *lab = &sim.level.lab;
    float s = fminf(w / (float)lab->width, h / (float)lab->height);
    if (s < 1) s = 1;
    View v = { MARGIN + (w - s * (float)lab->width) / 2, MARGIN + (h - s * (float)lab->height) / 2, s };
    return v;
}

static Vector2 to_screen(const View *v, double x, double y)
{
    return (Vector2){ v->ox + (float)x * v->scale, v->oy + (float)(sim.level.lab.height - y) * v->scale };
}

/* ---- loading ----------------------------------------------------------- */
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
    accum = 0;
}

/* ---- drawing ----------------------------------------------------------- */
static void draw_wall(const View *v, const Wall *w)
{
    Color fill = w->height >= 4 ? (Color){ 70, 90, 140, 255 } : (Color){ 110, 130, 180, 255 };
    Vector2 p0 = to_screen(v, w->pts[0].x, w->pts[0].y);
    for (int i = 1; i + 1 < w->n; i++) { /* fan; fine for convex walls */
        Vector2 a = to_screen(v, w->pts[i].x, w->pts[i].y);
        Vector2 b = to_screen(v, w->pts[i + 1].x, w->pts[i + 1].y);
        DrawTriangle(p0, a, b, fill);
        DrawTriangle(p0, b, a, fill); /* either winding */
    }
}

static void draw_robot(const View *v, const Robot *r, int idx)
{
    Vector2 c = to_screen(v, r->pose.x, r->pose.y);
    float rad = (float)SIM_ROBOT_RADIUS * v->scale;
    Color body = r->collision ? (Color){ 230, 80, 80, 255 } : (Color){ 200, 200, 210, 255 };
    DrawCircleV(c, rad, body);
    DrawCircleLinesV(c, rad, DARKGRAY);
    Vector2 h = to_screen(v, r->pose.x + cos(r->pose.dir) * SIM_ROBOT_RADIUS,
                             r->pose.y + sin(r->pose.dir) * SIM_ROBOT_RADIUS);
    DrawLineEx(c, h, 3, (Color){ 40, 40, 50, 255 });
    for (int k = 0; k < SIM_NUM_IR; k++) {
        double a = r->pose.dir + sim.level.params.ir_angles[k];
        Vector2 s = to_screen(v, r->pose.x + cos(a) * SIM_ROBOT_RADIUS, r->pose.y + sin(a) * SIM_ROBOT_RADIUS);
        DrawCircleV(s, 3, ORANGE);
    }
    DrawText(TextFormat("%d", idx + 1), (int)c.x - 4, (int)c.y - 6, 12, DARKGRAY);
}

static void draw_arena(void)
{
    View v = fit_view();
    const Lab *lab = &sim.level.lab;
    Vector2 tl = to_screen(&v, 0, lab->height);
    DrawRectangleV(tl, (Vector2){ (float)lab->width * v.scale, (float)lab->height * v.scale }, (Color){ 235, 232, 222, 255 });
    for (int x = 0; x <= (int)lab->width; x += 2)
        DrawLineV(to_screen(&v, x, 0), to_screen(&v, x, lab->height), (Color){ 220, 216, 204, 255 });
    for (int y = 0; y <= (int)lab->height; y += 2)
        DrawLineV(to_screen(&v, 0, y), to_screen(&v, lab->width, y), (Color){ 220, 216, 204, 255 });

    for (int t = 0; t < lab->ntargets; t++) {
        Vector2 c = to_screen(&v, lab->targets[t].pos.x, lab->targets[t].pos.y);
        DrawCircleV(c, (float)lab->targets[t].radius * v.scale, (Color){ 255, 205, 60, 160 });
    }
    for (int i = 1; i < lab->nwalls; i++)
        if (lab->walls[i].height > 0) draw_wall(&v, &lab->walls[i]);
    DrawRectangleLinesEx((Rectangle){ tl.x - 4, tl.y - 4, (float)lab->width * v.scale + 8, (float)lab->height * v.scale + 8 },
                         4, (Color){ 70, 90, 140, 255 });
    for (int b = 0; b < lab->nbeacons; b++) {
        Vector2 c = to_screen(&v, lab->beacons[b].pos.x, lab->beacons[b].pos.y);
        DrawCircleV(c, 0.25f * v.scale, (Color){ 255, 120, 40, 255 });
    }
    for (int i = 0; i < sim.nrobots; i++) draw_robot(&v, &sim.robots[i], i);
}

static void draw_hud(void)
{
    int x = GetScreenWidth() - HUD_W + 12, y = MARGIN;
    const Robot *r = &sim.robots[0];
    DrawRectangle(GetScreenWidth() - HUD_W, 0, HUD_W, GetScreenHeight(), (Color){ 28, 30, 38, 255 });
    DrawText("CiberRato Jr", x, y, 24, RAYWHITE); y += 34;
    DrawText(sim.level.lab.name, x, y, 16, LIGHTGRAY); y += 28;

    int limit = sim.level.params.time_limit;
    DrawText(limit > 0 ? TextFormat("Time  %d / %d", sim.cycle, limit) : TextFormat("Time  %d", sim.cycle), x, y, 18, RAYWHITE); y += 24;
    DrawText(TextFormat("Score %d   Hits %d", r->score, r->collisions), x, y, 18, RAYWHITE); y += 32;

    const char *irn[SIM_NUM_IR] = { "front", "left", "right", "back" };
    for (int k = 0; k < SIM_NUM_IR; k++) { DrawText(TextFormat("IR %-5s %6.1f", irn[k], r->ir[k]), x, y, 16, SKYBLUE); y += 20; }
    DrawText(TextFormat("Compass  %5.0f", r->compass), x, y, 16, SKYBLUE); y += 20;
    for (int b = 0; b < sim.level.lab.nbeacons; b++) {
        DrawText(r->beacon_visible[b] ? TextFormat("Beacon%d  %5.0f", b, r->beacon_dir[b]) : TextFormat("Beacon%d  hidden", b), x, y, 16, SKYBLUE);
        y += 20;
    }
    DrawText(TextFormat("Ground   %5d", r->ground), x, y, 16, SKYBLUE); y += 20;
    DrawText(TextFormat("Bumper   %5s", r->bumper ? "HIT" : "-"), x, y, 16, r->bumper ? RED : SKYBLUE); y += 32;

    if (sim.over) { DrawText("TIME UP / FINISHED", x, y, 18, GOLD); y += 26; }
    else if (paused) { DrawText("PAUSED", x, y, 18, GOLD); y += 26; }

    DrawText("Arrows/WASD  drive", x, y, 14, GRAY); y += 18;
    DrawText(TextFormat("Space pause  1-4 speed (x%d)", speed), x, y, 14, GRAY); y += 18;
    DrawText("R reset   L reload level", x, y, 14, GRAY); y += 26;
    DrawText(status_msg, x, GetScreenHeight() - 26, 12, GRAY);
}

static void draw_scene(void)
{
    ClearBackground((Color){ 18, 20, 26, 255 });
    draw_arena();
    draw_hud();
}

/* ---- frame ------------------------------------------------------------- */
static void frame(void)
{
    if (IsKeyPressed(KEY_SPACE)) paused = !paused;
    if (IsKeyPressed(KEY_R)) { sim_reset(&sim); accum = 0; }
    if (IsKeyPressed(KEY_L)) load_level(level_path);
    for (int k = 0; k < 4; k++) if (IsKeyPressed(KEY_ONE + k)) speed = 1 << k;

    double fwd = (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) - (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S));
    double turn = (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) - (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D));
    sim_set_motors(&sim, 0, 0.1 * fwd - 0.05 * turn, 0.1 * fwd + 0.05 * turn);

    if (!paused && !sim.over) {
        accum += GetFrameTime() * (float)speed;
        const float dt = SIM_CYCLE_MS / 1000.0f;
        int steps = 0;
        while (accum >= dt && steps < 32) { sim_step(&sim); accum -= dt; steps++; }
        if (steps == 32) accum = 0; /* fell behind: drop time rather than spiral */
    }

    BeginDrawing();
    draw_scene();
    EndDrawing();

    if (shot_file && ++frames == 90) {
        /* offscreen capture: TakeScreenshot double-scales on HiDPI in raylib 5.5 */
        RenderTexture2D rt = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
        BeginTextureMode(rt);
        draw_scene();
        EndTextureMode();
        Image img = LoadImageFromTexture(rt.texture);
        ImageFlipVertical(&img);
        ExportImage(img, shot_file);
        UnloadImage(img);
        UnloadRenderTexture(rt);
        shot_file = NULL;
        frames = -1;
    }
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc) shot_file = argv[++i];
        else snprintf(level_path, sizeof level_path, "%s", argv[i]);
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "CiberRato Jr");
    load_level(level_path);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(frame, 0, 1);
#else
    while (!WindowShouldClose() && frames >= 0) frame();
#endif
    CloseWindow();
    return 0;
}
