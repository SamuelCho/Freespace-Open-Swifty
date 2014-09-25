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

extern inline int in_sphere(vec3d *pos, float radius, vec3d *view_pos);
extern inline int in_box(vec3d *min, vec3d *max, vec3d *pos, vec3d *view_pos);
extern int model_interp_get_texture(texture_info *tinfo, fix base_frametime);

struct Transform
{
	matrix basis;
	vec3d origin;

	Transform(): basis(vmd_identity_matrix), origin(vmd_zero_vector) {}
	Transform(matrix *m, vec3d *v): basis(*m), origin(*v) {}
};

class model_render_params
{
	int Model_num;
	matrix Orientation;
	vec3d Position;
	int Model_flags;

	int Objnum;
	
	int Detail_level_locked;

	float Depth_scale;

	int Warp_bitmap;
	float Warp_alpha;
	vec3d Warp_scale;

	color Outline_color;

	float Xparent_alpha;

	int Forced_bitmap;

	int Insignia_bitmap;

	int *Replacement_textures;

	bool Team_color_set;
	team_color Current_team_color;

	bool Clip_plane_set;
	vec3d Clip_normal;
	vec3d Clip_pos;

	int Animated_effect;
	float Animated_timer;

	mst_info Thruster_info;

	bool Is_skybox;
	bool Desaturate;
public:
	model_render_params(int model_num, matrix &orient, vec3d &pos, uint flags);

	void set_object_number(int num);
	void set_detail_level_lock(int detail_level_lock);
	void set_depth_scale(float scale);
	void set_warp_params(int bitmap, float alpha, vec3d &scale);
	void set_outline_color(color &clr);
	void set_alpha(float alpha);
	void set_forced_bitmap(int bitmap);
	void set_insignia_bitmap(int bitmap);
	void set_replacement_textures(int *textures);
	void set_team_color(team_color &clr);
	void set_clip_plane(vec3d &pos, vec3d &normal);
	void set_animated_effect(int effect_num, float timer);
	void set_thruster_info(mst_info &info);

	int get_model_number();
	const matrix& get_orientation();
	const vec3d& get_position();
	const int get_model_flags();
	int get_object_number();
	int get_detail_level_lock();
	float get_depth_scale();
	int get_warp_bitmap();
	float get_warp_alpha();
	const vec3d& get_warp_scale();
	const color& get_outline_color();
	float get_alpha();
	int get_forced_bitmap();
	int get_insignia_bitmap();
	int* get_replacement_textures();
	const team_color& get_team_color();
	const vec3d& get_clip_plane_pos();
	const vec3d& get_clip_plane_normal();
	int get_animated_effect_num();
	float get_animated_effect_timer();
	const mst_info& get_thruster_info();
};

struct clip_plane_state
{
	vec3d normal;
	vec3d point;
};

struct arc_effect
{
	Transform transformation;
	vec3d v1;
	vec3d v2;
	color primary;
	color secondary;
	float width;
};

struct insignia_draw_data
{
	Transform transformation;
	polymodel *pm;
	int detail_level;
	int bitmap_num;

	// if there's a clip plane
	int clip_plane;
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
	float light_factor;
	
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
		light_factor = 1.0f;

		animated_timer = 0.0f;
		animated_effect = 0;
	}
};

struct queued_buffer_draw
{
	int render_state_handle;
	int texture_maps[TM_NUM_TYPES];
	int transform_buffer_offset;

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

	queued_buffer_draw()
	{
		depth_mode = GR_ZBUFF_FULL;

		texture_maps[TM_BASE_TYPE]		= -1;
		texture_maps[TM_GLOW_TYPE]		= -1;
		texture_maps[TM_HEIGHT_TYPE]	= -1;
		texture_maps[TM_MISC_TYPE]		= -1;
		texture_maps[TM_NORMAL_TYPE]	= -1;
		texture_maps[TM_SPECULAR_TYPE]	= -1;
	}
};

class ModelTransformBuffer
{
	SCP_vector<matrix4> TransformMatrices;
	void* MemAlloc;
	uint MemAllocSize;

	int CurrentOffset;

	void AllocateMemory();
public:
	ModelTransformBuffer() : CurrentOffset(0), MemAlloc(NULL), MemAllocSize(0) {};

	void reset();

	int getBufferOffset();
	void setNumModels(int n_models);
	void setModelTransform(matrix4 &transform, int model_id);

	void submitBufferData();

	void addMatrix(matrix4 &mat);
};

class DrawList
{
	Transform CurrentTransform;
	vec3d CurrentScale;
	SCP_vector<Transform> TransformStack;

	render_state current_render_state;
	bool dirty_render_state;

	SceneLights SceneLightHandler;
	
	int current_textures[TM_NUM_TYPES];
	int current_blend_filter;
	float current_alpha;
	int current_depth_mode;

	int set_clip_plane;
	SceneLights::LightIndexingInfo current_lights_set;

	void renderArc(arc_effect &arc);
	void renderInsignia(insignia_draw_data &insignia_info);
	void renderBuffer(queued_buffer_draw &render_elements);
	uint determineShaderFlags(render_state *state, queued_buffer_draw *draw_info, vertex_buffer *buffer, int tmap_flags);
	
	SCP_vector<clip_plane_state> clip_planes;
	SCP_vector<render_state> render_states;
	SCP_vector<queued_buffer_draw> render_elements;
	SCP_vector<int> render_keys;

	SCP_vector<arc_effect> arcs;
	SCP_vector<insignia_draw_data> insignias;

	static DrawList *Target;
	static bool sortDrawPair(const int a, const int b);
	void sortDraws();
public:
	DrawList();
	void init();

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
	void setTeamColor(team_color *color = NULL);
	void setAnimatedTimer(float time);
	void setAnimatedEffect(int effect);
	void setModelTransformBuffer(int model_num);
	void startModelDraw(int n_models);

	void addBufferDraw(vertex_buffer *buffer, int texi, uint tmap_flags, model_render_params *interp);
	
	vec3d getViewPosition();
	void clearTransforms();
	void pushTransform(vec3d* pos, matrix* orient);
	void popTransform();
	void setScale(vec3d *scale = NULL);

	void addArc(vec3d *v1, vec3d *v2, color *primary, color *secondary, float arc_width);
	void renderArcs();

	void addInsignia(polymodel *pm, int detail_level, int bitmap_num);
	void renderInsignias();

	void setLightFilter(int objnum, vec3d *pos, float rad);
	void setLightFactor(float factor);

	void initRender();
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

void model_immediate_render(int model_num, matrix *orient, vec3d * pos, uint flags = MR_NORMAL, int objnum = -1, int lighting_skip = -1, int *replacement_textures = NULL, int render = MODEL_RENDER_ALL, const bool is_skybox = false);
void model_queue_render(model_render_params *interp, DrawList* scene, int model_num, int model_instance_num, matrix *orient, vec3d *pos, uint flags, int objnum, int *replacement_textures, const bool is_skybox = false);
void submodel_immediate_render(int model_num, int submodel_num, matrix *orient, vec3d * pos, uint flags = MR_NORMAL, int objnum = -1, int *replacement_textures = NULL);
void submodel_queue_render(model_render_params *interp, DrawList *scene, int model_num, int submodel_num, matrix *orient, vec3d * pos, uint flags, int objnum = -1, int *replacement_textures = NULL);
void model_queue_render_buffers(DrawList* scene, model_render_params* interp, polymodel *pm, int mn, int detail_level, uint tmap_flags);
void model_render_set_thrust(model_render_params *interp, int model_num, mst_info *mst);
void model_render_set_clip_plane(model_render_params *interp, vec3d *pos = NULL, vec3d *normal = NULL);

#endif