/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "sim.h"
#include <math.h>
#include <string.h>

#define R SIM_ROBOT_RADIUS
#define IR_HALF_CONE (M_PI / 6.0)

void level_default_params(SimParams *p)
{
    p->time_limit        = 1800;
    p->noise.motors      = 1.5;
    p->noise.ir          = 0.1;
    p->noise.compass_deg = 2.0;
    p->noise.beacon_deg  = 2.0;
    p->ir_angles[0] = 0.0;
    p->ir_angles[1] = M_PI / 3.0;
    p->ir_angles[2] = -M_PI / 3.0;
    p->ir_angles[3] = M_PI;
    p->beacon_aperture   = M_PI;
    p->collision_penalty = 2;
    p->target_reward     = 100;
}

/* ---- lab queries (min_h: only walls with height >= min_h count) -------- */

static double lab_point_distance(const Lab *lab, Vec2 p, double min_h)
{
    double min = INFINITY;
    for (int i = 0; i < lab->nwalls; i++) {
        if (lab->walls[i].height < min_h) continue;
        double d = wall_point_distance(&lab->walls[i], p);
        if (d < min) min = d;
    }
    return min;
}

static double lab_ray_distance(const Lab *lab, Vec2 p, double dir, double min_h)
{
    double min = -1.0;
    for (int i = 0; i < lab->nwalls; i++) {
        if (lab->walls[i].height < min_h) continue;
        double d = wall_ray_distance(&lab->walls[i], p, dir);
        if (d >= 0.0 && (min < 0.0 || d < min)) min = d;
    }
    return min;
}

static double lab_corner_distance(const Lab *lab, Vec2 p, double dir, double margin)
{
    double min = -1.0;
    for (int i = 0; i < lab->nwalls; i++) {
        if (lab->walls[i].height < SIM_SOLID_HEIGHT) continue;
        double d = wall_corner_distance(&lab->walls[i], p, dir, margin);
        if (d >= 0.0 && (min < 0.0 || d < min)) min = d;
    }
    return min;
}

static double min_pos(double a, double b) /* min of two "maybe -1" distances */
{
    if (b < 0.0) return a;
    if (a < 0.0) return b;
    return a < b ? a : b;
}

static Vec2 center(const Pose *p) { return (Vec2){ p->x, p->y }; }

/* ---- motors & motion --------------------------------------------------- */

static double clamp_power(double p)
{
    if (p > SIM_MAX_POWER) return SIM_MAX_POWER;
    if (p < -SIM_MAX_POWER) return -SIM_MAX_POWER;
    return p;
}

static double motor_out(Motor *m, Sim *s)
{
    double k = rng_normal(&s->rng, 100.0, s->level.params.noise.motors) / 100.0;
    m->out = (0.5 * m->in + 0.5 * m->out) * k;
    return m->out;
}

static void compute_next(Sim *s, Robot *r)
{
    double l = motor_out(&r->left, s);
    double rr = motor_out(&r->right, s);
    r->vel = (l + rr) / 2.0;
    double rot = (rr - l) / (2.0 * R);
    double th = r->pose.dir;
    r->next.x = r->pose.x + r->vel * cos(th);
    r->next.y = r->pose.y + r->vel * sin(th);
    r->next.dir = angle_norm(th + rot);
}

static bool moving_towards(const Robot *r, Vec2 p)
{
    Vec2 c = center(&r->pose);
    if (c.x == p.x && c.y == p.y) return false;
    double a = fabs(angle_norm(vec_angle(c, p) - r->pose.dir));
    if (r->vel > 0) return a < M_PI / 2.0;
    if (r->vel < 0) return a > M_PI / 2.0;
    return false;
}

static void check_collisions(Sim *s)
{
    for (int i = 0; i < s->nrobots; i++) {
        Robot *r = &s->robots[i];
        r->collision_wall = lab_point_distance(&s->level.lab, center(&r->next), SIM_SOLID_HEIGHT) < R;
        r->collision_robot = false;
    }
    for (int i = 0; i < s->nrobots; i++) {
        for (int j = i + 1; j < s->nrobots; j++) {
            Robot *a = &s->robots[i], *b = &s->robots[j];
            if (vec_dist(center(&a->next), center(&b->next)) >= 2.0 * R) continue;
            if (moving_towards(a, center(&b->pose))) a->collision_robot = true;
            if (moving_towards(b, center(&a->pose))) b->collision_robot = true;
        }
    }
    for (int i = 0; i < s->nrobots; i++) {
        Robot *r = &s->robots[i];
        r->collision = r->collision_wall || r->collision_robot;
    }
}

static void commit(Robot *r)
{
    if (r->finished) return;
    if (r->collision) r->pose.dir = r->next.dir; /* blocked: rotate only */
    else r->pose = r->next;
}

/* ---- sensors ----------------------------------------------------------- */

static double ir_distance(const Sim *s, int ri, int k)
{
    const Robot *r = &s->robots[ri];
    const Lab *lab = &s->level.lab;
    double a = s->level.params.ir_angles[k];
    Vec2 p = { r->pose.x + R * cos(r->pose.dir + a), r->pose.y + R * sin(r->pose.dir + a) };
    double th = angle_norm(r->pose.dir + a);

    double min = lab_ray_distance(lab, p, th, SIM_SOLID_HEIGHT);
    min = min_pos(min, lab_ray_distance(lab, p, th + IR_HALF_CONE, SIM_SOLID_HEIGHT));
    min = min_pos(min, lab_ray_distance(lab, p, th - IR_HALF_CONE, SIM_SOLID_HEIGHT));
    min = min_pos(min, lab_corner_distance(lab, p, th, IR_HALF_CONE));

    /* other robots inside the sensor cone */
    for (int o = 0; o < s->nrobots; o++) {
        if (o == ri) continue;
        Vec2 oc = center(&s->robots[o].pose);
        double D = vec_dist(p, oc);
        double d;
        if (D <= R) {
            d = 0.0;
        } else {
            double delta = fabs(angle_norm(vec_angle(p, oc) - th));
            double margin = asin(R / D);
            if (delta > IR_HALF_CONE + margin) continue;
            if (delta <= IR_HALF_CONE) {
                d = D - R;
            } else {
                double psi = delta - IR_HALF_CONE;
                double l2 = D * sin(psi);
                d = D * cos(psi) - sqrt(R * R - l2 * l2);
            }
        }
        min = min_pos(min, d);
    }
    return min;
}

static double ir_value(Sim *s, double d)
{
    double ideal;
    if (d < 0.0) ideal = 0.0;          /* nothing seen */
    else if (d > 0.01) ideal = 1.0 / d;
    else ideal = 100.0;
    double v = ideal + rng_normal(&s->rng, 0.0, s->level.params.noise.ir);
    return floor(10.0 * v + 0.5) / 10.0;
}

static void update_sensors(Sim *s, int ri)
{
    Robot *r = &s->robots[ri];
    const Lab *lab = &s->level.lab;
    const SimParams *pp = &s->level.params;
    Vec2 c = center(&r->pose);

    for (int k = 0; k < SIM_NUM_IR; k++)
        r->ir[k] = ir_value(s, ir_distance(s, ri, k));

    double cv = angle_norm(r->pose.dir + rng_normal(&s->rng, 0.0, pp->noise.compass_deg * DEG2RAD_D));
    r->compass = floor(cv * RAD2DEG_D + 0.5);

    for (int b = 0; b < lab->nbeacons; b++) {
        const Beacon *bc = &lab->beacons[b];
        double ang = vec_angle(c, bc->pos);
        double shadow = lab_ray_distance(lab, c, ang, bc->height);
        r->beacon_visible[b] = false;
        if (shadow >= 0.0 && shadow < vec_dist(c, bc->pos)) continue;
        double v = angle_norm(ang - r->pose.dir + rng_normal(&s->rng, 0.0, pp->noise.beacon_deg * DEG2RAD_D));
        if (fabs(v) > pp->beacon_aperture) continue;
        r->beacon_visible[b] = true;
        r->beacon_dir[b] = floor(v * RAD2DEG_D + 0.5);
    }

    r->ground = -1;
    for (int t = 0; t < lab->ntargets; t++)
        if (vec_dist(lab->targets[t].pos, c) <= lab->targets[t].radius - R) r->ground = t;

    r->bumper = r->collision;
}

static void update_score(Sim *s, Robot *r)
{
    if (r->finished) return;
    if (r->collision && !r->collision_prev) {
        r->collisions++;
        r->score -= s->level.params.collision_penalty;
    }
    r->collision_prev = r->collision;
    if (r->ground >= 0 && !r->visited[r->ground]) {
        r->visited[r->ground] = true;
        r->visit_cycle[r->ground] = s->cycle;
        r->score += s->level.params.target_reward;
    }
}

/* ---- public API -------------------------------------------------------- */

void sim_init(Sim *s, const Level *lv, int nrobots, uint64_t seed)
{
    if (&s->level != lv) s->level = *lv;
    if (nrobots < 1) nrobots = 1;
    if (nrobots > SIM_MAX_ROBOTS) nrobots = SIM_MAX_ROBOTS;
    s->nrobots = nrobots;
    s->cycle = 0;
    s->over = false;
    s->seed = seed;
    rng_seed(&s->rng, seed);

    const Lab *lab = &s->level.lab;
    for (int i = 0; i < nrobots; i++) {
        Robot *r = &s->robots[i];
        memset(r, 0, sizeof *r);
        r->pose = lab->nstarts > 0 ? lab->starts[i % lab->nstarts]
                                   : (Pose){ lab->width / 2.0, lab->height / 2.0, 0.0 };
        r->next = r->pose;
        r->ground = -1;
    }
    for (int i = 0; i < nrobots; i++) update_sensors(s, i);
    for (int i = 0; i < nrobots; i++) update_score(s, &s->robots[i]); /* starting on a target counts */
}

void sim_reset(Sim *s) { sim_init(s, &s->level, s->nrobots, s->seed); }

void sim_step(Sim *s)
{
    if (s->over) return;
    s->cycle++;
    for (int i = 0; i < s->nrobots; i++) compute_next(s, &s->robots[i]);
    check_collisions(s);
    for (int i = 0; i < s->nrobots; i++) commit(&s->robots[i]);
    for (int i = 0; i < s->nrobots; i++) update_sensors(s, i);
    for (int i = 0; i < s->nrobots; i++) update_score(s, &s->robots[i]);

    bool all_done = true;
    for (int i = 0; i < s->nrobots; i++) all_done &= s->robots[i].finished;
    int limit = s->level.params.time_limit;
    if (all_done || (limit > 0 && s->cycle >= limit)) s->over = true;
}

void sim_set_motors(Sim *s, int r, double left, double right)
{
    if (r < 0 || r >= s->nrobots || s->robots[r].finished) return;
    s->robots[r].left.in = clamp_power(left);
    s->robots[r].right.in = clamp_power(right);
}

void sim_set_led(Sim *s, int r, bool visiting, bool returning)
{
    if (r < 0 || r >= s->nrobots) return;
    s->robots[r].led_visiting = visiting;
    s->robots[r].led_returning = returning;
}

void sim_finish(Sim *s, int r)
{
    if (r < 0 || r >= s->nrobots) return;
    Robot *rb = &s->robots[r];
    rb->finished = true;
    rb->left.in = rb->right.in = 0.0;
}
