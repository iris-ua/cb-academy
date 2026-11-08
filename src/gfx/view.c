/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "view.h"
#include <math.h>

View view_fit(Rectangle area, double lab_w, double lab_h, float pad)
{
    float w = area.width - 2 * pad, h = area.height - 2 * pad;
    float s = fminf(w / (float)lab_w, h / (float)lab_h);
    if (s < 1) s = 1;
    View v = {
        area.x + pad + (w - s * (float)lab_w) / 2,
        area.y + pad + (h - s * (float)lab_h) / 2,
        s, (float)lab_h,
    };
    return v;
}

View view_scaled(View v, float k)
{
    v.ox /= k;
    v.oy /= k;
    v.scale /= k;
    return v;
}

Vector2 view_pt(const View *v, double x, double y)
{
    return (Vector2){ v->ox + (float)x * v->scale, v->oy + (v->lab_h - (float)y) * v->scale };
}

float view_len(const View *v, double d) { return (float)d * v->scale; }

Frame frame_at(double x, double y, double dir) { return (Frame){ x, y, cos(dir), sin(dir) }; }

Vector2 frame_pt(const View *v, const Frame *f, double u, double w)
{
    return view_pt(v, f->x + u * f->c - w * f->s, f->y + u * f->s + w * f->c);
}

void fill_convex(const Vector2 *p, int n, Color c)
{
    for (int i = 1; i + 1 < n; i++) {
        Vector2 a = p[i], b = p[i + 1];
        float cr = (a.x - p[0].x) * (b.y - p[0].y) - (a.y - p[0].y) * (b.x - p[0].x);
        if (cr < 0) DrawTriangle(p[0], a, b, c); /* raylib wants on-screen CCW */
        else DrawTriangle(p[0], b, a, c);
    }
}

void fill_ellipse(const View *v, const Frame *f, double cu, double cw, double a, double b, Color c)
{
    enum { N = 28 };
    Vector2 pts[N];
    for (int i = 0; i < N; i++) {
        double t = 2.0 * M_PI * i / N;
        pts[i] = frame_pt(v, f, cu + a * cos(t), cw + b * sin(t));
    }
    fill_convex(pts, N, c);
}

int rounded_outline(const Vec2 *pts, int n, const double *radius, int seg, Vec2 *out, int max)
{
    double area = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) area += pts[j].x * pts[i].y - pts[i].x * pts[j].y;
    int m = 0;
    for (int i = 0; i < n && m < max; i++) {
        Vec2 p = pts[(i + n - 1) % n], v = pts[i], q = pts[(i + 1) % n];
        double ax = p.x - v.x, ay = p.y - v.y, bx = q.x - v.x, by = q.y - v.y;
        double la = hypot(ax, ay), lb = hypot(bx, by);
        double turn = ay * bx - ax * by; /* (v-p) x (q-v) */
        bool convex = area > 0 ? turn > 0 : turn < 0;
        double r = radius ? radius[i] : 0;
        if (r <= 0 || la < 1e-9 || lb < 1e-9 || !convex) { out[m++] = v; continue; }
        ax /= la; ay /= la; bx /= lb; by /= lb;
        double half = acos(fmax(-1.0, fmin(1.0, ax * bx + ay * by))) / 2.0;
        if (half < 1e-3) { out[m++] = v; continue; }
        double t = r / tan(half);
        double tmax = 0.5 * fmin(la, lb);
        if (t > tmax) { t = tmax; r = t * tan(half); }
        double hx = ax + bx, hy = ay + by, hl = hypot(hx, hy);
        double cd = r / sin(half);
        Vec2 c = { v.x + hx / hl * cd, v.y + hy / hl * cd };
        double a0 = atan2(v.y + ay * t - c.y, v.x + ax * t - c.x);
        double a1 = atan2(v.y + by * t - c.y, v.x + bx * t - c.x);
        double sweep = angle_norm(a1 - a0);
        for (int k = 0; k <= seg && m < max; k++) {
            double a = a0 + sweep * k / seg;
            out[m++] = (Vec2){ c.x + r * cos(a), c.y + r * sin(a) };
        }
    }
    return m;
}

Color color_mix(Color a, Color b, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t),
    };
}

Color color_alpha(Color c, float a)
{
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    c.a = (unsigned char)(c.a * a);
    return c;
}

float ease_out(float t) { return 1 - (1 - t) * (1 - t); }
