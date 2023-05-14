#include "clither/camera.h"
#include "clither/controls.h"
#include "clither/gfx.h"
#include "clither/log.h"
#include "clither/snake.h"
#include "clither/world.h"

#include <cstructures/memory.h>

#include <SDL.h>
#include <math.h>

struct gfx_sdl
{
    struct gfx base;
    SDL_Window* window;
    SDL_Renderer* renderer;
};

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
    SDL_Point pointBuf[128];

    /* 35 / 49 is a slightly biased approximation of 1/sqrt(2) */
    const int arrSize = round_up_multiple_of_8(radius * 8 * 35 / 49);
    points = arrSize <= 128 ? pointBuf : malloc(sizeof(SDL_Point) * arrSize);
    int drawCount = 0;

    const int32_t diameter = (radius * 2);

    int32_t x = (radius - 1);
    int32_t y = 0;
    int32_t tx = 1;
    int32_t ty = 1;
    int32_t error = (tx - diameter);

    while (x >= y)
    {
        /* Each of the following renders an octant of the circle */
        points[drawCount + 0] = (SDL_Point){ center.x + x, center.y - y };
        points[drawCount + 1] = (SDL_Point){ center.x + x, center.y + y };
        points[drawCount + 2] = (SDL_Point){ center.x - x, center.y - y };
        points[drawCount + 3] = (SDL_Point){ center.x - x, center.y + y };
        points[drawCount + 4] = (SDL_Point){ center.x + y, center.y - x };
        points[drawCount + 5] = (SDL_Point){ center.x + y, center.y + x };
        points[drawCount + 6] = (SDL_Point){ center.x - y, center.y - x };
        points[drawCount + 7] = (SDL_Point){ center.x - y, center.y + x };

        drawCount += 8;

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

    SDL_RenderDrawPoints(renderer, points, drawCount);
    if (arrSize > 128)
        free(points);
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
gfx_poll_input(struct gfx* g, struct input* c)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
        case SDL_QUIT:
            c->quit = 1;
            break;
        case SDL_KEYDOWN:
            if (e.key.keysym.sym == SDLK_ESCAPE)
                c->quit = 1;
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (e.button.button == 0)
                c->boost = 1;
            break;
        case SDL_MOUSEBUTTONUP:
            if (e.button.button == 0)
                c->boost = 0;
            break;
        case SDL_MOUSEMOTION:
            c->mousex = e.motion.x;
            c->mousey = e.motion.y;
        }
    }
}

/* ------------------------------------------------------------------------- */
void
gfx_calc_controls(
    struct controls* controls,
    const struct input* input,
    const struct gfx* gfx,
    const struct camera* camera,
    struct qwpos2 snake_head)
{
    int screen_x, screen_y, max_dist;
    struct gfx_sdl* g = (struct gfx_sdl*)gfx;

    SDL_GetWindowSize(g->window, &screen_x, &screen_y);
    max_dist = screen_x > screen_y ? screen_y / 4 : screen_x / 4;

    snake_head = gfx_world_to_screen(gfx, camera, snake_head);
    double dx = input->mousex - qw_to_int(snake_head.x);
    double dy = qw_to_int(snake_head.y) - input->mousey;
    double a = atan2(dy, dx) / (2*M_PI) + 0.5;
    double d = sqrt(dx*dx + dy*dy);
    if (d > max_dist)
        d = max_dist;
    controls->angle = (uint8_t)(a * 256);
    controls->speed = (uint8_t)(d / max_dist * 255);
}

/* ------------------------------------------------------------------------- */
struct qwpos2
gfx_screen_to_world(const struct gfx* gfx, const struct camera* camera, int x, int y)
{
    int screen_x, screen_y;
    struct qwpos2 pos = make_qwpos2(x, y);
    struct gfx_sdl* g = (struct gfx_sdl*)gfx;

    SDL_GetWindowSize(g->window, &screen_x, &screen_y);
    if (screen_x < screen_y)
    {
        int pad = (screen_y - screen_x) / 2;
        pos.x = qw_sub(pos.x, make_qw(screen_x / 2));
        pos.y = qw_sub(pos.y, make_qw(screen_x / 2 + pad));
        pos.x = qw_div(pos.x, make_qw(screen_x));
        pos.y = qw_div(pos.y, make_qw(-screen_x));
    }
    else
    {
        int pad = (screen_x - screen_y) / 2;
        pos.x = qw_sub(pos.x, make_qw(screen_y / 2 + pad));
        pos.y = qw_sub(pos.y, make_qw(screen_y / 2));
        pos.x = qw_div(pos.x, make_qw(screen_y));
        pos.y = qw_div(pos.y, make_qw(-screen_y));
    }

    pos.x = qw_div(pos.x, camera->scale);
    pos.y = qw_div(pos.y, camera->scale);
    pos.x = qw_add(pos.x, camera->pos.x);
    pos.y = qw_add(pos.y, camera->pos.y);

    return pos;
}

/* ------------------------------------------------------------------------- */
struct qwpos2
gfx_world_to_screen(const struct gfx* gfx, const struct camera* camera, struct qwpos2 pos)
{
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
        pos.x = qw_mul(pos.x, make_qw(screen_x));
        pos.y = qw_mul(pos.y, make_qw(-screen_x));
        pos.x = qw_add(pos.x, make_qw(screen_x / 2));
        pos.y = qw_add(pos.y, make_qw(screen_x / 2 + pad));
    }
    else
    {
        int pad = (screen_x - screen_y) / 2;
        pos.x = qw_mul(pos.x, make_qw(screen_y));
        pos.y = qw_mul(pos.y, make_qw(-screen_y));
        pos.x = qw_add(pos.x, make_qw(screen_y / 2 + pad));
        pos.y = qw_add(pos.y, make_qw(screen_y / 2));
    }

    return pos;
}

/* ------------------------------------------------------------------------- */
static void
draw_snake(const struct gfx_sdl* gfx, const struct camera* camera, const struct snake* snake)
{
    struct qwpos2 pos = gfx_world_to_screen((const struct gfx*)gfx, camera, snake->head_pos);

    SDL_SetRenderDrawColor(gfx->renderer, 0, 255, 0, 255);
    draw_circle(gfx->renderer, (SDL_Point) { qw_to_int(pos.x), qw_to_int(pos.y) }, 20);

    /* Debug: Draw how the "controls" structure interpreted the mouse position */
    {
        int screen_x, screen_y, max_dist;
        double a;
        SDL_GetWindowSize(gfx->window, &screen_x, &screen_y);
        max_dist = screen_x > screen_y ? screen_y / 4 : screen_x / 4;
        a = snake->controls.angle / 256.0 * 2 * M_PI;
        screen_x = (double)snake->controls.speed / 255 * -cos(a) * max_dist + qw_to_int(pos.x);
        screen_y = (double)snake->controls.speed / 255 * sin(a) * max_dist + qw_to_int(pos.y);
        draw_circle(gfx->renderer, (SDL_Point) { screen_x, screen_y }, 10);
    }
}

/* ------------------------------------------------------------------------- */
void
gfx_draw_world(struct gfx* gfx, const struct world* w, const struct camera* c, const struct input* i)
{
    const struct gfx_sdl* g = (const struct gfx_sdl*)gfx;

    SDL_SetRenderDrawColor(g->renderer, 0, 0, 0, 255);
    SDL_RenderClear(g->renderer);

    WORLD_FOR_EACH_SNAKE(w, uid, snake)
        draw_snake(g, c, snake);
    WORLD_END_EACH

    SDL_RenderPresent(g->renderer);
}
