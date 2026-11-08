
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

## Git 

Never change branch without asking.
Never commit without human authorization.

### Commit Messages

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
- `src/bot/` Lua robot scripts, no raylib. API doc in `bot.h`. Call `bot_tick` before each `sim_step`; motors/wait/finish yield one tick, sensors don't. Helpers (`forward`/`turn`) live in the Lua `PRELUDE` and use a noise-free motor model (`P.speed`).
- `src/gfx/view.*` camera (`view_fit`, `view_pt`), local frames (`frame_at`/`frame_pt`), `fill_convex` (any winding), `fill_ellipse`, `rounded_outline`.
- `src/gfx/scene.*` arena drawing + per-robot visual state (interp poses, wheels, trail); `scene_after_step` after every `sim_step` (fires fx).
- `src/gfx/fx.*` shake/flash/particles/popups on frame time. `src/gfx/hud.*` `hud_layout` (side or bottom panel) + cards. Panel is laid out in logical units and drawn under an rlgl scale (`hud_scale`, 1.2-1.5 by screen size); `text_set_scale` keeps atlases sharp.
- `src/gfx/text.*` all text: `text_draw`/`text_width` with `TEXT_UI`/`TEXT_BOLD` (Nunito) or `TEXT_MONO*` (JetBrains Mono, for changing numbers). Never raw `DrawText`.
- `web/` page around the canvas. `app.js` holds the API table (autocomplete + cheat-sheet: update it with `bot.h`; descriptions are `api.<name>` in `i18n.js`). Bridge: `web_*` `EMSCRIPTEN_KEEPALIVE` in `main.c`, called via `Module.ccall`; C->JS via `js_emit(kind, text, n)` -> `Module.onEvent` (print/error/state/speed/paused/level/finished/result/sound/ready). `web/vendor/codemirror.js` is a committed esbuild bundle from `web/vendor-src`. New page files go in `WEB_PAGE_FILES` (CMake) and the Pages workflow copy list.
- i18n: `web/i18n.js` (`I18N.t`, `data-i18n`/`data-i18n-title` on static HTML, `I18N.pick(d, key)` reads a JSON's `"pt"` block). C side: `web_set_lang` -> `hud_set_lang` (`L(en, pt)` in `hud.c`) and `tr()` in `main.c` for judge reasons. Lua error messages stay English.
- `web/sound.js` WebAudio-synth sfx (`Sfx.play('squeak'|'bump'|'win'|'fail')`), driven by `sound` events from `main.c` `sound_cues`. Web only.
- `web/leveledit.js` level editor overlay (`LevelEdit.open({current, onPlay})`); plays as the single custom stage (`ciberrato.custom`). Teacher mode (`ciberrato.teacher`, toggle shown with `?teacher`) unlocks all + loads a level file as the custom stage.
- Deploy: `.github/workflows/pages.yml` (emsdk pinned, Pages source must be "GitHub Actions").
- Challenges: `challenges/NN-id.json` = level JSON + `title`/`goal`/`starter`/`hints` and `pt: {title, goal, hints, starter: NN-id.pt.lua}` (page only) + `win`/`stars` (C). File order = play order, id = basename. `src/sim/challenge.*` parses/judges the win rule (`stop_on_target`: judged when the run ends); `main.c` `judge()` freezes the run and emits `result` (JSON). Each challenge needs `tests/solutions/<id>.lua` that wins it on 12 seeds (`test_challenges [dir] [id-prefix]`; tune stars from its tick range). Page: `ciberrato.progress`, `ciberrato.code.<id>`, `ciberrato.current` in localStorage.
- Dev flags: `*.lua` arg = robot script, `--shot out.png --frames N --size W H --motors L R --overlays` for headless screenshots.

## Scars

- raylib culls back faces: triangles must be on-screen CCW. Use `fill_convex`, not raw `DrawTriangle`.
- Never `BeginTextureMode` inside a draw pass: build RTs in `scene_prepare` before `BeginDrawing`.
- Wall shadow mask: hull-extruded silhouette at 1/2 res + 3x3 taps. 1/4 res or stacked shifted copies show stair-steps.
- `TakeScreenshot` double-scales on HiDPI (raylib 5.5); `--shot` renders to an RT instead.
- Web: canvas CSS must be 100vw/100vh; raylib's resize callback sets the buffer to the window size.
- CMake de-duplicates repeated link flags: multiple `--preload-file` need `"SHELL:--preload-file ..."`.
- raylib 5.5 HiDPI resize bug (X11/Windows): screen size became pixels -> layout 2x too big. Patched via `cmake/patch_raylib.cmake` (FetchContent PATCH_COMMAND); drop it when upgrading raylib.
- Web canvas sizing: patched raylib sizes it to `#stage` (canvas parent), not the window; JS fires a window `resize` on ResizeObserver. HiDPI is patched in too (buffer = CSS size x devicePixelRatio, screen stays CSS px; stock raylib web was blurry). Mouse coords would be in device px if ever used.
- Emscripten GLFW listens for keys on window in the *capture* phase (and preventDefaults Backspace/Tab): `shell.html` wraps `window.addEventListener` so keydown/keypress from `.ui` elements never reach raylib.
- Web page files are copied by the `web_page` target every build; POST_BUILD only ran when the wasm relinked.
- `pkill -f <pattern>` from Bash matches its own shell's command line and kills it: use `pgrep`+`kill` with a narrow pattern.
