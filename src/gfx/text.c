/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "text.h"
#include <math.h>
#include <stddef.h>

static const char *FILES[TEXT_KINDS] = {
    "Nunito-SemiBold.ttf", "Nunito-ExtraBold.ttf", "JetBrainsMono-Medium.ttf", "JetBrainsMono-Bold.ttf",
};
/* atlas pixel sizes; a request uses the next step up and is scaled down */
static const int STEPS[] = { 12, 16, 20, 24, 32, 40, 48, 64, 96 };
#define NSTEPS (int)(sizeof STEPS / sizeof STEPS[0])

static struct {
    unsigned char *data;
    int            len;
    bool           tried;
    Font           fonts[NSTEPS];
} kinds[TEXT_KINDS];

static bool load_data(TextKind k)
{
    if (kinds[k].tried) return kinds[k].data != NULL;
    kinds[k].tried = true;
    const char *paths[] = {
        TextFormat("assets/fonts/%s", FILES[k]),
        TextFormat("%sassets/fonts/%s", GetApplicationDirectory(), FILES[k]),
    };
    for (int i = 0; i < 2 && !kinds[k].data; i++)
        if (FileExists(paths[i])) kinds[k].data = LoadFileData(paths[i], &kinds[k].len);
    if (!kinds[k].data) TraceLog(LOG_WARNING, "TEXT: %s not found, using default font", FILES[k]);
    return kinds[k].data != NULL;
}

static Font get_font(TextKind k, float size)
{
    if (!load_data(k)) return GetFontDefault();
    Vector2 dpi = GetWindowScaleDPI();
    float px = size * fmaxf(1.0f, dpi.x);
    int i = 0;
    while (i < NSTEPS - 1 && STEPS[i] < px) i++;
    Font *f = &kinds[k].fonts[i];
    if (f->texture.id == 0) {
        int cps[95 + 96], n = 0;
        for (int c = 32; c <= 126; c++) cps[n++] = c;
        for (int c = 160; c <= 255; c++) cps[n++] = c; /* Latin-1: PT accents */
        *f = LoadFontFromMemory(".ttf", kinds[k].data, kinds[k].len, STEPS[i], cps, n);
        SetTextureFilter(f->texture, TEXTURE_FILTER_BILINEAR);
    }
    return *f;
}

void text_draw(TextKind k, const char *s, float x, float y, float size, Color c)
{
    DrawTextEx(get_font(k, size), s, (Vector2){ roundf(x), roundf(y) }, size, 0, c);
}

float text_width(TextKind k, const char *s, float size) { return MeasureTextEx(get_font(k, size), s, size, 0).x; }

void text_unload(void)
{
    for (int k = 0; k < TEXT_KINDS; k++) {
        for (int i = 0; i < NSTEPS; i++)
            if (kinds[k].fonts[i].texture.id) UnloadFont(kinds[k].fonts[i]);
        if (kinds[k].data) UnloadFileData(kinds[k].data);
        kinds[k].data = NULL;
        kinds[k].tried = false;
    }
}
