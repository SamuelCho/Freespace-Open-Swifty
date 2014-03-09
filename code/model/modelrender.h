/*
 * Copyright (C) Freespace Open 2013.  All rights reserved.
 *
 * All source code herein is the property of Freespace Open. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#ifndef _MODELRENDER_H
#define _MODELRENDER_H

#include "model/model.h"
#include "Math/vecmat.h"
#include "lighting/lighting.h"
#include "graphics/gropengltnl.h"

extern light Lights[MAX_LIGHTS];
extern int Num_lights;

extern bool in_shadow_map;

extern matrix Object_matrix;
extern vec3d Object_position;

extern team_color* Current_team_color;

extern inline int in_sphere(vec3d *pos, float radius);
extern inline int in_box(vec3d *min, vec3d *max, vec3d *pos);
extern int model_interp_get_texture(texture_info *tinfo, fix base_frametime);

struct interp_data
{
	int objnum;
	matrix orient;
	vec3d pos;

	int transform_texture;

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
	float thrust_scale_x;
	float thrust_scale_y;

	float warp_scale_x;
	float warp_scale_y;
	float warp_scale_z;

	bool draw_distortion;

	bool team_color_set;
	team_color current_team_color;

	interp_data() 
	{
		tmap_flags = 0;
		flags = 0;

		objnum = -1;

		desaturate = false;

		transform_texture = -1;

		detail_level_locked = 0;
		detail_level = 0;

		depth_scale = 1500.0f;

		light = 1.0f;

		base_frametime = 0;

		warp_scale_x = 1.0f;
		warp_scale_y = 1.0f;
		warp_scale_z = 1.0f;

		warp_bitmap = -1;
		warp_alpha = -1.0f;

		xparent_alpha = 1.0f;

		forced_bitmap = -1;

		new_replacement_textures = NULL;

		distortion_thrust_bitmap = -1;

		distortion_thrust_rad_factor = 1.0f;
		distortion_thrust_length_factor = 1.0f;

		draw_distortion = false;

		thrust_scale = 0.1f;
		thrust_scale_x = 0.0f;//added-Bobboau
		thrust_scale_y = 0.0f;//added-Bobboau
		thrust_bitmap = -1;
		thrust_glow_bitmap = -1;
		thrust_glow_noise = 1.0f;
		insignia_bitmap = -1;
		afterburner = false;

		// Bobboau's thruster stuff
		{
			thrust_glow_rad_factor = 1.0f;

			secondary_thrust_glow_bitmap = -1;
			secondary_thrust_glow_rad_factor = 1.0f;

			tertiary_thrust_glow_bitmap = -1;
			tertiary_thrust_glow_rad_factor = 1.0f;

			thrust_glow_len_factor = 1.0f;

			thrust_rotvel = vmd_zero_vector;
		}

		box_scale = 1.0f;
		render_box_min = vmd_zero_vector;
		render_box_max = vmd_zero_vector;
		render_sphere_radius = 0.0f;
		render_sphere_offset = vmd_zero_vector;

		team_color_set = false;
	}
};

struct clip_plane_state
{
	vec3d normal;
	vec3d point;
};

struct arc_effect
{
	matrix orient;
	vec3d pos;
	vec3d v1;
	vec3d v2;
	color primary;
	color secondary;
	float width;
};

struct render_state
{
	int clip_plane_handle;
	int texture_addressing;
	int fill_mode;
	int cull_mode;
	int center_alpha;
	int zbias;
	int buffer_id;

	float animated_timer;
	int animated_effect;

	bool lighting;
	SceneLights::LightIndexingInfo lights;
	
	bool using_team_color;
	team_color tm_color;

	// fog state maybe shouldn't belong here. if we have fog, then it's probably occurring for all objects in scene.
	int fog_mode;
	int r;
	int g;
	int b;
	float fog_near;
	float fog_far;

	render_state()
	{
		clip_plane_handle = -1;
		texture_addressing = TMAP_ADDRESS_WRAP;
		fill_mode = GR_FILL_MODE_SOLID;

		buffer_id = -1;

		lighting = false;

		fog_mode = GR_FOGMODE_NONE;
		r = 0;
		g = 0;
		b = 0;
		fog_near = -1.0f;
		fog_far = -1.0f;

		lights.index_start = 0;
		lights.num_lights = 0;

		animated_timer = 0.0f;
		animated_effect = 0;
	}
};

struct Transform
{
	matrix basis;
	vec3d origin;

	Transform(): basis(vmd_identity_matrix), origin(vmd_zero_vector) {}
	Transform(matrix *m, vec3d *v): basis(*m), origin(*v) {}
};

struct queued_buffer_draw
{
	int render_state_handle;
	int texture_maps[TM_NUM_TYPES];
	int transform_data;

	color clr;
	int blend_filter;
	float alpha;
	int depth_mode;

	Transform transformation;
	vec3d scale;

	vertex_buffer *buffer;
	int texi;
	int flags;
	int sdr_flags;
	uniform_block uniform_handle;

	float thrust_scale;
	float light_factor;

	queued_buffer_draw()
	{
		depth_mode = GR_ZBUFF_FULL;
		light_factor = 1.0f;

		texture_maps[TM_BASE_TYPE]		= -1;
		texture_maps[TM_GLOW_TYPE]		= -1;
		texture_maps[TM_HEIGHT_TYPE]	= -1;
		texture_maps[TM_MISC_TYPE]		= -1;
		texture_maps[TM_NORMAL_TYPE]	= -1;
		texture_maps[TM_SPECULAR_TYPE]	= -1;
	}
};

class DrawList
{
	Transform CurrentTransform;
	SCP_vector<Transform> TransformStack;

	render_state current_render_state;
	bool dirty_render_state;

	SceneLights Lights;
	
	int current_textures[TM_NUM_TYPES];
	int current_blend_filter;
	float current_alpha;
	int current_depth_mode;

	int set_clip_plane;
	SceneLights::LightIndexingInfo current_lights_set;

	void renderBuffer(queued_buffer_draw &render_elements);
	uint determineShaderFlags(render_state *state, queued_buffer_draw *draw_info, vertex_buffer *buffer, int tmap_flags);
	
	SCP_vector<clip_plane_state> clip_planes;
	SCP_vector<render_state> render_states;
	SCP_vector<queued_buffer_draw> render_elements;
	SCP_vector<int> render_keys;

	SCP_vector<arc_effect> arcs;
	
	static DrawList *Target;
	static bool sortDrawPair(const int a, const int b);
public:
	DrawList();
	void addLight(light *light_ptr);

	void resetState();
	void setClipPlane(vec3d *position = NULL, vec3d *normal = NULL);
	void setTexture(int texture_type, int texture_handle);
	void setDepthMode(int depth_set);
	void setBlendFilter(int filter, float alpha);
	void setTextureAddressing(int addressing);
	void setFog(int fog_mode, int r, int g, int b, float fog_near = -1.0f, float fog_far = -1.0f);
	void setFillMode(int mode);
	void setCullMode(int mode);
	void setZBias(int bias);
	void setCenterAlpha(int center_alpha);
	void setLighting(bool mode);
	void setBuffer(int buffer);
	void setTeamColor(team_color *color);
	void setAnimatedTimer(float time);
	void setAnimatedEffect(int effect);
	void addBufferDraw(vec3d *scale, vertex_buffer *buffer, int texi, uint tmap_flags, interp_data *interp);
	
	void clearTransforms();
	void pushTransform(vec3d* pos, matrix* orient);
	void popTransform();

	void addArc(vec3d *v1, vec3d *v2, color *primary, color *secondary, float arc_width);
	void renderArc(arc_effect &arc);
	void renderArcs();

	void setLightFilter(int objnum, vec3d *pos, float rad);

	void sortDraws();
	void renderAll(int blend_filter = -1);
	void reset();
};

class DrawListSorter
{
	static DrawList *Target;
public:
	static void sort(DrawList *target);
	static int sortDrawPair(const void* a, const void* b);
};

void model_immediate_render(int model_num, matrix *orient, vec3d * pos, uint flags = MR_NORMAL, int objnum = -1, int lighting_skip = -1, int *replacement_textures = NULL, int render = MODEL_RENDER_ALL);
void model_queue_render(interp_data *interp, DrawList* scene, int model_num, int model_instance_num, matrix *orient, vec3d *pos, uint flags, int objnum, int *replacement_textures);
void submodel_immediate_render(int model_num, int submodel_num, matrix *orient, vec3d * pos, uint flags = MR_NORMAL, int objnum = -1, int *replacement_textures = NULL);
void submodel_queue_render(interp_data *interp, DrawList *scene, int model_num, int submodel_num, matrix *orient, vec3d * pos, uint flags, int objnum = -1, int *replacement_textures = NULL);
void model_queue_render_buffers(DrawList* scene, interp_data* interp, polymodel *pm, int mn, bool is_child = false);
void model_render_set_thrust(interp_data *interp, int model_num, mst_info *mst);

#endif