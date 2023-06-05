#pragma once

#include "clither/config.h"
#include "cstructures/rb.h"
#include <stdint.h>

C_BEGIN

enum controls_action
{
    CONTROLS_ACTION_NONE,
    CONTROLS_ACTION_BOOST,
    CONTROLS_ACTION_SHOOT,
    CONTROLS_ACTION_REVERSE,
    CONTROLS_ACTION_SPLIT
};

/*!
 * \brief Maps directly to the user's mouse and button presses. This structure
 * is filled in by gfx_poll_input()
 */
struct input
{
    int mousex, mousey;
    unsigned boost : 1;
    unsigned quit  : 1;
};

/*!
 * \brief Optimized for network traffic. This is the direct input for stepping
 * a snake forwards by 1 frame.
 */
struct controls
{
    uint8_t angle;
    uint8_t speed;
    unsigned action : 3;
};

struct controls_rb
{
    uint16_t first_frame_number;
    struct cs_rb rb;
    struct controls last_predicted;
};

void
input_init(struct input* i);

void
controls_init(struct controls* c);

void
controls_rb_init(struct controls_rb* rb);

void
controls_rb_deinit(struct controls_rb* rb);

void
controls_rb_put(
    struct controls_rb* rb,
    const struct controls* controls,
    uint16_t frame_number);

const struct controls*
controls_rb_take_or_predict(
    struct controls_rb* rb,
    uint16_t frame_number);

const struct controls*
controls_rb_find_or_predict(
    const struct controls_rb* rb,
    uint16_t frame_number);

#define controls_rb_count(crb) rb_count(&(crb)->rb)
#define controls_rb_first_frame(crb) ((crb)->first_frame_number)
#define controls_rb_last_frame(crb) (uint16_t)((crb)->first_frame_number + rb_count(&(crb)->rb))
#define controls_rb_peek(crb, idx) rb_peek(&(crb)->rb, idx)
#define controls_rb_clear(crb) rb_clear(&(crb)->rb)

#define CONTROLS_RB_FOR_EACH(crb, frame_var, controls_var) { \
    uint16_t frame_var = (crb)->first_frame_number - 1;      \
    RB_FOR_EACH(&(crb)->rb, struct controls, controls_var)   \
        frame_var++; {

#define CONTROLS_RB_END_EACH \
    }} RB_END_EACH

C_END
