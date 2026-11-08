/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIM_GEOM_H
#define SIM_GEOM_H

#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define DEG2RAD_D (M_PI / 180.0)
#define RAD2DEG_D (180.0 / M_PI)

#define SIM_MAX_CORNERS 16

typedef struct { double x, y; } Vec2;

/* Closed polygon. Walls with height 0 are ignored by every sensor/collision. */
typedef struct {
    Vec2   pts[SIM_MAX_CORNERS];
    int    n;
    double height;
} Wall;

double angle_norm(double a);                 /* wrap to [-pi, pi] */
double vec_dist(Vec2 a, Vec2 b);
double vec_angle(Vec2 from, Vec2 to);        /* atan2 of to-from */

double seg_point_distance(Vec2 a, Vec2 b, Vec2 p);
/* Distance along ray (p, theta) to segment ab; -1 if not hit. */
double ray_segment_distance(Vec2 p, double theta, Vec2 a, Vec2 b);

double wall_point_distance(const Wall *w, Vec2 p);
double wall_ray_distance(const Wall *w, Vec2 p, double theta);               /* -1 if no hit */
double wall_corner_distance(const Wall *w, Vec2 p, double dir, double margin); /* -1 if none */
bool   wall_contains(const Wall *w, Vec2 p);

#endif
