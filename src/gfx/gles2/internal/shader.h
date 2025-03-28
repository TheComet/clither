#pragma once

#include "glad/gles2.h"

struct strlist;

GLuint gfx_gles2_load_shader_type(const char* code, GLint length, GLenum type);
GLuint gfx_gles2_load_shader(
    struct strlist* shader_fnames, const char* attribute_bindings[]);
GLuint
gfx_gles2_get_uniform_location_and_warn(GLuint program, const char* name);
