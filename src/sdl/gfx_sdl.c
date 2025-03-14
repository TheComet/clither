#include "clither/bezier_handle_rb.h"
#include "clither/bezier_point_vec.h"
#include "clither/camera.h"
#include "clither/cmd.h"
#include "clither/gfx.h"
#include "clither/hash.h"
#include "clither/input.h"
#include "clither/log.h"
#include "clither/mem.h"
#include "clither/qwaabb_rb.h"
#include "clither/rb.h"
#include "clither/snake.h"
#include "clither/snake_bmap.h"
#include "clither/vec.h"
#include "clither/world.h"
#include <SDL.h>
#include <SDL_image.h>
#include <math.h>

struct gfx
{
    SDL_Window*   window;
    SDL_Renderer* renderer;

    struct
    {
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
static int round_up_multiple_of_8(int v)
{
    return (v + (8 - 1)) & -8;
}

/* ------------------------------------------------------------------------- */
static void draw_circle(SDL_Renderer* renderer, SDL_Point center, int radius)
{
    SDL_Point* points;
    SDL_Point  point_buf[128];
    int        draw_count;

    const int32_t diameter = (radius * 2);

    /* 35 / 49 is a slightly biased approximation of 1/sqrt(2) */
    const int arr_size = round_up_multiple_of_8(radius * 8 * 35 / 49);

    int32_t x = (radius - 1);
    int32_t y = 0;
    int32_t tx = 1;
    int32_t ty = 1;
    int32_t error = (tx - diameter);

    points =
        arr_size <= 128 ? point_buf : mem_alloc(sizeof(SDL_Point) * arr_size);
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
        mem_free(points);
}

/* ------------------------------------------------------------------------- */
static void draw_box(SDL_Renderer* renderer, SDL_Point p1, SDL_Point p2)
{
    /* clang-format off */
    SDL_Point points[5];
    points[0].x = p1.x;   points[0].y = p1.y;
    points[1].x = p2.x;   points[1].y = p1.y;
    points[2].x = p2.x;   points[2].y = p2.y;
    points[3].x = p1.x;   points[3].y = p2.y;
    points[4].x = p1.x;   points[4].y = p1.y;
    /* clang-format on */

    SDL_RenderDrawLines(renderer, points, 5);
}

/* ------------------------------------------------------------------------- */
static struct qwpos gfx_screen_to_world(
    struct spos pos, const struct gfx* gfx, const struct camera* camera)
{
    int          screen_x, screen_y;
    struct qwpos result = make_qwposi(pos.x, pos.y);

    SDL_GetWindowSize(gfx->window, &screen_x, &screen_y);
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
static struct spos gfx_world_to_screen(
    struct qwpos pos, const struct gfx* gfx, const struct camera* camera)
{
    struct spos result;
    int         screen_x, screen_y;

    /* world -> camera space */
    pos.x = qw_sub(pos.x, camera->pos.x);
    pos.y = qw_sub(pos.y, camera->pos.y);
    pos.x = qw_mul(pos.x, camera->scale);
    pos.y = qw_mul(pos.y, camera->scale);

    /* camera space -> screen space + keep aspect ratio */
    SDL_GetWindowSize(gfx->window, &screen_x, &screen_y);
    if (screen_x < screen_y)
    {
        int pad = (screen_y - screen_x) / 2;
        result.x = qw_mul_to_int(pos.x, make_qw(screen_x)) + (screen_x / 2);
        result.y =
            qw_mul_to_int(pos.y, make_qw(-screen_x)) + (screen_x / 2 + pad);
    }
    else
    {
        int pad = (screen_x - screen_y) / 2;
        result.x =
            qw_mul_to_int(pos.x, make_qw(screen_y)) + (screen_y / 2 + pad);
        result.y = qw_mul_to_int(pos.y, make_qw(-screen_y)) + (screen_y / 2);
    }

    return result;
}

/* ------------------------------------------------------------------------- */
static int gfx_sdl_global_init(void)
{
    if (SDL_InitSubSystem(SDL_INIT_EVENTS) < 0)
    {
        log_err(
            "Failed to initialize SDL event subsystem: %s\n", SDL_GetError());
        goto sdl_init_events_failed;
    }
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
    {
        log_err(
            "Failed to initialize SDL video subsystem: %s\n", SDL_GetError());
        goto sdl_init_video_failed;
    }

    return 0;

sdl_init_video_failed:
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
sdl_init_events_failed:
    return -1;
}

/* ------------------------------------------------------------------------- */
static void gfx_sdl_global_deinit(void)
{
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
}

/* ------------------------------------------------------------------------- */
static struct gfx* gfx_sdl_create(int initial_width, int initial_height)
{
    struct gfx* gfx = mem_alloc(sizeof *gfx);

    gfx->window = SDL_CreateWindow(
        "clither",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        initial_width,
        initial_height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (gfx->window == NULL)
    {
        log_warn("Failed to create OpenGL window: %s\n", SDL_GetError());
        log_warn("Falling back to software window\n");
        gfx->window = SDL_CreateWindow(
            "clither",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            initial_width,
            initial_height,
            SDL_WINDOW_RESIZABLE);
    }
    if (gfx->window == NULL)
    {
        log_err("Failed to create window: %s\n", SDL_GetError());
        goto create_window_failed;
    }

    gfx->renderer = SDL_CreateRenderer(
        gfx->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (gfx->renderer == NULL)
    {
        log_warn("Failed to create renderer: %s\n", SDL_GetError());
        log_warn("Falling back to a software renderer\n");
        gfx->renderer =
            SDL_CreateRenderer(gfx->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (gfx->renderer == NULL)
    {
        log_err("Failed to create renderer: %s\n", SDL_GetError());
        goto create_renderer_failed;
    }

    gfx->textures.background =
        IMG_LoadTexture(gfx->renderer, "textures/tile.png");
    if (gfx->textures.background == NULL)
    {
        log_err("Failed to load image: textures/tile.png\n");
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    return gfx;

create_renderer_failed:
    SDL_DestroyWindow(gfx->window);
create_window_failed:
    return NULL;
}

/* ------------------------------------------------------------------------- */
static void gfx_sdl_destroy(struct gfx* gfx)
{
    SDL_DestroyRenderer(gfx->renderer);
    SDL_DestroyWindow(gfx->window);
    mem_free(gfx);
}

/* ------------------------------------------------------------------------- */
static int
gfx_sdl_load_resource_pack(struct gfx* gfx, const struct resource_pack* pack)
{
    (void)gfx;
    (void)pack;
    return 0;
}

/* ------------------------------------------------------------------------- */
static void gfx_sdl_poll_input(struct gfx* gfx, struct input* input)
{
    SDL_Event e;
    (void)gfx;
    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
            case SDL_QUIT: input->quit = 1; break;
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_ESCAPE)
                    input->quit = 1;
                if (e.key.keysym.sym == SDLK_LEFT)
                    input->prev_gfx_backend = 1;
                if (e.key.keysym.sym == SDLK_RIGHT)
                    input->next_gfx_backend = 1;
                break;
            case SDL_KEYUP:
                input->prev_gfx_backend = 0;
                input->next_gfx_backend = 0;
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
static struct cmd gfx_sdl_input_to_command(
    struct cmd           command,
    const struct input*  input,
    const struct gfx*    gfx,
    const struct camera* camera,
    struct qwpos         snake_head)
{
    double      a, d, dx, dy;
    int         screen_x, screen_y, max_dist;
    struct spos snake_head_screen;

    /* Scale the speed vector to a quarter of the screen's size */
    SDL_GetWindowSize(gfx->window, &screen_x, &screen_y);
    max_dist = screen_x > screen_y ? screen_y / 4 : screen_x / 4;

    snake_head_screen = gfx_world_to_screen(snake_head, gfx, camera);
    dx = input->mousex - snake_head_screen.x;
    dy = snake_head_screen.y - input->mousey;
    a = atan2(dy, dx);
    d = sqrt(dx * dx + dy * dy);
    if (d > max_dist)
        d = max_dist;

    return cmd_make(
        command,
        a,
        d / max_dist,
        input->boost ? CMD_ACTION_BOOST : CMD_ACTION_NONE);
}

/* ------------------------------------------------------------------------- */
static void draw_bezier(
    const struct gfx*           gfx,
    const struct camera*        camera,
    const struct bezier_handle* head,
    const struct bezier_handle* tail,
    int                         num_points)
{
    SDL_Point* points;
    SDL_Point  point_buf[64];
    int        i;

    struct spos p0 = gfx_world_to_screen(head->pos, gfx, camera);
    struct spos p1 = gfx_world_to_screen(
        make_qwposqw(
            qw_add(
                head->pos.x,
                make_qw(
                    head->len_backwards * cos(qa_to_float(head->angle)) / 255)),
            qw_add(
                head->pos.y,
                make_qw(
                    head->len_backwards * sin(qa_to_float(head->angle)) /
                    255))),
        gfx,
        camera);
    struct spos p2 = gfx_world_to_screen(
        make_qwposqw(
            qw_sub(
                tail->pos.x,
                make_qw(
                    tail->len_forwards * cos(qa_to_float(tail->angle)) / 255)),
            qw_sub(
                tail->pos.y,
                make_qw(
                    tail->len_forwards * sin(qa_to_float(tail->angle)) / 255))),
        gfx,
        camera);
    struct spos p3 = gfx_world_to_screen(tail->pos, gfx, camera);

    points = num_points <= 64 ? point_buf
                              : mem_alloc(sizeof(SDL_Point) * num_points);

    for (i = 0; i != num_points; ++i)
    {
        double t1 = (double)i / (num_points - 1);
        double t2 = t1 * t1;
        double t3 = t2 * t1;

        double a0 = p0.x;
        double a1 = -3 * p0.x + 3 * p1.x;
        double a2 = 3 * p0.x - 6 * p1.x + 3 * p2.x;
        double a3 = -p0.x + 3 * p1.x - 3 * p2.x + p3.x;
        points[i].x = (int)(a0 + a1 * t1 + a2 * t2 + a3 * t3);

        a0 = p0.y;
        a1 = -3 * p0.y + 3 * p1.y;
        a2 = 3 * p0.y - 6 * p1.y + 3 * p2.y;
        a3 = -p0.y + 3 * p1.y - 3 * p2.y + p3.y;
        points[i].y = (int)(a0 + a1 * t1 + a2 * t2 + a3 * t3);
    }

    SDL_RenderDrawLines(gfx->renderer, points, num_points);

    if (num_points > 64)
        mem_free(point_buf);
}

/* ------------------------------------------------------------------------- */
static void draw_snake(
    const struct gfx*    gfx,
    const struct camera* camera,
    const struct snake*  snake)
{
    struct spos          pos;
    int                  i;
    struct bezier_point* bp;
    struct qwaabb*       bb;

    SDL_SetRenderDrawColor(gfx->renderer, 0, 255, 0, 255);

    /*VECTOR_FOR_EACH(&snake->points, struct qwpos2, wpos)
        pos = gfx_world_to_screen(*wpos, gfx, camera);
        draw_circle(gfx->renderer, (SDL_Point) { pos.x, pos.y }, 5);
    VECTOR_END_EACH*/

    vec_for_each (snake->data.bezier_points, bp)
    {
        pos = gfx_world_to_screen(bp->pos, gfx, camera);
        draw_circle(gfx->renderer, make_SDL_Point(pos.x, pos.y), 5);
    }

    pos = gfx_world_to_screen(snake->head.pos, gfx, camera);
    draw_circle(gfx->renderer, make_SDL_Point(pos.x, pos.y), 10);

    /* Debug: Draw how the "command" structure interpreted the mouse position */
    if (cmd_queue_count(&snake->cmdq) > 0)
    {
        int         screen_x, screen_y, max_dist;
        double      a;
        struct cmd* cmd = cmd_queue_peek(&snake->cmdq, 0);
        SDL_GetWindowSize(gfx->window, &screen_x, &screen_y);
        max_dist = screen_x > screen_y ? screen_y / 4 : screen_x / 4;

        SDL_SetRenderDrawColor(gfx->renderer, 255, 255, 0, 255);
        a = cmd->angle / 256.0 * 2 * M_PI;
        screen_x = (double)cmd->speed / 255 * -cos(a) * max_dist + pos.x;
        screen_y = (double)cmd->speed / 255 * sin(a) * max_dist + pos.y;
        draw_circle(gfx->renderer, make_SDL_Point(screen_x, screen_y), 5);

        SDL_SetRenderDrawColor(gfx->renderer, 0, 255, 0, 255);
        cmd = cmd_queue_peek(&snake->cmdq, cmd_queue_count(&snake->cmdq) - 1);
        a = cmd->angle / 256.0 * 2 * M_PI;
        screen_x = (double)cmd->speed / 255 * -cos(a) * max_dist + pos.x;
        screen_y = (double)cmd->speed / 255 * sin(a) * max_dist + pos.y;
        draw_circle(gfx->renderer, make_SDL_Point(screen_x, screen_y), 5);
    }

    {
        SDL_SetRenderDrawColor(gfx->renderer, 255, 128, 0, 255);
        pos = gfx_world_to_screen(snake->head_ack.pos, gfx, camera);
        draw_circle(gfx->renderer, make_SDL_Point(pos.x, pos.y), 5);
    }

    for (i = 0; i < rb_count(snake->data.bezier_handles) - 1; ++i)
    {
        const struct bezier_handle* tail =
            rb_peek(snake->data.bezier_handles, i + 0);
        const struct bezier_handle* head =
            rb_peek(snake->data.bezier_handles, i + 1);
        if (i & 1)
            SDL_SetRenderDrawColor(gfx->renderer, 255, 0, 0, 255);
        else
            SDL_SetRenderDrawColor(gfx->renderer, 255, 255, 0, 255);
        draw_bezier((const struct gfx*)gfx, camera, head, tail, 50);
    }

    SDL_SetRenderDrawColor(gfx->renderer, 255, 255, 0, 255);
    rb_for_each (snake->data.bezier_aabbs, i, bb)
    {
        struct qwpos q1 = make_qwposqw(bb->x1, bb->y1);
        struct qwpos q2 = make_qwposqw(bb->x2, bb->y2);
        struct spos  s1 = gfx_world_to_screen(q1, gfx, camera);
        struct spos  s2 = gfx_world_to_screen(q2, gfx, camera);
        draw_box(
            gfx->renderer,
            make_SDL_Point(s1.x, s1.y),
            make_SDL_Point(s2.x, s2.y));
    }

    {
        struct qwpos q1 =
            make_qwposqw(snake->data.aabb.x1, snake->data.aabb.y1);
        struct qwpos q2 =
            make_qwposqw(snake->data.aabb.x2, snake->data.aabb.y2);
        struct spos s1 = gfx_world_to_screen(q1, gfx, camera);
        struct spos s2 = gfx_world_to_screen(q2, gfx, camera);
        draw_box(
            gfx->renderer,
            make_SDL_Point(s1.x, s1.y),
            make_SDL_Point(s2.x, s2.y));
    }
}

/* ------------------------------------------------------------------------- */
static void draw_background(const struct gfx* gfx, const struct camera* camera)
{
#define TILE_SCALE 2

    int screen_x, screen_y, pad;
    int startx, starty, dim, x, y;
    qw  dim_cam;

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
            SDL_RenderCopy(
                gfx->renderer, gfx->textures.background, NULL, &rect);
        }
}

/* ------------------------------------------------------------------------- */
static void gfx_sdl_step_anim(struct gfx* gfx, int sim_tick_rate)
{
    (void)gfx;
    (void)sim_tick_rate;
}

/* ------------------------------------------------------------------------- */
static void gfx_sdl_draw_world(
    struct gfx* gfx, const struct world* world, const struct camera* camera)
{
    int16_t             idx;
    uint16_t            uid;
    const struct snake* snake;

    SDL_SetRenderDrawColor(gfx->renderer, 0, 0, 0, 255);
    SDL_RenderClear(gfx->renderer);

    draw_background(gfx, camera);

    bmap_for_each (world->snakes, idx, uid, snake)
    {
        (void)uid;
        draw_snake(gfx, camera, snake);
    }

    {
        struct spos pos = gfx_world_to_screen(make_qwposi(0, 0), gfx, camera);
        SDL_SetRenderDrawColor(gfx->renderer, 0, 255, 0, 255);
        draw_circle(gfx->renderer, make_SDL_Point(pos.x, pos.y), 20);
    }

    {
        int    i;
        hash32 h = 250;
        for (i = 0; i != 255; ++i)
        {
            struct qwpos p;
            struct spos  sp;
            p.x = 0x8000 * i / 255;
            h = hash32_jenkins_oaat(&h, sizeof(h));
            p.y = h & 0x7F00;
            sp = gfx_world_to_screen(p, gfx, camera);
            draw_circle(gfx->renderer, make_SDL_Point(sp.x, sp.y), 3);
        }
    }

    SDL_RenderPresent(gfx->renderer);
}

/* ------------------------------------------------------------------------- */
struct gfx_interface gfx_sdl = {
    "SDL",
    gfx_sdl_global_init,
    gfx_sdl_global_deinit,
    gfx_sdl_create,
    gfx_sdl_destroy,
    gfx_sdl_load_resource_pack,
    gfx_sdl_poll_input,
    gfx_sdl_input_to_command,
    gfx_sdl_step_anim,
    gfx_sdl_draw_world};
