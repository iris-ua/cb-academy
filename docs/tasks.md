# CiberRato Academy — Tasks

Simplified, modern take on [ciberRatoTools](https://github.com/iris-ua/ciberRatoTools).
Stack: C99 + raylib + embedded Lua 5.4. Desktop + Web (Emscripten/WASM).
Audience: students in grades 10–12.

## Sim core (taken from the original)

Units: 1 robot diameter = 1.0. Cycle = 50 ms. Each tick runs, in order: actions → next pose → collisions → commit → sensors → score.

- **Robot**: circle, `R = 0.5`. Has 4 IR sensors at angles `0, +60°, -60°, 180°`, sitting on the rim.
- **Motors**: input is clamped to `[-0.15, 0.15]`. Output `out = (0.5*in + 0.5*prev) * N(1, σm)`, with `σm = 1.5%`.
- **Motion**: `v = (l+r)/2`, `rot = (r-l)/(2R)`. Then `x += v cosθ`, `y += v sinθ`, `θ += rot`, and θ is wrapped to `[-π, π]`.
- **Collision**: if any wall is closer than `R` to the next center, only θ is committed, not x/y. Robot-vs-robot collisions count only when that robot is moving toward the other one.
- **IR**: `d` = the smallest of 3 rays (`θ`, `θ±30°`) and the corner distance inside a ±30° cone. The reading is `1/d` (100 if `d<0.01`) plus `N(0, 0.1)`, rounded to 0.1.
- **Compass**: heading in degrees plus `N(0, 2°)`, rounded.
- **Beacon**: angle to the beacon relative to the heading, plus `N(0, 2°)`. It is hidden when a wall taller than the beacon blocks the line to it.
- **Ground**: the target index when the robot is fully inside it (`dist ≤ r - R`), otherwise -1.
- **Bumper**: the collision flag.
- **Lab**: `W×H` (e.g. 28×14). Walls are polygons with a height. Each beacon has a height, each target is a circle, and the grid holds start poses.
- **Score**: collision −2, target +100, home +100, plus time penalties.

This project is GPL-2.0-or-later, the same as the original, so its code and labs can be reused.

## Tasks

### M0 — Skeleton
1. [x] Repo layout: `src/`, `web/`, `levels/`, `docs/`. CMake build that fetches raylib and Lua.
2. [x] Desktop window that draws an empty arena.
3. [x] Emscripten build that produces `build-web/index.html` (from the `web/shell.html` template) and runs in a browser.

### M1 — Simulation (no graphics dependency)
4. [x] Data types: `Robot`, `Wall`, `Beacon`, `Target`, `Lab`, `Sim`. Seedable RNG plus `randNormal`.
5. [x] Motor model and motion.
6. [x] Point-to-polygon distance, ray-to-wall distance, corner-cone distance.
7. [x] Collision check and commit.
8. [x] Sensors: IR, compass, beacon (with occlusion), ground, bumper.
9. [x] `sim_step()` with a fixed 50 ms tick, separate from the render rate.
10. [x] Level loader using a small JSON format (walls, beacons, targets, start pose, sensor noise, time limit). It also accepts the original ASCII `maze` rows.
11. [x] Headless unit tests for motion, collision and IR against hand-computed values.

### M2 — Modern visuals
12. [x] Camera that fits the arena and keeps its aspect ratio (the canvas resizes).
13. [x] Floor with a subtle grid, soft walls with shadow/AO and rounded corners.
14. [x] Mouse sprite (vector drawing): body, ears, tail, and wheels that animate with motor power. Heading is obvious at a glance.
15. [x] Cheese target with a pulse glow. Beacon as a light pillar with radial rings.
16. [x] Debug overlays (toggle): IR cones/rays, beacon line, trail.
17. [x] Juice: collision shake/flash, particles when the target is reached, smooth interpolation between ticks.
18. [x] HUD: time, score, sensor readouts, LED state.

### M3 — Lua robot API
19. [x] Embed Lua and sandbox it (no `io`/`os`/`require`). Limit instructions per tick with `lua_sethook`.
20. [x] Run the student script in a coroutine. Every action yields one tick.
21. [x] Low-level API: `motors(l, r)`, `wait()`, `ir(i)`, `compass()`, `beacon()`, `ground()`, `bump()`, `time()`, `led(name, on)`, `finish()`, `say(msg)`.
22. [x] High-level helpers (built on 21): `forward(n)`, `turn(deg)`, `turn_to(deg)`.
23. [x] Errors show a friendly message with the line number and do not crash the sim.
24. [x] Desktop: load `robot.lua` from argv and hot-reload it.

### M4 — Browser editor
25. [x] Layout: editor on the left, canvas on the right, controls (Run / Stop / Reset / Speed ×1 ×2 ×4).
26. [x] CodeMirror 6 with Lua highlighting and autocomplete for the API.
27. [x] JS↔WASM bridge: `load_level(json)`, `run(code)`, `stop()`, plus callbacks for `print`/errors/events.
28. [x] Console panel for `say`/`print` and error output.
29. [x] API cheat-sheet side panel.

### M5 — Challenges
30. [x] Challenge format: level JSON plus `title`, `goal` (markdown), `starter.lua`, a win condition, optional hints, and a time limit.
31. [x] Win conditions: `reach_target`, `return_home`, `no_collisions`, `under_time`, `visit_all`. They can be combined.
32. [x] Success screen with stars (time and collisions), then **Next**.
33. [x] Challenge map: unlocked ones in order, completed ones can be replayed. Code and best score are saved per challenge (localStorage).
34. [x] Reset-to-starter button. Export/import progress as a JSON file.

### M6 — Challenge set (progressive)
35. [x] Drive forward to the cheese (motors only).
36. [x] Turn and drive (`turn`, `forward`).
37. [x] Stop before the wall (`ir(0)`).
38. [x] Follow a corridor (left/right IR).
39. [x] Find the cheese with the beacon.
40. [x] Small maze: wall following.
41. [x] Bumper recovery (noise on).
42. [x] Reach the cheese, then return home.
43. [x] Full noisy maze, scored like the original C1.

### M7 — Polish
44. [x] Sound effects (squeak, bump, win) with a mute toggle.
45. [x] Language toggle for UI and challenge text (PT/EN).
46. [x] Teacher mode: unlock all challenges, load a custom level JSON.
47. [x] Simple level editor (place walls, cheese and beacon; export JSON).
48. [x] Deploy the web build as static files (GitHub Pages).
