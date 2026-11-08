/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIM_LEVEL_H
#define SIM_LEVEL_H

#include <stddef.h>
#include "sim.h"

/* Level JSON (all keys optional except width/height):
 * {
 *   "name": "Maze", "width": 28, "height": 14,
 *   "time_limit": 1800,
 *   "noise": { "motors": 1.5, "ir": 0.1, "compass": 2, "beacon": 2 },
 *   "ir_angles": [0, 60, -60, 180],              // degrees
 *   "beacon_aperture": 180,                      // degrees
 *   "collision_penalty": 2, "target_reward": 100,
 *   "maze": ["  |  ...", ...],                   // original Row patterns, top row first
 *   "maze_wall_height": 4,
 *   "walls":   [ { "height": 4, "corners": [[x,y], ...] } ],
 *   "beacons": [ { "x": 25, "y": 7, "height": 4 } ],
 *   "targets": [ { "x": 25, "y": 7, "radius": 1 } ],
 *   "start":   [ { "x": 9, "y": 3, "dir": 0 } ]  // dir in degrees
 * }
 * Maze cells are 2x2; each string covers 3 chars per cell. Even rows hold '|'
 * walls, odd rows hold '-' walls between cell rows (same as ciberRatoTools).
 */
bool level_load_json(Level *lv, const char *json, char *err, size_t errlen);
bool level_load_file(Level *lv, const char *path, char *err, size_t errlen);

/* Exposed for tests: append maze row walls (pos = original Row Pos). */
bool level_add_maze_row(Lab *lab, int pos, const char *pattern, double height);

#endif
