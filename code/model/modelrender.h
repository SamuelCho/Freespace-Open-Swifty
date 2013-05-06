/*
 * Copyright (C) Freespace Open 2013.  All rights reserved.
 *
 * All source code herein is the property of Freespace Open. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#include "model/model.h"
#include "Math/vecmat.h"

extern bool in_shadow_map;

extern matrix Object_matrix;
extern vec3d Object_position;

extern team_color* Current_team_color;

extern inline int in_sphere(vec3d *pos, float radius);
extern inline int in_box(vec3d *min, vec3d *max, vec3d *pos);
extern int model_interp_get_texture(texture_info *tinfo, fix base_frametime);

struct clip_plane_state
{
	double clip_equation[4];
};

struct render_state
{
	int clip_plane_handle;
	int blend_filter;
	float alpha;
	int depth_mode;
	int shader_handle;
	int texture_maps[TM_NUM_TYPES];
	int texture_addressing;
	int fill_mode;
	int cull_mode;
	int center_alpha;
	int zbias;

	float animated_timer;
	int animated_effect;

	bool lighting;

	bool using_team_color;
	team_color tm_color;

	// fog state maybe shouldn't belong here. if we have fog, then it's probably occurring for all objects in scene.
	int fog_mode;
	int r;
	int g;
	int b;
	float fog_near;
	float fog_far;
};

struct queued_buffer_draw
{
	int render_state_handle;

	matrix orient;
	vec3d pos;
	vec3d scale;

	vertex_buffer *buffer;
	int texi;
};

class DrawList
{
	SCP_vector<clip_plane_state> clip_planes;
	SCP_vector<render_state> render_states;

	SCP_vector<queued_buffer_draw> render_elements;

	SCP_vector<int> render_keys;

	clip_plane_state current_clip_plane;
	bool clip_plane_set;

	render_state current_render_state;
public:
	void resetState();
	void setClipPlane(vec3d *position, vec3d *normal);
	void setShader(int shader_handle);
	void setTexture(int texture_type, int texture_handle);
	void setDepthMode(int depth_set);
	void setBlendFilter(int filter, float alpha);
	void setTextureAddressing(int addressing);
	void setFog(int fog_mode, int r, int g, int b, float fog_near, float fog_far);
	void setFillMode(int mode);
	void setCullMode(int mode);
	void setZBias(int bias);
	void setLighting(bool mode);
	void setScale(vec3d *scale = NULL);
	void setBuffer(int buffer);
	void setTeamColor(team_color *color);
	void setAnimatedTimer(float time);
	void setAnimatedEffect(int effect);
	void addBufferDraw(matrix* orient, vec3d* pos, vertex_buffer *buffer, int texi, uint tmap_flags);
	void addArc(matrix *orient, vec3d *pos, vec3d *v1, vec3d *v2, color *primary, color *secondary, float arc_width);
};

struct interp_data
{
	int objnum;
	matrix orient;
	vec3d pos;

	int thrust_scale_subobj;
	int flags;
	int tmap_flags;
	int detail_level_locked;
	int detail_level;
	float depth_scale;
	float light;
	fix base_frametime;
	bool desaturate;
	int warp_bitmap;
	float warp_alpha;
	color outline_color;

	float xparent_alpha;

	int forced_bitmap;

	int thrust_bitmap;

	int insignia_bitmap;

	int *new_replacement_textures;

	int thrust_bitmap;
	int thrust_glow_bitmap;

	int secondary_thrust_glow_bitmap;
	int tertiary_thrust_glow_bitmap;
	int distortion_thrust_bitmap;

	float thrust_glow_noise;
	bool afterburner;

	vec3d thrust_rotvel;

	float thrust_glow_rad_factor;

	float secondary_thrust_glow_rad_factor;
	float tertiary_thrust_glow_rad_factor;
	float distortion_thrust_rad_factor;
	float distortion_thrust_length_factor;
	float thrust_glow_len_factor;

	float box_scale; // this is used to scale both detail boxes and spheres
	vec3d render_box_min;
	vec3d render_box_max;
	float render_sphere_radius;
	vec3d render_sphere_offset;

	float thrust_scale;
	float warp_scale_x;
	float warp_scale_y;
	float warp_scale_z;

	interp_data() 
	{
		vec3d zero = ZERO_VECTOR;

		box_scale = 1.0f; 
		render_box_min = zero;
		render_box_max = zero;
		render_sphere_radius = 0.0f;
		render_sphere_offset = zero;

		thrust_scale = 0.1f;

		warp_scale_x = 1.0f;
		warp_scale_y = 1.0f;
		warp_scale_z = 1.0f;

		warp_alpha = -1.0f;

		xparent_alpha = 1.0f;

		thrust_bitmap = -1;

		forced_bitmap = -1;

		insignia_bitmap = -1;

		new_replacement_textures = NULL;

		thrust_bitmap = -1;
		thrust_glow_bitmap = -1;

		secondary_thrust_glow_bitmap = -1;
		tertiary_thrust_glow_bitmap = -1;
		distortion_thrust_bitmap = -1;

		thrust_glow_noise = 1.0f;

		thrust_rotvel = zero;

		thrust_glow_rad_factor = 1.0f;

		secondary_thrust_glow_rad_factor = 1.0f;
		tertiary_thrust_glow_rad_factor = 1.0f;
		distortion_thrust_rad_factor = 1.0f;
		distortion_thrust_length_factor = 1.0f;
		thrust_glow_len_factor = 1.0f;

		afterburner = false;
	}
};

void model_queue_render(interp_data *interp, DrawList* scene, int model_num, matrix *orient, vec3d *pos, uint flags, int objnum, int lighting_skip, int *replacement_textures);
void model_queue_render_set_thrust(interp_data *interp, int model_num, mst_info *mst);