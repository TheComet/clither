#include "clither/bezier.h"
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

#define SHADOW_MAP_SIZE_FACTOR 4

struct bg
{
    GLuint program;
    GLuint vbo;
    GLuint ibo;
    GLuint fbo;
    GLuint texShadow;
    GLuint texCol;
    GLuint texNM;
    GLuint uAspectRatio;
    GLuint uCamera;
    GLuint uRes;
    GLuint sShadow;
    GLuint sCol;
    GLuint sNM;
};

struct sprite_mesh
{
    GLuint vbo;
    GLuint ibo;
};

struct sprite_mat
{
    GLuint program;
    GLuint uAspectRatio;
    GLuint uPosCameraSpace;
    GLuint uDir;
    GLuint uSize;
    GLuint uAnim;
    GLuint sDiffuse;
    GLuint sNM;
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
    "precision mediump float;\n"

    "attribute vec2 vPosition;\n"
    "attribute vec2 vTexCoord;\n"

    "uniform vec2 uAspectRatio;\n"
    "uniform vec3 uPosCameraSpace;\n"
    "uniform vec2 uDir;\n"
    "uniform float uSize;\n"
    "uniform vec4 uAnim;\n"

    "varying vec2 fTexCoord;\n"
    "varying vec3 fLightDir_tangentSpace;\n"

    "void main()\n"
    "{\n"
    "    fTexCoord = vTexCoord * uAnim.xy + uAnim.zw;\n"
    "    vec2 pos = mat2(uDir.x, uDir.y, uDir.y, -uDir.x) * vPosition;\n"
    "    pos = pos * uSize * uPosCameraSpace.z + uPosCameraSpace.xy;\n"
    "    pos = pos / uAspectRatio;\n"

    "    vec2 lightDir_cameraSpace = vec2(pos.x, pos.y - 0.5);\n"
    "    fLightDir_tangentSpace = vec3(mat2(uDir.x, uDir.y, uDir.y, -uDir.x) * lightDir_cameraSpace, -1.0);\n"

    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "}\n";
static const char* sprite_fshader =
    "precision mediump float;\n"

    "varying vec2 fTexCoord;\n"
    "varying vec3 fLightDir_tangentSpace;\n"

    "uniform sampler2D sDiffuse;\n"
    "uniform sampler2D sNM;\n"

    "vec3 uTint = vec3(1.0, 0.8, 0.4);\n"

    "void main()\n"
    "{\n"
    "    vec4 col = texture2D(sDiffuse, fTexCoord);\n"
    "    vec4 nm = texture2D(sNM, fTexCoord);\n"
    "    float mask = col.a * 0.4;\n"
    "    vec3 color = col.rgb;\n"

    "    color = color * (1.0-mask) + uTint * mask;\n"

    "    vec3 normal;\n"
    "    normal.xy = nm.xy * 2.0 - 1.0;\n"
    "    normal.z = sqrt(1.0 - dot(normal.xy, normal.xy));\n"

    "    vec3 lightDir = normalize(fLightDir_tangentSpace);\n"
    "    float normFac = clamp(dot(normal, -lightDir), 0.0, 1.0);\n"
    "    color = color * (1.0 - 0.9) + color * normFac * 0.9;\n"

    "    gl_FragColor = vec4(color, nm.a);\n"
    "}\n";
static const char* sprite_shadow_vshader =
    "precision mediump float;\n"

    "attribute vec2 vPosition;\n"
    "attribute vec2 vTexCoord;\n"

    "uniform vec2 uAspectRatio;\n"
    "uniform vec3 uPosCameraSpace;\n"
    "uniform vec2 uDir;\n"
    "uniform float uSize;\n"
    "uniform vec4 uAnim;\n"

    "varying vec2 fTexCoord;\n"

    "void main()\n"
    "{\n"
    "    fTexCoord = vTexCoord * uAnim.xy + uAnim.zw;\n"
    "    vec2 pos = mat2(uDir.x, uDir.y, uDir.y, -uDir.x) * vPosition;\n"
    "    pos = pos * uSize * uPosCameraSpace.z + uPosCameraSpace.xy;\n"
    "    pos = pos / uAspectRatio;\n"

    "    vec3 lightDir = vec3(pos, -1.0);\n"
    //"    pos += pos.xy / 32.0;\n"

    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "}\n";
static const char* sprite_shadow_fshader =
    "precision mediump float;\n"

    "varying vec2 fTexCoord;\n"

    "uniform sampler2D sNM[4];\n"

    "void main()\n"
    "{\n"
    "    float mask = texture2D(sNM[0], fTexCoord).a;\n"
    "    gl_FragColor = vec4(vec3(1.0, 1.0, 1.0), mask);\n"
    "}\n";

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
static const char* bg_attr_bindings[] = {
    "vPosition",
    "vTexCoord",
    NULL
};
static const char* bg_vshader =
    "attribute vec2 vPosition;\n"
    "attribute vec2 vTexCoord;\n"

    "varying vec2 fTexCoord;\n"
    "varying vec3 fLightDir;\n"

    "void main()\n"
    "{\n"
    "    fTexCoord = vTexCoord;\n"
    "    fLightDir = vec3(vec2(vPosition.x, vPosition.y - 0.5), -1.0);\n"
    "    gl_Position = vec4(vPosition, 0.0, 1.0);\n"
    "}\n";
static const char* bg_fshader =
    "precision mediump float;\n"
    "#define TILE_SCALE 0.5\n"

    "uniform vec4 uAspectRatio;\n"
    "uniform vec3 uCamera;\n"
    "uniform vec2 uRes;\n"

    "uniform sampler2D sShadow;\n"
    "uniform sampler2D sCol;\n"
    "uniform sampler2D sNM;\n"

    "varying vec2 fTexCoord;\n"
    "varying vec3 fLightDir;\n"

    "void main()\n"
    "{\n"
    "    vec2 uv = (fTexCoord - 0.5) / uCamera.z * uAspectRatio.xy + uCamera.xy / 2.0;\n"
    "    uv = uv * TILE_SCALE;\n"
    "    vec3 color = dot(uv, uv) < 8.0 ? texture2D(sCol, uv).rgb : vec3(0.2, 0.2, 0.2);\n"
    "    vec3 nm = texture2D(sNM, uv).rgb;\n"

    "    vec3 normal;\n"
    "    normal.xy = nm.xy * 2.0 - 1.0;\n"
    "    normal.z = sqrt(1.0 - dot(normal.xy, normal.xy));\n"

    "    vec3 lightDir = normalize(fLightDir);\n"
    "    float normFac = clamp(dot(normal, -lightDir), 0.0, 1.0);\n"
    "    color = color * (1.0-0.9) + normFac * color * 0.9;\n"

    "    float shadowFac = 1.0  * texture2D(sShadow, vec2(fTexCoord.x-2.0*uRes.x, fTexCoord.y-2.0*uRes.y)).r +"
    "                      4.0  * texture2D(sShadow, vec2(fTexCoord.x-1.0*uRes.x, fTexCoord.y-2.0*uRes.y)).r +"
    "                      7.0  * texture2D(sShadow, vec2(fTexCoord.x+0.0*uRes.x, fTexCoord.y-2.0*uRes.y)).r +"
    "                      4.0  * texture2D(sShadow, vec2(fTexCoord.x+1.0*uRes.x, fTexCoord.y-2.0*uRes.y)).r +"
    "                      1.0  * texture2D(sShadow, vec2(fTexCoord.x+2.0*uRes.x, fTexCoord.y-2.0*uRes.y)).r +"

    "                      4.0  * texture2D(sShadow, vec2(fTexCoord.x-2.0*uRes.x, fTexCoord.y-1.0*uRes.y)).r +"
    "                      16.0 * texture2D(sShadow, vec2(fTexCoord.x-1.0*uRes.x, fTexCoord.y-1.0*uRes.y)).r +"
    "                      26.0 * texture2D(sShadow, vec2(fTexCoord.x+0.0*uRes.x, fTexCoord.y-1.0*uRes.y)).r +"
    "                      16.0 * texture2D(sShadow, vec2(fTexCoord.x+1.0*uRes.x, fTexCoord.y-1.0*uRes.y)).r +"
    "                      4.0  * texture2D(sShadow, vec2(fTexCoord.x+2.0*uRes.x, fTexCoord.y-1.0*uRes.y)).r +"

    "                      7.0  * texture2D(sShadow, vec2(fTexCoord.x-2.0*uRes.x, fTexCoord.y+0.0*uRes.y)).r +"
    "                      26.0 * texture2D(sShadow, vec2(fTexCoord.x-1.0*uRes.x, fTexCoord.y+0.0*uRes.y)).r +"
    "                      41.0 * texture2D(sShadow, vec2(fTexCoord.x+0.0*uRes.x, fTexCoord.y+0.0*uRes.y)).r +"
    "                      26.0 * texture2D(sShadow, vec2(fTexCoord.x+1.0*uRes.x, fTexCoord.y+0.0*uRes.y)).r +"
    "                      7.0  * texture2D(sShadow, vec2(fTexCoord.x+2.0*uRes.x, fTexCoord.y+0.0*uRes.y)).r +"

    "                      4.0  * texture2D(sShadow, vec2(fTexCoord.x-2.0*uRes.x, fTexCoord.y+1.0*uRes.y)).r +"
    "                      16.0 * texture2D(sShadow, vec2(fTexCoord.x-1.0*uRes.x, fTexCoord.y+1.0*uRes.y)).r +"
    "                      26.0 * texture2D(sShadow, vec2(fTexCoord.x+0.0*uRes.x, fTexCoord.y+1.0*uRes.y)).r +"
    "                      16.0 * texture2D(sShadow, vec2(fTexCoord.x+1.0*uRes.x, fTexCoord.y+1.0*uRes.y)).r +"
    "                      4.0  * texture2D(sShadow, vec2(fTexCoord.x+2.0*uRes.x, fTexCoord.y+1.0*uRes.y)).r +"

    "                      1.0  * texture2D(sShadow, vec2(fTexCoord.x-2.0*uRes.x, fTexCoord.y+2.0*uRes.y)).r +"
    "                      4.0  * texture2D(sShadow, vec2(fTexCoord.x-1.0*uRes.x, fTexCoord.y+2.0*uRes.y)).r +"
    "                      7.0  * texture2D(sShadow, vec2(fTexCoord.x+0.0*uRes.x, fTexCoord.y+2.0*uRes.y)).r +"
    "                      4.0  * texture2D(sShadow, vec2(fTexCoord.x+1.0*uRes.x, fTexCoord.y+2.0*uRes.y)).r +"
    "                      1.0  * texture2D(sShadow, vec2(fTexCoord.x+2.0*uRes.x, fTexCoord.y+2.0*uRes.y)).r;\n"
    "    shadowFac /= 273.0;\n"

    "    shadowFac = (1.0 - shadowFac) * 0.9 + 0.1;\n"

    "    gl_FragColor = vec4(color * shadowFac, 1.0);\n"
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
static void load_sprite_tex(
        struct sprite_tex* mat,
        const char* col,
        const char* nm,
        int8_t tile_x, int8_t tile_y, int8_t tile_count)
{
    int img_width, img_height, img_channels;
    stbi_uc* img_data;

    glGenTextures(1, &mat->texDiffuse);
    glBindTexture(GL_TEXTURE_2D, mat->texDiffuse);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    img_data = stbi_load(col, &img_width, &img_height, &img_channels, 4);
    if (img_data == NULL)
        log_err("Failed to load image \"%s\"\n", col);
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_width, img_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(img_data);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &mat->texNM);
    glBindTexture(GL_TEXTURE_2D, mat->texNM);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    img_data = stbi_load(nm, &img_width, &img_height, &img_channels, 4);
    if (img_data == NULL)
        log_err("Failed to load image \"%s\"\n", nm);
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_width, img_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(img_data);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    mat->tile_x = tile_x;
    mat->tile_y = tile_y;
    mat->tile_count = tile_count;
    mat->frame = 0;
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
    glfwGetFramebufferSize(gfx->window, &fbwidth, &fbheight);

    log_info("Using GLFW version %s\n", glfwGetVersionString());
    log_info("OpenGL version %s\n", glGetString(GL_VERSION));

    /* Load background */
    gfx->bg.program = load_shader(bg_vshader, bg_fshader, bg_attr_bindings);
    if (gfx->bg.program == 0)
        goto load_bg_program_failed;

    gfx->bg.uAspectRatio = glGetUniformLocation(gfx->bg.program, "uAspectRatio");
    gfx->bg.uCamera = glGetUniformLocation(gfx->bg.program, "uCamera");
    gfx->bg.uRes = glGetUniformLocation(gfx->bg.program, "uRes");
    gfx->bg.sShadow = glGetUniformLocation(gfx->bg.program, "sShadow");
    gfx->bg.sCol = glGetUniformLocation(gfx->bg.program, "sCol");
    gfx->bg.sNM = glGetUniformLocation(gfx->bg.program, "sNM");

    glGenTextures(1, &gfx->bg.texShadow);
    glBindTexture(GL_TEXTURE_2D, gfx->bg.texShadow);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fbwidth / SHADOW_MAP_SIZE_FACTOR, fbheight / SHADOW_MAP_SIZE_FACTOR, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &gfx->bg.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gfx->bg.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gfx->bg.texShadow, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        log_err("Incomplete framebuffer!\n");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &gfx->bg.fbo);
        goto incomplete_shadow_framebuffer;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenBuffers(1, &gfx->bg.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gfx->bg.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bg_vertices), bg_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &gfx->bg.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx->bg.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(bg_indices), bg_indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glGenTextures(1, &gfx->bg.texCol);
    glBindTexture(GL_TEXTURE_2D, gfx->bg.texCol);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    img_data = stbi_load("packs/pack_devel/bg/tile_col.png", &img_width, &img_height, &img_channels, 3);
    if (img_data == NULL)
        log_err("Failed to load image \"packs/pack_devel/bg/tile_col.png\"\n");
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img_width, img_height, 0, GL_RGB, GL_UNSIGNED_BYTE, img_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(img_data);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &gfx->bg.texNM);
    glBindTexture(GL_TEXTURE_2D, gfx->bg.texNM);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    img_data = stbi_load("packs/pack_devel/bg/tile_nor.png", &img_width, &img_height, &img_channels, 3);
    if (img_data == NULL)
        log_err("Failed to load image \"packs/pack_devel/bg/tile_nor.png\"\n");
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img_width, img_height, 0, GL_RGB, GL_UNSIGNED_BYTE, img_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(img_data);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    /* Load sprites */
    gfx->sprite_shadow.program = load_shader(sprite_shadow_vshader, sprite_shadow_fshader, sprite_attr_bindings);
    if (gfx->sprite_shadow.program == 0)
        goto load_sprite_shadow_program_failed;
    gfx->sprite_mat.program = load_shader(sprite_vshader, sprite_fshader, sprite_attr_bindings);
    if (gfx->sprite_mat.program == 0)
        goto load_sprite_program_failed;

    gfx->sprite_shadow.uAspectRatio = glGetUniformLocation(gfx->sprite_shadow.program, "uAspectRatio");
    gfx->sprite_shadow.uPosCameraSpace = glGetUniformLocation(gfx->sprite_shadow.program, "uPosCameraSpace");
    gfx->sprite_shadow.uDir = glGetUniformLocation(gfx->sprite_shadow.program, "uDir");
    gfx->sprite_shadow.uSize = glGetUniformLocation(gfx->sprite_shadow.program, "uSize");
    gfx->sprite_shadow.uAnim = glGetUniformLocation(gfx->sprite_shadow.program, "uAnim");
    gfx->sprite_shadow.sNM = glGetUniformLocation(gfx->sprite_shadow.program, "sCol");

    gfx->sprite_mat.uAspectRatio = glGetUniformLocation(gfx->sprite_mat.program, "uAspectRatio");
    gfx->sprite_mat.uPosCameraSpace = glGetUniformLocation(gfx->sprite_mat.program, "uPosCameraSpace");
    gfx->sprite_mat.uDir = glGetUniformLocation(gfx->sprite_mat.program, "uDir");
    gfx->sprite_mat.uSize = glGetUniformLocation(gfx->sprite_mat.program, "uSize");
    gfx->sprite_mat.uAnim = glGetUniformLocation(gfx->sprite_mat.program, "uAnim");
    gfx->sprite_mat.sDiffuse = glGetUniformLocation(gfx->sprite_mat.program, "sDiffuse");
    gfx->sprite_mat.sNM = glGetUniformLocation(gfx->sprite_mat.program, "sNM");

    glGenBuffers(1, &gfx->sprite_mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gfx->sprite_mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sprite_vertices), sprite_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &gfx->sprite_mesh.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx->sprite_mesh.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(sprite_indices), sprite_indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    load_sprite_tex(&gfx->head0_base, "packs/pack_devel/head0/base_col.png", "packs/pack_devel/head0/base_nm.png", 4, 3, 12);
    load_sprite_tex(&gfx->head0_gather, "packs/pack_devel/head0/gather_col.png", "packs/pack_devel/head0/gather_nm.png", 4, 3, 12);
    load_sprite_tex(&gfx->body0_base, "packs/pack_devel/body0/base_col.png", "packs/pack_devel/body0/base_nm.png", 4, 3, 12);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
    glDeleteProgram(gfx->sprite_shadow.program);
load_sprite_shadow_program_failed:
    glDeleteTextures(1, &gfx->bg.texNM);
    glDeleteTextures(1, &gfx->bg.texCol);
    glDeleteBuffers(1, &gfx->bg.ibo);
    glDeleteBuffers(1, &gfx->bg.vbo);
    glDeleteFramebuffers(1, &gfx->bg.fbo);
incomplete_shadow_framebuffer:
    glDeleteTextures(1, &gfx->bg.texShadow);
    glDeleteProgram(gfx->bg.program);
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
    glDeleteTextures(1, &gfx->body0_base.texDiffuse);
    glDeleteTextures(1, &gfx->body0_base.texNM);

    glDeleteTextures(1, &gfx->head0_gather.texDiffuse);
    glDeleteTextures(1, &gfx->head0_gather.texNM);

    glDeleteTextures(1, &gfx->head0_base.texDiffuse);
    glDeleteTextures(1, &gfx->head0_base.texNM);

    glDeleteProgram(gfx->sprite_mat.program);
    glDeleteProgram(gfx->sprite_shadow.program);
    glDeleteBuffers(1, &gfx->sprite_mesh.ibo);
    glDeleteBuffers(1, &gfx->sprite_mesh.vbo);

    glDeleteTextures(1, &gfx->bg.texNM);
    glDeleteTextures(1, &gfx->bg.texCol);
    glDeleteBuffers(1, &gfx->bg.ibo);
    glDeleteBuffers(1, &gfx->bg.vbo);
    glDeleteFramebuffers(1, &gfx->bg.fbo);
    glDeleteTextures(1, &gfx->bg.texShadow);
    glDeleteProgram(gfx->bg.program);

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
    glBindTexture(GL_TEXTURE_2D, gfx->bg.texNM);

    glUniform4f(gfx->bg.uAspectRatio, ar->scale_x, ar->scale_y, ar->pad_x, ar->pad_y);
    glUniform3f(gfx->bg.uCamera, qw_to_float(camera->pos.x), qw_to_float(camera->pos.y), qw_to_float(camera->scale));
    glUniform2f(gfx->bg.uRes, (GLfloat)SHADOW_MAP_SIZE_FACTOR / gfx->width, (GLfloat)SHADOW_MAP_SIZE_FACTOR / gfx->height);
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
    GLfloat size = 0.25;  /* todo snake size */

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
        glUniform1f(gfx->sprite_shadow.uSize, size);
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
        glUniform1f(gfx->sprite_mat.uSize, size);
        glUniform1i(gfx->sprite_mat.sDiffuse, 0);
        glUniform1i(gfx->sprite_mat.sNM, 1);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gfx->body0_base.texDiffuse);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gfx->body0_base.texNM);
    }

    /* body parts */
    for (i = vector_count(&snake->data.bezier_points) - 1; i >= 1; --i)
    {
        struct bezier_point* bp = vector_get_element(&snake->data.bezier_points, i);
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
    size = 0.5;  /* todo snake size */
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

            glUniform1f(gfx->sprite_shadow.uSize, size);
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
            glUniform1f(gfx->sprite_mat.uSize, size);
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
static int frame_update;
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
