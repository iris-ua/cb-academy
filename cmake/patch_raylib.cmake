# Idempotent fixes applied to the fetched raylib 5.5 sources (run in its source dir).
#
# HiDPI resize (Windows/X11): the GLFW size callback gets pixels but raylib stored
# them as the logical screen size while still scaling drawing by the DPI factor,
# so after any resize the layout was 2x too big and only a quarter was visible.
set(file src/platforms/rcore_desktop_glfw.c)
file(READ ${file} src)
set(old "    // Set current screen size\n\n    CORE.Window.screen.width = width;\n    CORE.Window.screen.height = height;\n")
set(new "    // Set current screen size (logical units under HighDPI) [ciberrato patch]\n#if !defined(__APPLE__)\n    if ((CORE.Window.flags & FLAG_WINDOW_HIGHDPI) > 0)\n    {\n        Vector2 scale = GetWindowScaleDPI();\n        width = (int)(width/scale.x);\n        height = (int)(height/scale.y);\n    }\n#endif\n    CORE.Window.screen.width = width;\n    CORE.Window.screen.height = height;\n")
string(FIND "${src}" "[ciberrato patch]" done)
if(done EQUAL -1)
  string(FIND "${src}" "${old}" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "patch_raylib: WindowSizeCallback not found; raylib changed, revisit the patch")
  endif()
  string(REPLACE "${old}" "${new}" src "${src}")
  file(WRITE ${file} "${src}")
endif()
