#include "clither/gfx.h"
#include <stddef.h>

#if defined(CLITHER_GFX_SDL)
extern const struct gfx_interface gfx_sdl;
#endif
#if defined(CLITHER_GFX_GLES2)
extern const struct gfx_interface gfx_gles2;
#endif
#if defined(CLITHER_GFX_VULKAN)
extern const struct gfx_interface gfx_vulkan;
#endif

const struct gfx_interface* gfx_backends[] = {
#if defined(CLITHER_GFX_GLES2)
    &gfx_gles2,
#endif
#if defined(CLITHER_GFX_VULKAN)
    &gfx_vulkan,
#endif
#if defined(CLITHER_GFX_SDL)
    &gfx_sdl,
#endif
    NULL
};
