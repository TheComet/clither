#include "clither/render.h"

#include <SDL.h>

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
    const int arrSize = round_up_multiple_of_8( radius * 8 * 35 / 49 );
    points = arrSize <= 128 ? pointBuf : malloc(sizeof(SDL_Point) * arrSize);
    int drawCount = 0;

    const int32_t diameter = (radius * 2);

    int32_t x = (radius - 1);
    int32_t y = 0;
    int32_t tx = 1;
    int32_t ty = 1;
    int32_t error = (tx - diameter);

    while( x >= y )
    {
        /* Each of the following renders an octant of the circle */
        points[drawCount+0] = (SDL_Point){ center.x + x, center.y - y };
        points[drawCount+1] = (SDL_Point){ center.x + x, center.y + y };
        points[drawCount+2] = (SDL_Point){ center.x - x, center.y - y };
        points[drawCount+3] = (SDL_Point){ center.x - x, center.y + y };
        points[drawCount+4] = (SDL_Point){ center.x + y, center.y - x };
        points[drawCount+5] = (SDL_Point){ center.x + y, center.y + x };
        points[drawCount+6] = (SDL_Point){ center.x - y, center.y - x };
        points[drawCount+7] = (SDL_Point){ center.x - y, center.y + x };

        drawCount += 8;

        if(error <= 0)
        {
            ++y;
            error += ty;
            ty += 2;
        }

        if(error > 0)
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
int render_main(void)
{
    int running;
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Event event;

    if (SDL_Init(0) < 0)
    {
        log_err("Failed to initialize SDL: %s\n", SDL_GetError());
        goto sdl_init_failed;
    }

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
    {
        log_err("Failed to initialize SDL video subsystem: %s\n", SDL_GetError());
        goto sdl_init_video_failed;
    }

    window = SDL_CreateWindow("clither",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1920, 1080,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
    );
    if (window == NULL)
    {
        log_warn("Failed to create OpenGL window: %s\n", SDL_GetError());
        log_warn("Falling back to software window\n");
        window = SDL_CreateWindow("clither",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            1920, 1080,
            SDL_WINDOW_RESIZABLE
        );
    }
    if (window == NULL)
    {
        log_err("Failed to create window: %s\n", SDL_GetError());
        goto create_window_failed;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL)
    {
        log_warn("Failed to create renderer: %s\n", SDL_GetError());
        log_warn("Falling back to a software renderer\n");
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == NULL)
    {
        log_err("Failed to create renderer: %s\n", SDL_GetError());
        goto create_renderer_failed;
    }

    running = 1;
    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_QUIT:
                    running = 0;
                    break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        draw_circle(renderer, (SDL_Point){200, 200}, 20);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();

    return 0;

    create_renderer_failed : SDL_DestroyWindow(window);
    create_window_failed   : SDL_QuitSubSystem(SDL_INIT_VIDEO);
    sdl_init_video_failed  : SDL_Quit();
    sdl_init_failed        : return -1;
}
