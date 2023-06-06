#include "clither/gfx.h"
#include "clither/controls.h"
#include "clither/log.h"

#include "cstructures/memory.h"

#include "glad/gles2.h"
#include "GLFW/glfw3.h"

struct gfx
{
    GLFWwindow* window;

    GLuint sprite_program;
    GLuint sprite_vbo;
    GLuint sprite_ibo;
};

struct vertex
{
    GLfloat pos[2];
    GLfloat uv[2];
};

static const struct vertex quad_vertices[4] = {
    {{-1, -1}, {0,  1}},
    {{-1,  1}, {0,  0}},
    {{ 1, -1}, {1,  1}},
    {{ 1,  1}, {1,  0}}
};
static GLushort quad_indices[6] = {
    0, 2, 1,
    1, 3, 2
};

/* ------------------------------------------------------------------------- */
static void
key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    log_dbg("key press: %d\n", key);
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

/* ------------------------------------------------------------------------- */
static void
framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

/* ------------------------------------------------------------------------- */
static GLuint load_shader_type(const char* code, GLenum type)
{
    GLuint shader;
    GLint compiled;

    shader = glCreateShader(type);
    if (shader == 0)
    {
        log_err("glCreateShader() failed\n");
        return 0;
    }

    glShaderSource(shader, 1, &code, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint info_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1)
        {
            char* info = MALLOC(sizeof(char) * info_len);
            glGetShaderInfoLog(shader, info_len, NULL, info);
            log_err("Failed to compile shader\n%s", info);
            FREE(info);
        }

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}
static GLuint load_shader(const char* vcode, const char* fcode)
{
    GLuint vshader, fshader, program;
    GLint linked;

    vshader = load_shader_type(vcode, GL_VERTEX_SHADER);
    if (vshader == 0)
        goto load_vshader_failed;
    fshader = load_shader_type(fcode, GL_FRAGMENT_SHADER);
    if (fshader == 0)
        goto load_fshader_failed;

    program = glCreateProgram();
    if (program == 0)
    {
        log_err("glCreateProgram() failed\n");
        goto create_program_failed;
    }

    glAttachShader(program, vshader);
    glAttachShader(program, fshader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint info_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1)
        {
            char* info = MALLOC(sizeof(char) * info_len);
            glGetProgramInfoLog(program, info_len, NULL, info);
            log_err("Failed to link shader\n%s\n", info);
            FREE(info);
            goto link_program_failed;
        }
    }

    return program;

link_program_failed:
    glDeleteProgram(program);
    return 0;

create_program_failed:
    glDeleteShader(fshader);
load_fshader_failed:
    glDeleteShader(vshader);
load_vshader_failed:
    return 0;
}

/* ------------------------------------------------------------------------- */
int
gfx_init(void)
{
    if (!glfwInit())
    {
        log_err("Failed to initialize GLFW\n");
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
void
gfx_deinit(void)
{
    glfwTerminate();
}

/* ------------------------------------------------------------------------- */
struct gfx*
gfx_create(int initial_width, int initial_height)
{
    struct gfx* gfx = MALLOC(sizeof *gfx);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);  /* Required for GL ES */

    gfx->window = glfwCreateWindow(initial_width, initial_height, "Clither", NULL, NULL);
    if (gfx->window == NULL)
        goto create_window_failed;

    glfwMakeContextCurrent(gfx->window);
    if (gladLoadGLES2((GLADloadfunc)glfwGetProcAddress) == 0)
    {
        log_err("GLES2 loader failed\n");
        goto load_gles2_ext_failed;
    }

    gfx->sprite_program = load_shader(
        "attribute vec2 vPosition;\n"
        "attribute vec2 vTexCoord;\n"
        "varying vec2 fTexCoord;\n"
        "void main()\n"
        "{\n"
        "    fTexCoord = vTexCoord;\n"
        "    gl_Position = vec4(vPosition, 0.0, 1.0);\n"
        "}\n",
        "precision mediump float;\n"
        "varying vec2 fTexCoord;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(fTexCoord.x, fTexCoord.y, 0.0, 1.0);\n"
        "}\n");
    if (gfx->sprite_program == 0)
        goto load_sprite_program_failed;

    glGenBuffers(1, &gfx->sprite_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gfx->sprite_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, uv));
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &gfx->sprite_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx->sprite_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_indices), quad_indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glViewport(0, 0, initial_width, initial_height);

    glfwSetKeyCallback(gfx->window, key_callback);
    glfwSetFramebufferSizeCallback(gfx->window, framebuffer_size_callback);

    log_info("Using GLFW version %s\n", glfwGetVersionString());
    log_info("OpenGL version %s\n", glGetString(GL_VERSION));
    
    return gfx;

load_sprite_program_failed:
load_gles2_ext_failed:
    glfwDestroyWindow(gfx->window);
create_window_failed:
    FREE(gfx);
    return NULL;
}

/* ------------------------------------------------------------------------- */
void
gfx_destroy(struct gfx* gfx)
{
    glfwDestroyWindow(gfx->window);
    FREE(gfx);
}

/* ------------------------------------------------------------------------- */
void
gfx_poll_input(struct gfx* gfx, struct input* input)
{
    glfwPollEvents();
    if (glfwWindowShouldClose(gfx->window))
        input->quit = 1;
}

/* ------------------------------------------------------------------------- */
void
gfx_update_controls(
    struct controls* controls,
    const struct input* input,
    const struct gfx* gfx,
    const struct camera* cam,
    struct qwpos snake_head)
{

}

/* ------------------------------------------------------------------------- */
void
gfx_draw_world(struct gfx* gfx, const struct world* world, const struct camera* camera, uint16_t frame_number)
{
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(gfx->sprite_program);

    glBindBuffer(GL_ARRAY_BUFFER, gfx->sprite_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx->sprite_ibo);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);

    glfwSwapBuffers(gfx->window);
}
