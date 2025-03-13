#include "clither/cmd.h"
#define _USE_MATH_DEFINES
#include <math.h>

/* ------------------------------------------------------------------------- */
struct cmd cmd_default(void)
{
    struct cmd command;
    command.angle = 128;
    command.speed = 0;
    command.action = CMD_ACTION_NONE;
    return command;
}

/* ------------------------------------------------------------------------- */
struct cmd cmd_make(
    struct cmd      prev,
    float           radians,
    float           normalized_speed,
    enum cmd_action action)
{
    /* Yes, this 256 is not a mistake -- makes sure that not both of -3.141
     * and 3.141 are included */
    float   a = radians / (2 * M_PI) + 0.5;
    uint8_t new_angle = (uint8_t)(a * 256);
    uint8_t new_speed = (uint8_t)(normalized_speed * 255);

    /*
     * The following code is designed to limit the number of bits necessary to
     * encode input deltas. The snake's turn speed is pretty slow, so we can
     * get away with 3 bits. Speed is a little more sensitive. Through testing,
     * 5 bits seems appropriate (see: snake.c, ACCELERATION is 8 per frame, so
     * we need at least 5 bits)
     */
    int da = new_angle - prev.angle;
    if (da > 128)
        da -= 256;
    if (da < -128)
        da += 256;
    if (da > 3)
        prev.angle += 3;
    else if (da < -3)
        prev.angle -= 3;
    else
        prev.angle = new_angle;

    /* (int) cast is necessary because msvc does not correctly deal with
     * bitfields */
    if (new_speed - (int)prev.speed > 15)
        prev.speed += 15;
    else if (new_speed - (int)prev.speed < -15)
        prev.speed -= 15;
    else
        prev.speed = new_speed;

    prev.action = action;
    return prev;
}
