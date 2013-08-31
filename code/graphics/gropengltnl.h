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


#include "globalincs/pstypes.h"
#include "model/model.h"

extern GLint GL_max_elements_vertices;
extern GLint GL_max_elements_indices;

struct poly_list;
struct vertex_buffer;

struct uniform_bind
{
	SCP_string name;
	int type;
	int index;
};

struct uniform_block
{
	int uniform_start_index;
	int num_uniforms;
};

class uniform_handler
{
	SCP_vector<uniform_bind> uniforms;

	SCP_vector<int> uniform_data_ints;
	SCP_vector<float> uniform_data_floats;

	int current_textures[TM_NUM_TYPES];

	SCP_map<SCP_string, int> uniform_lookup;

	void loadUniformLookup(uniform_block *uniforms = NULL);

	void queueUniformi(SCP_string name, int value);
	void queueUniformf(SCP_string name, float value);
public:
	void setTexture(int texture_type, int texture_handle);
	uniform_block generateUniforms(vertex_buffer *buffer, int texi, int flags, int sdr_flags, int prev_sdr_flags, uniform_block *prev_block = NULL);
	void setUniforms(uniform_block uniforms);
};

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

int gr_opengl_create_stream_buffer();
void gr_opengl_update_stream_buffer(int buffer, effect_vertex *buffer_data, uint size);
void gr_opengl_render_stream_buffer(int offset, int n_verts, int flags);
void gr_opengl_render_stream_buffer_start(int buffer_id);
void gr_opengl_render_stream_buffer_end();

void gr_opengl_start_state_block();
int gr_opengl_end_state_block();
void gr_opengl_set_state_block(int);

//void gr_opengl_set_team_color(const SCP_string &team, const SCP_string &secondaryteam = "<none>", fix timestamp = 0, int fadetime = 0);
void gr_opengl_set_team_color(team_color *colors);
void gr_opengl_disable_team_color();

void opengl_tnl_shutdown();

#endif //_GROPENGLTNL_H
