/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/


#ifndef _GROPENGLSHADER_H
#define _GROPENGLSHADER_H

#include "globalincs/pstypes.h"

#include "graphics/gropengl.h"

#include <string>

#define MAX_SHADER_UNIFORMS		15

#define SDR_ATTRIB_RADIUS		0
#define SDR_ATTRIB_SUBMODEL		1

#define MAX_SDR_ATTRIBUTES		4

#define MAX_SDR_UNIFORM_BLOCKS	4

struct opengl_shader_flag_t {
	char *vert;
	char *frag;

	int flags;
};

struct geometry_sdr_params
{
	int input_type;
	int output_type;
	int vertices_out;
};

struct opengl_shader_file_t {
	char *vert;
	char *frag;
	char *geo;

	int flags;

	int num_uniforms;
	char* uniforms[MAX_SHADER_UNIFORMS];

	int num_attributes;
	char* attributes[MAX_SDR_ATTRIBUTES];
};

struct opengl_shader_uniform_reference_t {
	unsigned int flag;

	int num_uniforms;
	char* uniforms[MAX_SHADER_UNIFORMS];

	int num_attributes;
	char* attributes[MAX_SDR_ATTRIBUTES];

	int num_uniform_blocks;
	char* uniform_blocks[MAX_SDR_UNIFORM_BLOCKS];

	const char* name;
};

typedef struct opengl_shader_uniform_t {
	SCP_string text_id;
	GLint location;

	opengl_shader_uniform_t() : location(-1) {}
} opengl_shader_uniform_t;

typedef struct opengl_shader_t {
	GLhandleARB program_id;
	unsigned int flags;
	int flags2;

	SCP_vector<opengl_shader_uniform_t> uniforms;
	SCP_vector<opengl_shader_uniform_t> uniform_blocks;
	SCP_vector<opengl_shader_uniform_t> attributes; // using the uniform data structure to keep track of vert attribs

	opengl_shader_t() :
		program_id(0), flags(0), flags2(0)
	{
	}
} opengl_shader_t;

extern SCP_vector<opengl_shader_t> GL_shader;
extern SCP_vector<opengl_shader_t> GL_effect_shaders;

extern opengl_shader_t *Current_shader;
extern opengl_shader_t Deferred_light_shader;

extern geometry_sdr_params *Current_geo_sdr_params;

int opengl_shader_get_effect_shader(uint flags);

int gr_opengl_maybe_create_shader(unsigned int flags);
void opengl_shader_set_current(opengl_shader_t *shader_obj = NULL);

void opengl_shader_init();
void opengl_shader_shutdown();

void opengl_compile_main_shader(unsigned int flags);
GLhandleARB opengl_shader_create(const char *vs, const char *fs, const char *gs);

void opengl_shader_init_attribute(const char *attribute_text);
GLint opengl_shader_get_attribute(const char *attribute_text);

void opengl_shader_init_uniform(const char *uniform_text);
GLint opengl_shader_get_uniform(const char *uniform_text);

void opengl_shader_init_uniform_block(const char *uniform_text);
GLint opengl_shader_get_uniform_block(const char *uniform_text);

void opengl_shader_set_animated_effect(int effect);
int opengl_shader_get_animated_effect();
void opengl_shader_set_animated_timer(float timer);
float opengl_shader_get_animated_timer();

void opengl_shader_compile_deferred_light_shader();

#define ANIMATED_SHADER_LOADOUTSELECT_FS1	0
#define ANIMATED_SHADER_LOADOUTSELECT_FS2	1
#define ANIMATED_SHADER_CLOAK				2

#endif	// _GROPENGLSHADER_H
