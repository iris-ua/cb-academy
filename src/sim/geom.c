/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "geom.h"
#include <math.h>

#define EPS 1e-12

double angle_norm(double a)
{
    while (a > M_PI) a -= 2.0 * M_PI;
    while (a < -M_PI) a += 2.0 * M_PI;
    return a;
}

double vec_dist(Vec2 a, Vec2 b) { return hypot(b.x - a.x, b.y - a.y); }
double vec_angle(Vec2 from, Vec2 to) { return atan2(to.y - from.y, to.x - from.x); }

static double cross(double ax, double ay, double bx, double by) { return ax * by - ay * bx; }

double seg_point_distance(Vec2 a, Vec2 b, Vec2 p)
{
    double ex = b.x - a.x, ey = b.y - a.y;
    double len2 = ex * ex + ey * ey;
    if (len2 < EPS) return vec_dist(a, p);
    double t = ((p.x - a.x) * ex + (p.y - a.y) * ey) / len2;
    if (t < 0.0) t = 0.0;
    else if (t > 1.0) t = 1.0;
    Vec2 q = { a.x + t * ex, a.y + t * ey };
    return vec_dist(q, p);
}

double ray_segment_distance(Vec2 p, double theta, Vec2 a, Vec2 b)
{
    double dx = cos(theta), dy = sin(theta);
    double ex = b.x - a.x, ey = b.y - a.y;
    double apx = a.x - p.x, apy = a.y - p.y;
    double denom = cross(dx, dy, ex, ey);

    if (fabs(denom) < EPS) {
        /* parallel: only a hit when collinear, then nearest endpoint in front */
        if (fabs(cross(apx, apy, dx, dy)) > 1e-9) return -1.0;
        double ta = apx * dx + apy * dy;
        double tb = (b.x - p.x) * dx + (b.y - p.y) * dy;
        if (ta < 0 && tb < 0) return -1.0;
        if (ta < 0) return tb;
        if (tb < 0) return ta;
        return ta < tb ? ta : tb;
    }
    double t = cross(apx, apy, ex, ey) / denom;
    double u = cross(apx, apy, dx, dy) / denom;
    if (t < 0.0 || u < 0.0 || u > 1.0) return -1.0;
    return t;
}

double wall_point_distance(const Wall *w, Vec2 p)
{
    double min = INFINITY;
    for (int i = 0, j = w->n - 1; i < w->n; j = i++) {
        double d = seg_point_distance(w->pts[j], w->pts[i], p);
        if (d < min) min = d;
    }
    return min;
}

double wall_ray_distance(const Wall *w, Vec2 p, double theta)
{
    double min = -1.0;
    for (int i = 0, j = w->n - 1; i < w->n; j = i++) {
        double d = ray_segment_distance(p, theta, w->pts[j], w->pts[i]);
        if (d >= 0.0 && (min < 0.0 || d < min)) min = d;
    }
    return min;
}

double wall_corner_distance(const Wall *w, Vec2 p, double dir, double margin)
{
    double min = -1.0;
    for (int i = 0; i < w->n; i++) {
        double delta = fabs(angle_norm(vec_angle(p, w->pts[i]) - dir));
        if (delta > margin) continue;
        double d = vec_dist(p, w->pts[i]);
        if (min < 0.0 || d < min) min = d;
    }
    return min;
}

bool wall_contains(const Wall *w, Vec2 p)
{
    bool inside = false;
    for (int i = 0, j = w->n - 1; i < w->n; j = i++) {
        Vec2 a = w->pts[i], b = w->pts[j];
        if ((a.y > p.y) != (b.y > p.y) &&
            p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)
            inside = !inside;
    }
    return inside;
}
