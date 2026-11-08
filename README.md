# CiberRato Academy

> **Note:** this project is developed with the assistance of AI.

A simplified, modern take on [ciberRatoTools](https://github.com/iris-ua/ciberRatoTools) for the kids of the University of Aveiro's Academia de Verão: program a mouse robot to find the cheese. Built with C99, raylib, and Lua. Runs on desktop and in the browser.

## Desktop

    cmake -S . -B build -G Ninja && cmake --build build
    ./build/cba [levels/sandbox.json] [examples/robot.lua]
    ctest --test-dir build        # simulation tests

With a `.lua` script the robot runs it and reloads whenever the file is saved
(API in `src/bot/bot.h`); without one, drive with the keyboard.

Keys: arrows/WASD drive · Space pause · 0–4 speed (×0.5…×8) · R reset · L reload level · O sensor overlays · T trail.

## Web

Needs [emsdk](https://emscripten.org/docs/getting_started/downloads.html):

    source ~/emsdk/emsdk_env.sh
    emcmake cmake -S . -B build-web -G Ninja && cmake --build build-web
    python3 -m http.server -d build-web 8000   # open http://localhost:8000

The page has a Lua editor (CodeMirror 6, with API autocomplete), a console and
an API cheat-sheet next to the arena. CodeMirror is pre-bundled into
`web/vendor/codemirror.js`; to update it: `cd web/vendor-src && npm install && npm run build`.

## Layout

    src/sim/    simulation core (no graphics): motion, collisions, sensors, level loader
    src/bot/    student Lua scripts: sandbox, coroutine per robot, API, friendly errors
    src/gfx/    rendering: camera/primitives (view), arena + mouse (scene), juice (fx), HUD
    src/main.c  raylib app: input, fixed-tick loop, interpolation
    assets/     fonts (OFL), University of Aveiro logo
    examples/   example robot scripts
    levels/     level JSON (format documented in src/sim/level.h)
    challenges/ challenge JSON + starter Lua (win rules in src/sim/challenge.h)
    web/        web page: shell.html, app.js (editor/console/bridge), app.css, vendor/ CodeMirror
    tests/      headless tests

## Acknowledgments

CiberRato Academy builds on [ciberRatoTools](https://github.com/iris-ua/ciberRatoTools),
the CiberRato Robot Simulation Environment from Universidade de Aveiro / IEETA.
Thanks to the University of Aveiro and to its original authors: Nuno Lau, Artur C. Pereira, Andreia Melo,
António Neves, João Figueiredo, Miguel Rodrigues and Eurico Pedrosa.

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).

The simulation model and `levels/default.json` come from
[ciberRatoTools](https://github.com/iris-ua/ciberRatoTools) (© 2001–2011 Universidade de Aveiro, GPLv2+).
Dependencies: raylib (zlib), Lua (MIT), cJSON (MIT).
Fonts in `assets/fonts/`: Nunito and JetBrains Mono (SIL OFL 1.1, licenses alongside).
`assets/images/ua-logo.png` is the University of Aveiro symbol, a mark of Universidade de Aveiro,
shown as a watermark in recognition; it is not covered by the GPL.
