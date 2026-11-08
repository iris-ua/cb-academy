/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GFX_TEXT_H
#define GFX_TEXT_H

/* Text with the bundled fonts (assets/fonts, OFL): Nunito for UI, JetBrains Mono
 * for numbers. Atlases are rasterised lazily per size step and HiDPI scale.
 * Falls back to raylib's default font if the files are missing. */

#include "raylib.h"

typedef enum { TEXT_UI, TEXT_BOLD, TEXT_MONO, TEXT_MONO_BOLD, TEXT_KINDS } TextKind;

void  text_draw(TextKind k, const char *s, float x, float y, float size, Color c);
float text_width(TextKind k, const char *s, float size);
void  text_unload(void);

#endif
