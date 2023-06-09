#include "clither/camera.h"
#include "clither/controls.h"
#include "clither/gfx.h"
#include "clither/log.h"
#include "clither/snake.h"
#include "clither/world.h"

#include "cstructures/memory.h"

#include "glad/gles2.h"
#include "GLFW/glfw3.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct sprite
{
    GLuint program;
    GLuint vbo;
    GLuint ibo;
    GLuint uAspectRatio;
    GLuint uPosCameraSpace;
    GLuint uDir;
    GLuint uSize;
    GLuint sDiffuse;
    GLuint sNM;
};

struct bg
{
    GLuint program;
    GLuint vbo;
    GLuint ibo;
    GLuint texTile;
    GLuint uAspectRatio;
    GLuint uCamera;
    GLuint sTile;
};

struct sprite_mat
{
    GLuint texDiffuse;
    GLuint texNM;
};

struct gfx
{
    GLFWwindow* window;
    int width, height;

    struct input input_buffer;

    struct bg bg;
    struct sprite sprite;
    struct sprite_mat body0;
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
    {{-0.5, -0.5}, {0,  1}},
    {{-0.5,  0.5}, {0,  0}},
    {{ 0.5, -0.5}, {1,  1}},
    {{ 0.5,  0.5}, {1,  0}}
};
static const GLushort sprite_indices[6] = {
    0, 2, 1,
    1, 3, 2
};
static const char* sprite_attr_bindings[] = {
    "vPosition",
    "vTexCoord",
    NULL
};
static const char* sprite_vshader =
    "attribute vec2 vPosition;\n"
    "attribute vec2 vTexCoord;\n"

    "uniform vec2 uAspectRatio;\n"
    "uniform vec3 uPosCameraSpace;\n"
    "uniform vec2 uDir;\n"
    "uniform float uSize;\n"

    "varying vec2 fTexCoord;\n"
    "varying vec3 vLightDir_tangentSpace;\n"

    "void main()\n"
    "{\n"
    "    fTexCoord = vTexCoord;\n"
    "    vec2 pos = mat2(uDir.x, uDir.y, uDir.y, -uDir.x) * vPosition;\n"
    "    pos = pos * uSize * uPosCameraSpace.z + uPosCameraSpace.xy;\n"
    "    pos = pos / uAspectRatio;\n"

    "    vec3 lightDir_cameraSpace = vec3(0.0, 0.0, 1.0);\n"
    "    mat3 TBN_inv = mat3(\n"
    "        -uDir.x, uDir.y, 0.0,\n"
    "        uDir.y, uDir.x, 0.0,\n"
    "        0.0, 0.0, 1.0);\n"
    "    vLightDir_tangentSpace = lightDir_cameraSpace;\n"

    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "}\n";
static const char* sprite_fshader =
    "precision mediump float;\n"

    "varying vec2 fTexCoord;\n"
    "varying vec3 vLightDir_tangentSpace;\n"

    "uniform sampler2D sDiffuse;\n"
    "uniform sampler2D sNM;\n"

    "vec3 uTint = vec3(1.0, 0.5, 0.0) * 1.2;\n"

    "void main()\n"
    "{\n"
    "    vec4 diffuse = texture2D(sDiffuse, fTexCoord);\n"
    "    vec3 nm = texture2D(sNM, fTexCoord).rgb;\n"
    "    float mask = nm.b * 0.5;\n"
    "    vec3 color = diffuse.rgb;\n"

    "    vec3 normal;\n"
    "    normal.xy = nm.xy * 2.0 - 1.0;\n"
    "    normal.z = sqrt(1.0 - dot(normal.xy, normal.xy));\n"
    "    float normFac = clamp(dot(normal, normalize(vLightDir_tangentSpace)), 0.0, 1.0);\n"
    "    color = color * (1.0-0.7) + normFac * color * 0.7;\n"

    "    color = color * (1.0-mask) + uTint * mask;\n"

    "    gl_FragColor = vec4(color, diffuse.a);\n"
    "}\n";

static const struct vertex bg_vertices[4] = {
    {{-1, -1}, {0,  1}},
    {{-1,  1}, {0,  0}},
    {{ 1, -1}, {1,  1}},
    {{ 1,  1}, {1,  0}}
};
static const GLushort bg_indices[6] = {
    0, 2, 1,
    1, 3, 2
};
static const char* bg_attr_bindings[] = {
    "vPosition",
    "vTexCoord",
    NULL
};
static const char* bg_vshader =
    "attribute vec2 vPosition;\n"
    "attribute vec2 vTexCoord;\n"

    "varying vec2 fTexCoord;\n"

    "void main()\n"
    "{\n"
    "    fTexCoord = vTexCoord;\n"
    "    gl_Position = vec4(vPosition, 0.0, 1.0);\n"
    "}\n";
static const char* bg_fshader =
    "precision mediump float;\n"
    "#define TILE_SCALE 2.0\n"

    "uniform vec4 uAspectRatio;\n"
    "uniform vec3 uCamera;\n"
    "uniform sampler2D sTile;\n"

    "varying vec2 fTexCoord;\n"

    "void main()\n"
    "{\n"
    "    vec2 uv = (fTexCoord - 0.5) / uCamera.z * uAspectRatio.xy * TILE_SCALE;\n"
    "    vec3 color = texture2D(sTile, uv + uCamera.xy).rgb;\n"
    "    gl_FragColor = vec4(color, 1.0);\n"
    "}\n";

/* ------------------------------------------------------------------------- */
static void
key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
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
}

/* ------------------------------------------------------------------------- */
static void
framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    struct gfx* gfx = glfwGetWindowUserPointer(window);
    gfx->width = width;
    gfx->height = height;
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
static GLuint load_shader(const char* vcode, const char* fcode, const char* attribute_bindings[])
{
    int i;
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
    int fbwidth, fbheight;
    int img_width, img_height, img_channels;
    stbi_uc* img_data;
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

    /* Load background */
    gfx->bg.program = load_shader(bg_vshader, bg_fshader, bg_attr_bindings);
    if (gfx->bg.program == 0)
        goto load_bg_program_failed;

    gfx->bg.uAspectRatio = glGetUniformLocation(gfx->bg.program, "uAspectRatio");
    gfx->bg.uCamera = glGetUniformLocation(gfx->bg.program, "uCamera");
    gfx->bg.sTile = glGetUniformLocation(gfx->bg.program, "sTile");

    glGenBuffers(1, &gfx->bg.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gfx->bg.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bg_vertices), bg_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &gfx->bg.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx->bg.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(bg_indices), bg_indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glGenTextures(1, &gfx->bg.texTile);
    glBindTexture(GL_TEXTURE_2D, gfx->bg.texTile);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    img_data = stbi_load("textures/tile.png", &img_width, &img_height, &img_channels, 3);
    if (img_data == NULL)
        log_err("Failed to load image \"textures/tile.png\"\n");
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img_width, img_height, 0, GL_RGB, GL_UNSIGNED_BYTE, img_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(img_data);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    /* Load sprites */
    gfx->sprite.program = load_shader(sprite_vshader, sprite_fshader, sprite_attr_bindings);
    if (gfx->sprite.program == 0)
        goto load_sprite_program_failed;

    gfx->sprite.uAspectRatio = glGetUniformLocation(gfx->sprite.program, "uAspectRatio");
    gfx->sprite.uPosCameraSpace = glGetUniformLocation(gfx->sprite.program, "uPosCameraSpace");
    gfx->sprite.uDir = glGetUniformLocation(gfx->sprite.program, "uDir");
    gfx->sprite.uSize = glGetUniformLocation(gfx->sprite.program, "uSize");
    gfx->sprite.sDiffuse = glGetUniformLocation(gfx->sprite.program, "sDiffuse");
    gfx->sprite.sNM = glGetUniformLocation(gfx->sprite.program, "sNM");
    
    glGenBuffers(1, &gfx->sprite.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gfx->sprite.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sprite_vertices), sprite_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &gfx->sprite.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx->sprite.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(sprite_indices), sprite_indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glGenTextures(1, &gfx->body0.texDiffuse);
    glBindTexture(GL_TEXTURE_2D, gfx->body0.texDiffuse);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    img_data = stbi_load("textures/body0_col.png", &img_width, &img_height, &img_channels, 4);
    if (img_data == NULL)
        log_err("Failed to load image \"textures/body0_col.png\"\n");
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_width, img_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(img_data);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &gfx->body0.texNM);
    glBindTexture(GL_TEXTURE_2D, gfx->body0.texNM);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    img_data = stbi_load("textures/body0_nm.png", &img_width, &img_height, &img_channels, 3);
    if (img_data == NULL)
        log_err("Failed to load image \"textures/body0_nm.png\"\n");
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img_width, img_height, 0, GL_RGB, GL_UNSIGNED_BYTE, img_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(img_data);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glfwGetFramebufferSize(gfx->window, &fbwidth, &fbheight);
    gfx->width = fbwidth;
    gfx->height = fbheight;
    glViewport(0, 0, fbwidth, fbheight);

    input_init(&gfx->input_buffer);

    glfwSetWindowUserPointer(gfx->window, gfx);
    glfwSetKeyCallback(gfx->window, key_callback);
    glfwSetMouseButtonCallback(gfx->window, mouse_button_callback);
    glfwSetCursorPosCallback(gfx->window, cursor_position_callback);
    glfwSetScrollCallback(gfx->window, scroll_callback);
    glfwSetFramebufferSizeCallback(gfx->window, framebuffer_size_callback);

    return gfx;

load_sprite_program_failed:
    glDeleteProgram(gfx->bg.program);
    glDeleteBuffers(1, &gfx->bg.ibo);
    glDeleteBuffers(1, &gfx->bg.vbo);
load_bg_program_failed:
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
    glDeleteTextures(1, &gfx->body0.texDiffuse);
    glDeleteTextures(1, &gfx->body0.texNM);
    glDeleteProgram(gfx->sprite.program);
    glDeleteBuffers(1, &gfx->sprite.ibo);
    glDeleteBuffers(1, &gfx->sprite.vbo);

    glDeleteTextures(1, &gfx->bg.texTile);
    glDeleteProgram(gfx->bg.program);
    glDeleteBuffers(1, &gfx->bg.ibo);
    glDeleteBuffers(1, &gfx->bg.vbo);

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
void
gfx_update_controls(
    struct controls* controls,
    const struct input* input,
    const struct gfx* gfx,
    const struct camera* camera,
    struct qwpos snake_head)
{
    double a, d, dx, dy;
    int max_dist, da;
    uint8_t new_angle, new_speed;
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
    a = atan2(dy, dx) / (2 * M_PI) + 0.5;
    d = sqrt(dx * dx + dy * dy);
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
static void
draw_background(const struct gfx* gfx, const struct camera* camera, const struct aspect_ratio* ar)
{
    glUseProgram(gfx->bg.program);
    glBindBuffer(GL_ARRAY_BUFFER, gfx->bg.vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, uv));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx->bg.ibo);
    glBindTexture(GL_TEXTURE_2D, gfx->bg.texTile);

    glUniform4f(gfx->bg.uAspectRatio, ar->scale_x, ar->scale_y, ar->pad_x, ar->pad_y);
    glUniform3f(gfx->bg.uCamera, qw_to_float(camera->pos.x), qw_to_float(-camera->pos.y), qw_to_float(camera->scale));
    glUniform1i(gfx->bg.sTile, 0);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

/* ------------------------------------------------------------------------- */
static GLfloat a = 0.0;
static void
draw_0_0(const struct gfx* gfx, const struct camera* camera, const struct aspect_ratio* ar)
{
    /* world -> camera space */
    struct qwpos pos_cameraSpace = {
        qw_mul(-camera->pos.x, camera->scale),
        qw_mul(-camera->pos.y, camera->scale)
    };

    glUseProgram(gfx->sprite.program);
    glBindBuffer(GL_ARRAY_BUFFER, gfx->sprite.vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, uv));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx->sprite.ibo);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gfx->body0.texDiffuse);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gfx->body0.texNM);

    glUniform2f(gfx->sprite.uAspectRatio, ar->scale_x, ar->scale_y);
    glUniform3f(gfx->sprite.uPosCameraSpace, qw_to_float(pos_cameraSpace.x), qw_to_float(pos_cameraSpace.y), qw_to_float(camera->scale));
    glUniform2f(gfx->sprite.uDir, cos(a), sin(a));
    //a+= 0.01;
    glUniform1f(gfx->sprite.uSize, 0.5);
    glUniform1i(gfx->sprite.sDiffuse, 0);
    glUniform1i(gfx->sprite.sNM, 1);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

/* ------------------------------------------------------------------------- */
static void
draw_snake(const struct snake* snake, const struct gfx* gfx, const struct camera* camera, const struct aspect_ratio* ar)
{
    /* world -> camera space */
    struct qwpos pos_cameraSpace = {
        qw_mul(qw_sub(snake->head.pos.x, camera->pos.x), camera->scale),
        qw_mul(qw_sub(snake->head.pos.y, camera->pos.y), camera->scale)
    };

    GLfloat dir_x = cos(qa_to_float(snake->head.angle));
    GLfloat dir_y = sin(qa_to_float(snake->head.angle));
    GLfloat size = 0.125;  /* todo snake size */

    glUseProgram(gfx->sprite.program);
    glBindBuffer(GL_ARRAY_BUFFER, gfx->sprite.vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, uv));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx->sprite.ibo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gfx->body0.texDiffuse);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gfx->body0.texNM);

    glUniform2f(gfx->sprite.uAspectRatio, ar->scale_x, ar->scale_y);
    glUniform3f(gfx->sprite.uPosCameraSpace, qw_to_float(pos_cameraSpace.x), qw_to_float(pos_cameraSpace.y), qw_to_float(camera->scale));
    glUniform2f(gfx->sprite.uDir, dir_x, dir_y);
    glUniform1f(gfx->sprite.uSize, size);
    glUniform1i(gfx->sprite.sDiffuse, 0);
    glUniform1i(gfx->sprite.sNM, 1);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

/* ------------------------------------------------------------------------- */
void
gfx_draw_world(struct gfx* gfx, const struct world* world, const struct camera* camera, uint16_t frame_number)
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

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    draw_background(gfx, camera, &ar);
    draw_0_0(gfx, camera, &ar);
    
    WORLD_FOR_EACH_SNAKE(world, snake_id, snake)
        draw_snake(snake, gfx, camera, &ar);
    WORLD_END_EACH

    glfwSwapBuffers(gfx->window);
}
