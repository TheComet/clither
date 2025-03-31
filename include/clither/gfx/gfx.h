#pragma once

#include "clither/config.h"
#include "clither/game/q.h"

struct camera;
struct command;
struct input;
struct resource_pack;
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
     * \brief Create a graphics contet. This will open the window,
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
     * \brief Map user input into a "snake command structure", also known as a
     * "command frame".
     *
     * The command structure stores the world-space target angle of the snake
     * head as well as the target speed. These values need to be calculated by
     * transforming the snake head into screen space (or mouse coordinates into
     * world space).
     *
     * \note Very important: Check command.h struct command: Due to network
     * optimizations, when calculating new command you must limit the speed at
     * which the "angle" and "speed" properties are allowed to change. This
     * limitation allows commands to be delta-compressed more efficiently.
     *
     * \param[in] gfx Graphics context.
     * \param[in] input Raw user input.
     * \param[in] cam Camera information required for transformation.
     * \param[in] cmd The previously calculated command from the previous
     * frame.
     * \param[in] snake_head Snake's head position in world space.
     * \return The new command. Make sure to use @see cmd_make()
     */
    struct cmd (*next_cmd)(
        const struct gfx*    gfx,
        const struct input*  input,
        const struct camera* cam,
        struct cmd           prev,
        struct qwpos         snake_head);

    /*!
     * \brief Advance sprite animations. This is called at a frequency of
     * sim_tick_rate.
     */
    void (*step_anim)(struct gfx* gfx, int sim_tick_rate);

    /*!
     * \brief Draw everything.
     */
    void (*draw_world)(
        struct gfx*          gfx,
        const struct world*  world,
        const struct camera* camera);
};

#if defined(CLITHER_GFX)
extern const struct gfx_interface* gfx_backends[];
#endif
