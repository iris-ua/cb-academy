# CiberRato Jr

A simplified, modern take on [ciberRatoTools](https://github.com/iris-ua/ciberRatoTools): program a mouse robot to find the cheese. Built with C99, raylib, and Lua. Runs on desktop and in the browser.

## Desktop

    cmake -S . -B build -G Ninja && cmake --build build
    ./build/ciberrato [levels/sandbox.json]
    ctest --test-dir build        # simulation tests

Keys: arrows/WASD drive · Space pause · 1–4 speed · R reset · L reload level.

## Web

Needs [emsdk](https://emscripten.org/docs/getting_started/downloads.html):

    source ~/emsdk/emsdk_env.sh
    emcmake cmake -S . -B build-web -G Ninja && cmake --build build-web
    python3 -m http.server -d build-web 8000   # open http://localhost:8000

## Layout

    src/sim/    simulation core (no graphics): motion, collisions, sensors, level loader
    src/main.c  raylib app
    levels/     level JSON (format documented in src/sim/level.h)
    web/        HTML shell for the web build
    tests/      headless tests

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).

The simulation model and `levels/default.json` come from
[ciberRatoTools](https://github.com/iris-ua/ciberRatoTools) (© 2001–2011 Universidade de Aveiro, GPLv2+).
Dependencies: raylib (zlib), Lua (MIT), cJSON (MIT).
