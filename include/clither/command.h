#pragma once

#include "clither/config.h"
#include "clither/q.h"
#include "cstructures/rb.h"
#include <stdint.h>

C_BEGIN

enum command_action
{
    COMMAND_ACTION_NONE,
    COMMAND_ACTION_BOOST,
    COMMAND_ACTION_SHOOT,
    COMMAND_ACTION_REVERSE,
    COMMAND_ACTION_SPLIT,
    COMMAND_ACTION_GUARD
};

/*!
 * \brief Optimized for network traffic. This is the direct input for stepping
 * a snake forwards by 1 frame.
 */
struct command
{
    uint8_t angle;
    uint8_t speed;
    unsigned action : 3;
};

struct command_rb
{
    struct cs_rb rb;
    struct command last_command_read;
    uint16_t first_frame;
};

struct command
command_default(void);

/*!
 * \brief Constructs a new command given the previous command and parameters.
 * 
 * Commands are constructed in a way to limit the number of bits necessary to
 * encode deltas. We do this by limiting the speed at which the "angle" and "speed"
 * properties can be updated from frame to frame. This function takes care of
 * the details.
 * 
 * \param[in] prev The command from the previous frame
 * \param[in] radians The angle, in world space, to steer the snake towards
 * \param[in] speed A value between [0..1]. Here, 0 maps to the minimum speed and
 * 1 maps to the maximum (non-boost) speed.
 * \param[in] action The current action to perform.
 */
struct command
command_make(
    struct command prev,
    float radians,
    float normalized_speed,
    enum command_action action);

#define command_angle_to_qa(command) \
    (qa_rescale(2*QA_PI, command.angle, 256) - QA_PI)
    //make_qa(command.angle / 256.0 * 2 * M_PI - M_PI)

void
command_rb_init(struct command_rb* rb);

void
command_rb_deinit(struct command_rb* rb);

void
command_rb_put(
    struct command_rb* rb,
    struct command command,
    uint16_t frame_number);

struct command
command_rb_take_or_predict(
    struct command_rb* rb,
    uint16_t frame_number);

struct command
command_rb_find_or_predict(
    const struct command_rb* rb,
    uint16_t frame_number);

#define command_rb_count(crb) rb_count(&(crb)->rb)
#define command_rb_frame_begin(crb) ((crb)->first_frame)
#define command_rb_frame_end(crb) (uint16_t)((crb)->first_frame + rb_count(&(crb)->rb))
#define command_rb_peek(crb, idx) ((struct command*)rb_peek(&(crb)->rb, idx))
#define command_rb_clear(crb) rb_clear(&(crb)->rb)

#define COMMAND_RB_FOR_EACH(crb, frame_var, command_var) { \
    uint16_t frame_var = (crb)->first_frame - 1;             \
    RB_FOR_EACH(&(crb)->rb, struct command, command_var)   \
        frame_var++; {

#define COMMAND_RB_END_EACH \
    }} RB_END_EACH

C_END
