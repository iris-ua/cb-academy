/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "level.h"
#include "cJSON.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CELL       2.0
#define WALL_HALF  0.1
#define BORDER_H   4.0

static bool fail(char *err, size_t n, const char *fmt, ...)
{
    if (err && n) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(err, n, fmt, ap);
        va_end(ap);
    }
    return false;
}

static bool add_rect(Lab *lab, double x0, double y0, double x1, double y1, double h)
{
    if (lab->nwalls >= SIM_MAX_WALLS) return false;
    Wall *w = &lab->walls[lab->nwalls++];
    w->n = 4;
    w->height = h;
    w->pts[0] = (Vec2){ x0, y0 };
    w->pts[1] = (Vec2){ x1, y0 };
    w->pts[2] = (Vec2){ x1, y1 };
    w->pts[3] = (Vec2){ x0, y1 };
    return true;
}

static bool flush_run(Lab *lab, int pos, int start, int last, double h)
{
    /* odd rows: walls between cell rows; even rows: floor lines (height 0) -> skipped */
    if (start < 0 || pos % 2 == 0) return true;
    double y = (pos + 1) * 0.5 * CELL;
    return add_rect(lab, (start / 3.0) * CELL, y - WALL_HALF, (last / 3.0 + 1.0) * CELL, y + WALL_HALF, h);
}

bool level_add_maze_row(Lab *lab, int pos, const char *pat, double h)
{
    int len = (int)strlen(pat);
    int start = -1, last = -1; /* current horizontal run, in cell-aligned columns */

    for (int col = 0; col < len; col++) {
        char c = pat[col];
        if (c == '|') {
            double x = ((col + 1) / 3.0) * CELL;
            if (!add_rect(lab, x - WALL_HALF, pos * 0.5 * CELL, x + WALL_HALF, (pos * 0.5 + 1.0) * CELL, h))
                return false;
        } else if (col % 3 == 0) {
            if (c == '-') {
                if (start < 0) start = col;
                last = col;
            } else {
                if (!flush_run(lab, pos, start, last, h)) return false;
                start = -1;
            }
        }
    }
    return flush_run(lab, pos, start, last, h);
}

static double num(const cJSON *obj, const char *key, double def)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(it) ? it->valuedouble : def;
}

static bool read_point(const cJSON *arr, Vec2 *out)
{
    if (!cJSON_IsArray(arr) || cJSON_GetArraySize(arr) != 2) return false;
    const cJSON *x = cJSON_GetArrayItem(arr, 0), *y = cJSON_GetArrayItem(arr, 1);
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y)) return false;
    *out = (Vec2){ x->valuedouble, y->valuedouble };
    return true;
}

bool level_load_json(Level *lv, const char *json, char *err, size_t errlen)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        const char *at = cJSON_GetErrorPtr();
        return fail(err, errlen, "invalid JSON near: %.30s", at ? at : "?");
    }
    bool ok = false;
    Lab *lab = &lv->lab;
    SimParams *p = &lv->params;
    memset(lab, 0, sizeof *lab);
    level_default_params(p);

    if (!cJSON_IsObject(root)) { fail(err, errlen, "level must be a JSON object"); goto done; }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    snprintf(lab->name, sizeof lab->name, "%s", cJSON_IsString(name) ? name->valuestring : "Unnamed");

    lab->width = num(root, "width", 0);
    lab->height = num(root, "height", 0);
    if (lab->width <= 0 || lab->height <= 0) { fail(err, errlen, "width and height must be > 0"); goto done; }

    /* border (walls[0]) */
    Wall *b = &lab->walls[lab->nwalls++];
    b->n = 4;
    b->height = BORDER_H;
    b->pts[0] = (Vec2){ 0, 0 };
    b->pts[1] = (Vec2){ 0, lab->height };
    b->pts[2] = (Vec2){ lab->width, lab->height };
    b->pts[3] = (Vec2){ lab->width, 0 };

    p->time_limit = (int)num(root, "time_limit", p->time_limit);
    p->collision_penalty = (int)num(root, "collision_penalty", p->collision_penalty);
    p->target_reward = (int)num(root, "target_reward", p->target_reward);
    p->beacon_aperture = num(root, "beacon_aperture", p->beacon_aperture * RAD2DEG_D) * DEG2RAD_D;

    const cJSON *noise = cJSON_GetObjectItemCaseSensitive(root, "noise");
    if (cJSON_IsObject(noise)) {
        p->noise.motors = num(noise, "motors", p->noise.motors);
        p->noise.ir = num(noise, "ir", p->noise.ir);
        p->noise.compass_deg = num(noise, "compass", p->noise.compass_deg);
        p->noise.beacon_deg = num(noise, "beacon", p->noise.beacon_deg);
    }

    const cJSON *ira = cJSON_GetObjectItemCaseSensitive(root, "ir_angles");
    if (ira) {
        if (!cJSON_IsArray(ira) || cJSON_GetArraySize(ira) != SIM_NUM_IR) {
            fail(err, errlen, "ir_angles must have %d numbers", SIM_NUM_IR);
            goto done;
        }
        for (int i = 0; i < SIM_NUM_IR; i++) {
            const cJSON *a = cJSON_GetArrayItem(ira, i);
            if (!cJSON_IsNumber(a)) { fail(err, errlen, "ir_angles[%d] not a number", i); goto done; }
            p->ir_angles[i] = a->valuedouble * DEG2RAD_D;
        }
    }

    const cJSON *maze = cJSON_GetObjectItemCaseSensitive(root, "maze");
    if (maze) {
        if (!cJSON_IsArray(maze)) { fail(err, errlen, "maze must be an array of strings"); goto done; }
        double mh = num(root, "maze_wall_height", BORDER_H);
        int n = cJSON_GetArraySize(maze);
        for (int i = 0; i < n; i++) {
            const cJSON *row = cJSON_GetArrayItem(maze, i);
            if (!cJSON_IsString(row)) { fail(err, errlen, "maze[%d] not a string", i); goto done; }
            if (!level_add_maze_row(lab, n - 1 - i, row->valuestring, mh)) {
                fail(err, errlen, "too many walls (max %d)", SIM_MAX_WALLS);
                goto done;
            }
        }
    }

    const cJSON *it;
    int i = 0;
    cJSON_ArrayForEach(it, cJSON_GetObjectItemCaseSensitive(root, "walls")) {
        if (lab->nwalls >= SIM_MAX_WALLS) { fail(err, errlen, "too many walls (max %d)", SIM_MAX_WALLS); goto done; }
        Wall *w = &lab->walls[lab->nwalls];
        w->height = num(it, "height", BORDER_H);
        w->n = 0;
        const cJSON *pt;
        cJSON_ArrayForEach(pt, cJSON_GetObjectItemCaseSensitive(it, "corners")) {
            if (w->n >= SIM_MAX_CORNERS) { fail(err, errlen, "walls[%d]: max %d corners", i, SIM_MAX_CORNERS); goto done; }
            if (!read_point(pt, &w->pts[w->n++])) { fail(err, errlen, "walls[%d]: corner must be [x, y]", i); goto done; }
        }
        if (w->n < 2) { fail(err, errlen, "walls[%d]: needs at least 2 corners", i); goto done; }
        lab->nwalls++;
        i++;
    }

    cJSON_ArrayForEach(it, cJSON_GetObjectItemCaseSensitive(root, "beacons")) {
        if (lab->nbeacons >= SIM_MAX_BEACONS) { fail(err, errlen, "too many beacons"); goto done; }
        lab->beacons[lab->nbeacons++] = (Beacon){ { num(it, "x", 0), num(it, "y", 0) }, num(it, "height", BORDER_H) };
    }
    cJSON_ArrayForEach(it, cJSON_GetObjectItemCaseSensitive(root, "targets")) {
        if (lab->ntargets >= SIM_MAX_TARGETS) { fail(err, errlen, "too many targets"); goto done; }
        lab->targets[lab->ntargets++] = (Target){ { num(it, "x", 0), num(it, "y", 0) }, num(it, "radius", 1.0) };
    }
    cJSON_ArrayForEach(it, cJSON_GetObjectItemCaseSensitive(root, "start")) {
        if (lab->nstarts >= SIM_MAX_ROBOTS) { fail(err, errlen, "too many start positions"); goto done; }
        lab->starts[lab->nstarts++] = (Pose){ num(it, "x", 0), num(it, "y", 0), num(it, "dir", 0) * DEG2RAD_D };
    }
    ok = true;

done:
    cJSON_Delete(root);
    return ok;
}

bool level_load_file(Level *lv, const char *path, char *err, size_t errlen)
{
    FILE *f = fopen(path, "rb");
    if (!f) return fail(err, errlen, "cannot open %s", path);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return fail(err, errlen, "cannot read %s", path); }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return fail(err, errlen, "out of memory"); }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    bool ok = level_load_json(lv, buf, err, errlen);
    free(buf);
    return ok;
}
