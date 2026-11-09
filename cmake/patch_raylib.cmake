# Idempotent fixes applied to the fetched raylib 6.2 sources (run in its source dir).
#
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
  "    int width = EM_ASM_INT( return window.innerWidth; );\n    int height = EM_ASM_INT( return window.innerHeight; );\n\n    if (width < "
  "    // Size of the canvas's parent element [ciberrato web-size]\n    int width = EM_ASM_INT( var p = Module.canvas.parentElement; return p ? p.clientWidth : window.innerWidth; );\n    int height = EM_ASM_INT( var p = Module.canvas.parentElement; return p ? p.clientHeight : window.innerHeight; );\n\n    if (width < ")
patch_once(${file} "[ciberrato web-hidpi]"
  "    emscripten_set_canvas_element_size(platform.canvasId, width, height);\n\n    SetupViewport(width, height); // Reset viewport and projection matrix for new size\n\n    CORE.Window.currentFbo.width = width;\n    CORE.Window.currentFbo.height = height;\n    CORE.Window.resizedLastFrame = true;\n\n    if (IsWindowFullscreen()) return 1;\n\n    // Set current screen size\n    CORE.Window.screen.width = width;\n    CORE.Window.screen.height = height;\n\n    glfwSetWindowSize(platform.handle, CORE.Window.screen.width, CORE.Window.screen.height);\n"
  "    // Framebuffer in device pixels, screen in CSS pixels [ciberrato web-hidpi]\n    // GLFW first: its size callback resets the canvas buffer and divides the screen by DPI\n    if (!IsWindowFullscreen()) glfwSetWindowSize(platform.handle, width, height);\n\n    float dpi = FLAG_IS_SET(CORE.Window.flags, FLAG_WINDOW_HIGHDPI)? GetWindowScaleDPI().x : 1.0f;\n    int fbWidth = (int)(width*dpi + 0.5f), fbHeight = (int)(height*dpi + 0.5f);\n    emscripten_set_canvas_element_size(platform.canvasId, fbWidth, fbHeight);\n\n    SetupViewport(fbWidth, fbHeight); // Reset viewport and projection matrix for new size\n    CORE.Window.screenScale = MatrixScale(dpi, dpi, 1.0f);\n\n    CORE.Window.currentFbo.width = fbWidth;\n    CORE.Window.currentFbo.height = fbHeight;\n    CORE.Window.resizedLastFrame = true;\n\n    if (IsWindowFullscreen()) return 1;\n\n    CORE.Window.screen.width = width;\n    CORE.Window.screen.height = height;\n")
