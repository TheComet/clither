#include "./gfx.h"
#include "./shader.h"
#include "clither/platform/mfile.h"
#include "clither/util/log.h"
#include "clither/util/mem.h"
#include "clither/util/str.h"
#include "clither/util/strlist.h"

GLuint gfx_gles2_load_shader_type(const char* code, GLint length, GLenum type)
{
    GLuint shader;
    GLint  compiled;

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
            char* info = mem_alloc(sizeof(char) * info_len);
            glGetShaderInfoLog(shader, info_len, NULL, info);
            log_err("glCompileShader() error:\n%s", info);
            mem_free(info);
        }

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint gfx_gles2_load_shader_str(
    const char* vs, const char* fs, const char* attribute_bindings[])
{
    int    i;
    GLuint shader;
    GLuint program;
    GLint  linked;

    program = glCreateProgram();
    if (program == 0)
    {
        log_err("glCreateProgram() failed\n");
        goto create_program_failed;
    }
    gfx_track_shader(program);

    shader = gfx_gles2_load_shader_type(vs, strlen(vs), GL_VERTEX_SHADER);
    if (shader == 0)
        goto link_program_failed;
    glAttachShader(program, shader);
    glDeleteShader(shader);

    shader = gfx_gles2_load_shader_type(fs, strlen(fs), GL_FRAGMENT_SHADER);
    if (shader == 0)
        goto link_program_failed;
    glAttachShader(program, shader);
    glDeleteShader(shader);

    for (i = 0; attribute_bindings[i]; ++i)
        if (attribute_bindings[i] != NULL)
            glBindAttribLocation(program, i, attribute_bindings[i]);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint info_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1)
        {
            char* info = mem_alloc(sizeof(char) * info_len);
            glGetProgramInfoLog(program, info_len, NULL, info);
            log_err("Failed to link shader\n%s\n", info);
            mem_free(info);
            goto link_program_failed;
        }
    }

    return program;

link_program_failed:
    gfx_untrack_shader(program);
    glDeleteProgram(program);
create_program_failed:
    log_err("Failed to compile shader (str)\n");
    return 0;
}

GLuint gfx_gles2_load_shader(
    struct strlist* shader_fnames, const char* attribute_bindings[])
{
    int         i;
    const char* fname;
    GLuint      program;
    GLint       linked;

    if (strlist_count(shader_fnames) == 0)
    {
        log_warn("No shader files provided\n");
        return 0;
    }

    program = glCreateProgram();
    if (program == 0)
    {
        log_err("glCreateProgram() failed\n");
        goto create_program_failed;
    }
    gfx_track_shader(program);

    strlist_for_each_cstr (shader_fnames, i, fname)
    {
        GLuint       shader;
        struct mfile source;

        log_dbg("Loading shader file \"%s\"\n", fname);
        if (mfile_map_read(&source, fname, 1) != 0)
            goto load_shaders_failed;

        shader = gfx_gles2_load_shader_type(
            source.address,
            source.size,
            cstr_ends_with(fname, ".vsh") ? GL_VERTEX_SHADER
                                          : GL_FRAGMENT_SHADER);
        mfile_unmap(&source);
        if (shader == 0)
            goto load_shaders_failed;

        glAttachShader(program, shader);
        glDeleteShader(shader);
    }

    for (i = 0; attribute_bindings[i]; ++i)
        if (attribute_bindings[i] != NULL)
            glBindAttribLocation(program, i, attribute_bindings[i]);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint info_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1)
        {
            char* info = mem_alloc(sizeof(char) * info_len);
            glGetProgramInfoLog(program, info_len, NULL, info);
            log_err("Failed to link shader\n%s\n", info);
            mem_free(info);
            goto link_program_failed;
        }
    }

    return program;

link_program_failed:
load_shaders_failed:
    glDeleteProgram(program);
create_program_failed:
    log_err("Failed to compile shader: %s\n", fname);
    return 0;
}

GLuint gfx_gles2_get_uniform_location_and_warn(GLuint program, const char* name)
{
    GLuint ret = glGetUniformLocation(program, name);
    if (ret == (GLuint)-1)
        log_warn("Failed to get uniform location of \"%s\"\n", name);
    return ret;
}
