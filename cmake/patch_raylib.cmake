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

# Web: size the canvas to its parent element (not the whole browser window), so
# the page can lay out an editor beside it; the page fires a window 'resize' when
# that element changes size. Under FLAG_WINDOW_HIGHDPI the canvas buffer gets
# devicePixelRatio x that size while the screen stays in CSS pixels (as desktop).
function(patch_once file marker old new)
  file(READ ${file} src)
  string(FIND "${src}" "${marker}" done)
  if(done EQUAL -1)
    string(FIND "${src}" "${old}" at)
    if(at EQUAL -1)
      message(FATAL_ERROR "patch_raylib: ${marker} target not found in ${file}; raylib changed, revisit the patch")
    endif()
    string(REPLACE "${old}" "${new}" src "${src}")
    file(WRITE ${file} "${src}")
  endif()
endfunction()

set(file src/platforms/rcore_web.c)
patch_once(${file} "[ciberrato web-size]"
  "    int width = EM_ASM_INT( return window.innerWidth; );\n    int height = EM_ASM_INT( return window.innerHeight; );\n"
  "    // Size of the canvas's parent element [ciberrato web-size]\n    int width = EM_ASM_INT( var p = Module.canvas.parentElement; return p ? p.clientWidth : window.innerWidth; );\n    int height = EM_ASM_INT( var p = Module.canvas.parentElement; return p ? p.clientHeight : window.innerHeight; );\n")
patch_once(${file} "[ciberrato web-hidpi]"
  "    emscripten_set_canvas_element_size(\"#canvas\", width, height);\n\n    SetupViewport(width, height); // Reset viewport and projection matrix for new size\n\n    CORE.Window.currentFbo.width = width;\n    CORE.Window.currentFbo.height = height;\n"
  "    // Framebuffer in device pixels, screen in CSS pixels [ciberrato web-hidpi]\n    float dpi = GetWindowScaleDPI().x;\n    int fbWidth = (int)(width*dpi + 0.5f), fbHeight = (int)(height*dpi + 0.5f);\n    emscripten_set_canvas_element_size(\"#canvas\", fbWidth, fbHeight);\n\n    SetupViewport(fbWidth, fbHeight); // Reset viewport and projection matrix for new size\n    CORE.Window.screenScale = MatrixScale(dpi, dpi, 1.0f);\n\n    CORE.Window.currentFbo.width = fbWidth;\n    CORE.Window.currentFbo.height = fbHeight;\n")
patch_once(${file} "[ciberrato web-dpi]"
  "    TRACELOG(LOG_WARNING, \"GetWindowScaleDPI() not implemented on target platform\");\n    return (Vector2){ 1.0f, 1.0f };\n"
  "    // devicePixelRatio under FLAG_WINDOW_HIGHDPI [ciberrato web-dpi]\n    float scale = 1.0f;\n    if ((CORE.Window.flags & FLAG_WINDOW_HIGHDPI) > 0) scale = (float)EM_ASM_DOUBLE( return window.devicePixelRatio || 1; );\n    return (Vector2){ scale, scale };\n")
