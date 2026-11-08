
No human reads this file. It is the map, the reuse index, and the scars.

When reporting information to me, be succinct and concise.
If my request is ambiguous, ask when the cost of being wrong is high, decide-and-mention when it's cheap.
Preplan your tool calls, and group in batches where it makes sense, wait for them to all return before reading any.

## Tests

Add a test only when it protects behavior that matters: new behavior, a bug fix,
or a regression not already covered. A useful test fails when that behavior
break and keeps passing when wording, names or internals change.

Don't add tests for non-behavioral edits (message or doc wording, renames,
refactors), for inputs the code can't receive, or for one more case in a guard
whose behavior is already tested.

## Git Commit Messages

Use short, single-line commit messages following Conventional Commits.

**Format:** `<type>: <description>`

**Prefixes:**
- `feat:` New feature
- `fix:` Bug fix
- `docs:` Documentation
- `refactor:` Code restructuring
- `perf:` Performance improvement
- `test:` Tests
- `style:` Formatting
- `build:` Build or dependencies
- `ci:` CI/CD changes
- `chore:` Maintenance

**Rules:**
- Prefer under 50 characters.
- Use imperative mood (e.g., "add", not "added").
- No trailing period.
- Be specific and avoid unnecessary details.
- Add a scope only when useful: `feat(auth): add login`.

## Map / reuse

- `src/sim/` pure sim, no raylib. Overlays query it via `lab_ray_distance`, `sim_ir_distance(s, r, pose, k)`.
- `src/gfx/view.*` camera (`view_fit`, `view_pt`), local frames (`frame_at`/`frame_pt`), `fill_convex` (any winding), `fill_ellipse`, `rounded_outline`.
- `src/gfx/scene.*` arena drawing + per-robot visual state (interp poses, wheels, trail); `scene_after_step` after every `sim_step` (fires fx).
- `src/gfx/fx.*` shake/flash/particles/popups on frame time. `src/gfx/hud.*` `hud_layout` (side or bottom panel) + cards.
- `src/gfx/text.*` all text: `text_draw`/`text_width` with `TEXT_UI`/`TEXT_BOLD` (Nunito) or `TEXT_MONO*` (JetBrains Mono, for changing numbers). Never raw `DrawText`.
- Dev flags: `--shot out.png --frames N --size W H --motors L R --overlays` for headless screenshots.

## Scars

- raylib culls back faces: triangles must be on-screen CCW. Use `fill_convex`, not raw `DrawTriangle`.
- Never `BeginTextureMode` inside a draw pass: build RTs in `scene_prepare` before `BeginDrawing`.
- Wall shadow mask: hull-extruded silhouette at 1/2 res + 3x3 taps. 1/4 res or stacked shifted copies show stair-steps.
- `TakeScreenshot` double-scales on HiDPI (raylib 5.5); `--shot` renders to an RT instead.
- Web: canvas CSS must be 100vw/100vh; raylib's resize callback sets the buffer to the window size.
- CMake de-duplicates repeated link flags: multiple `--preload-file` need `"SHELL:--preload-file ..."`.
