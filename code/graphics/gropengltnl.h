/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/




#ifndef _GROPENGLTNL_H
#define _GROPENGLTNL_H


#include "graphics/gropengl.h"
#include "globalincs/pstypes.h"
#include "model/model.h"
#include "graphics/shadows.h"

extern GLint GL_max_elements_vertices;
extern GLint GL_max_elements_indices;

class poly_list;
class vertex_buffer;

#define TEX_SLOT_DIFFUSE	0
#define TEX_SLOT_GLOW		1
#define TEX_SLOT_SPEC		2
#define TEX_SLOT_ENV		3
#define TEX_SLOT_NORMAL		4
#define TEX_SLOT_HEIGHT		5
#define TEX_SLOT_MISC		6
#define TEX_SLOT_SHADOW		7
#define TEX_SLOT_TRANSFORM	8
#define TEX_SLOT_ANIMATED	9

#define TEX_SLOT_MAX		10

struct uniform_bind
{
	SCP_string name;

	enum data_type {
		INT,
		FLOAT,
		VEC3,
		VEC4,
		MATRIX4
	};

	uniform_bind::data_type type;
	int index;

	int count;
	int tranpose;
};

class opengl_uniform_state
{
	SCP_vector<uniform_bind> uniforms;

	SCP_vector<int> uniform_data_ints;
	SCP_vector<float> uniform_data_floats;
	SCP_vector<vec3d> uniform_data_vec3d;
	SCP_vector<vec4> uniform_data_vec4;
	SCP_vector<matrix4> uniform_data_matrix4;

	SCP_map<SCP_string, int> uniform_lookup;

	int findUniform(SCP_string &name);
public:
	opengl_uniform_state();

	void setUniformi(SCP_string name, int value);
	void setUniformf(SCP_string name, float value);
	void setUniform3f(SCP_string name, vec3d &value);
	void setUniform4f(SCP_string name, vec4 &val);
	void setUniformMatrix4fv(SCP_string name, int count, int transpose, matrix4 *value);
	void setUniformMatrix4f(SCP_string name, int transpose, matrix4 &val);

	void reset();
};

extern opengl_uniform_state Current_uniforms;

extern float shadow_veryneardist;
extern float shadow_neardist;
extern float shadow_middist;
extern float shadow_fardist;
extern bool in_shadow_map;

extern bool GL_use_transform_buffer;

void gr_opengl_start_instance_matrix(vec3d *offset, matrix *rotation);
void gr_opengl_start_instance_angles(vec3d *pos, angles *rotation);
void gr_opengl_end_instance_matrix();
void gr_opengl_set_projection_matrix(float fov, float aspect, float z_near, float z_far);
void gr_opengl_end_projection_matrix();
void gr_opengl_set_view_matrix(vec3d *pos, matrix *orient);
void gr_opengl_end_view_matrix();
void gr_opengl_set_2d_matrix(/*int x, int y, int w, int h*/);
void gr_opengl_end_2d_matrix();
void gr_opengl_push_scale_matrix(vec3d *scale_factor);
void gr_opengl_pop_scale_matrix();

void gr_opengl_start_clip_plane();
void gr_opengl_end_clip_plane();

int gr_opengl_create_buffer();
bool gr_opengl_pack_buffer(const int buffer_id, vertex_buffer *vb);
bool gr_opengl_config_buffer(const int buffer_id, vertex_buffer *vb, bool update_ibuffer_only);
void gr_opengl_destroy_buffer(int idx);
void gr_opengl_set_buffer(int idx);
void gr_opengl_render_buffer(int start, const vertex_buffer *bufferp, int texi, int flags);
void gr_opengl_render_to_env(int FACE);

void gr_opengl_update_buffer_object(int handle, uint size, void* data);

void gr_opengl_update_transform_buffer(void* data, uint size);
void gr_opengl_set_transform_buffer_offset(int offset);

int gr_opengl_create_stream_buffer_object();
void gr_opengl_render_stream_buffer(int offset, int n_verts, int flags);
void gr_opengl_render_stream_buffer_start(int buffer_id);
void gr_opengl_render_stream_buffer_end();

void gr_opengl_start_state_block();
int gr_opengl_end_state_block();
void gr_opengl_set_state_block(int);

void gr_opengl_set_thrust_scale(float scale = -1.0f);
void gr_opengl_set_team_color(team_color *colors);
void gr_opengl_disable_team_color();

void opengl_tnl_shutdown();
void opengl_tnl_set_material(int flags, uint shader_flags, int tmap_type);

#endif //_GROPENGLTNL_H
