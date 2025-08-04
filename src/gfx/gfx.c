#include "clither/gfx/gfx.h"
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
    NULL};

/* ------------------------------------------------------------------------- */
static int switch_backends(
    const struct gfx_interface** igfx,
    struct gfx**                 gfx,
    const struct resource_pack*  pack,
    int                          direction)
{
    int count;
    int idx, new_idx;

    for (count = 0; gfx_backends[count]; ++count)
        ;
    for (idx = 0; gfx_backends[idx]; ++idx)
        if (*igfx == gfx_backends[idx])
            break;

    if (direction > 0)
        new_idx = idx + 1 >= count ? 0 : idx + 1;
    else
        new_idx = idx - 1 < 0 ? count - 1 : idx - 1;

    /*
     * On Windows it is possible to create a new backend and then
     * destroy the previous backend, however, on linux this doesn't
     * seem to work. GL contexts aren't properly transferred to the
     * new instance. This is why we destroy first - then create
     */
    (*igfx)->unload_resource_pack(*gfx, pack);
    (*igfx)->destroy(*gfx);
    (*igfx)->deinit();

    *igfx = gfx_backends[new_idx];
    if ((*igfx)->init() < 0)
        goto init_new_gfx_failed;
    *gfx = (*igfx)->create(640, 480);
    if (*gfx == NULL)
        goto create_new_gfx_failed;
    if ((*igfx)->load_resource_pack(*gfx, pack) < 0)
        goto load_new_resource_pack_failed;

    return 0;

load_new_resource_pack_failed:
    (*igfx)->destroy(*gfx);
create_new_gfx_failed:
    (*igfx)->deinit();
init_new_gfx_failed:
    /* Try to restore to previous backend. Shouldn't fail but who knows
     */
    (*igfx) = gfx_backends[idx];
    if ((*igfx)->init() < 0)
        return -1;
    *gfx = (*igfx)->create(640, 480);
    if (*gfx == NULL)
        return -1;
    if ((*igfx)->load_resource_pack(*gfx, pack) < 0)
        return -1;

    return -1;
}

/* ------------------------------------------------------------------------- */
int gfx_next_backend(
    const struct gfx_interface** igfx,
    struct gfx**                 gfx,
    struct resource_pack*        pack)
{
    return switch_backends(igfx, gfx, pack, 1);
}

/* ------------------------------------------------------------------------- */
int gfx_prev_backend(
    const struct gfx_interface** igfx,
    struct gfx**                 gfx,
    struct resource_pack*        pack)
{
    return switch_backends(igfx, gfx, pack, 1);
}
