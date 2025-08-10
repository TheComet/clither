#include "./internal/food.h"
#include "./internal/gfx.h"
#include "./internal/snake.h"
#include "GLFW/glfw3.h"
#include "clither/game/camera.h"
#include "clither/game/qwpos_vec.h"
#include "clither/game/resource_pack.h"
#include "clither/game/snake.h"
#include "clither/game/snake_bmap.h"
#include "clither/game/world.h"
#include "clither/gfx/gfx.h"
#include "clither/ui/ui.h"
#include "clither/util/hmap_str.h"
#include "clither/util/log.h"
#include "clither/util/str.h"
#include "clither/util/strlist.h"
#include "clither/util/tracker.h"
#include "internal/rectangle.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#if defined(__EMSCRIPTEN__)
#    include "emscripten.h"
#    include "emscripten/html5.h"
#endif

#define GFX_NAME "OpenGL ES 2.0"

enum
{
    SHADOW_MAP_SIZE_FACTOR = 4
};

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
void gfx_track_tex(GLuint tex, const char* name)
    {tracker_track(g_tracker_gfx.tex, (void*)(uintptr_t)tex, 0, name);}
void gfx_track_buf(GLuint buf, const char* name)
    {tracker_track(g_tracker_gfx.buf, (void*)(uintptr_t)buf, 0, name);}
void gfx_track_fbo(GLuint fbo, const char* name)
    {tracker_track(g_tracker_gfx.fbo, (void*)(uintptr_t)fbo, 0, name);}
void gfx_track_shader(GLuint shader, const char* name)
    {tracker_track(g_tracker_gfx.shader, (void*)(uintptr_t)shader, 0, name);}

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
        case GLFW_KEY_F1:
            if (action == GLFW_PRESS)
                gfx->input_buffer.next_gfx_backend = 1;
            break;
        case GLFW_KEY_F4:
            gfx->input_buffer.debug_gfx = (action == GLFW_PRESS);
            break;
        case GLFW_KEY_SPACE:
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

        case GLFW_KEY_A:
            if (action == GLFW_PRESS)
                gfx->input_buffer.mousex = -1.0;
            if (action == GLFW_RELEASE)
                gfx->input_buffer.mousex = 0.0;
            break;
        case GLFW_KEY_D:
            if (action == GLFW_PRESS)
                gfx->input_buffer.mousex = 1.0;
            if (action == GLFW_RELEASE)
                gfx->input_buffer.mousex = 0.0;
            break;
        case GLFW_KEY_S:
            if (action == GLFW_PRESS)
                gfx->input_buffer.mousey = -1.0;
            if (action == GLFW_RELEASE)
                gfx->input_buffer.mousey = 0.0;
            break;
        case GLFW_KEY_W:
            if (action == GLFW_PRESS)
                gfx->input_buffer.mousey = 1.0;
            if (action == GLFW_RELEASE)
                gfx->input_buffer.mousey = 0.0;
            break;

        case GLFW_KEY_ENTER:
            if (action == GLFW_PRESS || action == GLFW_REPEAT)
                gfx->input_buffer.enter = 1;
            break;
        case GLFW_KEY_BACKSPACE:
            if (action == GLFW_PRESS || action == GLFW_REPEAT)
                gfx->input_buffer.backspace = 1;
            break;
        case GLFW_KEY_ESCAPE:
            gfx->input_buffer.escape = (action == GLFW_PRESS);
            break;
    }
}

/* ------------------------------------------------------------------------- */
static void set_char_callback(GLFWwindow* window, unsigned int codepoint)
{
    struct gfx* gfx = glfwGetWindowUserPointer(window);
    codepoint_vec_push(&gfx->input_buffer.keys, codepoint);
}

/* ------------------------------------------------------------------------- */
static void
mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    struct gfx* gfx = glfwGetWindowUserPointer(window);
    (void)mods;

    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        gfx->input_buffer.boost = (action == GLFW_PRESS);
        gfx->input_buffer.mouse_down = (action == GLFW_PRESS);

        if (action == GLFW_PRESS)
            gfx->input_buffer.screen_clicked = 1;
    }
}

/* ------------------------------------------------------------------------- */
static void
cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    struct gfx* gfx = glfwGetWindowUserPointer(window);

    gfx->input_buffer.mousex = xpos * 2.0 / gfx->width - 1.0;
    gfx->input_buffer.mousey = 1.0 - ypos * 2.0 / gfx->height;

    if (gfx->width < gfx->height)
    {
        double padding = gfx->height - gfx->width;
        ypos -= padding / 2;
        xpos /= gfx->width;
        ypos /= gfx->width;
    }
    else
    {
        double padding = gfx->width - gfx->height;
        xpos -= padding / 2;
        xpos /= gfx->height;
        ypos /= gfx->height;
    }

    gfx->input_buffer.mousex_ar = 2.0 * xpos - 1.0;
    gfx->input_buffer.mousey_ar = 1.0 - 2.0 * ypos;

    gfx->input_buffer.mouse_moved = 1;
}

/* ------------------------------------------------------------------------- */
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    struct gfx* gfx = glfwGetWindowUserPointer(window);
    (void)xoffset;
    gfx->input_buffer.scroll += (int)yoffset;
}

/* ------------------------------------------------------------------------- */
#if defined(__EMSCRIPTEN__)
static struct spos get_web_canvas_size(void)
{
    double w, h;
    emscripten_get_element_css_size("#canvas", &w, &h);
    return make_spos((int)w, (int)h);
}
static EM_BOOL on_web_display_size_changed(
    int event_type, const EmscriptenUiEvent* event, void* user_data)
{
    struct gfx* gfx = (struct gfx*)user_data;
    struct spos size = get_web_canvas_size();
    glfwSetWindowSize(gfx->window, size.x, size.y);
    return EM_TRUE;
}
#endif

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

    snake = resource_snake_hmap_find(
        pack->snakes, strview("snake", 0, sizeof("snake") - 1));
    if (snake != NULL)
    {
        gfx_gles2_snake_load(&gfx->snake, snake, pack, shader);
    }

    gfx_gles2_food_load(&gfx->food, pack);

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
    (void)pack;

#if defined(CLITHER_GFX_DEBUG)
    gfx_gles2_debug_unload(&gfx->debug);
#endif

    gfx_gles2_food_unload(&gfx->food);
    gfx_gles2_snake_unload(&gfx->snake);
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
#if defined(CLITHER_DEBUG_MEMORY)
    tracker_gfx_deinit();
#endif
}

/* ------------------------------------------------------------------------- */
static struct gfx* gfx_gles2_create(int initial_width, int initial_height)
{
    int         fbwidth, fbheight;
    struct gfx* gfx = mem_alloc(sizeof *gfx);

#if defined(__EMSCRIPTEN__)
    struct spos size = get_web_canvas_size();
    initial_width = size.x;
    initial_height = size.y;
    emscripten_set_resize_callback(
        EMSCRIPTEN_EVENT_TARGET_WINDOW, gfx, 0, on_web_display_size_changed);
#endif

#if defined(__EMSCRIPTEN__)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    /* Required for GL ES */
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
#endif

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

    gfx_gles2_background_init(
        &gfx->background, fbwidth, fbheight, SHADOW_MAP_SIZE_FACTOR);
    gfx_gles2_quad_mesh_init(&gfx->quad_mesh);
    gfx_gles2_sprite_shadow_init(&gfx->sprite_shadow_mat);
    gfx_gles2_sprite_mat_init(&gfx->sprite_mat);
    gfx_gles2_snake_init(&gfx->snake);
    gfx_gles2_food_init(&gfx->food);
    gfx_gles2_rectangle_init(&gfx->rect);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glfwSetWindowUserPointer(gfx->window, gfx);
    glfwSetKeyCallback(gfx->window, key_callback);
    glfwSetCharCallback(gfx->window, set_char_callback);
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
#if defined(CLITHER_GFX_DEBUG)
    gfx_gles2_debug_deinit(&gfx->debug);
#endif

    gfx_gles2_rectangle_deinit(&gfx->rect);
    gfx_gles2_food_deinit(&gfx->food);
    gfx_gles2_snake_deinit(&gfx->snake);
    gfx_gles2_sprite_mat_deinit(&gfx->sprite_mat);
    gfx_gles2_sprite_shadow_deinit(&gfx->sprite_shadow_mat);
    gfx_gles2_quad_mesh_deinit(&gfx->quad_mesh);
    gfx_gles2_background_deinit(&gfx->background);
    gfx_gles2_font_deinit(&gfx->font);

    glfwDestroyWindow(gfx->window);
    input_deinit(&gfx->input_buffer);
    mem_free(gfx);
}

/* ------------------------------------------------------------------------- */
static void gfx_gles2_poll_input(struct gfx* gfx, struct input* input)
{
    glfwPollEvents();
    input_set_and_clear(input, &gfx->input_buffer);

    if (glfwWindowShouldClose(gfx->window))
        input->quit = 1;
}

/* ------------------------------------------------------------------------- */
static void gfx_gles2_step_anim(struct gfx* gfx, int sim_tick_rate)
{
    gfx_gles2_food_step_anim(&gfx->food, sim_tick_rate);
    gfx_gles2_snake_step_anim(&gfx->snake, sim_tick_rate);
}

/* ------------------------------------------------------------------------- */
static void gfx_gles2_draw_begin(struct gfx* gfx)
{
    glBindFramebuffer(GL_FRAMEBUFFER, gfx->background.fbo);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glClear(GL_COLOR_BUFFER_BIT);
}

/* ------------------------------------------------------------------------- */
static struct aspect_ratio calculate_aspect_ratio(const struct gfx* gfx)
{
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
    return ar;
}

/* ------------------------------------------------------------------------- */
static void gfx_gles2_draw_world(
    struct gfx* gfx, const struct world* world, const struct camera* camera)
{
    int16_t             idx;
    uint16_t            snake_id;
    const struct snake* snake;
    struct aspect_ratio ar = calculate_aspect_ratio(gfx);

    /* Shadows render to a separate frame buffer */
#if 0
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
#endif

    /* Background uses the shadow frame buffer */
    gfx_gles2_background_draw(world, gfx, camera, &ar, SHADOW_MAP_SIZE_FACTOR);
    gfx_gles2_draw_food(world->food_bmap, gfx, &gfx->food, camera, &ar);

    /* Snakes */
    bmap_for_each (world->snakes, idx, snake_id, snake)
    {
        (void)snake_id;
        if (snake_is_dead(snake))
            continue;
        gfx_gles2_draw_snake(&gfx->snake, gfx, snake, camera, &ar);
    }

    /* Snake usernames */
    gfx_gles2_text_prepare_draw(&gfx->font, &ar);
    bmap_for_each (world->snakes, idx, snake_id, snake)
    {
        gfx_gles2_text_draw(
            str_view(snake->data.name),
            &gfx->font,
            snake->head.pos,
            make_fpos(0, 0.1),
            1.0 / 64,
            0xA0FFFFFF,
            UI_ALIGN_CENTER,
            camera);
    }
    gfx_gles2_text_end_draw();

#if defined(CLITHER_GFX_DEBUG)
    gfx_gles2_debug_draw(gfx, &gfx->debug, &gfx->quad_mesh, camera, &ar);
#endif
}

/* ------------------------------------------------------------------------- */
static void gfx_gles2_draw_ui(struct gfx* gfx, const struct ui* ui)
{
    const struct ui_element* ui_elem;
    struct aspect_ratio      ar = calculate_aspect_ratio(gfx);

    ui_for_each_active (ui, ui_elem)
        switch (ui_elem->type)
        {
            case UI_CONTROLLER: break;
            case UI_RECTANGLE:
                gfx_gles2_rectangle_draw(
                    &gfx->rect,
                    &gfx->quad_mesh,
                    ui_elem->u.rectangle.pos,
                    ui_elem->u.rectangle.size,
                    ui_elem->u.rectangle.color,
                    &ar);
                break;
            case UI_TEXT: break;
            case UI_TEXTINPUT: {
                struct fpos screen_size, cursor_size;
                GLfloat     cursor_offset, text_scale;

                if (!ui_elem->u.textinput.blink_on)
                    break;

                text_scale = ui_elem->u.textinput.text.scale;
                cursor_offset = text_scale + 1.0 / gfx->width * 2;
                screen_size = gfx_gles2_text_screen_size(
                    &gfx->font,
                    str_view(ui_elem->u.textinput.text.str),
                    text_scale);
                /* twice as high as the text looks right */
                cursor_size = make_fpos(text_scale, screen_size.y * 2);

                gfx_gles2_rectangle_draw(
                    &gfx->rect,
                    &gfx->quad_mesh,
                    make_fpos(
                        ui_elem->u.textinput.text.pos.x + screen_size.x +
                            cursor_offset,
                        ui_elem->u.textinput.text.pos.y + cursor_size.y / 2),
                    cursor_size,
                    ui_elem->u.textinput.text.color,
                    &ar);
                break;
            }
            case UI_BUTTON: break;
            case UI_SLIDER: {
                GLfloat x1 = ui_elem->u.slider.start.x;
                GLfloat x2 = ui_elem->u.slider.end.x;
                GLfloat y1 = ui_elem->u.slider.start.y;
                GLfloat y2 = ui_elem->u.slider.end.y;
                GLfloat t = ui_elem->u.slider.value;
                GLfloat x = x1 + (x2 - x1) * t;
                GLfloat y = y1 + (y2 - y1) * t;
                GLfloat h = 1.0 / gfx->width * 2;

                gfx_gles2_rectangle_draw(
                    &gfx->rect,
                    &gfx->quad_mesh,
                    make_fpos(x1 + (x2 - x1) / 2, y1 + (y2 - y1) / 2),
                    y1 == y2 ? make_fpos((x2 - x1) / 2, h)
                             : make_fpos(h, (y2 - y1) / 2),
                    ui_elem->u.slider.normal_color,
                    &ar);
                gfx_gles2_rectangle_draw(
                    &gfx->rect,
                    &gfx->quad_mesh,
                    make_fpos(x, y),
                    make_fpos(
                        ui_elem->u.slider.knob_diameter / 2,
                        ui_elem->u.slider.knob_diameter / 2),
                    ui_elem->u.slider.color,
                    &ar);
                break;
            }
        }

    gfx_gles2_text_prepare_draw(&gfx->font, &ar);
    ui_for_each_active (ui, ui_elem)
        switch (ui_elem->type)
        {
            case UI_CONTROLLER: break;
            case UI_RECTANGLE: break;
            case UI_TEXT:
                gfx_gles2_text_draw_screen(
                    str_view(ui_elem->u.text.str),
                    &gfx->font,
                    ui_elem->u.text.pos,
                    ui_elem->u.text.scale,
                    ui_elem->u.text.color,
                    ui_elem->u.text.align);
                break;
            case UI_TEXTINPUT:
                gfx_gles2_text_draw_screen(
                    str_view(ui_elem->u.textinput.text.str),
                    &gfx->font,
                    ui_elem->u.textinput.text.pos,
                    ui_elem->u.textinput.text.scale,
                    ui_elem->u.textinput.text.color,
                    ui_elem->u.textinput.text.align);
                break;
            case UI_BUTTON:
                gfx_gles2_text_draw_screen(
                    str_view(ui_elem->u.button.text.str),
                    &gfx->font,
                    ui_elem->u.button.text.pos,
                    ui_elem->u.button.text.scale,
                    ui_elem->u.button.text.color,
                    ui_elem->u.button.text.align);
                break;
            case UI_SLIDER: break;
        }
    gfx_gles2_text_end_draw();
}

/* ------------------------------------------------------------------------- */
static void gfx_gles2_draw_end(struct gfx* gfx)
{
    gfx_gles2_text_clear_unused_from_cache(&gfx->font);
    glfwSwapBuffers(gfx->window);
}

/* ------------------------------------------------------------------------- */
#if defined(CLITHER_GFX_DEBUG)
static void gfx_gles2_draw_debug_circle(
    struct gfx* gfx, struct qwpos pos, qw radius, uint32_t argb)
{
    struct debug_circle* circle = debug_circle_vec_emplace(&gfx->debug.circles);
    if (circle == NULL)
        return;
    circle->pos = pos;
    circle->radius = radius;
    circle->argb = argb;
}
static void gfx_gles2_draw_debug_rectangle(
    struct gfx*  gfx,
    struct qwpos top_left,
    struct qwpos bottom_right,
    uint32_t     argb)
{
    struct debug_rectangle* rect =
        debug_rectangle_vec_emplace(&gfx->debug.rectangles);
    if (rect == NULL)
        return;
    rect->top_left = top_left;
    rect->bottom_right = bottom_right;
    rect->argb = argb;
}
static void gfx_gles2_draw_debug_line(
    struct gfx* gfx, struct qwpos start, struct qwpos end, uint32_t argb)
{
    struct debug_line* line = debug_line_vec_emplace(&gfx->debug.lines);
    if (line == NULL)
        return;
    line->start = start;
    line->end = end;
    line->argb = argb;
}
static void gfx_gles2_draw_debug_text(struct gfx* gfx, const char* text)
{
    strlist_add_cstr(&gfx->debug.strings, text);
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
    &gfx_gles2_step_anim,
    &gfx_gles2_draw_begin,
    &gfx_gles2_draw_world,
    &gfx_gles2_draw_ui,
    &gfx_gles2_draw_end,
#if defined(CLITHER_GFX_DEBUG)
    &gfx_gles2_draw_debug_circle,
    &gfx_gles2_draw_debug_rectangle,
    &gfx_gles2_draw_debug_line,
    &gfx_gles2_draw_debug_text
#endif
};
