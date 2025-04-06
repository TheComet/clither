#pragma once

#include "clither/config.h"
#include "clither/game/cmd.h"

struct bot;
struct camera;
struct gfx;
struct gfx_interface;
struct snake;
struct world;

struct bot_interface
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
     * \brief Create a graphics contet. This will open the window,
     * load all resources (images, sounds, shaders...) and prepare
     * everything for rendering.
     * \return Return a pointer to a new graphics context, or NULL on failure.
     */
    struct bot* (*create)(
        const char*                  script_filepath,
        const struct gfx_interface** igfx,
        struct gfx**                 gfx);

    /*!
     * \brief Destroy the graphics context.
     * Delete all resources, close the window, and clean up.
     */
    void (*destroy)(struct bot* bot);

    int (*next_cmd)(
        struct bot*         bot,
        struct cmd*         next,
        struct cmd          prev,
        const struct world* world,
        const struct snake* snake,
        uint8_t             sim_tick_rate);
};

#if defined(CLITHER_BOT_API)
extern const struct bot_interface* bot_backends[];
#endif
