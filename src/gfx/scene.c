/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "scene.h"
#include "text.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CORNER_R     0.07   /* wall corner rounding (world units) */
#define SHADOW_DOWN  2      /* shadow mask resolution divisor */
#define OVERLAY_MAX  3.0    /* IR cone length when nothing is seen */

static const Color C_FLOOR      = { 239, 234, 224, 255 };
static const Color C_GRID_MINOR = { 230, 224, 212, 255 };
static const Color C_GRID_MAJOR = { 220, 212, 197, 255 };
static const Color C_WATERMARK  = { 150, 138, 115, 100 };
static const Color C_FRAME      = { 62, 74, 114, 255 };
static const Color C_TALL_SIDE  = { 58, 69, 110, 255 };
static const Color C_TALL_TOP   = { 101, 119, 176, 255 };
static const Color C_LOW_SIDE   = { 52, 120, 128, 255 };
static const Color C_LOW_TOP    = { 96, 176, 180, 255 };
static const Color C_SHADOW     = { 40, 34, 60, 255 };
static const Color C_CHEESE     = { 255, 205, 60, 255 };
static const Color C_RIND       = { 226, 150, 42, 255 };
static const Color C_HOLE       = { 236, 178, 46, 255 };
static const Color C_DONE       = { 90, 180, 100, 255 };
static const Color C_BEACON     = { 255, 118, 64, 255 };
static const Color C_PINK       = { 236, 150, 160, 255 };
static const Color C_INK        = { 40, 38, 52, 255 };

static const Color C_FUR[SIM_MAX_ROBOTS] = {
    { 168, 162, 186, 255 }, { 196, 160, 120, 255 }, { 140, 166, 190, 255 }, { 226, 224, 230, 255 },
};

static double wall_depth(double h) { return fmin(0.06, fmax(0.02, 0.012 * h)); }
static double shadow_len(double h) { return fmin(0.32, fmax(0.1, 0.07 * h)); }

/* ---- level caches ------------------------------------------------------- */

static bool corner_joined(const Lab *lab, int self, Vec2 v)
{
    const double e = 1e-6;
    if (v.x <= e || v.y <= e || v.x >= lab->width - e || v.y >= lab->height - e) return true;
    for (int k = 1; k < lab->nwalls; k++) {
        if (k == self || lab->walls[k].height <= 0) continue;
        if (wall_contains(&lab->walls[k], v) || wall_point_distance(&lab->walls[k], v) < e) return true;
    }
    return false;
}

static void reset_vis(Scene *sc, const Sim *s)
{
    for (int i = 0; i < SIM_MAX_ROBOTS; i++) {
        RobotVis *rv = &sc->rv[i];
        memset(rv, 0, sizeof *rv);
        if (i >= s->nrobots) continue;
        const Robot *r = &s->robots[i];
        rv->prev = rv->cur = r->pose;
        rv->collisions = r->collisions;
        memcpy(rv->visited, r->visited, sizeof rv->visited);
        rv->trail[0] = (Vec2){ r->pose.x, r->pose.y };
        rv->trail_len = 1;
    }
}

static void load_logo(Scene *sc)
{
    if (sc->logo.id) return;
    const char *paths[] = {
        "assets/images/ua-logo.png",
        TextFormat("%sassets/images/ua-logo.png", GetApplicationDirectory()),
    };
    for (int i = 0; i < 2 && !sc->logo.id; i++)
        if (FileExists(paths[i])) sc->logo = LoadTexture(paths[i]);
    if (!sc->logo.id) { TraceLog(LOG_WARNING, "SCENE: ua-logo.png not found"); return; }
    GenTextureMipmaps(&sc->logo);
    SetTextureFilter(sc->logo, TEXTURE_FILTER_TRILINEAR);
}

void scene_level(Scene *sc, const Sim *s)
{
    load_logo(sc);
    const Lab *lab = &s->level.lab;
    sc->tall_h = 0;
    for (int b = 0; b < lab->nbeacons; b++) sc->tall_h = fmax(sc->tall_h, lab->beacons[b].height);
    for (int i = 1; i < lab->nwalls; i++) {
        const Wall *w = &lab->walls[i];
        double rad[SIM_MAX_CORNERS];
        for (int j = 0; j < w->n; j++) rad[j] = corner_joined(lab, i, w->pts[j]) ? 0 : CORNER_R;
        sc->nout[i] = w->height > 0 ? rounded_outline(w->pts, w->n, rad, 4, sc->outline[i], SCENE_WALL_PTS) : 0;
    }
    sc->shadow_dirty = true;
    reset_vis(sc, s);
}

void scene_reset(Scene *sc, const Sim *s) { reset_vis(sc, s); }

void scene_after_step(Scene *sc, const Sim *s, Fx *fx)
{
    for (int i = 0; i < s->nrobots; i++) {
        const Robot *r = &s->robots[i];
        RobotVis *rv = &sc->rv[i];
        rv->prev = rv->cur;
        rv->cur = r->pose;
        rv->wheel_prev[0] = rv->wheel[0];
        rv->wheel_prev[1] = rv->wheel[1];
        rv->speed_goal = (float)(fabs(r->vel) / SIM_MAX_POWER);
        if (!r->collision) {
            rv->wheel[0] += r->left.out;
            rv->wheel[1] += r->right.out;
        }

        Vec2 last = rv->trail[(rv->trail_head + rv->trail_len - 1) % SCENE_TRAIL_N];
        Vec2 now = { r->pose.x, r->pose.y };
        if (vec_dist(last, now) > 0.05) {
            if (rv->trail_len < SCENE_TRAIL_N) rv->trail_len++;
            else rv->trail_head = (rv->trail_head + 1) % SCENE_TRAIL_N;
            rv->trail[(rv->trail_head + rv->trail_len - 1) % SCENE_TRAIL_N] = now;
        }

        if (r->collisions > rv->collisions) {
            rv->hit = 1;
            double dir = r->pose.dir + (r->vel < 0 ? M_PI : 0);
            fx_collision(fx, now, dir, s->level.params.collision_penalty);
        }
        rv->collisions = r->collisions;
        for (int t = 0; t < s->level.lab.ntargets; t++) {
            if (r->visited[t] && !rv->visited[t])
                fx_target(fx, s->level.lab.targets[t].pos, s->level.params.target_reward);
            rv->visited[t] = r->visited[t];
        }
    }
}

void scene_update(Scene *sc, float dt)
{
    sc->time += dt;
    for (int i = 0; i < SIM_MAX_ROBOTS; i++) {
        RobotVis *rv = &sc->rv[i];
        rv->hit = fmaxf(0, rv->hit - dt * 2.5f);
        rv->speed += (rv->speed_goal - rv->speed) * fminf(1, dt * 6);
        rv->wag += dt * (4 + 8 * rv->speed);
    }
}

Pose scene_pose(const Scene *sc, int r, float a)
{
    const RobotVis *rv = &sc->rv[r];
    return (Pose){
        rv->prev.x + (rv->cur.x - rv->prev.x) * a,
        rv->prev.y + (rv->cur.y - rv->prev.y) * a,
        angle_norm(rv->prev.dir + angle_norm(rv->cur.dir - rv->prev.dir) * a),
    };
}

/* ---- walls -------------------------------------------------------------- */

static void fill_world(const View *v, const Vec2 *pts, int n, double dx, double dy, Color c)
{
    Vector2 sp[2 * SCENE_WALL_PTS];
    for (int i = 0; i < n; i++) sp[i] = view_pt(v, pts[i].x + dx, pts[i].y + dy);
    fill_convex(sp, n, c);
}

static int cmp_vec(const void *a, const void *b)
{
    const Vec2 *p = a, *q = b;
    if (p->x != q->x) return p->x < q->x ? -1 : 1;
    return (p->y > q->y) - (p->y < q->y);
}

static double turn(Vec2 o, Vec2 a, Vec2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }

/* Andrew's monotone chain; sorts pts in place, out needs n + 1 slots */
static int convex_hull(Vec2 *pts, int n, Vec2 *out)
{
    qsort(pts, (size_t)n, sizeof *pts, cmp_vec);
    int k = 0;
    for (int i = 0; i < n; i++) {
        while (k >= 2 && turn(out[k - 2], out[k - 1], pts[i]) <= 0) k--;
        out[k++] = pts[i];
    }
    for (int i = n - 2, lo = k + 1; i >= 0; i--) {
        while (k >= lo && turn(out[k - 2], out[k - 1], pts[i]) <= 0) k--;
        out[k++] = pts[i];
    }
    return k - 1;
}

void scene_prepare(Scene *sc, const Sim *s, View v)
{
    int w = (GetScreenWidth() + SHADOW_DOWN - 1) / SHADOW_DOWN;
    int h = (GetScreenHeight() + SHADOW_DOWN - 1) / SHADOW_DOWN;
    bool same_view = memcmp(&v, &sc->shadow_view, sizeof v) == 0;
    if (!sc->shadow_dirty && same_view && sc->shadow.id && sc->shadow.texture.width == w && sc->shadow.texture.height == h)
        return;
    if (sc->shadow.id && (sc->shadow.texture.width != w || sc->shadow.texture.height != h)) {
        UnloadRenderTexture(sc->shadow);
        sc->shadow.id = 0;
    }
    if (!sc->shadow.id) {
        sc->shadow = LoadRenderTexture(w, h);
        SetTextureFilter(sc->shadow.texture, TEXTURE_FILTER_BILINEAR);
    }
    sc->shadow_view = v;
    sc->shadow_dirty = false;

    View lv = view_scaled(v, SHADOW_DOWN);
    const Lab *lab = &s->level.lab;
    BeginTextureMode(sc->shadow);
    ClearBackground(BLANK);
    for (int i = 1; i < lab->nwalls; i++) {
        if (!sc->nout[i]) continue;
        /* extruded silhouette: hull of the outline and its shadow-shifted copy */
        double d = shadow_len(lab->walls[i].height) * 0.70710678;
        Vec2 pts[2 * SCENE_WALL_PTS], hull[2 * SCENE_WALL_PTS + 1];
        int n = sc->nout[i];
        for (int k = 0; k < n; k++) {
            pts[k] = sc->outline[i][k];
            pts[n + k] = (Vec2){ pts[k].x + d, pts[k].y - d };
        }
        int h = convex_hull(pts, 2 * n, hull);
        fill_world(&lv, hull, h, 0, 0, WHITE);
    }
    EndTextureMode();
}

static void draw_wall_shadows(const Scene *sc, Vector2 shake)
{
    if (!sc->shadow.id) return;
    Texture2D t = sc->shadow.texture;
    Rectangle src = { 0, 0, (float)t.width, -(float)t.height };
    /* cheap blur: 3x3 taps of the bilinear-filtered low-res mask */
    float r = 2.0f;
    for (int i = 0; i < 9; i++) {
        Rectangle dst = { shake.x + (i % 3 - 1) * r, shake.y + (i / 3 - 1) * r,
                          (float)t.width * SHADOW_DOWN, (float)t.height * SHADOW_DOWN };
        DrawTexturePro(t, src, dst, (Vector2){ 0, 0 }, 0, color_alpha(C_SHADOW, 0.065f));
    }
}

static void draw_walls(const Scene *sc, const Lab *lab, const View *v)
{
    for (int i = 1; i < lab->nwalls; i++) {
        if (!sc->nout[i]) continue;
        bool tall = lab->walls[i].height >= sc->tall_h;
        double d = wall_depth(lab->walls[i].height);
        fill_world(v, sc->outline[i], sc->nout[i], 0, 0, tall ? C_TALL_SIDE : C_LOW_SIDE);
        fill_world(v, sc->outline[i], sc->nout[i], 0, d, tall ? C_TALL_TOP : C_LOW_TOP);
    }
}

/* ---- floor -------------------------------------------------------------- */

static Rectangle arena_rect(const View *v, const Lab *lab)
{
    Vector2 tl = view_pt(v, 0, lab->height);
    return (Rectangle){ tl.x, tl.y, (float)lab->width * v->scale, (float)lab->height * v->scale };
}

static void draw_floor(const Scene *sc, const Lab *lab, const View *v)
{
    Rectangle a = arena_rect(v, lab);
    float m = fmaxf(6, 0.25f * v->scale);
    Rectangle fr = { a.x - m, a.y - m, a.width + 2 * m, a.height + 2 * m };
    float round = 2 * m / fminf(fr.width, fr.height);
    DrawRectangleRounded((Rectangle){ fr.x + m * 0.4f, fr.y + m * 0.7f, fr.width, fr.height }, round, 8, (Color){ 0, 0, 0, 90 });
    DrawRectangleRounded(fr, round, 8, C_FRAME);
    DrawRectangleRounded((Rectangle){ fr.x, fr.y, fr.width, fr.height - m * 0.25f }, round, 8, color_mix(C_FRAME, WHITE, 0.12f));
    DrawRectangleRec(a, C_FLOOR);

    for (int x = 1; x < (int)ceil(lab->width); x++) {
        Color c = x % 2 ? C_GRID_MINOR : C_GRID_MAJOR;
        DrawLineV(view_pt(v, x, 0), view_pt(v, x, lab->height), c);
    }
    for (int y = 1; y < (int)ceil(lab->height); y++) {
        Color c = y % 2 ? C_GRID_MINOR : C_GRID_MAJOR;
        DrawLineV(view_pt(v, 0, y), view_pt(v, lab->width, y), c);
    }
    /* University of Aveiro watermark, bottom-right of the floor */
    if (sc->logo.id) {
        float pad = 0.3f * v->scale, h = fminf(1.6f, 0.15f * (float)lab->height) * v->scale;
        float w = h * sc->logo.width / sc->logo.height;
        Rectangle src = { 0, 0, (float)sc->logo.width, (float)sc->logo.height };
        DrawTexturePro(sc->logo, src, (Rectangle){ a.x + a.width - pad - w, a.y + a.height - pad - h, w, h }, (Vector2){ 0 }, 0, C_WATERMARK);
    }
    /* the border casts a soft shadow onto the floor (light from top-left) */
    float e = 0.35f * v->scale;
    Color s0 = color_alpha(C_SHADOW, 0.22f), s1 = color_alpha(C_SHADOW, 0);
    DrawRectangleGradientV((int)a.x, (int)a.y, (int)a.width, (int)e, s0, s1);
    DrawRectangleGradientH((int)a.x, (int)a.y, (int)e, (int)a.height, s0, s1);
}

/* ---- start pads, targets & beacons ------------------------------------- */

/* painted floor marking: dashed ring with a chevron along the start heading */
static void draw_start_pad(const Pose *p, const View *v)
{
    Vector2 c = view_pt(v, p->x, p->y);
    float r = view_len(v, SIM_ROBOT_RADIUS * 1.25);
    DrawCircleV(c, r, color_alpha(C_FRAME, 0.10f));
    for (int k = 0; k < 16; k++)
        DrawRing(c, r - 2, r, k * 22.5f, k * 22.5f + 13.0f, 4, color_alpha(C_FRAME, 0.55f));
    Frame f = frame_at(p->x, p->y, p->dir);
    Vector2 tip[] = {
        frame_pt(v, &f, 0.45, 0), frame_pt(v, &f, 0.1, 0.32), frame_pt(v, &f, 0.1, 0.16),
        frame_pt(v, &f, 0.25, 0), frame_pt(v, &f, 0.1, -0.16), frame_pt(v, &f, 0.1, -0.32),
    };
    /* concave chevron: two convex halves */
    fill_convex((Vector2[]){ tip[0], tip[1], tip[2], tip[3] }, 4, color_alpha(C_FRAME, 0.45f));
    fill_convex((Vector2[]){ tip[0], tip[3], tip[4], tip[5] }, 4, color_alpha(C_FRAME, 0.45f));
}

static void draw_target_area(const Scene *sc, const Sim *s, int t, const View *v)
{
    const Target *tg = &s->level.lab.targets[t];
    bool done = false;
    for (int i = 0; i < s->nrobots; i++) done |= s->robots[i].visited[t];
    Color base = done ? C_DONE : C_CHEESE;
    Vector2 c = view_pt(v, tg->pos.x, tg->pos.y);
    float r = view_len(v, tg->radius);

    float pulse = 0.5f + 0.5f * sinf(sc->time * 3.2f);
    DrawCircleGradient(c, r * (1.05f + 0.1f * pulse), color_alpha(base, 0.30f + 0.15f * pulse), color_alpha(base, 0));
    DrawCircleV(c, r, color_alpha(base, 0.16f));

    float ph = fmodf(sc->time, 1.8f) / 1.8f;
    float rr = r * (0.7f + 0.6f * ease_out(ph));
    DrawRing(c, rr - 1.5f, rr + 1.5f, 0, 360, 48, color_alpha(base, (1 - ph) * 0.6f));

    /* dashed rim, slowly turning */
    float rot = sc->time * 12.0f;
    for (int k = 0; k < 24; k++)
        DrawRing(c, r - 2, r, rot + k * 15.0f, rot + k * 15.0f + 9.0f, 4, color_alpha(color_mix(base, C_INK, 0.25f), 0.8f));
}

static void draw_cheese(const Scene *sc, const Sim *s, int t, const View *v)
{
    const Target *tg = &s->level.lab.targets[t];
    bool done = false;
    for (int i = 0; i < s->nrobots; i++) done |= s->robots[i].visited[t];
    double k = fmin(1.0, tg->radius) * (1.0 + 0.04 * sin(sc->time * 3.2));
    double wob = 0.25 + 0.08 * sin(sc->time * 1.7);
    Frame f = frame_at(tg->pos.x, tg->pos.y, wob);
    float a = done ? 0.45f : 1.0f;

    enum { ARC = 10 };
    const double R = 0.84 * k, half = 28.0 * DEG2RAD_D, ax = -0.42 * k;
    Vector2 top[ARC + 2], side[ARC + 2];
    double lift = 0.08 * k;
    for (int pass = 0; pass < 2; pass++) {
        Vector2 *pts = pass ? top : side;
        Frame g = f;
        if (pass) g.y += lift;
        pts[0] = frame_pt(v, &g, ax, 0);
        for (int i = 0; i <= ARC; i++) {
            double th = -half + 2 * half * i / ARC;
            pts[i + 1] = frame_pt(v, &g, ax + R * cos(th), R * sin(th));
        }
    }
    /* soft drop shadow */
    Frame sh = f;
    sh.x += 0.06; sh.y -= 0.06;
    fill_ellipse(v, &sh, 0.05 * k, 0, 0.45 * k, 0.3 * k, color_alpha(C_SHADOW, 0.18f * a));
    fill_convex(side, ARC + 2, color_alpha(C_RIND, a));
    fill_convex(top, ARC + 2, color_alpha(C_CHEESE, a));
    Frame g = f;
    g.y += lift;
    static const double holes[][3] = { { -0.05, 0.05, 0.07 }, { 0.17, -0.12, 0.06 }, { 0.25, 0.16, 0.045 }, { 0.05, -0.02, 0.03 } };
    for (int i = 0; i < 4; i++)
        fill_ellipse(v, &g, holes[i][0] * k, holes[i][1] * k, holes[i][2] * k, holes[i][2] * k, color_alpha(C_HOLE, a));
    for (int i = 0; i < ARC + 2; i++)
        DrawLineEx(top[i], top[(i + 1) % (ARC + 2)], fmaxf(1, v->scale * 0.025f), color_alpha(color_mix(C_RIND, C_INK, 0.2f), a));
}

static void draw_beacon_base(const Scene *sc, const Beacon *b, const View *v)
{
    Vector2 c = view_pt(v, b->pos.x, b->pos.y);
    for (int k = 0; k < 3; k++) {
        float ph = fmodf(sc->time / 2.4f + k / 3.0f, 1.0f);
        float r = view_len(v, 0.35 + 2.4 * ph);
        DrawRing(c, r - 1.2f, r + 1.2f, 0, 360, 64, color_alpha(C_BEACON, (1 - ph) * 0.55f));
    }
    DrawCircleGradient(c, view_len(v, 0.9), color_alpha(C_BEACON, 0.45f), color_alpha(C_BEACON, 0));
}

static void draw_beacon_pillar(const Scene *sc, const Beacon *b, const View *v)
{
    Vector2 c = view_pt(v, b->pos.x, b->pos.y);
    float flick = 0.85f + 0.15f * sinf(sc->time * 9.0f);
    float hgt = view_len(v, 0.3 * b->height), w = view_len(v, 0.34), wc = view_len(v, 0.1);
    DrawRectangleGradientV((int)(c.x - w / 2), (int)(c.y - hgt), (int)w, (int)hgt,
                           color_alpha(C_BEACON, 0), color_alpha(C_BEACON, 0.55f * flick));
    DrawRectangleGradientV((int)(c.x - wc / 2), (int)(c.y - hgt * 1.1f), (int)wc, (int)(hgt * 1.1f),
                           color_alpha(WHITE, 0), color_alpha(WHITE, 0.8f * flick));
    DrawCircleV(c, view_len(v, 0.2), C_BEACON);
    DrawCircleV(c, view_len(v, 0.11), (Color){ 255, 236, 210, 255 });
}

/* index chip, for beacon(i) when there are several */
static void draw_beacon_label(const Beacon *b, int idx, const View *v)
{
    Vector2 c = view_pt(v, b->pos.x + 0.55, b->pos.y + 0.55);
    DrawCircleV(c, 9, C_BEACON);
    const char *t = TextFormat("%d", idx);
    text_draw(TEXT_BOLD, t, c.x - text_width(TEXT_BOLD, t, 13) / 2, c.y - 9, 13, WHITE);
}

/* ---- overlays ----------------------------------------------------------- */

static void dashed(Vector2 a, Vector2 b, float dash, float thick, Color c)
{
    float dx = b.x - a.x, dy = b.y - a.y, len = sqrtf(dx * dx + dy * dy);
    if (len < 1) return;
    dx /= len; dy /= len;
    for (float t = 0; t < len; t += 2 * dash) {
        float e = fminf(len, t + dash);
        DrawLineEx((Vector2){ a.x + dx * t, a.y + dy * t }, (Vector2){ a.x + dx * e, a.y + dy * e }, thick, c);
    }
}

static void draw_trail(const RobotVis *rv, const View *v, Color c)
{
    for (int i = 1; i < rv->trail_len; i++) {
        Vec2 a = rv->trail[(rv->trail_head + i - 1) % SCENE_TRAIL_N];
        Vec2 b = rv->trail[(rv->trail_head + i) % SCENE_TRAIL_N];
        float age = (float)i / (float)rv->trail_len;
        DrawLineEx(view_pt(v, a.x, a.y), view_pt(v, b.x, b.y), fmaxf(1.5f, v->scale * 0.06f), color_alpha(c, 0.15f + 0.55f * age));
    }
}

static void draw_ir(const Sim *s, int ri, Pose p, const View *v)
{
    const Lab *lab = &s->level.lab;
    for (int k = 0; k < SIM_NUM_IR; k++) {
        double a = s->level.params.ir_angles[k];
        Vec2 o = { p.x + SIM_ROBOT_RADIUS * cos(p.dir + a), p.y + SIM_ROBOT_RADIUS * sin(p.dir + a) };
        double th = p.dir + a;
        double d = sim_ir_distance(s, ri, &p, k);
        bool seen = d >= 0 && d < OVERLAY_MAX;
        double len = seen ? fmax(d, 0.05) : OVERLAY_MAX;
        float near = seen ? (float)fmax(0, 1 - d / 1.5) : 0;
        Color col = color_mix((Color){ 60, 170, 230, 255 }, (Color){ 235, 80, 70, 255 }, near);

        enum { N = 10 };
        Vector2 cone[N + 2];
        cone[0] = view_pt(v, o.x, o.y);
        for (int i = 0; i <= N; i++) {
            double t = th - SIM_IR_HALF_CONE + 2 * SIM_IR_HALF_CONE * i / N;
            cone[i + 1] = view_pt(v, o.x + len * cos(t), o.y + len * sin(t));
        }
        fill_convex(cone, N + 2, color_alpha(col, seen ? 0.22f : 0.08f));
        for (int i = 1; i <= N; i++) DrawLineV(cone[i], cone[i + 1], color_alpha(col, seen ? 0.8f : 0.25f));

        for (int j = -1; j <= 1; j++) {
            double rt = th + j * SIM_IR_HALF_CONE;
            double rd = lab_ray_distance(lab, o, rt, SIM_SOLID_HEIGHT);
            bool hit = rd >= 0 && rd < OVERLAY_MAX;
            double l = hit ? rd : OVERLAY_MAX;
            Vector2 e = view_pt(v, o.x + l * cos(rt), o.y + l * sin(rt));
            DrawLineEx(cone[0], e, 1.2f, color_alpha(col, hit ? 0.7f : 0.2f));
            if (hit) DrawCircleV(e, fmaxf(2, v->scale * 0.05f), col);
        }
    }
}

static void draw_beacon_lines(const Sim *s, int ri, Pose p, const View *v)
{
    const Lab *lab = &s->level.lab;
    const Robot *r = &s->robots[ri];
    Vec2 c = { p.x, p.y };
    Vector2 sc = view_pt(v, c.x, c.y);
    for (int b = 0; b < lab->nbeacons; b++) {
        const Beacon *bc = &lab->beacons[b];
        double ang = vec_angle(c, bc->pos), dist = vec_dist(c, bc->pos);
        double sh = lab_ray_distance(lab, c, ang, bc->height);
        bool blocked = sh >= 0 && sh < dist;
        double l = blocked ? sh : dist;
        Vector2 e = view_pt(v, c.x + l * cos(ang), c.y + l * sin(ang));
        Color col = blocked ? (Color){ 220, 80, 80, 200 } : (Color){ 70, 170, 90, 200 };
        dashed(sc, e, fmaxf(4, v->scale * 0.2f), 2, col);
        if (blocked) {
            float x = fmaxf(4, v->scale * 0.15f);
            DrawLineEx((Vector2){ e.x - x, e.y - x }, (Vector2){ e.x + x, e.y + x }, 2.5f, col);
            DrawLineEx((Vector2){ e.x - x, e.y + x }, (Vector2){ e.x + x, e.y - x }, 2.5f, col);
        }
        if (r->beacon_visible[b]) { /* what the sensor reports (with noise) */
            double ma = p.dir + r->beacon_dir[b] * DEG2RAD_D;
            Vector2 tip = view_pt(v, c.x + 1.2 * cos(ma), c.y + 1.2 * sin(ma));
            Color gold = { 230, 160, 30, 255 };
            DrawLineEx(sc, tip, 3, gold);
            Vector2 l1 = view_pt(v, c.x + 1.0 * cos(ma) + 0.12 * cos(ma + M_PI / 2), c.y + 1.0 * sin(ma) + 0.12 * sin(ma + M_PI / 2));
            Vector2 l2 = view_pt(v, c.x + 1.0 * cos(ma) - 0.12 * cos(ma + M_PI / 2), c.y + 1.0 * sin(ma) - 0.12 * sin(ma + M_PI / 2));
            Vector2 tri[3] = { tip, l1, l2 };
            fill_convex(tri, 3, gold);
        }
    }
}

/* ---- the mouse ---------------------------------------------------------- */

static void draw_wheel(const View *v, const Frame *f, double w, double rolled)
{
    const double L = 0.34, W = 0.12, sp = 0.085;
    Vec2 box[4] = { { -L / 2, w - W / 2 }, { L / 2, w - W / 2 }, { L / 2, w + W / 2 }, { -L / 2, w + W / 2 } };
    double rad[4] = { 0.045, 0.045, 0.045, 0.045 };
    Vec2 out[24];
    int n = rounded_outline(box, 4, rad, 4, out, 24);
    Vector2 sp_[24];
    for (int i = 0; i < n; i++) sp_[i] = frame_pt(v, f, out[i].x, out[i].y);
    fill_convex(sp_, n, (Color){ 48, 46, 58, 255 });
    double off = fmod(rolled, sp);
    if (off < 0) off += sp;
    for (double u = -L / 2 + off; u < L / 2; u += sp) {
        if (u < -L / 2 + 0.02 || u > L / 2 - 0.02) continue;
        DrawLineEx(frame_pt(v, f, u, w - W * 0.38), frame_pt(v, f, u, w + W * 0.38), fmaxf(1, v->scale * 0.025f), (Color){ 110, 108, 122, 255 });
    }
}

static void draw_mouse(const Scene *sc, const Sim *s, int i, Pose p, float alpha, const View *v)
{
    const Robot *r = &s->robots[i];
    const RobotVis *rv = &sc->rv[i];
    Frame f = frame_at(p.x, p.y, p.dir);
    Color fur = color_mix(C_FUR[i], (Color){ 235, 70, 70, 255 }, rv->hit * 0.8f);
    Color dark = color_mix(fur, C_INK, 0.45f);
    double squash = 1.0 - 0.08 * rv->hit;

    Frame sh = f;
    sh.x += 0.07; sh.y -= 0.07;
    fill_ellipse(v, &sh, -0.02, 0, 0.52, 0.46, color_alpha(C_SHADOW, 0.22f));

    /* tail: wags faster when moving */
    double amp = 0.06 + 0.12 * rv->speed, ph = rv->wag;
    enum { TN = 10 };
    Vector2 tail[TN];
    for (int k = 0; k < TN; k++) {
        double t = (double)k / (TN - 1);
        tail[k] = frame_pt(v, &f, -0.38 - 0.62 * t, amp * t * sin(ph - 3.0 * t) + 0.08 * t * t);
    }
    for (int k = 0; k + 1 < TN; k++) {
        float th = view_len(v, 0.07 - 0.05 * k / (TN - 1));
        DrawLineEx(tail[k], tail[k + 1], th, C_PINK);
        DrawCircleV(tail[k + 1], th / 2, C_PINK);
    }

    double wl = rv->wheel_prev[0] + (rv->wheel[0] - rv->wheel_prev[0]) * alpha;
    double wr = rv->wheel_prev[1] + (rv->wheel[1] - rv->wheel_prev[1]) * alpha;
    draw_wheel(v, &f, 0.40, wl);
    draw_wheel(v, &f, -0.40, wr);

    /* body + head, outlined */
    fill_ellipse(v, &f, -0.06, 0, 0.45 * squash, 0.38, dark);
    fill_ellipse(v, &f, 0.22, 0, 0.29, 0.27, dark);
    fill_ellipse(v, &f, -0.06, 0, 0.42 * squash, 0.35, fur);
    fill_ellipse(v, &f, 0.22, 0, 0.26, 0.24, fur);
    fill_ellipse(v, &f, -0.12, 0, 0.26, 0.2, color_mix(fur, WHITE, 0.18f));  /* back highlight */
    fill_ellipse(v, &f, 0.40, 0, 0.12, 0.11, fur);                           /* snout */

    for (int sgn = -1; sgn <= 1; sgn += 2) {
        fill_ellipse(v, &f, 0.10, sgn * 0.25, 0.12, 0.14, dark);
        fill_ellipse(v, &f, 0.10, sgn * 0.25, 0.09, 0.11, C_PINK);
        fill_ellipse(v, &f, 0.32, sgn * 0.11, 0.045, 0.045, C_INK);
        fill_ellipse(v, &f, 0.335, sgn * 0.11 + 0.015, 0.015, 0.015, WHITE);
        for (int k = -1; k <= 1; k++)
            DrawLineEx(frame_pt(v, &f, 0.44, sgn * 0.05), frame_pt(v, &f, 0.48 + 0.03 * k, sgn * 0.30),
                       fmaxf(1, v->scale * 0.018f), color_alpha(C_INK, 0.55f));
    }
    fill_ellipse(v, &f, 0.51, 0, 0.05, 0.05, (Color){ 228, 100, 120, 255 });

    /* LEDs on the back */
    if (r->led_visiting || r->led_returning) {
        Color lc = r->led_returning ? (Color){ 80, 220, 120, 255 } : (Color){ 255, 200, 40, 255 };
        Vector2 lp = frame_pt(v, &f, -0.25, 0);
        DrawCircleGradient(lp, view_len(v, 0.22), color_alpha(lc, 0.7f), color_alpha(lc, 0));
        DrawCircleV(lp, view_len(v, 0.06), lc);
    }
    if (s->nrobots > 1) {
        Vector2 c = view_pt(v, p.x, p.y);
        text_draw(TEXT_BOLD, TextFormat("%d", i + 1), c.x - 4, c.y - view_len(v, 1.0), 15, C_INK);
    }
}

/* ---- frame -------------------------------------------------------------- */

void scene_draw(const Scene *sc, const Sim *s, View v, Vector2 shake, float alpha)
{
    v.ox += shake.x;
    v.oy += shake.y;
    const Lab *lab = &s->level.lab;

    draw_floor(sc, lab, &v);
    Rectangle a = arena_rect(&v, lab); /* floor-level effects stay on the floor */
    BeginScissorMode((int)a.x, (int)a.y, (int)ceilf(a.width), (int)ceilf(a.height));
    for (int i = 0; i < lab->nstarts; i++) draw_start_pad(&lab->starts[i], &v);
    for (int t = 0; t < lab->ntargets; t++) draw_target_area(sc, s, t, &v);
    draw_wall_shadows(sc, shake);
    for (int b = 0; b < lab->nbeacons; b++) draw_beacon_base(sc, &lab->beacons[b], &v);
    EndScissorMode();
    draw_walls(sc, lab, &v);

    if (sc->trail)
        for (int i = 0; i < s->nrobots; i++) draw_trail(&sc->rv[i], &v, color_mix(C_FUR[i], C_INK, 0.35f));
    /* beacons stand under the cheese: one usually marks the target */
    for (int b = 0; b < lab->nbeacons; b++) draw_beacon_pillar(sc, &lab->beacons[b], &v);
    for (int t = 0; t < lab->ntargets; t++) draw_cheese(sc, s, t, &v);
    if (lab->nbeacons > 1)
        for (int b = 0; b < lab->nbeacons; b++) draw_beacon_label(&lab->beacons[b], b, &v);

    for (int i = 0; i < s->nrobots; i++) {
        Pose p = scene_pose(sc, i, alpha);
        if (sc->overlays) { draw_ir(s, i, p, &v); draw_beacon_lines(s, i, p, &v); }
    }
    for (int i = 0; i < s->nrobots; i++) draw_mouse(sc, s, i, scene_pose(sc, i, alpha), alpha, &v);
}

void scene_unload(Scene *sc)
{
    if (sc->shadow.id) UnloadRenderTexture(sc->shadow);
    sc->shadow.id = 0;
    if (sc->logo.id) UnloadTexture(sc->logo);
    sc->logo.id = 0;
}
