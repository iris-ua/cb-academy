/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "fx.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static float frand(float a, float b) { return a + (b - a) * (float)GetRandomValue(0, 10000) / 10000.0f; }

void fx_reset(Fx *fx) { memset(fx, 0, sizeof *fx); }

static void emit(Fx *fx, Vec2 at, double dir, double spread, float speed0, float speed1,
                 float life, float size, Color c)
{
    if (fx->np >= FX_MAX_PARTICLES) return;
    double a = dir + frand((float)-spread, (float)spread);
    float sp = frand(speed0, speed1);
    fx->p[fx->np++] = (Particle){
        .pos = at, .vel = { cos(a) * sp, sin(a) * sp },
        .life = life * frand(0.7f, 1.2f), .size = size * frand(0.6f, 1.3f),
        .spin = frand(-10, 10), .angle = frand(0, 6.28f), .color = c,
    };
    fx->p[fx->np - 1].max = fx->p[fx->np - 1].life;
}

static void popup(Fx *fx, Vec2 at, const char *text, Color c)
{
    int best = 0;
    for (int i = 0; i < FX_MAX_POPUPS; i++)
        if (fx->pop[i].life < fx->pop[best].life) best = i;
    Popup *p = &fx->pop[best];
    p->pos = at;
    p->life = 1.0f;
    p->color = c;
    snprintf(p->text, sizeof p->text, "%s", text);
}

void fx_collision(Fx *fx, Vec2 at, double dir, int penalty)
{
    fx->shake = fminf(1.0f, fx->shake + 0.7f);
    fx->flash = 1.0f;
    Vec2 hit = { at.x + 0.5 * cos(dir), at.y + 0.5 * sin(dir) };
    for (int i = 0; i < 14; i++)
        emit(fx, hit, dir + M_PI, 1.3, 1.0f, 3.5f, 0.35f, 0.06f, (Color){ 255, 214, 120, 255 });
    char t[16];
    snprintf(t, sizeof t, "-%d", penalty);
    popup(fx, (Vec2){ at.x, at.y + 0.7 }, t, (Color){ 225, 70, 70, 255 });
}

void fx_target(Fx *fx, Vec2 at, int reward)
{
    static const Color pal[] = {
        { 255, 205, 60, 255 }, { 255, 160, 40, 255 }, { 255, 245, 200, 255 }, { 120, 200, 120, 255 },
    };
    for (int i = 0; i < 90; i++)
        emit(fx, at, 0, M_PI, 1.5f, 6.0f, 1.3f, 0.12f, pal[i % 4]);
    fx->shake = fminf(1.0f, fx->shake + 0.25f);
    char t[16];
    snprintf(t, sizeof t, "+%d", reward);
    popup(fx, (Vec2){ at.x, at.y + 1.0 }, t, (Color){ 60, 150, 70, 255 });
}

void fx_update(Fx *fx, float dt)
{
    fx->time += dt;
    fx->shake = fmaxf(0, fx->shake - dt * 2.2f);
    fx->flash = fmaxf(0, fx->flash - dt * 3.0f);
    float drag = expf(-3.0f * dt);
    for (int i = 0; i < fx->np;) {
        Particle *p = &fx->p[i];
        p->life -= dt;
        if (p->life <= 0) { fx->p[i] = fx->p[--fx->np]; continue; }
        p->pos.x += p->vel.x * dt;
        p->pos.y += p->vel.y * dt;
        p->vel.x *= drag;
        p->vel.y *= drag;
        p->angle += p->spin * dt;
        i++;
    }
    for (int i = 0; i < FX_MAX_POPUPS; i++) {
        Popup *p = &fx->pop[i];
        if (p->life <= 0) continue;
        p->life -= dt * 0.9f;
        p->pos.y += dt * 0.8;
    }
}

Vector2 fx_shake_offset(const Fx *fx)
{
    float a = 7.0f * fx->shake * fx->shake;
    return (Vector2){ a * sinf(fx->time * 71.0f), a * cosf(fx->time * 53.0f + 1.3f) };
}

void fx_draw_world(const Fx *fx, const View *v)
{
    for (int i = 0; i < fx->np; i++) {
        const Particle *p = &fx->p[i];
        float t = p->life / p->max;
        Frame f = frame_at(p->pos.x, p->pos.y, p->angle);
        double h = p->size * 0.5 * (0.4 + 0.6 * t);
        Vector2 q[4] = {
            frame_pt(v, &f, -h, -h * 0.6), frame_pt(v, &f, h, -h * 0.6),
            frame_pt(v, &f, h, h * 0.6), frame_pt(v, &f, -h, h * 0.6),
        };
        fill_convex(q, 4, color_alpha(p->color, fminf(1, t * 2)));
    }
    for (int i = 0; i < FX_MAX_POPUPS; i++) {
        const Popup *p = &fx->pop[i];
        if (p->life <= 0) continue;
        Vector2 s = view_pt(v, p->pos.x, p->pos.y);
        int size = (int)fmaxf(16, v->scale * 0.7f);
        float pop = p->life > 0.85f ? 1 + (p->life - 0.85f) * 3 : 1;
        size = (int)(size * pop);
        int w = MeasureText(p->text, size);
        float a = fminf(1, p->life * 2.5f);
        DrawText(p->text, (int)s.x - w / 2 + 1, (int)s.y - size / 2 + 2, size, color_alpha((Color){ 255, 255, 255, 220 }, a));
        DrawText(p->text, (int)s.x - w / 2, (int)s.y - size / 2, size, color_alpha(p->color, a));
    }
}

void fx_draw_flash(const Fx *fx, Rectangle r)
{
    if (fx->flash <= 0) return;
    Color c = color_alpha((Color){ 230, 60, 60, 150 }, fx->flash);
    Color z = { 230, 60, 60, 0 };
    float e = fminf(r.width, r.height) * 0.18f;
    DrawRectangleGradientV((int)r.x, (int)r.y, (int)r.width, (int)e, c, z);
    DrawRectangleGradientV((int)r.x, (int)(r.y + r.height - e), (int)r.width, (int)e, z, c);
    DrawRectangleGradientH((int)r.x, (int)r.y, (int)e, (int)r.height, c, z);
    DrawRectangleGradientH((int)(r.x + r.width - e), (int)r.y, (int)e, (int)r.height, z, c);
}
