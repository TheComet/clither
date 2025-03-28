#include "./internal/food.h"
#include "./internal/gfx.h"
#include "./internal/snake.h"
#include "GLFW/glfw3.h"
#include "clither/camera.h"
#include "clither/gfx.h"
#include "clither/log.h"
#include "clither/resource_pack.h"
#include "clither/resource_snake_part_vec.h"
#include "clither/snake.h"
#include "clither/snake_bmap.h"
#include "clither/world.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

enum
{
    SHADOW_MAP_SIZE_FACTOR = 4
};

/* ------------------------------------------------------------------------- */
static void error_callback(int error_code, const char* error_msg)
{
    log_warn("GLFW Error %d: %s\n", error_code, error_msg);
}

/* ------------------------------------------------------------------------- */
static void
key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    struct gfx* gfx = glfwGetWindowUserPointer(window);
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (key == GLFW_KEY_LEFT)
        gfx->input_buffer.prev_gfx_backend = (action == GLFW_PRESS);
    if (key == GLFW_KEY_RIGHT)
        gfx->input_buffer.next_gfx_backend = (action == GLFW_PRESS);

    (void)mods;
    (void)scancode;
}

/* ------------------------------------------------------------------------- */
static void
mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    struct gfx* gfx = glfwGetWindowUserPointer(window);
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        gfx->input_buffer.boost = (action == GLFW_PRESS);
    (void)mods;
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
    gfx->input_buffer.scroll += (int)yoffset;
    (void)xoffset;
}

/* ------------------------------------------------------------------------- */
static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    struct gfx* gfx = glfwGetWindowUserPointer(window);
    gfx->width = width;
    gfx->height = height;
    glViewport(0, 0, width, height);

    /* Resize shadow framebuffer */
    glDeleteTextures(1, &gfx->background.texShadow);
    glGenTextures(1, &gfx->background.texShadow);
    glBindTexture(GL_TEXTURE_2D, gfx->background.texShadow);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB,
        width / SHADOW_MAP_SIZE_FACTOR,
        height / SHADOW_MAP_SIZE_FACTOR,
        0,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, gfx->background.fbo);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        gfx->background.texShadow,
        0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static int
gfx_gles2_load_resource_pack(struct gfx* gfx, const struct resource_pack* pack)
{
    if (gfx_gles2_background_load(&gfx->background, pack) < 0)
        goto bg_load_failed;
    if (gfx_gles2_sprite_shadow_load(&gfx->sprite_shadow_mat, pack) < 0)
        goto sprite_shadow_load_failed;
    if (gfx_gles2_sprite_mat_load(&gfx->sprite_mat, pack) < 0)
        goto sprite_mat_load_failed;

    if (pack->sprites.food)
        gfx_gles2_sprite_tex_load(&gfx->food, pack->sprites.food);

    if (vec_count(pack->sprites.heads) > 0)
    {
        struct resource_snake_part* head = vec_first(pack->sprites.heads);
        gfx_gles2_sprite_tex_load(&gfx->head0_base, head->base);
        gfx_gles2_sprite_tex_load(&gfx->head0_gather, head->gather);
    }
    if (vec_count(pack->sprites.bodies) > 0)
    {
        struct resource_snake_part* body = vec_first(pack->sprites.bodies);
        gfx_gles2_sprite_tex_load(&gfx->body0_base, body->base);
    }

    return 0;

sprite_mat_load_failed:
    gfx_gles2_sprite_shadow_unload(&gfx->sprite_shadow_mat);
sprite_shadow_load_failed:
    gfx_gles2_background_unload(&gfx->background);
bg_load_failed:
    return -1;
}

static int gfx_gles2_global_init(void)
{
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
    {
        log_err("Failed to initialize GLFW\n");
        return -1;
    }

    return 0;
}

static void gfx_gles2_global_deinit(void)
{
    glfwTerminate();
    glfwSetErrorCallback(NULL);
}

static struct gfx* gfx_gles2_create(int initial_width, int initial_height)
{
    FT_Error    ft_error;
    int         fbwidth, fbheight;
    struct gfx* gfx = mem_alloc(sizeof *gfx);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(
        GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE); /* Required for GL ES */

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

    ft_error = FT_Init_FreeType(&gfx->ft_lib);
    if (ft_error)
    {
        log_err("Failed to initialize FreeType library.\n");
        goto ft_init_failed;
    }

    glfwGetFramebufferSize(gfx->window, &fbwidth, &fbheight);
    gfx->width = fbwidth;
    gfx->height = fbheight;
    glViewport(0, 0, fbwidth, fbheight);

    gfx_gles2_background_init(
        &gfx->background, fbwidth, fbheight, SHADOW_MAP_SIZE_FACTOR);
    gfx_gles2_quad_mesh_init(&gfx->quad_mesh);
    gfx_gles2_sprite_shadow_init(&gfx->sprite_shadow_mat);
    gfx_gles2_sprite_mat_init(&gfx->sprite_mat);
    gfx_gles2_sprite_tex_init(&gfx->food);
    gfx_gles2_sprite_tex_init(&gfx->head0_base);
    gfx_gles2_sprite_tex_init(&gfx->head0_gather);
    gfx_gles2_sprite_tex_init(&gfx->body0_base);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    input_init(&gfx->input_buffer);

    glfwSetWindowUserPointer(gfx->window, gfx);
    glfwSetKeyCallback(gfx->window, key_callback);
    glfwSetMouseButtonCallback(gfx->window, mouse_button_callback);
    glfwSetCursorPosCallback(gfx->window, cursor_position_callback);
    glfwSetScrollCallback(gfx->window, scroll_callback);
    glfwSetFramebufferSizeCallback(gfx->window, framebuffer_size_callback);

    return gfx;

ft_init_failed:
load_gles2_ext_failed:
    glfwDestroyWindow(gfx->window);
create_window_failed:
    mem_free(gfx);
    return NULL;
}

static void gfx_gles2_destroy(struct gfx* gfx)
{
    gfx_gles2_sprite_tex_deinit(&gfx->head0_base);
    gfx_gles2_sprite_tex_deinit(&gfx->head0_gather);
    gfx_gles2_sprite_tex_deinit(&gfx->body0_base);

    gfx_gles2_sprite_mat_deinit(&gfx->sprite_mat);
    gfx_gles2_sprite_shadow_deinit(&gfx->sprite_shadow_mat);
    gfx_gles2_quad_mesh_deinit(&gfx->quad_mesh);
    gfx_gles2_background_deinit(&gfx->background);

    FT_Done_FreeType(gfx->ft_lib);

    glfwDestroyWindow(gfx->window);
    mem_free(gfx);
}

static void gfx_gles2_poll_input(struct gfx* gfx, struct input* input)
{
    glfwPollEvents();
    *input = gfx->input_buffer;
    gfx->input_buffer.scroll = 0; /* Clear deltas */

    if (glfwWindowShouldClose(gfx->window))
        input->quit = 1;
}

static struct cmd gfx_gles2_input_to_cmd(
    struct cmd           prev,
    const struct input*  input,
    const struct gfx*    gfx,
    const struct camera* camera,
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
        (void)snake_id, gfx_gles2_draw_snake_shadow(
                            snake, gfx, camera, &ar, SHADOW_MAP_SIZE_FACTOR);
    gfx_gles2_draw_food_shadows(
        &world->food_grid, gfx, camera, &ar, SHADOW_MAP_SIZE_FACTOR);

    gfx_gles2_draw_background(world, gfx, camera, &ar, SHADOW_MAP_SIZE_FACTOR);
    gfx_gles2_draw_food(&world->food_grid, gfx, camera, &ar);

    bmap_for_each (world->snakes, idx, snake_id, snake)
        (void)snake_id, gfx_gles2_draw_snake(snake, gfx, camera, &ar);

    glfwSwapBuffers(gfx->window);
}

/* ------------------------------------------------------------------------- */
struct gfx_interface gfx_gles2 = {
    "OpenGL ES 2.0",
    &gfx_gles2_global_init,
    &gfx_gles2_global_deinit,
    &gfx_gles2_create,
    &gfx_gles2_destroy,
    &gfx_gles2_load_resource_pack,
    &gfx_gles2_poll_input,
    &gfx_gles2_input_to_cmd,
    &gfx_gles2_step_anim,
    &gfx_gles2_draw_world};
