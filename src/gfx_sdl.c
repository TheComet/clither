#include "clither/controls.h"
#include "clither/gfx.h"
#include "clither/log.h"

#include <cstructures/memory.h>

#include <SDL.h>

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
    /*if (SDL_Init(0) < 0)
    {
        log_err("Failed to initialize SDL: %s\n", SDL_GetError());
        goto sdl_init_failed;
    }*/
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
gfx_poll_input(struct gfx* g, struct controls* c)
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
gfx_update(struct gfx* gb)
{
    struct gfx_sdl* g = (struct gfx_sdl*)gb;

    SDL_SetRenderDrawColor(g->renderer, 0, 0, 0, 255);
    SDL_RenderClear(g->renderer);

    SDL_SetRenderDrawColor(g->renderer, 0, 255, 0, 255);
    draw_circle(g->renderer, (SDL_Point) { 200, 200 }, 20);

    SDL_RenderPresent(g->renderer);
}