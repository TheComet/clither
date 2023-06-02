#include "clither/bezier.h"
#include "clither/camera.h"
#include "clither/controls.h"
#include "clither/gfx.h"
#include "clither/log.h"
#include "clither/snake.h"
#include "clither/world.h"

#include <cstructures/memory.h>

#include <SDL.h>
#include <SDL_image.h>
#include <math.h>

struct gfx_sdl
{
    struct gfx base;
    SDL_Window* window;
    SDL_Renderer* renderer;

    struct {
        SDL_Texture* background;
    } textures;
};

/* ------------------------------------------------------------------------- */
static SDL_Point make_SDL_Point(int x, int y)
{
    SDL_Point ret;
    ret.x = x;
    ret.y = y;
    return ret;
}

/* ------------------------------------------------------------------------- */
static SDL_Rect make_SDL_Rect(int x, int y, int w, int h)
{
    SDL_Rect ret;
    ret.x = x;
    ret.y = y;
    ret.w = w;
    ret.h = h;
    return ret;
}

/* ------------------------------------------------------------------------- */
static int
round_up_multiple_of_8(int v)
{
    return (v + (8 - 1)) & -8;
}

/* ------------------------------------------------------------------------- */
static void
draw_circle(SDL_Renderer* renderer, SDL_Point center, int radius)
{
    SDL_Point* points;
    SDL_Point point_buf[128];
    int draw_count;

    const int32_t diameter = (radius * 2);

    /* 35 / 49 is a slightly biased approximation of 1/sqrt(2) */
    const int arr_size = round_up_multiple_of_8(radius * 8 * 35 / 49);

    int32_t x = (radius - 1);
    int32_t y = 0;
    int32_t tx = 1;
    int32_t ty = 1;
    int32_t error = (tx - diameter);

    points = arr_size <= 128 ? point_buf : MALLOC(sizeof(SDL_Point) * arr_size);
    draw_count = 0;

    while (x >= y)
    {
        /* Each of the following renders an octant of the circle */
        points[draw_count + 0] = make_SDL_Point(center.x + x, center.y - y);
        points[draw_count + 1] = make_SDL_Point(center.x + x, center.y + y);
        points[draw_count + 2] = make_SDL_Point(center.x - x, center.y - y);
        points[draw_count + 3] = make_SDL_Point(center.x - x, center.y + y);
        points[draw_count + 4] = make_SDL_Point(center.x + y, center.y - x);
        points[draw_count + 5] = make_SDL_Point(center.x + y, center.y + x);
        points[draw_count + 6] = make_SDL_Point(center.x - y, center.y - x);
        points[draw_count + 7] = make_SDL_Point(center.x - y, center.y + x);

        draw_count += 8;

        if (error <= 0)
        {
            ++y;
            error += ty;
            ty += 2;
        }

        if (error > 0)
        {
            --x;
            tx += 2;
            error += (tx - diameter);
        }
    }

    SDL_RenderDrawPoints(renderer, points, draw_count);
    if (arr_size > 128)
        FREE(points);
}

/* ------------------------------------------------------------------------- */
int
gfx_init(void)
{
    if (SDL_InitSubSystem(SDL_INIT_EVENTS) < 0)
    {
        log_err("Failed to initialize SDL event subsystem: %s\n", SDL_GetError());
        goto sdl_init_events_failed;
    }
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
    {
        log_err("Failed to initialize SDL video subsystem: %s\n", SDL_GetError());
        goto sdl_init_video_failed;
    }

    return 0;

    sdl_init_video_failed  : SDL_QuitSubSystem(SDL_INIT_EVENTS);
    sdl_init_events_failed : return -1;

}

/* ------------------------------------------------------------------------- */
void
gfx_deinit(void)
{
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
}

/* ------------------------------------------------------------------------- */
struct gfx*
gfx_create(int initial_width, int initial_height)
{
    struct gfx_sdl* g = MALLOC(sizeof *g);

    g->window = SDL_CreateWindow("clither",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        initial_width, initial_height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
    );
    if (g->window == NULL)
    {
        log_warn("Failed to create OpenGL window: %s\n", SDL_GetError());
        log_warn("Falling back to software window\n");
        g->window = SDL_CreateWindow("clither",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            initial_width, initial_height,
            SDL_WINDOW_RESIZABLE
        );
    }
    if (g->window == NULL)
    {
        log_err("Failed to create window: %s\n", SDL_GetError());
        goto create_window_failed;
    }

    g->renderer = SDL_CreateRenderer(g->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (g->renderer == NULL)
    {
        log_warn("Failed to create renderer: %s\n", SDL_GetError());
        log_warn("Falling back to a software renderer\n");
        g->renderer = SDL_CreateRenderer(g->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (g->renderer == NULL)
    {
        log_err("Failed to create renderer: %s\n", SDL_GetError());
        goto create_renderer_failed;
    }

    g->textures.background = IMG_LoadTexture(g->renderer, "textures/tile.png");
    if (g->textures.background == NULL)
    {
        log_err("Failed to load image: textures/tile.png\n");
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    return (struct gfx*)g;

    create_renderer_failed : SDL_DestroyWindow(g->window);
    create_window_failed   : return NULL;
}

/* ------------------------------------------------------------------------- */
void
gfx_destroy(struct gfx* gb)
{
    struct gfx_sdl* g = (struct gfx_sdl*)gb;
    SDL_DestroyRenderer(g->renderer);
    SDL_DestroyWindow(g->window);
    FREE(g);
}

/* ------------------------------------------------------------------------- */
void
gfx_poll_input(struct gfx* gfx, struct input* input)
{
    SDL_Event e;
    (void)gfx;
    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
            case SDL_QUIT:
                input->quit = 1;
                break;
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_ESCAPE)
                    input->quit = 1;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT)
                    input->boost = 1;
                break;
            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT)
                    input->boost = 0;
                break;
            case SDL_MOUSEMOTION:
                input->mousex = e.motion.x;
                input->mousey = e.motion.y;
                break;
        }
    }
}

/* ------------------------------------------------------------------------- */
void
gfx_update_controls(
    struct controls* controls,
    const struct input* input,
    const struct gfx* gfx,
    const struct camera* camera,
    struct qwpos snake_head)
{
    double a, d, dx, dy;
    int screen_x, screen_y, max_dist, da;
    uint8_t new_angle, new_speed;
    struct spos snake_head_screen;
    struct gfx_sdl* g = (struct gfx_sdl*)gfx;

    /* Scale the speed vector to a quarter of the screen's size */
    SDL_GetWindowSize(g->window, &screen_x, &screen_y);
    max_dist = screen_x > screen_y ? screen_y / 4 : screen_x / 4;

    snake_head_screen = gfx_world_to_screen(snake_head, gfx, camera);
    dx = input->mousex - snake_head_screen.x;
    dy = snake_head_screen.y - input->mousey;
    a = atan2(dy, dx) / (2*M_PI) + 0.5;
    d = sqrt(dx*dx + dy*dy);
    if (d > max_dist)
        d = max_dist;

    /* Yes, this 256 is not a mistake -- makes sure that not both of -3.141 and 3.141 are included */
    new_angle = (uint8_t)(a * 256);
    new_speed = (uint8_t)(d / max_dist * 255);

    /*
     * The following code is designed to limit the number of bits necessary to
     * encode input deltas. The snake's turn speed is pretty slow, so we can
     * get away with 3 bits. Speed is a little more sensitive. Through testing,
     * 5 bits seems appropriate (see: snake.c, ACCELERATION is 8 per frame, so
     * we need at least 5 bits)
     */
    da = new_angle - controls->angle;
    if (da > 128)
        da -= 256;
    if (da < -128)
        da += 256;
    if (da > 3)
        controls->angle += 3;
    else if (da < -3)
        controls->angle -= 3;
    else
        controls->angle = new_angle;

    /* (int) cast is necessary because msvc does not correctly deal with bitfields */
    if (new_speed - (int)controls->speed > 15)
        controls->speed += 15;
    else if (new_speed - (int)controls->speed < -15)
        controls->speed -= 15;
    else
        controls->speed = new_speed;

    controls->action = input->boost ? CONTROLS_ACTION_BOOST : CONTROLS_ACTION_NONE;
}

/* ------------------------------------------------------------------------- */
struct qwpos
gfx_screen_to_world(struct spos pos, const struct gfx* gfx, const struct camera* camera)
{
    int screen_x, screen_y;
    struct qwpos result = make_qwposi(pos.x, pos.y);
    struct gfx_sdl* g = (struct gfx_sdl*)gfx;

    SDL_GetWindowSize(g->window, &screen_x, &screen_y);
    if (screen_x < screen_y)
    {
        int pad = (screen_y - screen_x) / 2;
        result.x = qw_sub(result.x, make_qw(screen_x / 2));
        result.y = qw_sub(result.y, make_qw(screen_x / 2 + pad));
        result.x = qw_div(result.x, make_qw(screen_x));
        result.y = qw_div(result.y, make_qw(-screen_x));
    }
    else
    {
        int pad = (screen_x - screen_y) / 2;
        result.x = qw_sub(result.x, make_qw(screen_y / 2 + pad));
        result.y = qw_sub(result.y, make_qw(screen_y / 2));
        result.x = qw_div(result.x, make_qw(screen_y));
        result.y = qw_div(result.y, make_qw(-screen_y));
    }

    result.x = qw_div(result.x, camera->scale);
    result.y = qw_div(result.y, camera->scale);
    result.x = qw_add(result.x, camera->pos.x);
    result.y = qw_add(result.y, camera->pos.y);

    return result;
}

/* ------------------------------------------------------------------------- */
struct spos
gfx_world_to_screen(struct qwpos pos, const struct gfx* gfx, const struct camera* camera)
{
    struct spos result;
    int screen_x, screen_y;
    struct gfx_sdl* g = (struct gfx_sdl*)gfx;

    /* world -> camera space */
    pos.x = qw_mul(pos.x, camera->scale);
    pos.y = qw_mul(pos.y, camera->scale);
    pos.x = qw_sub(pos.x, camera->pos.x);
    pos.y = qw_sub(pos.y, camera->pos.y);

    /* camera space -> screen space + keep aspect ratio */
    SDL_GetWindowSize(g->window, &screen_x, &screen_y);
    if (screen_x < screen_y)
    {
        int pad = (screen_y - screen_x) / 2;
        result.x = qw_mul_to_int(pos.x, make_qw(screen_x)) + (screen_x / 2);
        result.y = qw_mul_to_int(pos.y, make_qw(-screen_x)) + (screen_x / 2 + pad);
    }
    else
    {
        int pad = (screen_x - screen_y) / 2;
        result.x = qw_mul_to_int(pos.x, make_qw(screen_y)) + (screen_y / 2 + pad);
        result.y = qw_mul_to_int(pos.y, make_qw(-screen_y)) + (screen_y / 2);
    }

    return result;
}

/* ------------------------------------------------------------------------- */
static void
draw_bezier(
    const struct gfx* gfx,
    const struct camera* camera,
    const struct bezier_handle* head,
    const struct bezier_handle* tail,
    int num_points)
{
    SDL_Point* points;
    SDL_Point point_buf[64];
    int i;

    const struct gfx_sdl* g = (const struct gfx_sdl*)gfx;

    struct spos p0 = gfx_world_to_screen(head->pos, gfx, camera);
    struct spos p1 = gfx_world_to_screen(make_qwposqw(
        qw_add(head->pos.x, make_qw(head->len_backwards * cos(qa_to_float(head->angle)) / 255)),
        qw_add(head->pos.y, make_qw(head->len_backwards * sin(qa_to_float(head->angle)) / 255))
    ), gfx, camera);
    struct spos p2 = gfx_world_to_screen(make_qwposqw(
        qw_add(tail->pos.x, make_qw(tail->len_forwards * -cos(qa_to_float(tail->angle)) / 255)),
        qw_add(tail->pos.y, make_qw(tail->len_forwards * -sin(qa_to_float(tail->angle)) / 255))
    ), gfx, camera);
    struct spos p3 = gfx_world_to_screen(tail->pos, gfx, camera);

    points = num_points <= 64 ? point_buf : MALLOC(sizeof(SDL_Point) * num_points);

    for (i = 0; i != num_points; ++i)
    {
        double t1 = (double)i / (num_points - 1);
        double t2 = t1*t1;
        double t3 = t2*t1;

        double a0 = p0.x;
        double a1 = -3*p0.x + 3*p1.x;
        double a2 = 3*p0.x - 6*p1.x + 3*p2.x;
        double a3 = -p0.x + 3*p1.x - 3*p2.x + p3.x;
        points[i].x = (int)(a0 + a1*t1 + a2*t2 + a3*t3);

        a0 = p0.y;
        a1 = -3*p0.y + 3*p1.y;
        a2 = 3*p0.y - 6*p1.y + 3*p2.y;
        a3 = -p0.y + 3*p1.y - 3*p2.y + p3.y;
        points[i].y = (int)(a0 + a1*t1 + a2*t2 + a3*t3);
    }

    SDL_RenderDrawLines(g->renderer, points, num_points);

    if (num_points > 64)
        FREE(point_buf);
}

/* ------------------------------------------------------------------------- */
static void
draw_snake(const struct gfx_sdl* gfx, const struct camera* camera, const struct snake* snake, uint16_t frame_number)
{
    struct controls* controls;
    struct spos pos;
    int i;

    SDL_SetRenderDrawColor(gfx->renderer, 0, 255, 0, 255);

    /*VECTOR_FOR_EACH(&snake->points, struct qwpos2, wpos)
        pos = gfx_world_to_screen(*wpos, (const struct gfx*)gfx, camera);
        draw_circle(gfx->renderer, (SDL_Point) { pos.x, pos.y }, 5);
    VECTOR_END_EACH*/

    pos = gfx_world_to_screen(snake->head.pos, (const struct gfx*)gfx, camera);
    draw_circle(gfx->renderer, make_SDL_Point(pos.x, pos.y), 10);

    /* Debug: Draw how the "controls" structure interpreted the mouse position */
    controls = btree_find(&snake->controls_buffer, frame_number);
    if (controls != NULL)
    {
        int screen_x, screen_y, max_dist;
        double a;
        SDL_GetWindowSize(gfx->window, &screen_x, &screen_y);
        max_dist = screen_x > screen_y ? screen_y / 4 : screen_x / 4;
        a = controls->angle / 256.0 * 2 * M_PI;
        screen_x = (double)controls->speed / 255 * -cos(a) * max_dist + pos.x;
        screen_y = (double)controls->speed / 255 * sin(a) * max_dist + pos.y;
        draw_circle(gfx->renderer, make_SDL_Point(screen_x, screen_y), 5);
    }

    for (i = 0; i < (int)vector_count(&snake->data.bezier_handles) - 1; ++i)
    {
        struct bezier_handle* tail = vector_get_element(&snake->data.bezier_handles, i+0);
        struct bezier_handle* head = vector_get_element(&snake->data.bezier_handles, i+1);
        if (i&1)
            SDL_SetRenderDrawColor(gfx->renderer, 255, 0, 0, 255);
        else
            SDL_SetRenderDrawColor(gfx->renderer, 255, 255, 0, 255);
        draw_bezier((const struct gfx*)gfx, camera, head, tail, 50);
    }
}

/* ------------------------------------------------------------------------- */
static void
draw_background(const struct gfx_sdl* gfx, const struct camera* camera)
{
#define TILE_SCALE 2

    int screen_x, screen_y, pad;
    int startx, starty, dim, x, y;
    qw dim_cam;

    if (gfx->textures.background == NULL)
        return;

    dim_cam = qw_mul(make_qw2(1, TILE_SCALE), camera->scale);

    SDL_GetWindowSize(gfx->window, &screen_x, &screen_y);
    pad = (screen_y - screen_x) / 2;
    if (screen_x < screen_y)
    {
        qw sx = qw_mul(-camera->pos.x, camera->scale) % make_qw2(1, TILE_SCALE);
        qw sy = qw_mul(camera->pos.y, camera->scale) % make_qw2(1, TILE_SCALE);
        startx = qw_mul_to_int(sx, make_qw(screen_x));
        starty = qw_mul_to_int(sy, make_qw(screen_x)) + pad;

        dim = qw_mul_to_int(dim_cam, make_qw(screen_x));
    }
    else
    {
        qw sx = qw_mul(-camera->pos.x, camera->scale) % make_qw2(1, TILE_SCALE);
        qw sy = qw_mul(camera->pos.y, camera->scale) % make_qw2(1, TILE_SCALE);
        startx = qw_mul_to_int(sx, make_qw(screen_y)) - pad;
        starty = qw_mul_to_int(sy, make_qw(screen_y));

        dim = qw_mul_to_int(dim_cam, make_qw(screen_y));
    }

    /*
     * Accounts for negative modulus and extreme aspect ratios. Otherwise the
     * top-right corner of the map will have missing tiles on the edges of the
     * screen.
     */
    while (startx > 0)
        startx -= dim;
    while (starty > 0)
        starty -= dim;

    if (dim < 1)
    {
        log_warn("Background tiles are smaller than 1x1, can't draw!\n");
        return;
    }


    for (x = startx; x < screen_x; x += dim)
        for (y = starty; y < screen_y; y += dim)
        {
            SDL_Rect rect = make_SDL_Rect(x, y, dim + 1, dim + 1);
            SDL_RenderCopy(gfx->renderer, gfx->textures.background, NULL, &rect);
        }
}

/* ------------------------------------------------------------------------- */
void
gfx_draw_world(struct gfx* gfx, const struct world* world, const struct camera* camera, uint16_t frame_number)
{
    const struct gfx_sdl* g = (const struct gfx_sdl*)gfx;

    SDL_SetRenderDrawColor(g->renderer, 0, 0, 0, 255);
    SDL_RenderClear(g->renderer);

    draw_background(g, camera);

    WORLD_FOR_EACH_SNAKE(world, uid, snake)
        draw_snake(g, camera, snake, frame_number);
    WORLD_END_EACH

    {
        struct spos pos = gfx_world_to_screen(make_qwposi(0, 0), (const struct gfx*)gfx, camera);
        SDL_SetRenderDrawColor(((const struct gfx_sdl*)gfx)->renderer, 0, 255, 0, 255);
        draw_circle(g->renderer, make_SDL_Point(pos.x, pos.y), 20);
    }

    SDL_RenderPresent(g->renderer);
}
