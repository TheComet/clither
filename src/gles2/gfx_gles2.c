#include "clither/bezier.h"
#include "clither/camera.h"
#include "clither/command.h"
#include "clither/fs.h"
#include "clither/gfx.h"
#include "clither/input.h"
#include "clither/log.h"
#include "clither/resource_pack.h"
#include "clither/snake.h"
#include "clither/world.h"

#include "cstructures/memory.h"
#include "cstructures/string.h"

#include "glad/gles2.h"
#include "GLFW/glfw3.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define SHADOW_MAP_SIZE_FACTOR 4

struct bg
{
    GLuint program;
    GLuint vbo;
    GLuint ibo;
    GLuint fbo;
    GLuint texShadow;
    GLuint texCol;
    GLuint texNor;
    GLuint uAspectRatio;
    GLuint uCamera;
    GLuint uShadowInvRes;
    GLuint sShadow;
    GLuint sCol;
    GLuint sNM;
};

struct sprite_mesh
{
    GLuint vbo;
    GLuint ibo;
};

struct sprite_shadow
{
    GLuint program;
    GLuint uAspectRatio;
    GLuint uPosCameraSpace;
    GLuint uDir;
    GLuint uSize;
    GLuint uAnim;
    GLuint sNM;
};

struct sprite_mat
{
    GLuint program;
    GLuint uAspectRatio;
    GLuint uPosCameraSpace;
    GLuint uDir;
    GLuint uSize;
    GLuint uAnim;
    GLuint sCol;
    GLuint sNM;
};

struct sprite_tex
{
    GLuint texDiffuse;
    GLuint texNM;
    int8_t tile_x, tile_y, tile_count, frame;
};

struct gfx
{
    GLFWwindow* window;
    int width, height;

    struct input input_buffer;

    struct bg bg;
    struct sprite_mesh sprite_mesh;
    struct sprite_mat sprite_mat;
    struct sprite_shadow sprite_shadow;
    struct sprite_tex head0_base;
    struct sprite_tex head0_gather;
    struct sprite_tex body0_base;
    struct sprite_tex tail0_base;
};

struct aspect_ratio
{
    float scale_x, scale_y;
    float pad_x, pad_y;
};

struct vertex
{
    GLfloat pos[2];
    GLfloat uv[2];
};

static const struct vertex sprite_vertices[4] = {
    {{-0.125, -0.125}, {0,  1}},
    {{-0.125,  0.125}, {0,  0}},
    {{ 0.125, -0.125}, {1,  1}},
    {{ 0.125,  0.125}, {1,  0}}
};
static const GLushort sprite_indices[6] = {
    0, 2, 1,
    1, 3, 2
};
static const struct vertex bg_vertices[4] = {
    {{-1, -1}, {0,  0}},
    {{-1,  1}, {0,  1}},
    {{ 1, -1}, {1,  0}},
    {{ 1,  1}, {1,  1}}
};
static const GLushort bg_indices[6] = {
    0, 2, 1,
    1, 3, 2
};
static const char* attr_bindings[] = {
    "vPosition",
    "vTexCoord",
    NULL
};

/* ------------------------------------------------------------------------- */
static void
key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)mods;
    (void)scancode;
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

/* ------------------------------------------------------------------------- */
static void
mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    struct gfx* gfx = glfwGetWindowUserPointer(window);
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        gfx->input_buffer.boost = action == GLFW_PRESS;
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
static void
scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    struct gfx* gfx = glfwGetWindowUserPointer(window);
    gfx->input_buffer.scroll += (int)yoffset;
    (void)xoffset;
}

/* ------------------------------------------------------------------------- */
static void
framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    struct gfx* gfx = glfwGetWindowUserPointer(window);
    gfx->width = width;
    gfx->height = height;
    glViewport(0, 0, width, height);

    // Resize shadow framebuffer
    glDeleteTextures(1, &gfx->bg.texShadow);
    glGenTextures(1, &gfx->bg.texShadow);
    glBindTexture(GL_TEXTURE_2D, gfx->bg.texShadow);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width / SHADOW_MAP_SIZE_FACTOR, height / SHADOW_MAP_SIZE_FACTOR, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, gfx->bg.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gfx->bg.texShadow, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/* ------------------------------------------------------------------------- */
static GLuint load_shader_type(const char* code, GLint length, GLenum type)
{
    GLuint shader;
    GLint compiled;

    shader = glCreateShader(type);
    if (shader == 0)
    {
        log_err("glCreateShader() failed\n");
        return 0;
    }

    glShaderSource(shader, 1, &code, &length);
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
static GLuint load_shader(char* shaders[], const char* attribute_bindings[])
{
    int i;
    GLuint program;
    GLint linked;

    program = glCreateProgram();
    if (program == 0)
    {
        log_err("glCreateProgram() failed\n");
        goto create_program_failed;
    }

    for (i = 0; shaders[i]; ++i)
    {
        int length;
        GLuint shader;
        void* source = fs_map_file(shaders[i], &length);
        if (source == NULL)
            goto load_shaders_failed;

        shader = load_shader_type(source, length, string_ends_with(shaders[i], ".vsh") ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
        fs_unmap_file(source, length);
        if (shader == 0)
            goto load_shaders_failed;

        glAttachShader(program, shader);
        glDeleteShader(shader);
    }

    for (i = 0; attribute_bindings[i]; ++i)
        glBindAttribLocation(program, i, attribute_bindings[i]);

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
load_shaders_failed:
    glDeleteProgram(program);
create_program_failed:
    return 0;
}

/* ------------------------------------------------------------------------- */
static GLuint
get_uniform_location_and_warn(GLuint program, const char* name)
{
    GLuint ret = glGetUniformLocation(program, name);
    if (ret == (GLuint)-1)
        log_warn("Failed to get uniform location of \"%s\"\n", name);
    return ret;
}

/* ------------------------------------------------------------------------- */
static int
bg_init(struct bg* bg, int fbwidth, int fbheight)
{
    memset(bg, 0, sizeof * bg);

    /* Set up shadow framebuffer */
    glGenTextures(1, &bg->texShadow);
    glBindTexture(GL_TEXTURE_2D, bg->texShadow);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fbwidth / SHADOW_MAP_SIZE_FACTOR, fbheight / SHADOW_MAP_SIZE_FACTOR, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &bg->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, bg->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bg->texShadow, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        log_err("Incomplete framebuffer!\n");
        goto incomplete_shadow_framebuffer;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    /* Set up quad mesh */
    glGenBuffers(1, &bg->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, bg->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bg_vertices), bg_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &bg->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bg->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(bg_indices), bg_indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    /* Prepare background textures */
    glGenTextures(1, &bg->texCol);
    glBindTexture(GL_TEXTURE_2D, bg->texCol);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &bg->texNor);
    glBindTexture(GL_TEXTURE_2D, bg->texNor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* Set default values for shader uniforms */
    bg->program = 0;
    bg->uAspectRatio = (GLuint)-1;
    bg->uCamera = (GLuint)-1;
    bg->uShadowInvRes = (GLuint)-1;
    bg->sShadow = (GLuint)-1;
    bg->sCol = (GLuint)-1;
    bg->sNM = (GLuint)-1;

    return 0;

incomplete_shadow_framebuffer:
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &bg->fbo);
    glDeleteTextures(1, &bg->texShadow);

    return -1;
}

/* ------------------------------------------------------------------------- */
static void
bg_deinit(struct bg* bg)
{
    glDeleteTextures(1, &bg->texNor);
    glDeleteTextures(1, &bg->texCol);
    glDeleteBuffers(1, &bg->ibo);
    glDeleteBuffers(1, &bg->vbo);
    glDeleteFramebuffers(1, &bg->fbo);
    glDeleteTextures(1, &bg->texShadow);
    if (bg->program != 0)
        glDeleteProgram(bg->program);
}

/* ------------------------------------------------------------------------- */
static void
bg_unload(struct bg* bg)
{
    if (bg->program != 0)
        glDeleteProgram(bg->program);
    bg->program = 0;

    glBindTexture(GL_TEXTURE_2D, bg->texCol);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, bg->texNor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/* ------------------------------------------------------------------------- */
static int
bg_load(struct bg* bg, const struct resource_pack* pack)
{
    int img_width, img_height, img_channels;
    stbi_uc* img_data;

    assert(bg->program == 0);

    /* For now we only support a single background layer */
    if (pack->sprites.background[0] == NULL)
    {
        log_warn("No background texture defined\n");
        return -1;
    }

    /* Load shaders */
    bg->program = load_shader(pack->shaders.glsl.background, attr_bindings);
    if (bg->program == 0)
        return -1;

    bg->uAspectRatio = get_uniform_location_and_warn(bg->program, "uAspectRatio");
    bg->uCamera = get_uniform_location_and_warn(bg->program, "uCamera");
    bg->uShadowInvRes = get_uniform_location_and_warn(bg->program, "uShadowInvRes");
    bg->sShadow = get_uniform_location_and_warn(bg->program, "sShadow");
    bg->sCol = get_uniform_location_and_warn(bg->program, "sCol");
    bg->sNM = get_uniform_location_and_warn(bg->program, "sNM");

    img_data = stbi_load(pack->sprites.background[0]->texture0, &img_width, &img_height, &img_channels, 3);
    if (img_data)
    {
        glBindTexture(GL_TEXTURE_2D, bg->texCol);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img_width, img_height, 0, GL_RGB, GL_UNSIGNED_BYTE, img_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(img_data);
    }
    else
        log_err("Failed to load image \"%s\"\n", pack->sprites.background[0]->texture0);

    img_data = stbi_load(pack->sprites.background[0]->texture1, &img_width, &img_height, &img_channels, 3);
    if (img_data)
    {
        glBindTexture(GL_TEXTURE_2D, bg->texNor);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img_width, img_height, 0, GL_RGB, GL_UNSIGNED_BYTE, img_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(img_data);
    }
    else
        log_err("Failed to load image \"%s\"\n", pack->sprites.background[0]->texture1);

    return 0;
}

/* ------------------------------------------------------------------------- */
static void
sprite_mesh_init(struct sprite_mesh* sm)
{
    glGenBuffers(1, &sm->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, sm->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sprite_vertices), sprite_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &sm->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sm->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(sprite_indices), sprite_indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

/* ------------------------------------------------------------------------- */
static void
sprite_mesh_deinit(struct sprite_mesh* sm)
{
    glDeleteBuffers(1, &sm->ibo);
    glDeleteBuffers(1, &sm->vbo);
}

/* ------------------------------------------------------------------------- */
static void
sprite_shadow_init(struct sprite_shadow* ss)
{
    ss->program = 0;
    ss->uAspectRatio = (GLuint)-1;
    ss->uPosCameraSpace = (GLuint)-1;
    ss->uDir = (GLuint)-1;
    ss->uSize = (GLuint)-1;
    ss->uAnim = (GLuint)-1;
    ss->sNM = (GLuint)-1;
}

/* ------------------------------------------------------------------------- */
static void
sprite_shadow_deinit(struct sprite_shadow* ss)
{
    if (ss->program != 0)
        glDeleteProgram(ss->program);
}

/* ------------------------------------------------------------------------- */
static void
sprite_shadow_unload(struct sprite_shadow* ss)
{
    if (ss->program != 0)
        glDeleteProgram(ss->program);
    ss->program = 0;
}

/* ------------------------------------------------------------------------- */
static int
sprite_shadow_load(struct sprite_shadow* ss, const struct resource_pack* pack)
{
    assert(ss->program == 0);
    ss->program = load_shader(pack->shaders.glsl.shadow, attr_bindings);
    if (ss->program == 0)
        return -1;

    ss->uAspectRatio = get_uniform_location_and_warn(ss->program, "uAspectRatio");
    ss->uPosCameraSpace = get_uniform_location_and_warn(ss->program, "uPosCameraSpace");
    ss->uDir = get_uniform_location_and_warn(ss->program, "uDir");
    ss->uSize = get_uniform_location_and_warn(ss->program, "uSize");
    ss->uAnim = get_uniform_location_and_warn(ss->program, "uAnim");
    ss->sNM = get_uniform_location_and_warn(ss->program, "sNM");

    return 0;
}

/* ------------------------------------------------------------------------- */
static void
sprite_mat_init(struct sprite_mat* mat)
{
    mat->program = 0;
    mat->uAspectRatio = (GLuint)-1;
    mat->uPosCameraSpace = (GLuint)-1;
    mat->uDir = (GLuint)-1;
    mat->uSize = (GLuint)-1;
    mat->uAnim = (GLuint)-1;
    mat->sCol = (GLuint)-1;
    mat->sNM = (GLuint)-1;
}

/* ------------------------------------------------------------------------- */
static void
sprite_mat_deinit(struct sprite_mat* mat)
{
    if (mat->program != 0)
        glDeleteProgram(mat->program);
}

/* ------------------------------------------------------------------------- */
static void
sprite_mat_unload(struct sprite_mat* mat)
{
    if (mat->program != 0)
        glDeleteProgram(mat->program);
    mat->program = 0;
}

/* ------------------------------------------------------------------------- */
static int
sprite_mat_load(struct sprite_mat* mat, const struct resource_pack* pack)
{
    assert(mat->program == 0);
    mat->program = load_shader(pack->shaders.glsl.sprite, attr_bindings);
    if (mat->program == 0)
        return -1;

    mat->uAspectRatio = get_uniform_location_and_warn(mat->program, "uAspectRatio");
    mat->uPosCameraSpace = get_uniform_location_and_warn(mat->program, "uPosCameraSpace");
    mat->uDir = get_uniform_location_and_warn(mat->program, "uDir");
    mat->uSize = get_uniform_location_and_warn(mat->program, "uSize");
    mat->uAnim = get_uniform_location_and_warn(mat->program, "uAnim");
    mat->sCol = get_uniform_location_and_warn(mat->program, "sCol");
    mat->sNM = get_uniform_location_and_warn(mat->program, "sNM");

    return 0;
}

/* ------------------------------------------------------------------------- */
static void
sprite_tex_init(struct sprite_tex* tex)
{
    glGenTextures(1, &tex->texDiffuse);
    glBindTexture(GL_TEXTURE_2D, tex->texDiffuse);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &tex->texNM);
    glBindTexture(GL_TEXTURE_2D, tex->texNM);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/* ------------------------------------------------------------------------- */
static void
sprite_tex_deinit(struct sprite_tex* tex)
{
    glDeleteTextures(1, &tex->texNM);
    glDeleteTextures(1, &tex->texDiffuse);
}

/* ------------------------------------------------------------------------- */
static void
sprite_tex_unload(struct sprite_tex* tex)
{
    glBindTexture(GL_TEXTURE_2D, tex->texNM);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, tex->texDiffuse);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/* ------------------------------------------------------------------------- */
static void
sprite_tex_load(struct sprite_tex* tex, const struct resource_sprite* res)
{
    int img_width, img_height, img_channels;
    stbi_uc* img_data;

    img_data = stbi_load(res->texture0, &img_width, &img_height, &img_channels, 4);
    if (img_data != NULL)
    {
        glBindTexture(GL_TEXTURE_2D, tex->texDiffuse);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_width, img_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(img_data);
    }
    else
        log_err("Failed to load image \"%s\"\n", res->texture0);

    img_data = stbi_load(res->texture1, &img_width, &img_height, &img_channels, 4);
    if (img_data != NULL)
    {
        glBindTexture(GL_TEXTURE_2D, tex->texNM);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_width, img_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(img_data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else
        log_err("Failed to load image \"%s\"\n", res->texture1);

    tex->tile_x = res->tile_x;
    tex->tile_y = res->tile_y;
    tex->tile_count = res->num_frames;
    tex->frame = 0;
}

/* ------------------------------------------------------------------------- */
int
gfx_load_resource_pack(struct gfx* gfx, const struct resource_pack* pack)
{
    if (bg_load(&gfx->bg, pack) < 0)
        goto bg_load_failed;
    if (sprite_shadow_load(&gfx->sprite_shadow, pack) < 0)
        goto sprite_shadow_load_failed;
    if (sprite_mat_load(&gfx->sprite_mat, pack) < 0)
        goto sprite_mat_load_failed;

    if (pack->sprites.head[0])
    {
        sprite_tex_load(&gfx->head0_base, pack->sprites.head[0]->base);
        sprite_tex_load(&gfx->head0_gather, pack->sprites.head[0]->gather);
    }
    if (pack->sprites.body[0])
        sprite_tex_load(&gfx->body0_base, pack->sprites.body[0]->base);

    return 0;

sprite_mat_load_failed:
    sprite_shadow_unload(&gfx->sprite_shadow);
sprite_shadow_load_failed:
    bg_unload(&gfx->bg);
bg_load_failed:
    return -1;
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
    int fbwidth, fbheight;
    struct gfx* gfx = MALLOC(sizeof *gfx);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);  /* Required for GL ES */

    gfx->window = glfwCreateWindow(initial_width, initial_height, "Clither", NULL, NULL);
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

    bg_init(&gfx->bg, fbwidth, fbheight);
    sprite_mesh_init(&gfx->sprite_mesh);
    sprite_shadow_init(&gfx->sprite_shadow);
    sprite_mat_init(&gfx->sprite_mat);
    sprite_tex_init(&gfx->head0_base);
    sprite_tex_init(&gfx->head0_gather);
    sprite_tex_init(&gfx->body0_base);

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
    sprite_tex_deinit(&gfx->head0_base);
    sprite_tex_deinit(&gfx->head0_gather);
    sprite_tex_deinit(&gfx->body0_base);

    sprite_mat_deinit(&gfx->sprite_mat);
    sprite_shadow_deinit(&gfx->sprite_shadow);
    sprite_mesh_deinit(&gfx->sprite_mesh);
    bg_deinit(&gfx->bg);

    glfwDestroyWindow(gfx->window);
    FREE(gfx);
}

/* ------------------------------------------------------------------------- */
void
gfx_poll_input(struct gfx* gfx, struct input* input)
{
    glfwPollEvents();
    *input = gfx->input_buffer;
    gfx->input_buffer.scroll = 0;  /* Clear deltas */

    if (glfwWindowShouldClose(gfx->window))
        input->quit = 1;
}

/* ------------------------------------------------------------------------- */
struct command
gfx_input_to_command(
    struct command prev,
    const struct input* input,
    const struct gfx* gfx,
    const struct camera* camera,
    struct qwpos snake_head)
{
    float a, d, dx, dy;
    int max_dist;
    struct spos snake_head_screen;

    /* world -> camera space */
    struct qwpos pos_cameraSpace = {
        qw_mul(qw_sub(snake_head.x, camera->pos.x), camera->scale),
        qw_mul(qw_sub(snake_head.y, camera->pos.y), camera->scale)
    };

    /* camera space -> screen space + keep aspect ratio */
    if (gfx->width < gfx->height)
    {
        int pad = (gfx->height - gfx->width) / 2;
        snake_head_screen.x = qw_mul_to_int(pos_cameraSpace.x, make_qw(gfx->width)) + (gfx->width / 2);
        snake_head_screen.y = qw_mul_to_int(pos_cameraSpace.y, make_qw(-gfx->width)) + (gfx->width / 2 + pad);
    }
    else
    {
        int pad = (gfx->width - gfx->height) / 2;
        snake_head_screen.x = qw_mul_to_int(pos_cameraSpace.x, make_qw(gfx->height)) + (gfx->height / 2 + pad);
        snake_head_screen.y = qw_mul_to_int(pos_cameraSpace.y, make_qw(-gfx->height)) + (gfx->height / 2);
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

    return command_make(prev, a, d / max_dist, input->boost ? COMMAND_ACTION_BOOST : COMMAND_ACTION_NONE);
}

/* ------------------------------------------------------------------------- */
static void
draw_background(const struct gfx* gfx, const struct camera* camera, const struct aspect_ratio* ar)
{
    glBindBuffer(GL_ARRAY_BUFFER, gfx->bg.vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, uv));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx->bg.ibo);

    glUseProgram(gfx->bg.program);
    glBindTexture(GL_TEXTURE_2D, gfx->bg.texShadow);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gfx->bg.texCol);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gfx->bg.texNor);

    glUniform4f(gfx->bg.uAspectRatio, ar->scale_x, ar->scale_y, ar->pad_x, ar->pad_y);
    glUniform3f(gfx->bg.uCamera, qw_to_float(camera->pos.x), qw_to_float(camera->pos.y), qw_to_float(camera->scale));
    glUniform2f(gfx->bg.uShadowInvRes, (GLfloat)SHADOW_MAP_SIZE_FACTOR / gfx->width, (GLfloat)SHADOW_MAP_SIZE_FACTOR / gfx->height);
    glUniform1i(gfx->bg.sCol, 1);
    glUniform1i(gfx->bg.sNM, 2);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

/* ------------------------------------------------------------------------- */
static void
draw_snake(const struct snake* snake, const struct gfx* gfx, const struct camera* camera, const struct aspect_ratio* ar, char shadow_pass)
{
    int i;
    glBindBuffer(GL_ARRAY_BUFFER, gfx->sprite_mesh.vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, uv));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx->sprite_mesh.ibo);

    if (shadow_pass)
    {
        const GLint nmUnits[4] = {0, 1, 2, 3};
        glUseProgram(gfx->sprite_shadow.program);
        glUniform2f(gfx->sprite_shadow.uAspectRatio, ar->scale_x, ar->scale_y);
        glUniform1f(gfx->sprite_shadow.uSize, qw_to_float(snake_scale(&snake->param)));
        glUniform1iv(gfx->sprite_shadow.sNM, 4, nmUnits);

        glBindTexture(GL_TEXTURE_2D, gfx->body0_base.texNM);

        glBindFramebuffer(GL_FRAMEBUFFER, gfx->bg.fbo);
        glViewport(0, 0, gfx->width / SHADOW_MAP_SIZE_FACTOR, gfx->height / SHADOW_MAP_SIZE_FACTOR);
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    else
    {
        glUseProgram(gfx->sprite_mat.program);
        glUniform2f(gfx->sprite_mat.uAspectRatio, ar->scale_x, ar->scale_y);
        glUniform1f(gfx->sprite_mat.uSize, qw_to_float(snake_scale(&snake->param)));
        glUniform1i(gfx->sprite_mat.sCol, 0);
        glUniform1i(gfx->sprite_mat.sNM, 1);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gfx->body0_base.texDiffuse);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gfx->body0_base.texNM);
    }

    /* body parts */
    for (i = vector_count(&snake->data.bezier_points) - 1; i >= 1; --i)
    {
        struct bezier_point* bp = vector_get(&snake->data.bezier_points, i);
        /* world -> camera space */
        struct qwpos pos_cameraSpace = {
            qw_mul(qw_sub(bp->pos.x, camera->pos.x), camera->scale),
            qw_mul(qw_sub(bp->pos.y, camera->pos.y), camera->scale)
        };

        int tile_x = (gfx->body0_base.frame+i*3) % gfx->body0_base.tile_x;
        int tile_y = ((gfx->body0_base.frame+i*3) / gfx->body0_base.tile_x) % gfx->body0_base.tile_y;

        if (shadow_pass)
        {
            pos_cameraSpace.x = qw_sub(pos_cameraSpace.x, qw_mul(make_qw2(1, 128), camera->scale));
            pos_cameraSpace.y = qw_sub(pos_cameraSpace.y, qw_mul(make_qw2(1, 64), camera->scale));

            glUniform3f(gfx->sprite_shadow.uPosCameraSpace, qw_to_float(pos_cameraSpace.x), qw_to_float(pos_cameraSpace.y), qw_to_float(camera->scale));
            glUniform2f(gfx->sprite_shadow.uDir, qw_to_float(bp->dir.x), qw_to_float(bp->dir.y));
            glUniform4f(gfx->sprite_shadow.uAnim,
                1.0 / gfx->body0_base.tile_x,
                1.0 / gfx->body0_base.tile_y,
                (GLfloat)tile_x / gfx->body0_base.tile_x,
                (GLfloat)tile_y / gfx->body0_base.tile_y);
        }
        else
        {
            glUniform3f(gfx->sprite_mat.uPosCameraSpace, qw_to_float(pos_cameraSpace.x), qw_to_float(pos_cameraSpace.y), qw_to_float(camera->scale));
            glUniform2f(gfx->sprite_mat.uDir, qw_to_float(bp->dir.x), qw_to_float(bp->dir.y));
            glUniform4f(gfx->sprite_mat.uAnim,
                1.0 / gfx->body0_base.tile_x,
                1.0 / gfx->body0_base.tile_y,
                (GLfloat)tile_x / gfx->body0_base.tile_x,
                (GLfloat)tile_y / gfx->body0_base.tile_y);
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);
    }

    /* head */
    {
        struct bezier_point* bp = vector_front(&snake->data.bezier_points);
        /* world -> camera space */
        struct qwpos pos_cameraSpace = {
            qw_mul(qw_sub(bp->pos.x, camera->pos.x), camera->scale),
            qw_mul(qw_sub(bp->pos.y, camera->pos.y), camera->scale)
        };
        int tile_x = gfx->head0_base.frame % gfx->head0_base.tile_x;
        int tile_y = gfx->head0_base.frame / gfx->head0_base.tile_x;

        if (shadow_pass)
            glBindTexture(GL_TEXTURE_2D, gfx->head0_base.texNM);
        else
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, gfx->head0_base.texDiffuse);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, gfx->head0_base.texNM);
        }

        if (shadow_pass)
        {
            pos_cameraSpace.x = qw_sub(pos_cameraSpace.x, qw_mul(make_qw2(1, 128), camera->scale));
            pos_cameraSpace.y = qw_sub(pos_cameraSpace.y, qw_mul(make_qw2(1, 64), camera->scale));

            glUniform1f(gfx->sprite_shadow.uSize, 2 * qw_to_float(snake_scale(&snake->param)));
            glUniform3f(gfx->sprite_shadow.uPosCameraSpace, qw_to_float(pos_cameraSpace.x), qw_to_float(pos_cameraSpace.y), qw_to_float(camera->scale));
            glUniform2f(gfx->sprite_shadow.uDir, qw_to_float(bp->dir.x), qw_to_float(bp->dir.y));
            glUniform4f(gfx->sprite_shadow.uAnim,
                1.0 / gfx->head0_base.tile_x,
                1.0 / gfx->head0_base.tile_y,
                (GLfloat)tile_x / gfx->head0_base.tile_x,
                (GLfloat)tile_y / gfx->head0_base.tile_y);
        }
        else
        {
            glUniform1f(gfx->sprite_mat.uSize, 2 * qw_to_float(snake_scale(&snake->param)));
            glUniform3f(gfx->sprite_mat.uPosCameraSpace, qw_to_float(pos_cameraSpace.x), qw_to_float(pos_cameraSpace.y), qw_to_float(camera->scale));
            glUniform2f(gfx->sprite_mat.uDir, qw_to_float(bp->dir.x), qw_to_float(bp->dir.y));
            glUniform4f(gfx->sprite_mat.uAnim,
                1.0 / gfx->head0_base.tile_x,
                1.0 / gfx->head0_base.tile_y,
                (GLfloat)tile_x / gfx->head0_base.tile_x,
                (GLfloat)tile_y / gfx->head0_base.tile_y);
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

        if (shadow_pass)
            glBindTexture(GL_TEXTURE_2D, gfx->head0_gather.texNM);
        else
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, gfx->head0_gather.texDiffuse);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, gfx->head0_gather.texNM);
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    if (shadow_pass)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, gfx->width, gfx->height);
    }
}

/* ------------------------------------------------------------------------- */
void
gfx_step_anim(struct gfx* gfx, int sim_tick_rate)
{
    static int frame_update;
    if (frame_update-- == 0)
    {
        frame_update = 3;

        gfx->head0_base.frame++;
        if (gfx->head0_base.frame >= gfx->head0_base.tile_count)
            gfx->head0_base.frame = 0;

        gfx->body0_base.frame++;
        if (gfx->body0_base.frame >= gfx->body0_base.tile_count)
            gfx->body0_base.frame = 0;
    }
}

/* ------------------------------------------------------------------------- */
void
gfx_draw_world(struct gfx* gfx, const struct world* world, const struct camera* camera)
{
    struct aspect_ratio ar = { 1.0, 1.0, 0.0, 0.0 };
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

    WORLD_FOR_EACH_SNAKE(world, snake_id, snake)
        draw_snake(snake, gfx, camera, &ar, 1);
    WORLD_END_EACH

    draw_background(gfx, camera, &ar);
    //draw_0_0(gfx, camera, &ar);

    WORLD_FOR_EACH_SNAKE(world, snake_id, snake)
        draw_snake(snake, gfx, camera, &ar, 0);
    WORLD_END_EACH

    glfwSwapBuffers(gfx->window);
}
