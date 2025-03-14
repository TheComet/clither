#pragma once

#include "clither/config.h"
#if defined(CLITHER_GFX)

#    include "clither/q.h"

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
     * \brief Global init. Called once at program start.
     * \return Returns 0 on success, or a negative value for failure.
     */
    int (*global_init)(void);

    /*!
     * \brief Global de-init. Called once at program exit.
     */
    void (*global_deinit)(void);

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
     * \note This function can be called more than once. For example, when
     * switching resource packs. \return Return 0 if successful, -1 if an error
     * occurred.
     */
    int (*load_resource_pack)(
        struct gfx* gfx, const struct resource_pack* pack);

    /*!
     * \brief Poll for mouse and keyboard input. See struct input in input.h
     * for more details.
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
     * \param[in] command The previously calculated command from the previous
     * frame. \param[in] input Raw user input. \param[in] gfx Graphics context.
     * \param[in] cam Camera information required for transformation.
     * \param[in] snake_head Snake's head position in world space.
     * \return The new command. Make sure to use @see command_make()
     */
    struct cmd (*input_to_cmd)(
        struct cmd           prev,
        const struct input*  input,
        const struct gfx*    gfx,
        const struct camera* cam,
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

extern const struct gfx_interface* gfx_backends[];

#endif
