/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "hud.h"
#include <math.h>
#include "text.h"
#include "view.h"

static const Color C_PANEL = { 23, 26, 35, 255 };
static const Color C_CARD  = { 32, 36, 48, 255 };
static const Color C_TEXT  = { 235, 236, 242, 255 };
static const Color C_DIM   = { 138, 144, 162, 255 };
static const Color C_ACC   = { 110, 190, 240, 255 };
static const Color C_GOLD  = { 255, 200, 70, 255 };
static const Color C_RED   = { 235, 85, 80, 255 };
static const Color C_GREEN = { 90, 205, 120, 255 };


static void card(float x, float y, float w, float h)
{
    DrawRectangleRounded((Rectangle){ x, y, w, h }, 10.0f / fminf(w, h), 6, C_CARD);
}

static void label(const char *t, float x, float y) { text_draw(TEXT_BOLD, t, x, y - 3, 12, C_DIM); }

static void time_str(char *buf, int cycles)
{
    int ms = cycles * SIM_CYCLE_MS;
    TextCopy(buf, TextFormat("%d:%02d.%d", ms / 60000, ms / 1000 % 60, ms / 100 % 10));
}

static void dial(Vector2 c, float r, double deg, bool on, Color col)
{
    DrawCircleV(c, r, (Color){ 20, 23, 31, 255 });
    DrawRing(c, r - 1.5f, r, 0, 360, 32, (Color){ 60, 66, 84, 255 });
    for (int k = 0; k < 4; k++) {
        float a = k * PI / 2;
        DrawLineEx((Vector2){ c.x + cosf(a) * (r - 5), c.y + sinf(a) * (r - 5) },
                   (Vector2){ c.x + cosf(a) * (r - 2), c.y + sinf(a) * (r - 2) }, 1.5f, (Color){ 80, 86, 104, 255 });
    }
    if (!on) {
        text_draw(TEXT_BOLD, "?", c.x - 4, c.y - 10, 16, C_DIM);
        return;
    }
    float a = (float)(-deg * DEG2RAD_D); /* sim CCW -> screen */
    Vector2 tip = { c.x + cosf(a) * (r - 4), c.y + sinf(a) * (r - 4) };
    DrawLineEx(c, tip, 2.5f, col);
    DrawCircleV(tip, 3, col);
    DrawCircleV(c, 2.5f, col);
}

static void led(float x, float y, const char *name, bool on, Color c)
{
    if (on) DrawCircleGradient((int)x, (int)y, 13, color_alpha(c, 0.6f), color_alpha(c, 0));
    DrawCircleV((Vector2){ x, y }, 6, on ? c : (Color){ 55, 60, 76, 255 });
    text_draw(TEXT_UI, name, x + 12, y - 9, 13, on ? C_TEXT : C_DIM);
}

/* ---- sections: each draws at (x, y) with width w and returns its height --- */

static float sec_title(const Sim *s, const HudState *st, float x, float y, float w)
{
    text_draw(TEXT_BOLD, "CiberRato Jr", x, y - 6, 24, C_TEXT);
    text_draw(TEXT_UI, s->level.lab.name, x, y + 22, 13, C_DIM);
    if (st->paused || s->over) {
        const char *b = s->over ? "FINISHED" : "PAUSED";
        float bw = text_width(TEXT_BOLD, b, 11) + 14;
        DrawRectangleRounded((Rectangle){ x + w - bw, y + 22, bw, 18 }, 0.5f, 4, s->over ? C_GOLD : C_ACC);
        text_draw(TEXT_BOLD, b, x + w - bw + 7, y + 23, 11, (Color){ 20, 22, 30, 255 });
    }
    return 46;
}

static float sec_time_score(const Sim *s, const HudState *st, float x, float y, float w)
{
    const Robot *r = &s->robots[0];
    const Lab *lab = &s->level.lab;
    float cw = (w - 8) / 2;
    char t[32];
    card(x, y, cw, 64);
    label("TIME", x + 10, y + 8);
    const char *sp = TextFormat("x%d", st->speed);
    text_draw(TEXT_MONO_BOLD, sp, x + cw - 10 - text_width(TEXT_MONO_BOLD, sp, 11), y + 6, 11, C_ACC);
    time_str(t, s->cycle);
    text_draw(TEXT_MONO_BOLD, t, x + 10, y + 19, 22, C_TEXT);
    int limit = s->level.params.time_limit;
    if (limit > 0) {
        float f = fminf(1.0f, (float)s->cycle / (float)limit);
        DrawRectangleRounded((Rectangle){ x + 10, y + 50, cw - 20, 5 }, 1, 4, (Color){ 50, 55, 72, 255 });
        DrawRectangleRounded((Rectangle){ x + 10, y + 50, (cw - 20) * f, 5 }, 1, 4, f > 0.85f ? C_RED : C_ACC);
    }

    float sx = x + cw + 8;
    card(sx, y, cw, 64);
    label("SCORE", sx + 10, y + 8);
    text_draw(TEXT_MONO_BOLD, TextFormat("%d", r->score), sx + 10, y + 19, 22, C_GOLD);
    label(TextFormat("hits %d", r->collisions), sx + 10, y + 46);
    if (lab->ntargets > 0) {
        int got = 0;
        for (int k = 0; k < lab->ntargets; k++) got += r->visited[k];
        const char *ct = TextFormat("%d/%d", got, lab->ntargets);
        float tx = sx + cw - 10 - text_width(TEXT_BOLD, ct, 12);
        DrawCircleV((Vector2){ tx - 7, y + 51 }, 3.5f, C_GOLD);
        label(ct, tx, y + 46);
    }
    return 72;
}

static float sec_ir(const Sim *s, float x, float y, float w)
{
    const Robot *r = &s->robots[0];
    static const char *irn[SIM_NUM_IR] = { "front", "left", "right", "back" };
    float ch = 22 + SIM_NUM_IR * 18;
    card(x, y, w, ch);
    label("IR SENSORS  (1 / distance)", x + 10, y + 8);
    for (int k = 0; k < SIM_NUM_IR; k++) {
        float ry = y + 24 + k * 18, bx = x + 54, bw = w - 54 - 48;
        text_draw(TEXT_UI, irn[k], x + 10, ry - 4, 13, C_TEXT);
        float f = fminf(1.0f, (float)r->ir[k] / 5.0f);
        DrawRectangleRounded((Rectangle){ bx, ry + 1, bw, 8 }, 1, 4, (Color){ 22, 25, 34, 255 });
        if (f > 0) DrawRectangleRounded((Rectangle){ bx, ry + 1, fmaxf(8, bw * f), 8 }, 1, 4, color_mix(C_ACC, C_RED, f));
        const char *iv = TextFormat("%.1f", r->ir[k]);
        text_draw(TEXT_MONO, iv, x + w - 10 - text_width(TEXT_MONO, iv, 13), ry - 4, 13, C_TEXT);
    }
    return ch + 8;
}

static float sec_dials(const Sim *s, float x, float y, float w)
{
    const Robot *r = &s->robots[0];
    card(x, y, w, 86);
    float dr = 24;
    Vector2 c1 = { x + 10 + dr, y + 46 };
    label("COMPASS", x + 10, y + 8);
    dial(c1, dr, r->compass, true, C_ACC);
    text_draw(TEXT_MONO_BOLD, TextFormat("%.0f°", r->compass), c1.x + dr + 10, c1.y - 10, 16, C_TEXT);
    if (s->level.lab.nbeacons > 0) {
        float bx = x + w / 2 + 4;
        Vector2 c2 = { bx + dr, y + 46 };
        bool vis = r->beacon_visible[0];
        label("BEACON", bx, y + 8);
        /* robot-relative: heading points up on the dial */
        dial(c2, dr, vis ? r->beacon_dir[0] + 90 : 0, vis, C_GOLD);
        text_draw(TEXT_MONO_BOLD, vis ? TextFormat("%.0f°", r->beacon_dir[0]) : "--", c2.x + dr + 10, c2.y - 10, 16, vis ? C_TEXT : C_DIM);
    }
    return 94;
}

static float sec_ground(const Sim *s, float x, float y, float w)
{
    const Robot *r = &s->robots[0];
    card(x, y, w, 62);
    label("GROUND", x + 10, y + 8);
    text_draw(TEXT_BOLD, r->ground >= 0 ? TextFormat("target %d", r->ground) : "floor", x + 10, y + 18, 16,
              r->ground >= 0 ? C_GOLD : C_TEXT);
    float bx = x + w / 2 + 4;
    label("BUMPER", bx, y + 8);
    DrawRectangleRounded((Rectangle){ bx, y + 20, 54, 18 }, 0.5f, 4, r->bumper ? C_RED : (Color){ 50, 55, 72, 255 });
    const char *bt = r->bumper ? "HIT" : "ok";
    text_draw(TEXT_BOLD, bt, bx + 27 - text_width(TEXT_BOLD, bt, 12) / 2, y + 20, 12, r->bumper ? WHITE : C_DIM);
    led(x + 16, y + 48, "visiting", r->led_visiting, C_GOLD);
    led(bx + 6, y + 48, "returning", r->led_returning, C_GREEN);
    return 70;
}

static void sec_keys(const HudState *st, float x, float y)
{
    const char *keys[] = {
        "arrows/WASD drive   space pause",
        "1-4 speed   R reset   L reload",
        TextFormat("O overlays [%s]   T trail [%s]", st->overlays ? "on" : "off", st->trail ? "on" : "off"),
    };
    for (int k = 0; k < 3; k++) text_draw(TEXT_UI, keys[k], x, y + k * 15 - 4, 13, C_DIM);
    if (st->status && st->status[0]) text_draw(TEXT_UI, st->status, x, y + 46, 13, (Color){ 100, 106, 124, 255 });
}

/* ---- layout --------------------------------------------------------------- */

#define PAD      14
#define BOTTOM_H 214

static float side_width(int sw) { return fminf(300.0f, fmaxf(240.0f, sw * 0.24f)); }

static float fit_scale(Rectangle a, double lab_w, double lab_h)
{
    return fminf(a.width / (float)lab_w, a.height / (float)lab_h);
}

void hud_layout(int sw, int sh, double lab_w, double lab_h, Rectangle *arena, Rectangle *panel)
{
    float hw = side_width(sw);
    Rectangle side_a = { 0, 0, sw - hw, (float)sh }, side_p = { sw - hw, 0, hw, (float)sh };
    Rectangle bot_a = { 0, 0, (float)sw, (float)sh - BOTTOM_H }, bot_p = { 0, (float)sh - BOTTOM_H, (float)sw, BOTTOM_H };
    /* bottom panel only on narrow screens, where it leaves a bigger arena */
    bool bottom = sw < 2 * 2 * side_width(sw) && fit_scale(bot_a, lab_w, lab_h) > fit_scale(side_a, lab_w, lab_h);
    *arena = bottom ? bot_a : side_a;
    *panel = bottom ? bot_p : side_p;
}

void hud_draw(const Sim *s, Rectangle p, const HudState *st)
{
    DrawRectangleRec(p, C_PANEL);
    bool bottom = p.y > 0;
    if (bottom) DrawLineEx((Vector2){ p.x, p.y }, (Vector2){ p.x + p.width, p.y }, 1, (Color){ 45, 50, 66, 255 });
    else DrawLineEx((Vector2){ p.x, p.y }, (Vector2){ p.x, p.y + p.height }, 1, (Color){ 45, 50, 66, 255 });

    float x = p.x + PAD, y = p.y + PAD;
    if (!bottom) {
        float w = p.width - 2 * PAD;
        y += sec_title(s, st, x, y, w);
        y += sec_time_score(s, st, x, y, w);
        y += sec_ir(s, x, y, w);
        y += sec_dials(s, x, y, w);
        y += sec_ground(s, x, y, w);
        sec_keys(st, x, fmaxf(y + 4, p.y + p.height - 78));
        return;
    }
    /* two columns */
    float w = (p.width - 3 * PAD) / 2, x2 = x + w + PAD, y2 = y;
    y += sec_title(s, st, x, y, w);
    y += sec_time_score(s, st, x, y, w);
    sec_ground(s, x, y, w);
    y2 += sec_ir(s, x2, y2, w);
    sec_dials(s, x2, y2, w);
}
