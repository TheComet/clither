#include "./internal/food.h"
#include "./internal/gfx.h"
#include "./internal/snake.h"
#include "GLFW/glfw3.h"
#include "clither/game/camera.h"
#include "clither/game/resource_pack.h"
#include "clither/game/snake.h"
#include "clither/game/snake_bmap.h"
#include "clither/game/world.h"
#include "clither/gfx/gfx.h"
#include "clither/util/hmap_str.h"
#include "clither/util/log.h"
#include "clither/util/str.h"
#include "clither/util/strlist.h"
#include "clither/util/tracker.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define GFX_NAME "OpenGL ES 2.0"

enum
{
    SHADOW_MAP_SIZE_FACTOR = 4
};

HMAP_DECLARE_STR(static, gfx_text_hmap, struct text, 16)
HMAP_DEFINE_STR(static, gfx_text_hmap, struct text, 16)

/* ------------------------------------------------------------------------- */
#if defined(CLITHER_DEBUG_MEMORY)
struct tracker_gfx
{
    struct tracker* tex;
    struct tracker* buf;
    struct tracker* fbo;
    struct tracker* shader;
};

static struct tracker_gfx g_tracker_gfx;

static int tracker_gfx_init(void)
{
    g_tracker_gfx.tex = tracker_create("GL Texture");
    if (g_tracker_gfx.tex == NULL)
        goto tracker_tex_create_failed;
    g_tracker_gfx.buf = tracker_create("GL Buffer");
    if (g_tracker_gfx.buf == NULL)
        goto tracker_buf_create_failed;
    g_tracker_gfx.fbo = tracker_create("GL Framebuffer");
    if (g_tracker_gfx.fbo == NULL)
        goto tracker_fbo_create_failed;
    g_tracker_gfx.shader = tracker_create("GL Shader");
    if (g_tracker_gfx.shader == NULL)
        goto tracker_shader_create_failed;

    return 0;

tracker_shader_create_failed:
    tracker_destroy(g_tracker_gfx.fbo);
tracker_fbo_create_failed:
    tracker_destroy(g_tracker_gfx.buf);
tracker_buf_create_failed:
    tracker_destroy(g_tracker_gfx.tex);
tracker_tex_create_failed:
    return -1;
}

static void tracker_gfx_deinit(void)
{
    tracker_destroy(g_tracker_gfx.shader);
    tracker_destroy(g_tracker_gfx.fbo);
    tracker_destroy(g_tracker_gfx.buf);
    tracker_destroy(g_tracker_gfx.tex);
}

/* clang-format off */
void gfx_track_tex(GLuint tex)
    {tracker_track(g_tracker_gfx.tex, (void*)(uintptr_t)tex, 0);}
void gfx_track_buf(GLuint buf)
    {tracker_track(g_tracker_gfx.buf, (void*)(uintptr_t)buf, 0);}
void gfx_track_fbo(GLuint fbo)
    {tracker_track(g_tracker_gfx.fbo, (void*)(uintptr_t)fbo, 0);}
void gfx_track_shader(GLuint shader)
    {tracker_track(g_tracker_gfx.shader, (void*)(uintptr_t)shader, 0);}

void gfx_untrack_tex(GLuint tex)
    {tracker_untrack(g_tracker_gfx.tex, (void*)(uintptr_t)tex);}
void gfx_untrack_buf(GLuint buf)
    {tracker_untrack(g_tracker_gfx.buf, (void*)(uintptr_t)buf);}
void gfx_untrack_fbo(GLuint fbo)
    {tracker_untrack(g_tracker_gfx.fbo, (void*)(uintptr_t)fbo);}
void gfx_untrack_shader(GLuint shader)
    {tracker_untrack(g_tracker_gfx.shader, (void*)(uintptr_t)shader);}
/* clang-format on */
#endif

/* ------------------------------------------------------------------------- */
static void error_callback(int error_code, const char* error_msg)
{
    log_err("GLFW Error %d: %s\n", error_code, error_msg);
}

/* ------------------------------------------------------------------------- */
static void
key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    struct gfx* gfx = glfwGetWindowUserPointer(window);
    (void)mods;
    (void)scancode;

    switch (key)
    {
        case GLFW_KEY_ESCAPE:
            if (action == GLFW_PRESS)
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_LEFT:
            gfx->input_buffer.prev_gfx_backend = (action == GLFW_PRESS);
            break;
        case GLFW_KEY_RIGHT:
            gfx->input_buffer.next_gfx_backend = (action == GLFW_PRESS);
            break;
        case GLFW_KEY_F1:
            gfx->input_buffer.debug_gfx = (action == GLFW_PRESS);
            break;
        case GLFW_KEY_1:
            if (action == GLFW_PRESS)
                gfx->input_buffer.boost = 1;
            if (action == GLFW_RELEASE)
                gfx->input_buffer.boost = 0;
            break;
        case GLFW_KEY_2:
            if (action == GLFW_PRESS)
                gfx->input_buffer.shoot = 1;
            if (action == GLFW_RELEASE)
                gfx->input_buffer.shoot = 0;
            break;
        case GLFW_KEY_3:
            if (action == GLFW_PRESS)
                gfx->input_buffer.split = 1;
            if (action == GLFW_RELEASE)
                gfx->input_buffer.split = 0;
            break;
    }
}

/* ------------------------------------------------------------------------- */
static void
mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    struct gfx* gfx = glfwGetWindowUserPointer(window);
    (void)mods;

    if (button == GLFW_MOUSE_BUTTON_LEFT)
        gfx->input_buffer.boost = (action == GLFW_PRESS);
}

/* ------------------------------------------------------------------------- */
static void
cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    struct gfx* gfx = glfwGetWindowUserPointer(window);
    gfx->input_buffer.mousex = xpos;
    gfx->input_buffer.mousey = ypos;
}

/* ------------------------------------------------------------------------- */
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    struct gfx* gfx = glfwGetWindowUserPointer(window);
    (void)xoffset;
    gfx->input_buffer.scroll += (int)yoffset;
}

/* ------------------------------------------------------------------------- */
static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    struct gfx* gfx = glfwGetWindowUserPointer(window);
    gfx->width = width;
    gfx->height = height;
    glViewport(0, 0, width, height);

    gfx_gles2_background_resize(
        &gfx->background, width, height, SHADOW_MAP_SIZE_FACTOR);
}

/* ------------------------------------------------------------------------- */
static int
gfx_gles2_load_resource_pack(struct gfx* gfx, const struct resource_pack* pack)
{
    struct resource_snake*  snake;
    struct resource_shader* shader = resource_shader_hmap_find(
        pack->shaders, strview(GFX_NAME, 0, sizeof(GFX_NAME) - 1));
    if (shader == NULL)
        return log_err(
            "No shader found for graphics backend \"%s\"\n", GFX_NAME);

    if (gfx_gles2_font_load(&gfx->font, &pack->text, shader) < 0)
        goto font_load_failed;
    if (gfx_gles2_background_load(&gfx->background, &pack->background, shader) <
        0)
        goto bg_load_failed;
    if (gfx_gles2_sprite_shadow_load(&gfx->sprite_shadow_mat, shader) < 0)
        goto sprite_shadow_load_failed;
    if (gfx_gles2_sprite_mat_load(&gfx->sprite_mat, shader) < 0)
        goto sprite_mat_load_failed;

    if (str_len(pack->food.sprite) > 0)
    {
        struct resource_sprite* sprite = resource_sprite_hmap_find(
            pack->sprites, str_view(pack->food.sprite));
        if (sprite != NULL)
        {
            gfx_gles2_sprite_tex_load(
                &gfx->food, &sprite->layer[RESOURCE_LAYER_BASE]);
        }
    }

    snake = resource_snake_hmap_find(
        pack->snakes, strview("snake", 0, sizeof("snake") - 1));
    if (snake != NULL)
    {
        struct resource_sprite* sprite;
        struct resource_spine*  spine;

        sprite = resource_sprite_hmap_find(
            pack->sprites, str_view(snake->head_sprite));
        if (sprite != NULL)
        {
            gfx_gles2_sprite_tex_load(
                &gfx->head0_base, &sprite->layer[RESOURCE_LAYER_BASE]);
            gfx_gles2_sprite_tex_load(
                &gfx->head0_gather, &sprite->layer[RESOURCE_LAYER_GATHER]);
        }

        sprite = resource_sprite_hmap_find(
            pack->sprites, strlist_view(snake->body_sprites, 0));
        if (sprite != NULL)
        {
            gfx_gles2_sprite_tex_load(
                &gfx->body0_base, &sprite->layer[RESOURCE_LAYER_BASE]);
        }

        spine = resource_spine_hmap_find(pack->spines, str_view(snake->spine));
        if (spine != NULL)
        {
            gfx_gles2_spine_load(&gfx->spine, spine, shader);
        }
    }

#if defined(CLITHER_GFX_DEBUG)
    gfx_gles2_debug_load(&gfx->debug);
#endif

    return 0;

sprite_mat_load_failed:
    gfx_gles2_sprite_shadow_unload(&gfx->sprite_shadow_mat);
sprite_shadow_load_failed:
    gfx_gles2_background_unload(&gfx->background);
bg_load_failed:
    gfx_gles2_font_unload(&gfx->font);
font_load_failed:
    return -1;
}

/* ------------------------------------------------------------------------- */
static void gfx_gles2_unload_resource_pack(
    struct gfx* gfx, const struct resource_pack* pack)
{
    struct resource_snake* snake;

#if defined(CLITHER_GFX_DEBUG)
    gfx_gles2_debug_unload(&gfx->debug);
#endif

    snake = resource_snake_hmap_find(
        pack->snakes, strview("snake", 0, sizeof("snake") - 1));
    if (snake != NULL)
    {
        struct resource_sprite* sprite;
        struct resource_spine*  spine;

        spine = resource_spine_hmap_find(pack->spines, str_view(snake->spine));
        if (spine != NULL)
        {
            gfx_gles2_spine_unload(&gfx->spine);
        }

        sprite = resource_sprite_hmap_find(
            pack->sprites, strlist_view(snake->body_sprites, 0));
        if (sprite != NULL)
        {
            gfx_gles2_sprite_tex_unload(&gfx->body0_base);
        }

        sprite = resource_sprite_hmap_find(
            pack->sprites, str_view(snake->head_sprite));
        if (sprite != NULL)
        {
            gfx_gles2_sprite_tex_unload(&gfx->head0_base);
            gfx_gles2_sprite_tex_unload(&gfx->head0_gather);
        }
    }

    if (str_len(pack->food.sprite) > 0)
    {
        struct resource_sprite* sprite = resource_sprite_hmap_find(
            pack->sprites, str_view(pack->food.sprite));
        if (sprite != NULL)
            gfx_gles2_sprite_tex_unload(&gfx->food);
    }

    gfx_gles2_sprite_mat_unload(&gfx->sprite_mat);
    gfx_gles2_sprite_shadow_unload(&gfx->sprite_shadow_mat);
    gfx_gles2_background_unload(&gfx->background);
    gfx_gles2_font_unload(&gfx->font);
}

/* ------------------------------------------------------------------------- */
static int gfx_gles2_global_init(void)
{
#if defined(CLITHER_DEBUG_MEMORY)
    if (tracker_gfx_init() < 0)
        goto tracker_gfx_init_failed;
#endif

    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
    {
        log_err("Failed to initialize GLFW\n");
        goto glfw_init_failed;
    }

    return 0;

glfw_init_failed:
    glfwSetErrorCallback(NULL);
#if defined(CLITHER_DEBUG_MEMORY)
    tracker_gfx_deinit();
tracker_gfx_init_failed:
#endif
    return -1;
}

/* ------------------------------------------------------------------------- */
static void gfx_gles2_global_deinit(void)
{
    glfwTerminate();
    glfwSetErrorCallback(NULL);
    tracker_gfx_deinit();
}

/* ------------------------------------------------------------------------- */
static struct gfx* gfx_gles2_create(int initial_width, int initial_height)
{
    int         fbwidth, fbheight;
    struct gfx* gfx = mem_alloc(sizeof *gfx);

    /* It appears GLFW will automatically use OpenGL ES 2.0 if necessary, and
     * use regular OpenGL for desktop. This is fine so far.
     *
     *   glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
     *   glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
     *   glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
     *   glfwWindowHint(
     *     GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE); // Required for GL ES
     */

    gfx->window =
        glfwCreateWindow(initial_width, initial_height, "Clither", NULL, NULL);
    if (gfx->window == NULL)
    {
        log_err("Failed to create Window\n");
        goto create_window_failed;
    }

    glfwMakeContextCurrent(gfx->window);
    if (gladLoadGLES2((GLADloadfunc)glfwGetProcAddress) == 0)
    {
        log_err("GLES2 loader failed\n");
        goto load_gles2_ext_failed;
    }

    log_info("Using GLFW version %s\n", glfwGetVersionString());
    log_info("OpenGL version %s\n", glGetString(GL_VERSION));

    glfwGetFramebufferSize(gfx->window, &fbwidth, &fbheight);
    gfx->width = fbwidth;
    gfx->height = fbheight;
    glViewport(0, 0, fbwidth, fbheight);

    input_init(&gfx->input_buffer);

    if (gfx_gles2_font_init(&gfx->font) != 0)
        goto font_init_failed;
    gfx_text_hmap_init(&gfx->text_hmap);

    gfx_gles2_background_init(
        &gfx->background, fbwidth, fbheight, SHADOW_MAP_SIZE_FACTOR);
    gfx_gles2_quad_mesh_init(&gfx->quad_mesh);
    gfx_gles2_sprite_shadow_init(&gfx->sprite_shadow_mat);
    gfx_gles2_sprite_mat_init(&gfx->sprite_mat);
    gfx_gles2_sprite_tex_init(&gfx->food);
    gfx_gles2_sprite_tex_init(&gfx->head0_base);
    gfx_gles2_sprite_tex_init(&gfx->head0_gather);
    gfx_gles2_sprite_tex_init(&gfx->body0_base);

    gfx_gles2_spine_init(&gfx->spine);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glfwSetWindowUserPointer(gfx->window, gfx);
    glfwSetKeyCallback(gfx->window, key_callback);
    glfwSetMouseButtonCallback(gfx->window, mouse_button_callback);
    glfwSetCursorPosCallback(gfx->window, cursor_position_callback);
    glfwSetScrollCallback(gfx->window, scroll_callback);
    glfwSetFramebufferSizeCallback(gfx->window, framebuffer_size_callback);

#if defined(CLITHER_GFX_DEBUG)
    gfx_gles2_debug_init(&gfx->debug);
#endif

    return gfx;

    gfx_gles2_font_deinit(&gfx->font);
font_init_failed:
load_gles2_ext_failed:
    glfwDestroyWindow(gfx->window);
create_window_failed:
    mem_free(gfx);
    return NULL;
}

/* ------------------------------------------------------------------------- */
static void gfx_gles2_destroy(struct gfx* gfx)
{
    int16_t      slot;
    struct str*  str;
    struct text* text;

#if defined(CLITHER_GFX_DEBUG)
    gfx_gles2_debug_deinit(&gfx->debug);
#endif

    gfx_gles2_spine_deinit(&gfx->spine);

    gfx_gles2_sprite_tex_deinit(&gfx->body0_base);
    gfx_gles2_sprite_tex_deinit(&gfx->head0_gather);
    gfx_gles2_sprite_tex_deinit(&gfx->head0_base);
    gfx_gles2_sprite_tex_deinit(&gfx->food);
    gfx_gles2_sprite_mat_deinit(&gfx->sprite_mat);
    gfx_gles2_sprite_shadow_deinit(&gfx->sprite_shadow_mat);
    gfx_gles2_quad_mesh_deinit(&gfx->quad_mesh);
    gfx_gles2_background_deinit(&gfx->background);

    hmap_for_each (gfx->text_hmap, slot, str, text)
        (void)slot, (void)str, gfx_gles2_text_deinit(text);
    gfx_text_hmap_deinit(gfx->text_hmap);
    gfx_gles2_font_deinit(&gfx->font);

    glfwDestroyWindow(gfx->window);
    mem_free(gfx);
}

/* ------------------------------------------------------------------------- */
static void gfx_gles2_poll_input(struct gfx* gfx, struct input* input)
{
    glfwPollEvents();
    *input = gfx->input_buffer;
    gfx->input_buffer.scroll = 0; /* Clear deltas */

    if (glfwWindowShouldClose(gfx->window))
        input->quit = 1;
}

/* ------------------------------------------------------------------------- */
static struct cmd gfx_gles2_next_cmd(
    const struct gfx*    gfx,
    const struct input*  input,
    const struct camera* camera,
    struct cmd           prev,
    struct qwpos         snake_head)
{
    float       a, d, dx, dy;
    int         max_dist;
    struct spos snake_head_screen;

    /* world -> camera space */
    struct qwpos pos_cameraSpace;
    pos_cameraSpace.x =
        qw_mul(qw_sub(snake_head.x, camera->pos.x), camera->scale);
    pos_cameraSpace.y =
        qw_mul(qw_sub(snake_head.y, camera->pos.y), camera->scale);

    /* camera space -> screen space + keep aspect ratio */
    if (gfx->width < gfx->height)
    {
        int pad = (gfx->height - gfx->width) / 2;
        snake_head_screen.x =
            qw_mul_to_int(pos_cameraSpace.x, make_qw(gfx->width)) +
            (gfx->width / 2);
        snake_head_screen.y =
            qw_mul_to_int(pos_cameraSpace.y, make_qw(-gfx->width)) +
            (gfx->width / 2 + pad);
    }
    else
    {
        int pad = (gfx->width - gfx->height) / 2;
        snake_head_screen.x =
            qw_mul_to_int(pos_cameraSpace.x, make_qw(gfx->height)) +
            (gfx->height / 2 + pad);
        snake_head_screen.y =
            qw_mul_to_int(pos_cameraSpace.y, make_qw(-gfx->height)) +
            (gfx->height / 2);
    }

    /* Scale the speed vector to a quarter of the screen's size */
    max_dist = gfx->width > gfx->height ? gfx->height / 4 : gfx->width / 4;

    /* Calc angle and distance from mouse position and snake head position */
    dx = input->mousex - snake_head_screen.x;
    dy = snake_head_screen.y - input->mousey;
    a = atan2(dy, dx);
    d = sqrt(dx * dx + dy * dy);
    if (d > max_dist)
        d = max_dist;

    return cmd_make(
        prev,
        a,
        d / max_dist,
        input->boost ? CMD_ACTION_BOOST : CMD_ACTION_NONE);
}

/* ------------------------------------------------------------------------- */
static void gfx_gles2_step_anim(struct gfx* gfx, int sim_tick_rate)
{
    gfx_gles2_step_sprite_anim(&gfx->food, sim_tick_rate);
    gfx_gles2_step_sprite_anim(&gfx->head0_base, sim_tick_rate);
    gfx_gles2_step_sprite_anim(&gfx->body0_base, sim_tick_rate);
}

/* ------------------------------------------------------------------------- */
static void gfx_gles2_draw_world(
    struct gfx* gfx, const struct world* world, const struct camera* camera)
{
    int16_t             idx;
    uint16_t            snake_id;
    const struct snake* snake;
    const struct str*   str;
    struct text*        text;

    struct aspect_ratio ar = {1.0, 1.0, 0.0, 0.0};
    if (gfx->width > gfx->height)
    {
        ar.scale_x = (GLfloat)gfx->width / gfx->height;
        ar.pad_x = (ar.scale_x - 1.0) / 2.0;
    }
    else
    {
        ar.scale_y = (GLfloat)gfx->height / gfx->width;
        ar.pad_y = (ar.scale_y - 1.0) / 2.0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, gfx->background.fbo);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    bmap_for_each (world->snakes, idx, snake_id, snake)
    {
        (void)snake_id;
        if (snake_is_dead(snake))
            continue;
        gfx_gles2_draw_snake_shadow(
            snake, gfx, camera, &ar, SHADOW_MAP_SIZE_FACTOR);
    }
    gfx_gles2_draw_food_shadows(
        world->food_bmap, gfx, camera, &ar, SHADOW_MAP_SIZE_FACTOR);

    gfx_gles2_background_draw(world, gfx, camera, &ar, SHADOW_MAP_SIZE_FACTOR);

    gfx_gles2_draw_food(world->food_bmap, gfx, camera, &ar);

    bmap_for_each (world->snakes, idx, snake_id, snake)
    {
        (void)snake_id;
        if (snake_is_dead(snake))
            continue;
        gfx_gles2_draw_snake(snake, gfx, camera, &ar);
        gfx_gles2_draw_snake_spine(snake, gfx, camera, &ar);
    }

    hmap_for_each (gfx->text_hmap, idx, str, text)
        (void)idx, (void)str, text->was_used = 0;
    bmap_for_each (world->snakes, idx, snake_id, snake)
    {
        struct strview name = str_view(snake->data.name);
        switch (gfx_text_hmap_emplace_or_get(&gfx->text_hmap, name, &text))
        {
            case HMAP_NEW:
                gfx_gles2_text_init(text);
                gfx_gles2_text_shape(
                    text, &gfx->font, str_cstr(snake->data.name));
            /* fallthrough */
            case HMAP_EXISTS:
                text->was_used = 1;
                break;
                /* fallthrough */
            case HMAP_OOM: break;
        }
    }
    hmap_for_each (gfx->text_hmap, idx, str, text)
        if (text->was_used == 0)
        {
            (void)str;
            gfx_gles2_text_deinit(text);
            gfx_text_hmap_erase_slot(gfx->text_hmap, idx);
        }

    gfx_gles2_text_prepare_draw(&gfx->font, &ar);
    hmap_for_each (gfx->text_hmap, idx, str, text)
        gfx_gles2_text_draw(
            text, &gfx->font, snake->head.pos, 0, 0.1, 0.15, camera);
    gfx_gles2_text_end_draw();

#if defined(CLITHER_GFX_DEBUG)
    gfx_gles2_debug_draw(&gfx->debug, &gfx->quad_mesh, camera, &ar);
#endif

    glfwSwapBuffers(gfx->window);
}

/* ------------------------------------------------------------------------- */
#if defined(CLITHER_GFX_DEBUG)
static void gfx_gles2_draw_debug_circle(
    struct gfx* gfx, const struct qwpos pos, qw radius, uint32_t rgba)
{
    struct debug_circle* circle = debug_circle_vec_emplace(&gfx->debug.circles);
    if (circle == NULL)
        return;
    circle->pos = pos;
    circle->radius = radius;
    circle->rgba = rgba;
}
#endif

/* ------------------------------------------------------------------------- */
const struct gfx_interface gfx_gles2 = {
    GFX_NAME,
    &gfx_gles2_global_init,
    &gfx_gles2_global_deinit,
    &gfx_gles2_create,
    &gfx_gles2_destroy,
    &gfx_gles2_load_resource_pack,
    &gfx_gles2_unload_resource_pack,
    &gfx_gles2_poll_input,
    &gfx_gles2_next_cmd,
    &gfx_gles2_step_anim,
    &gfx_gles2_draw_world,
#if defined(CLITHER_GFX_DEBUG)
    &gfx_gles2_draw_debug_circle
#endif
};
