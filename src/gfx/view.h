/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GFX_VIEW_H
#define GFX_VIEW_H

/* Camera (world units -> screen pixels, sim y up -> screen y down) and
 * small drawing primitives shared by the scene, fx and HUD. */

#include "raylib.h"
#include "sim/geom.h"

typedef struct { float ox, oy, scale, lab_h; } View; /* (ox, oy) = screen pos of world (0, lab_h) */

/* Largest view that fits a lab_w x lab_h arena in area (keeping aspect), centred, pad px inside. */
View    view_fit(Rectangle area, double lab_w, double lab_h, float pad);
View    view_scaled(View v, float k);          /* same view drawn into a target k times smaller */
Vector2 view_pt(const View *v, double x, double y);
float   view_len(const View *v, double d);

/* Local frame: u forward along dir, w to the left (world CCW). */
typedef struct { double x, y, c, s; } Frame;
Frame   frame_at(double x, double y, double dir);
Vector2 frame_pt(const View *v, const Frame *f, double u, double w);

/* Filled convex polygon in screen space, any winding. */
void fill_convex(const Vector2 *pts, int n, Color c);
/* Filled ellipse in a local frame (semi-axes a along u, b along w). */
void fill_ellipse(const View *v, const Frame *f, double cu, double cw, double a, double b, Color c);

/* Outline of a convex polygon with corner i rounded by radius[i] (0 = sharp); returns point count. */
int  rounded_outline(const Vec2 *pts, int n, const double *radius, int seg, Vec2 *out, int max);

Color color_mix(Color a, Color b, float t);
Color color_alpha(Color c, float a);           /* multiply alpha */
float ease_out(float t);

#endif
