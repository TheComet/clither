#pragma once

#include "clither/config.h"
#include "clither/game/q.h"

struct camera;
struct command;
struct input;
struct resource_pack;
struct ui;
struct world;

/*! Opaque type. This is implemented differently depending on the graphics
 * backend */
struct gfx;

struct gfx_interface
{
    const char* name;

    /*!
     * \brief Initialize global data here. Called once before create().
     * \return Returns 0 on success, or a negative value for failure.
     */
    int (*init)(void);

    /*!
     * \brief Clean up global data here. Called after destroy().
     */
    void (*deinit)(void);

    /*!
     * \brief Create a graphics context. This will open the window,
     * load all resources (images, sounds, shaders...) and prepare
     * everything for rendering.
     * \return Return a pointer to a new graphics context, or NULL on failure.
     */
    struct gfx* (*create)(int initial_width, int initial_height);

    /*!
     * \brief Destroy the graphics context.
     * Delete all resources, close the window, and clean up.
     */
    void (*destroy)(struct gfx* gfx);

    /*!
     * \brief Load resources from a resource pack.
     * \return 0 if successful. -1 if an error occurred.
     */
    int (*load_resource_pack)(
        struct gfx* gfx, const struct resource_pack* pack);

    /*!
     * \brief Unload a previously loaded resource pack.
     */
    void (*unload_resource_pack)(
        struct gfx* gfx, const struct resource_pack* pack);

    /*!
     * \brief Poll for mouse and keyboard input and fill in the "input"
     * structure. See struct input in input.h for more details.
     */
    void (*poll_input)(struct gfx* gfx, struct input* input);

    /*!
     * \brief Advance sprite animations. This is called at a frequency of
     * sim_tick_rate.
     */
    void (*step_anim)(struct gfx* gfx, int sim_tick_rate);

    void (*draw_begin)(struct gfx* gfx);
    void (*draw_world)(
        struct gfx*          gfx,
        const struct world*  world,
        const struct camera* camera);
    void (*draw_ui)(struct gfx* gfx, const struct ui* ui);
    void (*draw_end)(struct gfx* gfx);

#if defined(CLITHER_GFX_DEBUG)
    void (*draw_debug_circle)(
        struct gfx* gfx, const struct qwpos pos, qw radius, uint32_t rgba);
#endif
};

#if defined(CLITHER_GFX)

extern const struct gfx_interface* gfx_backends[];

int gfx_prev_backend(
    const struct gfx_interface** igfx,
    struct gfx**                 gfx,
    struct resource_pack*        pack);

int gfx_next_backend(
    const struct gfx_interface** igfx,
    struct gfx**                 gfx,
    struct resource_pack*        pack);

#endif
