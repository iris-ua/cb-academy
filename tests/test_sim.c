/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Headless tests for the simulation core. Expected values are hand-computed
 * from the simulation model (noise disabled unless stated). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "sim/sim.h"
#include "sim/level.h"

static int failures, checks;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define NEAR(a, b, eps) do { checks++; double _a = (a), _b = (b); \
    if (fabs(_a - _b) > (eps)) { failures++; \
    printf("  FAIL %s:%d: %s = %.6f, expected %.6f\n", __FILE__, __LINE__, #a, _a, _b); } } while (0)

static Level lv;
static Sim sim;

/* empty W x H box, noise off, robot at (x, y, deg) */
static void box(double w, double h, double x, double y, double deg)
{
    char json[256];
    snprintf(json, sizeof json,
        "{\"width\":%g,\"height\":%g,\"noise\":{\"motors\":0,\"ir\":0,\"compass\":0,\"beacon\":0},"
        "\"start\":[{\"x\":%g,\"y\":%g,\"dir\":%g}]}", w, h, x, y, deg);
    char err[128];
    if (!level_load_json(&lv, json, err, sizeof err)) printf("  box: %s\n", err);
}

static void add_rect_wall(double x0, double y0, double x1, double y1, double h)
{
    Wall *w = &lv.lab.walls[lv.lab.nwalls++];
    w->n = 4; w->height = h;
    w->pts[0] = (Vec2){x0, y0}; w->pts[1] = (Vec2){x1, y0};
    w->pts[2] = (Vec2){x1, y1}; w->pts[3] = (Vec2){x0, y1};
}

static void test_geom(void)
{
    puts("geom");
    Wall sq = { .n = 4, .height = 1, .pts = { {2, -1}, {3, -1}, {3, 1}, {2, 1} } };
    NEAR(wall_point_distance(&sq, (Vec2){0, 0}), 2.0, 1e-12);
    NEAR(wall_point_distance(&sq, (Vec2){4, 2}), sqrt(2.0), 1e-12);
    NEAR(wall_ray_distance(&sq, (Vec2){0, 0}, 0.0), 2.0, 1e-12);
    NEAR(wall_ray_distance(&sq, (Vec2){0, 0}, M_PI), -1.0, 0);
    NEAR(wall_ray_distance(&sq, (Vec2){0, 0}, M_PI / 2), -1.0, 0);
    NEAR(wall_ray_distance(&sq, (Vec2){0, 1}, 0.0), 2.0, 1e-12); /* collinear with top face */
    NEAR(wall_corner_distance(&sq, (Vec2){0, 0}, 0.0, M_PI / 6), sqrt(5.0), 1e-12);
    NEAR(wall_corner_distance(&sq, (Vec2){0, 0}, M_PI, M_PI / 6), -1.0, 0);
    CHECK(wall_contains(&sq, (Vec2){2.5, 0}));
    CHECK(!wall_contains(&sq, (Vec2){0, 0}));
    NEAR(angle_norm(3 * M_PI), M_PI, 1e-12);
    NEAR(angle_norm(-1.5 * M_PI), 0.5 * M_PI, 1e-12);
}

static void test_motors(void)
{
    puts("motors & motion");
    box(20, 20, 5, 5, 0);
    sim_init(&sim, &lv, 1, 1);
    sim_set_motors(&sim, 0, 1.0, -1.0);
    NEAR(sim.robots[0].left.in, SIM_MAX_POWER, 0);
    NEAR(sim.robots[0].right.in, -SIM_MAX_POWER, 0);

    sim_reset(&sim);
    sim_set_motors(&sim, 0, 0.1, 0.1);
    sim_step(&sim);
    NEAR(sim.robots[0].left.out, 0.05, 1e-12);   /* 0.5*in + 0.5*prev */
    NEAR(sim.robots[0].pose.x, 5.05, 1e-12);
    sim_step(&sim);
    NEAR(sim.robots[0].left.out, 0.075, 1e-12);
    NEAR(sim.robots[0].pose.x, 5.125, 1e-12);
    NEAR(sim.robots[0].pose.y, 5.0, 1e-12);

    sim_reset(&sim);
    sim_set_motors(&sim, 0, -0.1, 0.1);           /* spin in place, CCW */
    sim_step(&sim);
    NEAR(sim.robots[0].pose.dir, 0.1, 1e-12);     /* (0.05 - -0.05) / (2R) */
    NEAR(sim.robots[0].pose.x, 5.0, 1e-12);

    /* heading 90 deg moves along +y */
    box(20, 20, 5, 5, 90);
    sim_init(&sim, &lv, 1, 1);
    sim_set_motors(&sim, 0, 0.1, 0.1);
    sim_step(&sim);
    NEAR(sim.robots[0].pose.x, 5.0, 1e-12);
    NEAR(sim.robots[0].pose.y, 5.05, 1e-12);
}

static void test_collision(void)
{
    puts("collision");
    box(10, 10, 9.48, 5, 0);   /* 0.52 from the right border */
    sim_init(&sim, &lv, 1, 1);
    CHECK(!sim.robots[0].bumper);
    sim_set_motors(&sim, 0, 0.1, 0.12);          /* moves 0.055 -> would be 0.465 from wall */
    sim_step(&sim);
    CHECK(sim.robots[0].collision_wall);
    CHECK(sim.robots[0].bumper);
    NEAR(sim.robots[0].pose.x, 9.48, 1e-12);     /* position not committed */
    NEAR(sim.robots[0].pose.dir, 0.01, 1e-12);   /* rotation is */
    CHECK(sim.robots[0].collisions == 1);
    CHECK(sim.robots[0].score == -2);
    sim_step(&sim);                               /* still touching: no new penalty */
    CHECK(sim.robots[0].collisions == 1);

    /* height-0 walls are ignored */
    box(10, 10, 5, 5, 0);
    add_rect_wall(5.3, 0, 5.5, 10, 0.0);
    sim_init(&sim, &lv, 1, 1);
    sim_set_motors(&sim, 0, 0.1, 0.1);
    sim_step(&sim);
    CHECK(!sim.robots[0].collision);
}

static void test_robot_collision(void)
{
    puts("robot vs robot");
    char err[128];
    CHECK(level_load_json(&lv,
        "{\"width\":10,\"height\":10,\"noise\":{\"motors\":0,\"ir\":0,\"compass\":0,\"beacon\":0},"
        "\"start\":[{\"x\":4,\"y\":5,\"dir\":0},{\"x\":5.04,\"y\":5,\"dir\":0}]}", err, sizeof err));
    sim_init(&sim, &lv, 2, 1);
    sim_set_motors(&sim, 0, 0.1, 0.1);   /* chases robot 1 */
    sim_set_motors(&sim, 1, 0.0, 0.0);   /* parked */
    sim_step(&sim);
    CHECK(sim.robots[0].collision_robot);
    CHECK(!sim.robots[1].collision_robot);
    NEAR(sim.robots[0].pose.x, 4.0, 1e-12);
    /* robot 1 is seen by robot 0's front IR: gap = 1.04 - 0.5 - 0.5 = 0.04 -> 1/0.04 = 25 */
    NEAR(sim.robots[0].ir[0], 25.0, 1e-9);
}

static void test_ir(void)
{
    puts("ir sensors");
    box(10, 10, 8, 5, 0);
    sim_init(&sim, &lv, 1, 1);
    Robot *r = &sim.robots[0];
    NEAR(r->ir[0], 0.7, 1e-9);   /* front: 1/1.5 = 0.667 */
    NEAR(r->ir[1], 0.5, 1e-9);   /* +60: ray at +30 hits x=10 at 1.75/cos30 = 2.02 -> 0.495 */
    NEAR(r->ir[2], 0.5, 1e-9);   /* symmetric */
    NEAR(r->ir[3], 0.1, 1e-9);   /* back: 1/7.5 = 0.133 */

    /* touching a wall saturates at 100 */
    box(10, 10, 9.5, 5, 0);
    sim_init(&sim, &lv, 1, 1);
    NEAR(sim.robots[0].ir[0], 100.0, 1e-9);

    /* noise on: values vary but stay near ideal */
    box(10, 10, 8, 5, 0);
    lv.params.noise.ir = 0.1;
    sim_init(&sim, &lv, 1, 42);
    double sum = 0;
    for (int i = 0; i < 400; i++) { sim_step(&sim); sum += sim.robots[0].ir[0]; }
    NEAR(sum / 400, 0.667, 0.03);
}

static void test_compass_beacon_ground(void)
{
    puts("compass, beacon, ground");
    box(10, 10, 5, 5, 90);
    lv.lab.beacons[lv.lab.nbeacons++] = (Beacon){ {5, 8}, 2.0 };
    lv.lab.beacons[lv.lab.nbeacons++] = (Beacon){ {2, 5}, 2.0 };
    sim_init(&sim, &lv, 1, 1);
    Robot *r = &sim.robots[0];
    NEAR(r->compass, 90.0, 0);
    CHECK(r->beacon_visible[0]);
    NEAR(r->beacon_dir[0], 0.0, 0);              /* straight ahead */
    CHECK(r->beacon_visible[1]);
    NEAR(r->beacon_dir[1], 90.0, 0);             /* to the left */

    /* a wall as tall as the beacon blocks it; a lower one does not */
    add_rect_wall(4, 6.5, 6, 6.7, 2.0);
    sim_init(&sim, &lv, 1, 1);
    CHECK(!sim.robots[0].beacon_visible[0]);
    lv.lab.walls[lv.lab.nwalls - 1].height = 1.0;
    sim_init(&sim, &lv, 1, 1);
    CHECK(sim.robots[0].beacon_visible[0]);

    /* aperture */
    lv.params.beacon_aperture = 45 * DEG2RAD_D;
    sim_init(&sim, &lv, 1, 1);
    CHECK(sim.robots[0].beacon_visible[0]);
    CHECK(!sim.robots[0].beacon_visible[1]);

    /* ground: robot must be fully inside the target */
    box(10, 10, 5, 5, 0);
    lv.lab.targets[lv.lab.ntargets++] = (Target){ {5.4, 5}, 1.0 };
    sim_init(&sim, &lv, 1, 1);
    CHECK(sim.robots[0].ground == 0);
    CHECK(sim.robots[0].visited[0]);
    CHECK(sim.robots[0].score == 100);
    lv.lab.targets[0].pos.x = 5.6;
    sim_init(&sim, &lv, 1, 1);
    CHECK(sim.robots[0].ground == -1);
    sim_set_motors(&sim, 0, 0.15, 0.15);
    for (int i = 0; i < 3; i++) sim_step(&sim);
    CHECK(sim.robots[0].ground == 0);
    CHECK(sim.robots[0].visit_cycle[0] > 0);
}

static void test_time_finish_determinism(void)
{
    puts("time limit, finish, determinism");
    box(10, 10, 5, 5, 0);
    lv.params.time_limit = 10;
    sim_init(&sim, &lv, 1, 1);
    for (int i = 0; i < 20; i++) sim_step(&sim);
    CHECK(sim.over);
    CHECK(sim.cycle == 10);

    box(10, 10, 5, 5, 0);
    sim_init(&sim, &lv, 1, 1);
    sim_set_motors(&sim, 0, 0.1, 0.1);
    sim_step(&sim);
    sim_finish(&sim, 0);
    double x = sim.robots[0].pose.x;
    sim_step(&sim);
    NEAR(sim.robots[0].pose.x, x, 0);
    CHECK(sim.over);

    char err[128];
    CHECK(level_load_file(&lv, LEVELS_DIR "/default.json", err, sizeof err));
    static Sim a, b;
    sim_init(&a, &lv, 1, 7);
    sim_init(&b, &lv, 1, 7);
    for (int i = 0; i < 200; i++) {
        sim_set_motors(&a, 0, 0.1, 0.09); sim_step(&a);
        sim_set_motors(&b, 0, 0.1, 0.09); sim_step(&b);
    }
    CHECK(memcmp(&a.robots[0], &b.robots[0], sizeof(Robot)) == 0);
    sim_init(&b, &lv, 1, 8);
    for (int i = 0; i < 200; i++) { sim_set_motors(&b, 0, 0.1, 0.09); sim_step(&b); }
    CHECK(a.robots[0].pose.x != b.robots[0].pose.x);
}

static void test_level(void)
{
    puts("level loading");
    char err[128];
    CHECK(level_load_file(&lv, LEVELS_DIR "/default.json", err, sizeof err));
    CHECK(strcmp(lv.lab.name, "Classic Maze") == 0);
    NEAR(lv.lab.width, 28, 0);
    NEAR(lv.lab.walls[0].pts[2].x, 28, 0);
    NEAR(lv.lab.walls[0].pts[2].y, 14, 0);
    CHECK(lv.lab.nwalls > 50);
    CHECK(lv.lab.nbeacons == 1 && lv.lab.ntargets == 1 && lv.lab.nstarts == 1);
    NEAR(lv.lab.starts[0].x, 9, 0);
    NEAR(lv.params.noise.motors, 1.5, 0);
    /* robot starts in free space */
    sim_init(&sim, &lv, 1, 1);
    CHECK(!sim.robots[0].collision);

    /* maze rows: '|' at col 2 of row pos 0 -> vertical wall at x=2, y in [0,2] */
    Lab lab = {0};
    CHECK(level_add_maze_row(&lab, 0, "  |", 4));
    CHECK(lab.nwalls == 1);
    NEAR(lab.walls[0].pts[0].x, 1.9, 1e-12);
    NEAR(lab.walls[0].pts[2].x, 2.1, 1e-12);
    NEAR(lab.walls[0].pts[2].y, 2.0, 1e-12);
    /* "--+--+  +--" on odd row 1: runs [0,4] and [6,8] at y=2 */
    lab.nwalls = 0;
    CHECK(level_add_maze_row(&lab, 1, "--+--+  +--", 4));
    CHECK(lab.nwalls == 2);
    NEAR(lab.walls[0].pts[0].x, 0.0, 1e-12);
    NEAR(lab.walls[0].pts[1].x, 4.0, 1e-12);
    NEAR(lab.walls[0].pts[2].y, 2.1, 1e-12);
    NEAR(lab.walls[1].pts[0].x, 6.0, 1e-12);
    NEAR(lab.walls[1].pts[1].x, 8.0, 1e-12);
    /* even-row '-' are floor lines: no walls */
    lab.nwalls = 0;
    CHECK(level_add_maze_row(&lab, 2, "--+--", 4));
    CHECK(lab.nwalls == 0);

    CHECK(!level_load_json(&lv, "{\"width\":0}", err, sizeof err));
    CHECK(!level_load_json(&lv, "{nope", err, sizeof err));
    CHECK(!level_load_json(&lv, "{\"width\":5,\"height\":5,\"ir_angles\":[1,2]}", err, sizeof err));
}

int main(void)
{
    test_geom();
    test_motors();
    test_collision();
    test_robot_collision();
    test_ir();
    test_compass_beacon_ground();
    test_time_finish_determinism();
    test_level();
    printf("%d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
