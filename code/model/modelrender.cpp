/*
 * Copyright (C) Freespace Open 2013.  All rights reserved.
 *
 * All source code herein is the property of Freespace Open. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#include "globalincs/pstypes.h"
#include "io/timer.h"
#include "Math/vecmat.h"
#include "model/model.h"
#include "model/modelrender.h"
#include "ship/ship.h"
#include "ship/shipfx.h"
#include "cmdline/cmdline.h"
#include "nebula/neb.h"
#include "graphics/tmapper.h"
#include "graphics/gropenglextension.h"
#include "graphics/gropenglshader.h"
#include "graphics/gropengldraw.h"
#include "particle/particle.h"
#include "gamesequence/gamesequence.h"
#include "render/3dinternal.h"
#include "Math/staticrand.h"

extern int Model_texturing;
extern int Model_polys;
extern int tiling;
extern float model_radius;

extern const int MAX_ARC_SEGMENT_POINTS;
extern int Num_arc_segment_points;
extern vec3d Arc_segment_points[];

extern bool Scene_framebuffer_in_frame;

extern void interp_render_arc_segment( vec3d *v1, vec3d *v2, int depth );

extern int Interp_thrust_scale_subobj;
extern float Interp_thrust_scale;

DrawList *DrawList::Target = NULL;

ModelTransformBuffer TransformBufferHandler;

model_render_params::model_render_params():
	Model_flags(MR_NORMAL),
	Debug_flags(0),
	Objnum(-1),
	Detail_level_locked(-1),
	Depth_scale(1500.0f),
	Warp_bitmap(-1),
	Warp_alpha(-1.0f),
	Xparent_alpha(1.0f),
	Forced_bitmap(-1),
	Replacement_textures(NULL),
	Insignia_bitmap(-1),
	Team_color_set(false),
	Clip_plane_set(false),
	Animated_effect(-1),
	Animated_timer(0.0f),
	Thruster_info()
{
	Warp_scale.xyz.x = 1.0f;
	Warp_scale.xyz.y = 1.0f;
	Warp_scale.xyz.z = 1.0f;
	
	Clip_normal = vmd_zero_vector;
	Clip_pos = vmd_zero_vector;

	if ( !Model_texturing )
		Model_flags |= MR_NO_TEXTURING;

	if ( !Model_polys )	{
		Model_flags |= MR_NO_POLYS;
	}
}

const uint model_render_params::get_model_flags()
{
	return Model_flags; 
}

const uint model_render_params::get_debug_flags()
{
	return Debug_flags;
}

const int model_render_params::get_object_number()
{ 
	return Objnum; 
}

const int model_render_params::get_detail_level_lock()
{ 
	return Detail_level_locked; 
}

const float model_render_params::get_depth_scale()
{ 
	return Depth_scale; 
}

const int model_render_params::get_warp_bitmap()
{ 
	return Warp_bitmap; 
}

const float model_render_params::get_warp_alpha()
{ 
	return Warp_alpha; 
}

const vec3d& model_render_params::get_warp_scale()
{ 
	return Warp_scale; 
}

const color& model_render_params::get_outline_color()
{ 
	return Outline_color; 
}
const float model_render_params::get_alpha()
{ 
	return Xparent_alpha; 
}

const int model_render_params::get_forced_bitmap()
{ 
	return Forced_bitmap; 
}

const int model_render_params::get_insignia_bitmap()
{ 
	return Insignia_bitmap; 
}

const int* model_render_params::get_replacement_textures()
{ 
	return Replacement_textures; 
}

const team_color& model_render_params::get_team_color()
{ 
	return Current_team_color; 
}

const vec3d& model_render_params::get_clip_plane_pos()
{ 
	return Clip_pos; 
}

const vec3d& model_render_params::get_clip_plane_normal()
{ 
	return Clip_normal; 
}

const int model_render_params::get_animated_effect_num()
{ 
	return Animated_effect; 
}

const float model_render_params::get_animated_effect_timer()
{ 
	return Animated_timer; 
}

void model_render_params::set_animated_effect(int effect_num, float timer)
{
	Animated_effect = effect_num;
	Animated_timer = timer;
}

void model_render_params::set_clip_plane(vec3d &pos, vec3d &normal)
{
	Clip_plane_set = true;

	Clip_normal = normal;
	Clip_pos = pos;
}

bool model_render_params::is_clip_plane_set()
{
	return Clip_plane_set;
}

void model_render_params::set_team_color(team_color &clr)
{
	Team_color_set = true;

	Current_team_color = clr;
}

void model_render_params::set_team_color(const SCP_string &team, const SCP_string &secondaryteam, fix timestamp, int fadetime)
{
	Team_color_set = model_get_team_color(&Current_team_color, team, secondaryteam, timestamp, fadetime);
}

bool model_render_params::is_team_color_set()
{
	return Team_color_set;
}

void model_render_params::set_replacement_textures(int *textures)
{
	Replacement_textures = textures;
}

void model_render_params::set_insignia_bitmap(int bitmap)
{
	Insignia_bitmap = bitmap;
}

void model_render_params::set_forced_bitmap(int bitmap)
{
	Forced_bitmap = bitmap;
}

void model_render_params::set_alpha(float alpha)
{
	Xparent_alpha = alpha;
}

void model_render_params::set_outline_color(color &clr)
{
	Outline_color = clr;
}

void model_render_params::set_outline_color(int r, int g, int b)
{
	gr_init_color( &Outline_color, r, g, b );
}

void model_render_params::set_warp_params(int bitmap, float alpha, vec3d &scale)
{
	Warp_bitmap = bitmap;
	Warp_alpha = alpha;
	Warp_scale = scale;
}

void model_render_params::set_depth_scale(float scale)
{
	Depth_scale = scale;
}

void model_render_params::set_debug_flags(uint flags)
{
	Debug_flags = flags;
}

void model_render_params::set_object_number(int num)
{
	Objnum = num;
}

void model_render_params::set_flags(uint flags)
{
	Model_flags = flags;
}

void model_render_params::set_detail_level_lock(int detail_level_lock)
{
	Detail_level_locked = detail_level_lock;
}

void model_render_params::set_thruster_info(mst_info &info)
{
	Thruster_info = info;

	CLAMP(Thruster_info.length.xyz.z, 0.1f, 1.0f);
}

const mst_info& model_render_params::get_thruster_info()
{
	return Thruster_info;
}

void ModelTransformBuffer::reset()
{
	TransformMatrices.clear();

	CurrentOffset = 0;
}

void ModelTransformBuffer::setNumModels(int n_models)
{
	matrix4 init_mat;

	memset(&init_mat, 0, sizeof(matrix4));

	init_mat.a1d[0] = 1.0f;
	init_mat.a1d[5] = 1.0f;
	init_mat.a1d[10] = 1.0f;
	init_mat.a1d[15] = 1.0f;	// set this to zero to indicate it's not to be drawn in the shader

	CurrentOffset = TransformMatrices.size();

	for ( int i = 0; i < n_models; ++i ) {
		TransformMatrices.push_back(init_mat);
	}
}

void ModelTransformBuffer::setModelTransform(matrix4 &transform, int model_id)
{
	TransformMatrices[CurrentOffset + model_id] = transform;
}

void ModelTransformBuffer::addMatrix(matrix4 &mat)
{
	TransformMatrices.push_back(mat);
}

int ModelTransformBuffer::getBufferOffset()
{
	return CurrentOffset;
}

void ModelTransformBuffer::AllocateMemory()
{
	uint size = TransformMatrices.size() * sizeof(matrix4);

	if ( MemAlloc == NULL || MemAllocSize < size ) {
		if ( MemAlloc != NULL ) {
			vm_free(MemAlloc);
		}

		MemAlloc = vm_malloc(size);
	}

	MemAllocSize = size;
	memcpy(MemAlloc, &TransformMatrices[0], size);
}

void ModelTransformBuffer::submitBufferData()
{
	if ( TransformMatrices.size() == 0 ) {
		return;
	}

	AllocateMemory();

	gr_update_transform_buffer(MemAlloc, MemAllocSize);
}

DrawList::DrawList()
{
	reset();
}

void DrawList::reset()
{
	set_clip_plane = -1;

	dirty_render_state = true;

	current_render_state = render_state();

	current_textures[TM_BASE_TYPE] = -1;
	current_textures[TM_GLOW_TYPE] = -1;
	current_textures[TM_SPECULAR_TYPE] = -1;
	current_textures[TM_NORMAL_TYPE] = -1;
	current_textures[TM_HEIGHT_TYPE] = -1;
	current_textures[TM_MISC_TYPE] = -1;

	clip_planes.clear();
	render_states.clear();
	render_elements.clear();
	render_keys.clear();

	clearTransforms();

	CurrentScale.xyz.x = 1.0f;
	CurrentScale.xyz.y = 1.0f;
	CurrentScale.xyz.z = 1.0f;
}

void DrawList::sortDraws()
{
	Target = this;
	std::sort(Target->render_keys.begin(), Target->render_keys.end(), DrawList::sortDrawPair);
}

void DrawList::startModelDraw(int n_models)
{
	TransformBufferHandler.setNumModels(n_models);
}

void DrawList::setModelTransformBuffer(int model_num)
{
	matrix4 transform;

	memset(&transform, 0, sizeof(matrix4));

	// set basis
	transform.a1d[0] = CurrentTransform.basis.a1d[0] * CurrentScale.xyz.x;
	transform.a1d[1] = CurrentTransform.basis.a1d[1];
	transform.a1d[2] = CurrentTransform.basis.a1d[2];

	transform.a1d[4] = CurrentTransform.basis.a1d[3];
	transform.a1d[5] = CurrentTransform.basis.a1d[4] * CurrentScale.xyz.y;
	transform.a1d[6] = CurrentTransform.basis.a1d[5];

	transform.a1d[8] = CurrentTransform.basis.a1d[6];
	transform.a1d[9] = CurrentTransform.basis.a1d[7];
	transform.a1d[10] = CurrentTransform.basis.a1d[8] * CurrentScale.xyz.z;

	// set position
	transform.a1d[12] = CurrentTransform.origin.a1d[0];
	transform.a1d[13] = CurrentTransform.origin.a1d[1];
	transform.a1d[14] = CurrentTransform.origin.a1d[2];

	// set visibility
	transform.a1d[15] = 0.0f;

	TransformBufferHandler.setModelTransform(transform, model_num);
}

void DrawList::setDepthMode(int depth_set)
{
// 	if ( !dirty_render_state && depth_set != current_render_state.depth_mode ) {
// 		dirty_render_state = true;
// 	}

	current_depth_mode = depth_set;
}

void DrawList::addArc(vec3d *v1, vec3d *v2, color *primary, color *secondary, float arc_width)
{
	arc_effect new_arc;

	new_arc.transformation = CurrentTransform;
	new_arc.v1 = *v1;
	new_arc.v2 = *v2;
	new_arc.primary = *primary;
	new_arc.secondary = *secondary;
	new_arc.width = arc_width;

	arcs.push_back(new_arc);
}

void DrawList::setLightFilter(int objnum, vec3d *pos, float rad)
{
	SceneLightHandler.setLightFilter(objnum, pos, rad);

	dirty_render_state = true;
	current_render_state.lights = SceneLightHandler.bufferLights();
}

void DrawList::setLightFactor(float factor)
{
	current_render_state.light_factor = factor;
}

void DrawList::setClipPlane(const vec3d &position, const vec3d &normal)
{
	clip_plane_state clip_normal;

	clip_normal.point = position;
	clip_normal.normal = normal;

	clip_planes.push_back(clip_normal);

	current_render_state.clip_plane_handle = clip_planes.size() - 1;
}

void DrawList::setClipPlane()
{
	current_render_state.clip_plane_handle = -1;
}

void DrawList::setThrustScale(float scale)
{
	current_render_state.thrust_scale = scale;
}

void DrawList::addBufferDraw(vertex_buffer *buffer, int texi, uint tmap_flags, model_render_params *interp)
{
	// need to do a check to see if the top render state matches the current.
	//if ( dirty_render_state ) {
		render_states.push_back(current_render_state);
	//}

	dirty_render_state = false;

	queued_buffer_draw draw_data;

	draw_data.render_state_handle = render_states.size() - 1;
	draw_data.buffer = buffer;
	draw_data.texi = texi;
	draw_data.flags = tmap_flags;

	draw_data.clr = gr_screen.current_color;
	draw_data.alpha = current_alpha;
	draw_data.blend_filter = current_blend_filter;
	draw_data.depth_mode = current_depth_mode;

	if ( tmap_flags & TMAP_FLAG_BATCH_TRANSFORMS ) {
 		draw_data.transformation = Transform();
 
  		draw_data.scale.xyz.x = 1.0f;
  		draw_data.scale.xyz.y = 1.0f;
  		draw_data.scale.xyz.z = 1.0f;

		draw_data.transform_buffer_offset = TransformBufferHandler.getBufferOffset();
	} else {
		draw_data.transformation = CurrentTransform;
		draw_data.scale = CurrentScale;
		draw_data.transform_buffer_offset = -1;
	}
	
	draw_data.texture_maps[TM_BASE_TYPE] = current_textures[TM_BASE_TYPE];
	draw_data.texture_maps[TM_GLOW_TYPE] = current_textures[TM_GLOW_TYPE];
	draw_data.texture_maps[TM_SPECULAR_TYPE] = current_textures[TM_SPECULAR_TYPE];
	draw_data.texture_maps[TM_NORMAL_TYPE] = current_textures[TM_NORMAL_TYPE];
	draw_data.texture_maps[TM_HEIGHT_TYPE] = current_textures[TM_HEIGHT_TYPE];
	draw_data.texture_maps[TM_MISC_TYPE] = current_textures[TM_MISC_TYPE];

	draw_data.sdr_flags = determineShaderFlags(&current_render_state, &draw_data, buffer, tmap_flags);

	render_elements.push_back(draw_data);

	render_keys.push_back(render_elements.size() - 1);
}

uint DrawList::determineShaderFlags(render_state *state, queued_buffer_draw *draw_info, vertex_buffer *buffer, int tmap_flags)
{
	bool texture = (tmap_flags & TMAP_FLAG_TEXTURED) && (buffer->flags & VB_FLAG_UV1);
	bool fog = false;
	bool use_thrust_scale = false;

	if ( state->fog_mode == GR_FOGMODE_FOG ) {
		fog = true;
	}

	if ( draw_info->thrust_scale > 0.0f ) {
		use_thrust_scale = true;
	}

	return gr_determine_shader_flags(
		state->lighting, 
		fog, 
		texture, 
		in_shadow_map, 
		use_thrust_scale,
		tmap_flags & TMAP_FLAG_BATCH_TRANSFORMS && draw_info->transform_buffer_offset >= 0 && buffer->flags & VB_FLAG_MODEL_ID,
		state->using_team_color,
		tmap_flags, 
		draw_info->texture_maps[TM_SPECULAR_TYPE],
		draw_info->texture_maps[TM_GLOW_TYPE],
		draw_info->texture_maps[TM_NORMAL_TYPE],
		draw_info->texture_maps[TM_HEIGHT_TYPE],
		ENVMAP,
		draw_info->texture_maps[TM_MISC_TYPE]
	);
}

void DrawList::renderBuffer(queued_buffer_draw &render_elements)
{
	// get the render state for this draw call
	int render_state_num = render_elements.render_state_handle;

	render_state &draw_state = render_states[render_state_num];

	// set clip plane if necessary
	if ( draw_state.clip_plane_handle >= 0 && draw_state.clip_plane_handle != set_clip_plane ) {
		if ( set_clip_plane >= 0 ) {
			g3_stop_user_clip_plane();
		}

		set_clip_plane = draw_state.clip_plane_handle;

		clip_plane_state *clip_plane = &clip_planes[set_clip_plane];

		g3_start_user_clip_plane(&clip_plane->point, &clip_plane->normal);
	} else if ( draw_state.clip_plane_handle < 0 && set_clip_plane >= 0 ) {
		// stop the clip plane if this draw call doesn't have clip plane and clip plane is set.
		set_clip_plane = -1;
		g3_stop_user_clip_plane();
	}

	opengl_shader_set_animated_effect(draw_state.animated_effect);
	opengl_shader_set_animated_timer(draw_state.animated_timer);

	if ( draw_state.using_team_color ) {
		gr_set_team_color(&draw_state.tm_color);
	} else {
		gr_set_team_color(NULL);
	}

	gr_set_texture_addressing(draw_state.texture_addressing);

	gr_fog_set(draw_state.fog_mode, draw_state.r, draw_state.g, draw_state.b, draw_state.fog_near, draw_state.fog_far);

	gr_zbuffer_set(render_elements.depth_mode);

	gr_set_cull(draw_state.cull_mode);

	gr_set_fill_mode(draw_state.fill_mode);

	gr_center_alpha(draw_state.center_alpha);

	gr_set_transform_buffer_offset(render_elements.transform_buffer_offset);

	gr_set_color_fast(&render_elements.clr);

	gr_set_light_factor(draw_state.light_factor);

	if ( draw_state.lighting ) {
		SceneLightHandler.setLights(&draw_state.lights);
	} else {
		gr_set_lighting(false, false);

		SceneLightHandler.resetLightState();
	}

	gr_set_buffer(draw_state.buffer_id);

	gr_zbias(draw_state.zbias);

	gr_set_thrust_scale(draw_state.thrust_scale);

	g3_start_instance_matrix(&render_elements.transformation.origin, &render_elements.transformation.basis);

	gr_push_scale_matrix(&render_elements.scale);

	gr_set_bitmap(render_elements.texture_maps[TM_BASE_TYPE], render_elements.blend_filter, GR_BITBLT_MODE_NORMAL, render_elements.alpha);

	GLOWMAP = render_elements.texture_maps[TM_GLOW_TYPE];
	SPECMAP = render_elements.texture_maps[TM_SPECULAR_TYPE];
	NORMMAP = render_elements.texture_maps[TM_NORMAL_TYPE];
	HEIGHTMAP = render_elements.texture_maps[TM_HEIGHT_TYPE];
	MISCMAP = render_elements.texture_maps[TM_MISC_TYPE];

	gr_render_buffer(0, render_elements.buffer, render_elements.texi, render_elements.flags);

	GLOWMAP = -1;
	SPECMAP = -1;
	NORMMAP = -1;
	HEIGHTMAP = -1;
	MISCMAP = -1;

	gr_pop_scale_matrix();

	g3_done_instance(true);
}

vec3d DrawList::getViewPosition()
{
	matrix basis_world;

	// get the world basis of our current local space.
	vm_matrix_x_matrix(&basis_world, &Object_matrix, &CurrentTransform.basis);

	vec3d eye_pos_local;
	vm_vec_sub(&eye_pos_local, &Eye_position, &CurrentTransform.origin);

	vec3d return_val;
	vm_vec_rotate(&return_val, &eye_pos_local, &basis_world);

	return return_val;
}

void DrawList::clearTransforms()
{
	CurrentTransform = Transform();
	TransformStack.clear();
}

void DrawList::pushTransform(vec3d *pos, matrix *orient)
{
	matrix basis;
	vec3d origin;

	if ( orient == NULL ) {
		basis = vmd_identity_matrix;
	} else {
		basis = *orient;
	}

	if ( pos == NULL ) {
		origin = vmd_zero_vector;
	} else {
		origin = *pos;
	}

	if ( TransformStack.size() == 0 ) {
		CurrentTransform.basis = basis;
		CurrentTransform.origin = origin;

		TransformStack.push_back(CurrentTransform);

		return;
	}

	vec3d tempv;
	Transform newTransform = CurrentTransform;

	vm_vec_unrotate(&tempv, &origin, &CurrentTransform.basis);
	vm_vec_add2(&newTransform.origin, &tempv);

	vm_matrix_x_matrix(&newTransform.basis, &CurrentTransform.basis, &basis);

	CurrentTransform = newTransform;
	TransformStack.push_back(CurrentTransform);
}

void DrawList::popTransform()
{
	Assert( TransformStack.size() > 0 );

	TransformStack.pop_back();

	if ( TransformStack.size() > 0 ) {
		CurrentTransform = TransformStack.back();
	} else {
		CurrentTransform = Transform();
	}
}

void DrawList::setScale(vec3d *scale)
{
	if ( scale == NULL ) {
		CurrentScale.xyz.x = 1.0f;
		CurrentScale.xyz.y = 1.0f;
		CurrentScale.xyz.z = 1.0f;
		return;
	}

	CurrentScale = *scale;
}

void DrawList::setBuffer(int buffer)
{
// 	if ( !dirty_render_state && current_render_state.buffer_id != buffer) {
// 		dirty_render_state = true;
// 	}

	current_render_state.buffer_id = buffer;
}

void DrawList::setBlendFilter(int filter, float alpha)
{
// 	if ( !dirty_render_state && ( current_render_state.alpha != alpha && current_render_state.blend_filter != filter ) ) {
// 		dirty_render_state = true;
// 	}

// 	current_blend_filter = GR_ALPHABLEND_NONE;
// 	current_alpha = 1.0f;

	current_blend_filter = filter;
	current_alpha = alpha;
}

void DrawList::setTexture(int texture_type, int texture_handle)
{
	Assert(texture_type > -1);
	Assert(texture_type < TM_NUM_TYPES);

	current_textures[texture_type] = texture_handle;
}

void DrawList::setCullMode(int mode)
{
	current_render_state.cull_mode = mode;
}

void DrawList::setZBias(int bias)
{
	current_render_state.zbias = bias;
}

void DrawList::setLighting(bool lighting)
{
	current_render_state.lighting = lighting;
}

void DrawList::setTeamColor(const team_color &color)
{
	current_render_state.using_team_color = true;
	current_render_state.tm_color = color;
}

void DrawList::setTeamColor()
{
	current_render_state.using_team_color = false;
}

void DrawList::setAnimatedTimer(float time)
{
	current_render_state.animated_timer = time;
}

void DrawList::setAnimatedEffect(int effect)
{
	current_render_state.animated_effect = effect;
}

void DrawList::setTextureAddressing(int addressing)
{
	current_render_state.texture_addressing = addressing;
}

void DrawList::setFog(int fog_mode, int r, int g, int b, float fog_near, float fog_far)
{
	current_render_state.fog_mode = fog_mode;
	current_render_state.r = r;
	current_render_state.g = g;
	current_render_state.b = b;
	current_render_state.fog_near = fog_near;
	current_render_state.fog_far = fog_far;
}

void DrawList::setFillMode(int mode)
{
	current_render_state.fill_mode = mode;
}

void DrawList::setCenterAlpha(int center_alpha)
{
	current_render_state.center_alpha = center_alpha;
}

void DrawList::init()
{
	reset();

	for ( int i = 0; i < Num_lights; ++i ) {
		if ( Lights[i].type == LT_DIRECTIONAL || !Deferred_lighting ) {
			SceneLightHandler.addLight(&Lights[i]);
		}	
	}

	TransformBufferHandler.reset();
}

void DrawList::initRender()
{
	sortDraws();

	SceneLightHandler.resetLightState();
	Current_uniforms.resetAll();
	TransformBufferHandler.submitBufferData();
}

void DrawList::renderAll(int blend_filter)
{
	for ( size_t i = 0; i < render_keys.size(); ++i ) {
		int render_index = render_keys[i];

		if ( blend_filter == -1 || render_elements[render_index].blend_filter == blend_filter ) {
			renderBuffer(render_elements[render_index]);
		}
	}

	if ( set_clip_plane >= 0 ) {
		g3_stop_user_clip_plane();
	}
}

void DrawList::renderArc(arc_effect &arc)
{
	g3_start_instance_matrix(&arc.transformation.origin, &arc.transformation.basis);	

	interp_render_arc(&arc.v1, &arc.v2, &arc.primary, &arc.secondary, arc.width);

	g3_done_instance(true);
}

void DrawList::renderArcs()
{
	int mode = gr_zbuffer_set(GR_ZBUFF_READ);

	for ( size_t i = 0; i < arcs.size(); ++i ) {
		renderArc(arcs[i]);
	}

	gr_zbuffer_set(mode);
}

void DrawList::addInsignia(polymodel *pm, int detail_level, int bitmap_num)
{
	insignia_draw_data new_insignia;

	new_insignia.transformation = CurrentTransform;
	new_insignia.pm = pm;
	new_insignia.detail_level = detail_level;
	new_insignia.bitmap_num = bitmap_num;

	new_insignia.clip_plane = current_render_state.clip_plane_handle;

	insignias.push_back(new_insignia);
}

void DrawList::renderInsignia(insignia_draw_data &insignia_info)
{
	if ( insignia_info.clip_plane >= 0 ) {
		clip_plane_state &plane_data = clip_planes[insignia_info.clip_plane];

		vec3d tmp;
		vm_vec_sub(&tmp, &insignia_info.transformation.origin, &plane_data.point);
		vm_vec_normalize(&tmp);

		if ( vm_vec_dot(&tmp, &plane_data.normal) < 0.0f) {
			return;
		}
	}

	g3_start_instance_matrix(&insignia_info.transformation.origin, &insignia_info.transformation.basis);	

	model_render_insignias(insignia_info.pm, insignia_info.detail_level, insignia_info.bitmap_num);

	g3_done_instance(true);
}

void DrawList::renderInsignias()
{
	int mode = gr_zbuffer_set(GR_ZBUFF_READ);
	gr_zbias(1);

	for ( size_t i = 0; i < insignias.size(); ++i ) {
		renderInsignia(insignias[i]);
	}

	gr_zbias(0);
	gr_zbuffer_set(mode);
}

bool DrawList::sortDrawPair(const int a, const int b)
{
	queued_buffer_draw *draw_call_a = &Target->render_elements[a];
	queued_buffer_draw *draw_call_b = &Target->render_elements[b];

	render_state *render_state_a = &Target->render_states[draw_call_a->render_state_handle];
	render_state *render_state_b = &Target->render_states[draw_call_b->render_state_handle];

	if ( render_state_a->clip_plane_handle != render_state_b->clip_plane_handle ) {
		return render_state_a->clip_plane_handle < render_state_b->clip_plane_handle;
	}
	
	if ( draw_call_a->blend_filter != draw_call_b->blend_filter ) {
		return draw_call_a->blend_filter < draw_call_b->blend_filter;
	}

	if ( draw_call_a->sdr_flags != draw_call_b->sdr_flags ) {
		return draw_call_a->sdr_flags < draw_call_b->sdr_flags;
	}
	
	if ( render_state_a->buffer_id != render_state_b->buffer_id) {
		return render_state_a->buffer_id < render_state_b->buffer_id;
	}

	if ( draw_call_a->texture_maps[TM_BASE_TYPE] != draw_call_b->texture_maps[TM_BASE_TYPE] ) {
		return draw_call_a->texture_maps[TM_BASE_TYPE] < draw_call_b->texture_maps[TM_BASE_TYPE];
	}

	if ( draw_call_a->texture_maps[TM_SPECULAR_TYPE] != draw_call_b->texture_maps[TM_SPECULAR_TYPE] ) {
		return draw_call_a->texture_maps[TM_SPECULAR_TYPE] < draw_call_b->texture_maps[TM_SPECULAR_TYPE];
	}

	if ( draw_call_a->texture_maps[TM_GLOW_TYPE] != draw_call_b->texture_maps[TM_GLOW_TYPE] ) {
		return draw_call_a->texture_maps[TM_GLOW_TYPE] < draw_call_b->texture_maps[TM_GLOW_TYPE];
	}

	if ( draw_call_a->texture_maps[TM_NORMAL_TYPE] != draw_call_b->texture_maps[TM_NORMAL_TYPE] ) {
		return draw_call_a->texture_maps[TM_NORMAL_TYPE] < draw_call_b->texture_maps[TM_NORMAL_TYPE];
	}

	if ( draw_call_a->texture_maps[TM_HEIGHT_TYPE] != draw_call_b->texture_maps[TM_HEIGHT_TYPE] ) {
		return draw_call_a->texture_maps[TM_HEIGHT_TYPE] < draw_call_b->texture_maps[TM_HEIGHT_TYPE];
	}

	if ( draw_call_a->texture_maps[TM_MISC_TYPE] != draw_call_b->texture_maps[TM_MISC_TYPE] ) {
		return draw_call_a->texture_maps[TM_MISC_TYPE] < draw_call_b->texture_maps[TM_MISC_TYPE];
	}

	return render_state_a->lights.index_start < render_state_b->lights.index_start;
}

void model_queue_render_lightning( DrawList *scene, model_render_params* interp, polymodel *pm, bsp_info * sm )
{
	int i;
	float width = 0.9f;
	color primary, secondary;

	const int AR = 64;
	const int AG = 64;
	const int AB = 5;
	const int AR2 = 128;
	const int AG2 = 128;
	const int AB2 = 10;

	Assert( sm->num_arcs > 0 );

	if ( interp->get_model_flags() & MR_SHOW_OUTLINE_PRESET ) {
		return;
	}

	extern int Interp_lightning;
	if ( !Interp_lightning ) {
 		return;
 	}

	// try and scale the size a bit so that it looks equally well on smaller vessels
	if ( pm->rad < 500.0f ) {
		width *= (pm->rad * 0.01f);

		if ( width < 0.2f ) {
			width = 0.2f;
		}
	}

	for ( i = 0; i < sm->num_arcs; i++ ) {
		// pick a color based upon arc type
		switch ( sm->arc_type[i] ) {
			// "normal", FreeSpace 1 style arcs
		case MARC_TYPE_NORMAL:
			if ( (rand()>>4) & 1 )	{
				gr_init_color(&primary, 64, 64, 255);
			} else {
				gr_init_color(&primary, 128, 128, 255);
			}

			gr_init_color(&secondary, 200, 200, 255);
			break;

			// "EMP" style arcs
		case MARC_TYPE_EMP:
			if ( (rand()>>4) & 1 )	{
				gr_init_color(&primary, AR, AG, AB);
			} else {
				gr_init_color(&primary, AR2, AG2, AB2);
			}

			gr_init_color(&secondary, 255, 255, 10);
			break;

		default:
			Int3();
		}

		// render the actual arc segment
		scene->addArc(&sm->arc_pts[i][0], &sm->arc_pts[i][1], &primary, &secondary, width);
	}
}

int model_queue_render_determine_detail(int obj_num, int model_num, matrix* orient, vec3d* pos, int flags, int detail_level_locked)
{
	int tmp_detail_level = Game_detail_level;

	polymodel *pm = model_get(model_num);

	Assert( pm->n_detail_levels < MAX_MODEL_DETAIL_LEVELS );

	vec3d closest_pos;
	float depth = model_find_closest_point( &closest_pos, model_num, -1, orient, pos, &Eye_position );

	int i;

	if ( pm->n_detail_levels > 1 ) {
		if ( detail_level_locked >= 0 ) {
			i = detail_level_locked+1;
		} else {

#if MAX_DETAIL_LEVEL != 4
#error Code in modelrender.cpp assumes MAX_DETAIL_LEVEL == 4
#endif

			switch ( Detail.detail_distance ) {
			case 0:		// lowest
				depth /= The_mission.ai_profile->detail_distance_mult[0];
				break;
			case 1:		// lower than normal
				depth /= The_mission.ai_profile->detail_distance_mult[1];
				break;
			case 2:		// default
				depth /= The_mission.ai_profile->detail_distance_mult[2];
				break;
			case 3:		// above normal
				depth /= The_mission.ai_profile->detail_distance_mult[3];
				break;
			case 4:		// even more normal
				depth /= The_mission.ai_profile->detail_distance_mult[4];
				break;
			}

			// nebula ?
			if ( The_mission.flags & MISSION_FLAG_FULLNEB ) {
				depth *= neb2_get_lod_scale(obj_num);
			}

			for ( i = 0; i < pm->n_detail_levels; i++ )	{
				if ( depth <= pm->detail_depth[i] ) {
					break;
				}
			}

			// If no valid detail depths specified, use highest.
			if ( (i > 1) && (pm->detail_depth[i-1] < 1.0f) )	{
				i = 1;
			}
		}

		int detail_level = i - 1 - tmp_detail_level;

		if ( detail_level < 0 ) {
			return 0;
		} else if ( detail_level >= pm->n_detail_levels ) {
			return pm->n_detail_levels - 1;
		}

		return detail_level;
	} else {
		return 0;
	}
}

void model_queue_render_buffers(DrawList* scene, model_render_params* interp, polymodel *pm, int mn, int detail_level, uint tmap_flags)
{
	if ( pm->vertex_buffer_id < 0 ) {
		return;
	}

	vertex_buffer *buffer;
	bsp_info *model = NULL;
	const uint model_flags = interp->get_model_flags();
	const int obj_num = interp->get_object_number();

	Assert(detail_level >= 0);

	if ( (mn < 0) || (mn >= pm->n_models) ) {
		buffer = &pm->detail_buffers[detail_level];
	} else {
		model = &pm->submodel[mn];
		buffer = &model->buffer;
	}

	bool render_as_thruster = (model != NULL) && model->is_thruster && (model_flags & MR_SHOW_THRUSTERS);

	vec3d scale;

	if ( render_as_thruster ) {
		scale.xyz.x = 1.0f;
		scale.xyz.y = 1.0f;

		if ( Use_GLSL > 1 ) {
			scale.xyz.z = 1.0f;
			scene->setThrustScale(interp->get_thruster_info().length.xyz.z);
		} else {
			scale.xyz.z = interp->get_thruster_info().length.xyz.z;
			scene->setThrustScale();
		}
	} else {
		scale = interp->get_warp_scale();
		scene->setThrustScale();
	}

	scene->setScale(&scale);

	if ( tmap_flags & TMAP_FLAG_BATCH_TRANSFORMS && (mn >= 0) && (mn < pm->n_models) ) {
		scene->setModelTransformBuffer(mn);
		return;
	}

	fix base_frametime = model_render_determine_base_frametime(obj_num, model_flags);

	texture_info tex_replace[TM_NUM_TYPES];

	int no_texturing = model_flags & MR_NO_TEXTURING;

	int forced_texture = -2;
	float forced_alpha = 1.0f;
	int forced_blend_filter = GR_ALPHABLEND_NONE;

	if ( ( model_flags & MR_FORCE_TEXTURE ) && ( interp->get_forced_bitmap() >= 0 ) ) {
		forced_texture = interp->get_forced_bitmap();
	} else if ( interp->get_warp_bitmap() >= 0 ) {
		forced_texture = interp->get_warp_bitmap();
		forced_alpha = interp->get_warp_alpha();
		forced_blend_filter = GR_ALPHABLEND_FILTER;
	} else if ( render_as_thruster ) {
		if ( ( interp->get_thruster_info().primary_bitmap >= 0 ) && ( interp->get_thruster_info().length.xyz.z > 0.0f ) ) {
			forced_texture = interp->get_thruster_info().primary_bitmap;
		} else {
			forced_texture = -1;
		}

		forced_alpha = 1.2f;
		forced_blend_filter = GR_ALPHABLEND_FILTER;
	} else if ( model_flags & MR_ALL_XPARENT ) {
		forced_alpha = interp->get_alpha();
		forced_blend_filter = GR_ALPHABLEND_FILTER;
	}

	int texture_maps[TM_NUM_TYPES] = {-1, -1, -1, -1, -1, -1};
	size_t buffer_size = buffer->tex_buf.size();
	const int *replacement_textures = interp->get_replacement_textures();

	for ( size_t i = 0; i < buffer_size; i++ ) {
		int tmap_num = buffer->tex_buf[i].texture;
		texture_map *tmap = &pm->maps[tmap_num];
		int rt_begin_index = tmap_num*TM_NUM_TYPES;
		float alpha = 1.0f;
		int blend_filter = GR_ALPHABLEND_NONE;

		texture_maps[TM_BASE_TYPE] = -1;
		texture_maps[TM_GLOW_TYPE] = -1;
		texture_maps[TM_SPECULAR_TYPE] = -1;
		texture_maps[TM_NORMAL_TYPE] = -1;
		texture_maps[TM_HEIGHT_TYPE] = -1;
		texture_maps[TM_MISC_TYPE] = -1;

		if (forced_texture != -2) {
			texture_maps[TM_BASE_TYPE] = forced_texture;
			alpha = forced_alpha;
		} else if ( !no_texturing ) {
			// pick the texture, animating it if necessary
			if ( (replacement_textures != NULL) && (replacement_textures[rt_begin_index + TM_BASE_TYPE] == REPLACE_WITH_INVISIBLE) ) {
				// invisible textures aren't rendered, but we still have to skip assigning the underlying model texture
				texture_maps[TM_BASE_TYPE] = -1;
			} else if ( (replacement_textures != NULL) && (replacement_textures[rt_begin_index + TM_BASE_TYPE] >= 0) ) {
				// an underlying texture is replaced with a real new texture
				tex_replace[TM_BASE_TYPE] = texture_info(replacement_textures[rt_begin_index + TM_BASE_TYPE]);
				texture_maps[TM_BASE_TYPE] = model_interp_get_texture(&tex_replace[TM_BASE_TYPE], base_frametime);
			} else {
				// we just use the underlying texture
				texture_maps[TM_BASE_TYPE] = model_interp_get_texture(&tmap->textures[TM_BASE_TYPE], base_frametime);
			}

			if ( texture_maps[TM_BASE_TYPE] < 0 ) {
				continue;
			}

			// doing glow maps?
			if ( !(model_flags & MR_NO_GLOWMAPS) ) {
				texture_info *tglow = &tmap->textures[TM_GLOW_TYPE];

				if ( (replacement_textures != NULL) && (replacement_textures[rt_begin_index + TM_GLOW_TYPE] >= 0) ) {
					tex_replace[TM_GLOW_TYPE] = texture_info(replacement_textures[rt_begin_index + TM_GLOW_TYPE]);
					texture_maps[TM_GLOW_TYPE] = model_interp_get_texture(&tex_replace[TM_GLOW_TYPE], base_frametime);
				} else if (tglow->GetTexture() >= 0) {
					// shockwaves are special, their current frame has to come out of the shockwave code to get the timing correct
					if ( (obj_num >= 0) && (Objects[obj_num].type == OBJ_SHOCKWAVE) && (tglow->GetNumFrames() > 1) ) {
						texture_maps[TM_GLOW_TYPE] = tglow->GetTexture() + shockwave_get_framenum(Objects[obj_num].instance, tglow->GetNumFrames());
					} else {
						texture_maps[TM_GLOW_TYPE] = model_interp_get_texture(tglow, base_frametime);
					}
				}
			}

			if ( (Detail.lighting > 2)  && (detail_level < 2) ) {
				// likewise, etc.
				texture_info *spec_map = &tmap->textures[TM_SPECULAR_TYPE];
				texture_info *norm_map = &tmap->textures[TM_NORMAL_TYPE];
				texture_info *height_map = &tmap->textures[TM_HEIGHT_TYPE];
				texture_info *misc_map = &tmap->textures[TM_MISC_TYPE];

				if (replacement_textures != NULL) {
					if (replacement_textures[rt_begin_index + TM_SPECULAR_TYPE] >= 0) {
						tex_replace[TM_SPECULAR_TYPE] = texture_info(replacement_textures[rt_begin_index + TM_SPECULAR_TYPE]);
						spec_map = &tex_replace[TM_SPECULAR_TYPE];
					}

					if (replacement_textures[rt_begin_index + TM_NORMAL_TYPE] >= 0) {
						tex_replace[TM_NORMAL_TYPE] = texture_info(replacement_textures[rt_begin_index + TM_NORMAL_TYPE]);
						norm_map = &tex_replace[TM_NORMAL_TYPE];
					}

					if (replacement_textures[rt_begin_index + TM_HEIGHT_TYPE] >= 0) {
						tex_replace[TM_HEIGHT_TYPE] = texture_info(replacement_textures[rt_begin_index + TM_HEIGHT_TYPE]);
						height_map = &tex_replace[TM_HEIGHT_TYPE];
					}

					if (replacement_textures[rt_begin_index + TM_MISC_TYPE] >= 0) {
						tex_replace[TM_MISC_TYPE] = texture_info(replacement_textures[rt_begin_index + TM_MISC_TYPE]);
						misc_map = &tex_replace[TM_MISC_TYPE];
					}
				}

				texture_maps[TM_SPECULAR_TYPE] = model_interp_get_texture(spec_map, base_frametime);
				texture_maps[TM_NORMAL_TYPE] = model_interp_get_texture(norm_map, base_frametime);
				texture_maps[TM_HEIGHT_TYPE] = model_interp_get_texture(height_map, base_frametime);
				texture_maps[TM_MISC_TYPE] = model_interp_get_texture(misc_map, base_frametime);
			}
		} else {
			alpha = forced_alpha;

			//Check for invisible or transparent textures so they don't show up in the shadow maps - Valathil
			if ( in_shadow_map ) {
				if ( (replacement_textures != NULL) && (replacement_textures[rt_begin_index + TM_BASE_TYPE] >= 0) ) {
					tex_replace[TM_BASE_TYPE] = texture_info(replacement_textures[rt_begin_index + TM_BASE_TYPE]);
					texture_maps[TM_BASE_TYPE] = model_interp_get_texture(&tex_replace[TM_BASE_TYPE], base_frametime);
				} else {
					texture_maps[TM_BASE_TYPE] = model_interp_get_texture(&tmap->textures[TM_BASE_TYPE], base_frametime);
				}

				if ( texture_maps[TM_BASE_TYPE] <= 0 ) {
					continue;
				}
			}
		}

		if ( (texture_maps[TM_BASE_TYPE] == -1) && !no_texturing ) {
			continue;
		}

		// trying to get transparent textures-Bobboau
		if (tmap->is_transparent) {
			// for special shockwave/warp map usage
			alpha = (interp->get_warp_alpha() != -1.0f) ? interp->get_warp_alpha() : 0.8f;
			blend_filter = GR_ALPHABLEND_FILTER;

		}

		if (forced_blend_filter != GR_ALPHABLEND_NONE) {
			blend_filter = forced_blend_filter;
		}

		if (blend_filter != GR_ALPHABLEND_NONE) {
			scene->setDepthMode(GR_ZBUFF_READ);
		} else {
			if ( (model_flags & MR_NO_ZBUFFER) || (model_flags & MR_ALL_XPARENT) ) {
				scene->setDepthMode(GR_ZBUFF_NONE);
			} else {
				scene->setDepthMode(GR_ZBUFF_FULL);
			}
		}

		scene->setBlendFilter(blend_filter, alpha);

		scene->setTexture(TM_BASE_TYPE,	texture_maps[TM_BASE_TYPE]);
		scene->setTexture(TM_GLOW_TYPE,	texture_maps[TM_GLOW_TYPE]);
		scene->setTexture(TM_SPECULAR_TYPE, texture_maps[TM_SPECULAR_TYPE]);
		scene->setTexture(TM_NORMAL_TYPE, texture_maps[TM_NORMAL_TYPE]);
		scene->setTexture(TM_HEIGHT_TYPE, texture_maps[TM_HEIGHT_TYPE]);
		scene->setTexture(TM_MISC_TYPE,	texture_maps[TM_MISC_TYPE]);

		scene->addBufferDraw(buffer, i, tmap_flags, interp);
	}
}

void model_queue_render_children_buffers(DrawList* scene, model_render_params* interp, polymodel* pm, int mn, int detail_level, uint tmap_flags)
{
	int i;

	if ( (mn < 0) || (mn >= pm->n_models) ) {
		Int3();
		return;
	}

	bsp_info *model = &pm->submodel[mn];

	if (model->blown_off)
		return;

	const uint model_flags = interp->get_model_flags();

	if (model->is_thruster) {
		if ( !( model_flags & MR_SHOW_THRUSTERS ) ) {
			return;
		}

		scene->setLighting(false);
	}

	vec3d view_pos = scene->getViewPosition();

	if ( !model_render_check_detail_box(&view_pos, pm, mn, model_flags) ) {
		return;
	}

	// Get submodel rotation data and use submodel orientation matrix
	// to put together a matrix describing the final orientation of
	// the submodel relative to its parent
	angles ang = model->angs;

	// Add barrel rotation if needed
	if ( model->gun_rotation ) {
		if ( pm->gun_submodel_rotation > PI2 ) {
			pm->gun_submodel_rotation -= PI2;
		} else if ( pm->gun_submodel_rotation < 0.0f ) {
			pm->gun_submodel_rotation += PI2;
		}

		ang.b += pm->gun_submodel_rotation;
	}

	// Compute final submodel orientation by using the orientation matrix
	// and the rotation angles.
	// By using this kind of computation, the rotational angles can always
	// be computed relative to the submodel itself, instead of relative
	// to the parent
	matrix rotation_matrix = model->orientation;
	vm_rotate_matrix_by_angles(&rotation_matrix, &ang);

	matrix inv_orientation;
	vm_copy_transpose_matrix(&inv_orientation, &model->orientation);

	matrix submodel_matrix;
	vm_matrix_x_matrix(&submodel_matrix, &rotation_matrix, &inv_orientation);

	scene->pushTransform(&model->offset, &submodel_matrix);

	model_queue_render_buffers(scene, interp, pm, mn, detail_level, tmap_flags);

	if ( model->num_arcs ) {
		model_queue_render_lightning( scene, interp, pm, &pm->submodel[mn] );
	}

	i = model->first_child;

	while ( i >= 0 ) {
		if ( !pm->submodel[i].is_thruster ) {
			model_queue_render_children_buffers( scene, interp, pm, i, detail_level, tmap_flags );
		}

		i = pm->submodel[i].next_sibling;
	}

	if ( model->is_thruster ) {
		scene->setLighting(true);
	}

	scene->popTransform();
}

float model_queue_render_determine_light(model_render_params* interp, vec3d *pos, uint flags)
{
	if ( flags & MR_NO_LIGHTING ) {
		return 1.0f;
	} else if ( flags & MR_IS_ASTEROID ) {
		// Dim it based on distance
		float depth = vm_vec_dist_quick( pos, &Eye_position );
		if ( depth > interp->get_depth_scale() )	{
			float temp_light = interp->get_depth_scale()/depth;

			// If it is too far, exit
			if ( temp_light < (1.0f/32.0f) ) {
				return 0.0f;
			} else if ( temp_light > 1.0f )	{
				return 1.0f;
			}

			return temp_light;
		} else {
			return 1.0f;
		}
	} else {
		return 1.0f;
	}
}

float model_render_determine_box_scale()
{
	float box_scale = 1.2f;

	// scale the render box settings based on the "Model Detail" slider
	switch ( Detail.detail_distance ) {
	case 0:		// 1st dot is 20%
		box_scale = 0.2f;
		break;
	case 1:		// 2nd dot is 50%
		box_scale = 0.5f;
		break;
	case 2:		// 3rd dot is 80%
		box_scale = 0.8f;
		break;
	case 3:		// 4th dot is 100% (this is the default setting for "High" and "Very High" settings)
		box_scale = 1.0f;
		break;
	case 4:		// 5th dot (max) is 120%
	default:
		box_scale = 1.2f;
		break;
	}

	return box_scale;
}

fix model_render_determine_base_frametime(int objnum, uint flags)
{
	// Goober5000
	fix base_frametime = 0;

	if ( objnum >= 0 ) {
		object *objp = &Objects[objnum];

		if ( objp->type == OBJ_SHIP ) {
			base_frametime = Ships[objp->instance].base_texture_anim_frametime;
		}
	} else if ( flags & MR_SKYBOX ) {
		base_frametime = Skybox_timestamp;
	}

	return base_frametime;
}

bool model_render_determine_autocenter(vec3d *auto_back, polymodel *pm, int detail_level, uint flags)
{
	if ( flags & MR_AUTOCENTER ) {
		// standard autocenter using data in model
		if ( pm->flags & PM_FLAG_AUTOCEN ) {
			*auto_back = pm->autocenter;
			vm_vec_scale(auto_back, -1.0f);
			return true;
		} else if ( flags & MR_IS_MISSILE ) {
			// fake autocenter if we are a missile and don't already have autocen info
			auto_back->xyz.x = -( (pm->submodel[pm->detail[detail_level]].max.xyz.x + pm->submodel[pm->detail[detail_level]].min.xyz.x) / 2.0f );
			auto_back->xyz.y = -( (pm->submodel[pm->detail[detail_level]].max.xyz.y + pm->submodel[pm->detail[detail_level]].min.xyz.y) / 2.0f );
			auto_back->xyz.z = -( (pm->submodel[pm->detail[detail_level]].max.xyz.z + pm->submodel[pm->detail[detail_level]].min.xyz.z) / 2.0f );
			return true;
		}
	}

	return false;
}

bool model_render_check_detail_box(vec3d *view_pos, polymodel *pm, int submodel_num, uint flags)
{
	Assert(pm != NULL);

	bsp_info *model = &pm->submodel[submodel_num];

	float box_scale = model_render_determine_box_scale();

	if ( !( flags & MR_FULL_DETAIL ) && model->use_render_box ) {
		vec3d box_min, box_max;

		vm_vec_copy_scale(&box_min, &model->render_box_min, box_scale);
		vm_vec_copy_scale(&box_max, &model->render_box_max, box_scale);

		if ( (-model->use_render_box + in_box(&box_min, &box_max, &model->offset, view_pos)) ) {
			return true;
		}

		return false;
	}

	if ( !(flags & MR_FULL_DETAIL) && model->use_render_sphere ) {
		float sphere_radius = model->render_sphere_radius * box_scale;

		// TODO: doesn't consider submodel rotations yet -zookeeper
		vec3d offset;
		model_find_submodel_offset(&offset, pm->id, submodel_num);
		vm_vec_add2(&offset, &model->render_sphere_offset);

		if ( (-model->use_render_sphere + in_sphere(&offset, sphere_radius, view_pos)) ) {
			return true;
		}

		return false;
	}

	return true;
}

void submodel_immediate_render(int model_num, int submodel_num, matrix *orient, vec3d * pos, uint flags, int objnum, int *replacement_textures)
{

}

void submodel_immediate_render(model_render_params *render_info, int model_num, int submodel_num, matrix *orient, vec3d * pos)
{
	DrawList model_list;
	
	model_list.init();

	submodel_queue_render(render_info, &model_list, model_num, submodel_num, orient, pos);
	
	model_list.renderAll();

	gr_zbias(0);
	gr_zbuffer_set(ZBUFFER_TYPE_READ);
	gr_set_cull(0);
	gr_set_fill_mode(GR_FILL_MODE_SOLID);

	gr_flush_data_states();
	gr_set_buffer(-1);

	gr_reset_lighting();
	gr_set_lighting(false, false);
}

void submodel_queue_render(model_render_params *render_info, DrawList *scene, int model_num, int submodel_num, matrix *orient, vec3d * pos, uint flags, int objnum)
{

}

void submodel_queue_render(model_render_params *render_info, DrawList *scene, int model_num, int submodel_num, matrix *orient, vec3d * pos)
{
	polymodel * pm;

	//MONITOR_INC( NumModelsRend, 1 );	

	if ( !( Game_detail_flags & DETAIL_FLAG_MODELS ) )	return;
	
	if ( render_info->is_clip_plane_set() ) {
		scene->setClipPlane(render_info->get_clip_plane_pos(), render_info->get_clip_plane_normal());
	} else {
		scene->setClipPlane();
	}

	if ( render_info->is_team_color_set() ) {
		scene->setTeamColor(render_info->get_team_color());
	} else {
		scene->setTeamColor();
	}
		
	uint flags = render_info->get_model_flags();
	int objnum = render_info->get_object_number();

	pm = model_get(model_num);

	// Set the flags we will pass to the tmapper
	uint tmap_flags = TMAP_FLAG_GOURAUD | TMAP_FLAG_RGB;

	// if we're in nebula mode
	if( ( The_mission.flags & MISSION_FLAG_FULLNEB ) && ( Neb2_render_mode != NEB2_RENDER_NONE ) ) {
		tmap_flags |= TMAP_FLAG_PIXEL_FOG;
	}

	if ( !( flags & MR_NO_TEXTURING ) )	{
		tmap_flags |= TMAP_FLAG_TEXTURED;

		if ( ( pm->flags & PM_FLAG_ALLOW_TILING ) && tiling )
			tmap_flags |= TMAP_FLAG_TILED;

		if ( !( flags & MR_NO_CORRECT ) )	{
			tmap_flags |= TMAP_FLAG_CORRECT;
		}
	}

	if ( render_info->get_animated_effect_num() >= 0 ) {
		tmap_flags |= TMAP_ANIMATED_SHADER;
		scene->setAnimatedEffect(render_info->get_animated_effect_num());
		scene->setAnimatedTimer(render_info->get_animated_effect_timer());
	}

	bool is_outlines_only_htl = !Cmdline_nohtl && (flags & MR_NO_POLYS) && (flags & MR_SHOW_OUTLINE_HTL);

	//set to true since D3d and OGL need the api matrices set
	scene->pushTransform(pos, orient);

	
	vec3d auto_back = ZERO_VECTOR;
	bool set_autocen = model_render_determine_autocenter(&auto_back, pm, render_info->get_detail_level_lock(), flags);

	if ( set_autocen ) {
		scene->pushTransform(&auto_back, NULL);
	}

	if (is_outlines_only_htl) {
		scene->setFillMode(GR_FILL_MODE_WIRE);

		color outline_color = render_info->get_outline_color();
		gr_set_color_fast( &outline_color );

		tmap_flags &= ~TMAP_FLAG_RGB;
	} else {
		scene->setFillMode(GR_FILL_MODE_SOLID);
	}

	scene->setLightFactor(1.0f);

	if ( !( flags & MR_NO_LIGHTING ) ) {
		scene->setLightFilter(-1, pos, pm->submodel[submodel_num].rad);
		scene->setLighting(true);
	} else {
		scene->setLighting(false);
	}

	// fixes disappearing HUD in OGL - taylor
	scene->setCullMode(1);

	// RT - Put this here to fog debris
	if( tmap_flags & TMAP_FLAG_PIXEL_FOG ) {
		float fog_near, fog_far;
		object *obj = NULL;

		if (objnum >= 0)
			obj = &Objects[objnum];

		neb2_get_adjusted_fog_values(&fog_near, &fog_far, obj);
		unsigned char r, g, b;
		neb2_get_fog_color(&r, &g, &b);

		scene->setFog(GR_FOGMODE_FOG, r, g, b, fog_near, fog_far);
	} else {
		scene->setFog(GR_FOGMODE_NONE, 0, 0, 0);
	}

	if(in_shadow_map) {
		scene->setZBias(-1024);
	} else {
		scene->setZBias(0);
	}

	scene->setBuffer(pm->vertex_buffer_id);

	vec3d view_pos = scene->getViewPosition();

	if ( model_render_check_detail_box(&view_pos, pm, submodel_num, flags) ) {
		model_queue_render_buffers(scene, render_info, pm, submodel_num, 0, tmap_flags);
	}
	
	if ( pm->submodel[submodel_num].num_arcs )	{
		model_queue_render_lightning( scene, render_info, pm, &pm->submodel[submodel_num] );
	}

	if ( set_autocen ) {
		scene->popTransform();
	}

	scene->popTransform();
}

void model_queue_render_glowpoint(int point_num, vec3d *pos, matrix *orient, glow_point_bank *bank, glow_point_bank_override *gpo, polymodel *pm, ship* shipp, bool use_depth_buffer)
{
	glow_point *gpt = &bank->points[point_num];
	vec3d loc_offset = gpt->pnt;
	vec3d loc_norm = gpt->norm;
	vec3d world_pnt;
	vec3d world_norm;
	vec3d tempv;
	vec3d submodel_static_offset; // The associated submodel's static offset in the ship's frame of reference
	bool submodel_rotation = false;

	if ( bank->submodel_parent > 0 && pm->submodel[bank->submodel_parent].can_move && (gameseq_get_state_idx(GS_STATE_LAB) == -1) && shipp != NULL ) {
		model_find_submodel_offset(&submodel_static_offset, Ship_info[shipp->ship_info_index].model_num, bank->submodel_parent);

		submodel_rotation = true;
	}

	if ( submodel_rotation ) {
		vm_vec_sub(&loc_offset, &gpt->pnt, &submodel_static_offset);

		tempv = loc_offset;
		find_submodel_instance_point_normal(&loc_offset, &loc_norm, &Objects[shipp->objnum], bank->submodel_parent, &tempv, &loc_norm);
	}

	vm_vec_unrotate(&world_pnt, &loc_offset, orient);
	vm_vec_add2(&world_pnt, pos);

	vm_vec_unrotate(&world_norm, &loc_norm, orient);

	if ( shipp != NULL ) {
		if ( (shipp->flags & (SF_ARRIVING) ) && (shipp->warpin_effect) && Ship_info[shipp->ship_info_index].warpin_type != WT_HYPERSPACE) {
			vec3d warp_pnt, tmp;
			matrix warp_orient;

			shipp->warpin_effect->getWarpPosition(&warp_pnt);
			shipp->warpin_effect->getWarpOrientation(&warp_orient);
			vm_vec_sub( &tmp, &world_pnt, &warp_pnt );

			if ( vm_vec_dot( &tmp, &warp_orient.vec.fvec ) < 0.0f ) {
				return;
			}
		}

		if ( (shipp->flags & (SF_DEPART_WARP) ) && (shipp->warpout_effect) && Ship_info[shipp->ship_info_index].warpout_type != WT_HYPERSPACE) {
			vec3d warp_pnt, tmp;
			matrix warp_orient;

			shipp->warpout_effect->getWarpPosition(&warp_pnt);
			shipp->warpout_effect->getWarpOrientation(&warp_orient);
			vm_vec_sub( &tmp, &world_pnt, &warp_pnt );

			if ( vm_vec_dot( &tmp, &warp_orient.vec.fvec ) > 0.0f ) {
				return;
			}
		}
	}

	switch ((gpo && gpo->type_override)?gpo->type:bank->type)
	{
	case 0:
		{
			float d,pulse = 1.0f;

			if ( IS_VEC_NULL(&world_norm) ) {
				d = 1.0f;	//if given a nul vector then always show it
			} else {
				vm_vec_sub(&tempv,&View_position,&world_pnt);
				vm_vec_normalize(&tempv);

				d = vm_vec_dot(&tempv,&world_norm);
				d -= 0.25;	
			}

			float w = gpt->radius;
			if (d > 0.0f) {
				vertex p;

				d *= 3.0f;

				if (d > 1.0f)
					d = 1.0f;


				// fade them in the nebula as well
				if ( The_mission.flags & MISSION_FLAG_FULLNEB ) {
					//vec3d npnt;
					//vm_vec_add(&npnt, &loc_offset, pos);

					d *= (1.0f - neb2_get_fog_intensity(&world_pnt));
					w *= 1.5;	//make it bigger in a nebula
				}
				
				if (!Cmdline_nohtl) {
					g3_transfer_vertex(&p, &world_pnt);
				} else {
					g3_rotate_vertex(&p, &world_pnt);
				}

				p.r = p.g = p.b = p.a = (ubyte)(255.0f * MAX(d,0.0f));

				if((gpo && gpo->glow_bitmap_override)?(gpo->glow_bitmap > -1):(bank->glow_bitmap > -1)) {
					int gpflags = TMAP_FLAG_GOURAUD | TMAP_FLAG_RGB | TMAP_FLAG_TEXTURED | TMAP_HTL_3D_UNLIT;

					if (use_depth_buffer)
						gpflags |= TMAP_FLAG_SOFT_QUAD;

					batch_add_bitmap(
						(gpo && gpo->glow_bitmap_override)?gpo->glow_bitmap:bank->glow_bitmap,
						gpflags,  
						&p,
						0,
						(w * 0.5f),
						d * pulse,
						w
						);
				}
			} //d>0.0f
			if ( gpo && gpo->pulse_type ) {
				int period;

				if(gpo->pulse_period_override) {
					period = gpo->pulse_period;
				} else {
					if(gpo->on_time_override) {
						period = 2 * gpo->on_time;
					} else {
						period = 2 * bank->on_time;
					}
				}

				int x = 0;

				if ( (gpo && gpo->off_time_override) ? gpo->off_time : bank->off_time ) {
					x = (timestamp() - ((gpo && gpo->disp_time_override)?gpo->disp_time:bank->disp_time)) % ( ((gpo && gpo->on_time_override)?gpo->on_time:bank->on_time) + ((gpo && gpo->off_time_override)?gpo->off_time:bank->off_time) ) - ((gpo && gpo->off_time_override)?gpo->off_time:bank->off_time);
				} else {
					x = (timestamp() - ((gpo && gpo->disp_time_override)?gpo->disp_time:bank->disp_time)) % gpo->pulse_period;
				}

				switch ( gpo->pulse_type ) {

				case PULSE_SIN:
					pulse = gpo->pulse_bias + gpo->pulse_amplitude * pow(sin( PI2 / period * x),gpo->pulse_exponent);
					break;

				case PULSE_COS:
					pulse = gpo->pulse_bias + gpo->pulse_amplitude * pow(cos( PI2 / period * x),gpo->pulse_exponent);
					break;

				case PULSE_SHIFTTRI:
					x += period / 4;
					if((gpo && gpo->off_time_override)?gpo->off_time:bank->off_time) {
						x %= ( ((gpo && gpo->on_time_override)?gpo->on_time:bank->on_time) + ((gpo && gpo->off_time_override)?gpo->off_time:bank->off_time) );
					} else {
						x %= gpo->pulse_period;
					}

				case PULSE_TRI:
					float inv;
					if( x > period / 2) {
						inv = -1;
					} else {
						inv = 1;
					}
					if( x > period / 4) {
						pulse = gpo->pulse_bias + gpo->pulse_amplitude * inv * pow( 1.0f - ((x - period / 4.0f) * 4 / period) ,gpo->pulse_exponent);
					} else {
						pulse = gpo->pulse_bias + gpo->pulse_amplitude * inv * pow( (x * 4.0f / period) ,gpo->pulse_exponent);
					}
					break;
				}
			}

			if ( Deferred_lighting && gpo && gpo->is_lightsource ) {
				if ( gpo->lightcone ) {
					vec3d cone_dir_rot;
					vec3d cone_dir_model;
					vec3d cone_dir_world;
					vec3d cone_dir_screen;
					vec3d unused;

					if ( gpo->rotating ) {
						vm_rot_point_around_line(&cone_dir_rot, &gpo->cone_direction, PI * timestamp() * 0.000033333f * gpo->rotation_speed, &vmd_zero_vector, &gpo->rotation_axis);
					} else {
						cone_dir_rot = gpo->cone_direction; 
					}

					find_submodel_instance_point_normal(&unused, &cone_dir_model, &Objects[shipp->objnum], bank->submodel_parent, &unused, &cone_dir_rot);
					vm_vec_unrotate(&cone_dir_world, &cone_dir_model, orient);
					vm_vec_rotate(&cone_dir_screen, &cone_dir_world, &Eye_matrix);
					cone_dir_screen.xyz.z = -cone_dir_screen.xyz.z;
					light_add_cone(&world_pnt, &cone_dir_screen, gpo->cone_angle, gpo->cone_inner_angle, gpo->dualcone, 1.0f, w * gpo->radius_multi, 1, pulse * gpo->light_color.xyz.x + (1.0f-pulse) * gpo->light_mix_color.xyz.x, pulse * gpo->light_color.xyz.y + (1.0f-pulse) * gpo->light_mix_color.xyz.y, pulse * gpo->light_color.xyz.z + (1.0f-pulse) * gpo->light_mix_color.xyz.z, -1);
				} else {
					light_add_point(&world_pnt, 1.0f, w * gpo->radius_multi, 1, pulse * gpo->light_color.xyz.x + (1.0f-pulse) * gpo->light_mix_color.xyz.x, pulse * gpo->light_color.xyz.y + (1.0f-pulse) * gpo->light_mix_color.xyz.y, pulse * gpo->light_color.xyz.z + (1.0f-pulse) * gpo->light_mix_color.xyz.z, -1);
				}
			}
			break;
		}

	case 1:
		{
			vertex verts[4];
			vec3d fvec, top1, bottom1, top2, bottom2, start, end;

			vm_vec_add2(&loc_norm, &loc_offset);

			vm_vec_rotate(&start, &loc_offset, orient);
			vm_vec_rotate(&end, &loc_norm, orient);
			vm_vec_sub(&fvec, &end, &start);

			vm_vec_normalize(&fvec);

			moldel_calc_facing_pts(&top1, &bottom1, &fvec, &loc_offset, gpt->radius, 1.0f, &View_position);
			moldel_calc_facing_pts(&top2, &bottom2, &fvec, &loc_norm, gpt->radius, 1.0f, &View_position);

			int idx = 0;

			if ( Cmdline_nohtl ) {
				g3_rotate_vertex(&verts[0], &bottom1);
				g3_rotate_vertex(&verts[1], &bottom2);
				g3_rotate_vertex(&verts[2], &top2);
				g3_rotate_vertex(&verts[3], &top1);

				for ( idx = 0; idx < 4; idx++ ) {
					g3_project_vertex(&verts[idx]);
				}
			} else {
				g3_transfer_vertex(&verts[0], &bottom1);
				g3_transfer_vertex(&verts[1], &bottom2);
				g3_transfer_vertex(&verts[2], &top2);
				g3_transfer_vertex(&verts[3], &top1);
			}

			verts[0].texture_position.u = 0.0f;
			verts[0].texture_position.v = 0.0f;

			verts[1].texture_position.u = 1.0f;
			verts[1].texture_position.v = 0.0f;

			verts[2].texture_position.u = 1.0f;
			verts[2].texture_position.v = 1.0f;

			verts[3].texture_position.u = 0.0f;
			verts[3].texture_position.v = 1.0f;

			vm_vec_sub(&tempv,&View_position,&loc_offset);
			vm_vec_normalize(&tempv);

			if ( The_mission.flags & MISSION_FLAG_FULLNEB ) {
				batch_add_quad(bank->glow_neb_bitmap, TMAP_FLAG_TILED | TMAP_FLAG_TEXTURED | TMAP_FLAG_CORRECT | TMAP_HTL_3D_UNLIT, verts);
			} else {
				batch_add_quad(bank->glow_bitmap, TMAP_FLAG_TILED | TMAP_FLAG_TEXTURED | TMAP_FLAG_CORRECT | TMAP_HTL_3D_UNLIT, verts);
			}

			break;
		}
	}
}

void model_queue_render_set_glow_points(polymodel *pm, int objnum)
{
	int time = timestamp();
	glow_point_bank_override *gpo = NULL;
	bool override_all = false;
	SCP_hash_map<int, void*>::iterator gpoi;
	ship_info *sip = NULL;
	ship *shipp = NULL;

	if ( Glowpoint_override ) {
		return;
	}

	if ( objnum > -1 ) {
		shipp = &Ships[Objects[objnum].instance];
		sip = &Ship_info[shipp->ship_info_index];
		SCP_hash_map<int, void*>::iterator gpoi = sip->glowpoint_bank_override_map.find(-1);

		if ( gpoi != sip->glowpoint_bank_override_map.end() ) {
			override_all = true;
			gpo = (glow_point_bank_override*) sip->glowpoint_bank_override_map[-1];
		}
	}

	for ( int i = 0; i < pm->n_glow_point_banks; i++ ) { //glow point blink code -Bobboau
		glow_point_bank *bank = &pm->glow_point_banks[i];

		if ( !override_all && sip ) {
			gpoi = sip->glowpoint_bank_override_map.find(i);

			if ( gpoi != sip->glowpoint_bank_override_map.end() ) {
				gpo = (glow_point_bank_override*) sip->glowpoint_bank_override_map[i];
			} else {
				gpo = NULL;
			}
		}

		if ( bank->glow_timestamp == 0 ) {
			bank->glow_timestamp=time;
		}

		if ( ( gpo && gpo->off_time_override ) ? gpo->off_time : bank->off_time ) {
			if ( bank->is_on ) {
				if( ((gpo && gpo->on_time_override) ? gpo->on_time : bank->on_time) > ((time - ((gpo && gpo->disp_time_override) ? gpo->disp_time : bank->disp_time)) % (((gpo && gpo->on_time_override) ? gpo->on_time : bank->on_time) + ((gpo && gpo->off_time_override) ? gpo->off_time : bank->off_time))) ){
					bank->glow_timestamp = time;
					bank->is_on = 0;
				}
			} else {
				if( ((gpo && gpo->off_time_override)?gpo->off_time:bank->off_time) < ((time - ((gpo && gpo->disp_time_override)?gpo->disp_time:bank->disp_time)) % (((gpo && gpo->on_time_override)?gpo->on_time:bank->on_time) + ((gpo && gpo->off_time_override)?gpo->off_time:bank->off_time))) ){
					bank->glow_timestamp = time;
					bank->is_on = 1;
				}
			}
		}
	}
}

void model_queue_render_glow_points(polymodel *pm, ship *shipp, matrix *orient, vec3d *pos, bool use_depth_buffer = true)
{
	if ( in_shadow_map ) {
		return;
	}

	int i, j;

	int cull = gr_set_cull(0);

	glow_point_bank_override *gpo = NULL;
	bool override_all = false;
	SCP_hash_map<int, void*>::iterator gpoi;
	ship_info *sip = NULL;

	if ( shipp ) {
		sip = &Ship_info[shipp->ship_info_index];
		SCP_hash_map<int, void*>::iterator gpoi = sip->glowpoint_bank_override_map.find(-1);

		if(gpoi != sip->glowpoint_bank_override_map.end()) {
			override_all = true;
			gpo = (glow_point_bank_override*) sip->glowpoint_bank_override_map[-1];
		}
	}

	for (i = 0; i < pm->n_glow_point_banks; i++ ) {
		glow_point_bank *bank = &pm->glow_point_banks[i];

		if(!override_all && sip) {
			gpoi = sip->glowpoint_bank_override_map.find(i);
			if(gpoi != sip->glowpoint_bank_override_map.end()) {
				gpo = (glow_point_bank_override*) sip->glowpoint_bank_override_map[i];
			} else {
				gpo = NULL;
			}
		}

		//Only continue if there actually is a glowpoint bitmap available
		if (bank->glow_bitmap == -1)
			continue;

		if (pm->submodel[bank->submodel_parent].blown_off)
			continue;

		if ((gpo && gpo->off_time_override && !gpo->off_time)?gpo->is_on:bank->is_on) {
			if ( (shipp != NULL) && !(shipp->glow_point_bank_active[i]) )
				continue;

			for (j = 0; j < bank->num_points; j++) {
				Assert( bank->points != NULL );
				int flick;

				if (pm->submodel[pm->detail[0]].num_arcs) {
					flick = static_rand( timestamp() % 20 ) % (pm->submodel[pm->detail[0]].num_arcs + j); //the more damage, the more arcs, the more likely the lights will fail
				} else {
					flick = 1;
				}

				if (flick == 1) {
					model_queue_render_glowpoint(j, pos, orient, bank, gpo, pm, shipp, use_depth_buffer);
				} // flick
			} // for slot
		} // bank is on
	} // for bank

	gr_set_cull(cull);
}

void model_queue_render_thrusters(model_render_params *interp, polymodel *pm, int objnum, ship *shipp, matrix *orient, vec3d *pos)
{
	int i, j;
	int n_q = 0;
	size_t 	k;
	vec3d norm, norm2, fvec, pnt, npnt;
	thruster_bank *bank = NULL;
	vertex p;
	bool do_render = false;

	if ( in_shadow_map ) {
		return;
	}

	if ( pm == NULL ) {
		Int3();
		return;
	}

	if ( !(interp->get_model_flags() & MR_SHOW_THRUSTERS) ) {
		return;
	}

	// get an initial count to figure out how man geo batchers we need allocated
	for (i = 0; i < pm->n_thrusters; i++ ) {
		bank = &pm->thrusters[i];
		n_q += bank->num_points;
	}

	if (n_q <= 0) {
		return;
	}

	const mst_info& thruster_info = interp->get_thruster_info();

	// primary_thruster_batcher
	if (thruster_info.primary_glow_bitmap >= 0) {
		do_render = true;
	}

	// secondary_thruster_batcher
	if (thruster_info.secondary_glow_bitmap >= 0) {
		do_render = true;
	}

	// tertiary_thruster_batcher
	if (thruster_info.tertiary_glow_bitmap >= 0) {
		do_render = true;
	}

	if (do_render == false) {
		return;
	}

	// this is used for the secondary thruster glows 
	// it only needs to be calculated once so I'm doing it here -Bobboau
	norm.xyz.z = -1.0f;
	norm.xyz.x = 1.0f;
	norm.xyz.y = -1.0f;
	norm.xyz.x *= thruster_info.rotvel.xyz.y/2;
	norm.xyz.y *= thruster_info.rotvel.xyz.x/2;
	vm_vec_normalize(&norm);

	for (i = 0; i < pm->n_thrusters; i++ ) {
		vec3d submodel_static_offset; // The associated submodel's static offset in the ship's frame of reference
		bool submodel_rotation = false;

		bank = &pm->thrusters[i];

		// don't draw this thruster if the engine is destroyed or just not on
		if ( !model_should_render_engine_glow(objnum, bank->obj_num) )
			continue;

		// If bank is attached to a submodel, prepare to account for rotations
		//
		// TODO: This won't work in the ship lab, because the lab code doesn't
		// set the the necessary submodel instance info needed here. The second
		// condition is thus a hack to disable the feature while in the lab, and
		// can be removed if the lab is re-structured accordingly. -zookeeper
		if ( bank->submodel_num > -1 && pm->submodel[bank->submodel_num].can_move && (gameseq_get_state_idx(GS_STATE_LAB) == -1) ) {
			model_find_submodel_offset(&submodel_static_offset, Ship_info[shipp->ship_info_index].model_num, bank->submodel_num);

			submodel_rotation = true;
		}

		for (j = 0; j < bank->num_points; j++) {
			Assert( bank->points != NULL );

			float d, D;
			vec3d tempv;
			glow_point *gpt = &bank->points[j];
			vec3d loc_offset = gpt->pnt;
			vec3d loc_norm = gpt->norm;
			vec3d world_pnt;
			vec3d world_norm;

			if ( submodel_rotation ) {
				vm_vec_sub(&loc_offset, &gpt->pnt, &submodel_static_offset);

				tempv = loc_offset;
				find_submodel_instance_point_normal(&loc_offset, &loc_norm, &Objects[objnum], bank->submodel_num, &tempv, &loc_norm);
			}

			vm_vec_unrotate(&world_pnt, &loc_offset, orient);
			vm_vec_add2(&world_pnt, pos);

			if (shipp) {
				// if ship is warping out, check position of the engine glow to the warp plane
				if ( (shipp->flags & (SF_ARRIVING) ) && (shipp->warpin_effect) && Ship_info[shipp->ship_info_index].warpin_type != WT_HYPERSPACE) {
					vec3d warp_pnt, tmp;
					matrix warp_orient;

					shipp->warpin_effect->getWarpPosition(&warp_pnt);
					shipp->warpin_effect->getWarpOrientation(&warp_orient);
					vm_vec_sub( &tmp, &world_pnt, &warp_pnt );

					if ( vm_vec_dot( &tmp, &warp_orient.vec.fvec ) < 0.0f ) {
						break;
					}
				}

				if ( (shipp->flags & (SF_DEPART_WARP) ) && (shipp->warpout_effect) && Ship_info[shipp->ship_info_index].warpout_type != WT_HYPERSPACE) {
					vec3d warp_pnt, tmp;
					matrix warp_orient;

					shipp->warpout_effect->getWarpPosition(&warp_pnt);
					shipp->warpout_effect->getWarpOrientation(&warp_orient);
					vm_vec_sub( &tmp, &world_pnt, &warp_pnt );

					if ( vm_vec_dot( &tmp, &warp_orient.vec.fvec ) > 0.0f ) {
						break;
					}
				}
			}

			vm_vec_sub(&tempv, &View_position, &world_pnt);
			vm_vec_normalize(&tempv);
			vm_vec_unrotate(&world_norm, &loc_norm, orient);
			D = d = vm_vec_dot(&tempv, &world_norm);

			// ADAM: Min throttle draws rad*MIN_SCALE, max uses max.
#define NOISE_SCALE 0.5f
#define MIN_SCALE 3.4f
#define MAX_SCALE 4.7f

			float magnitude;
			vec3d scale_vec = { { { 1.0f, 0.0f, 0.0f } } };

			// normalize banks, in case of incredibly big normals
			if ( !IS_VEC_NULL_SQ_SAFE(&world_norm) )
				vm_vec_copy_normalize(&scale_vec, &world_norm);

			// adjust for thrust
			(scale_vec.xyz.x *= thruster_info.length.xyz.x) -= 0.1f;
			(scale_vec.xyz.y *= thruster_info.length.xyz.y) -= 0.1f;
			(scale_vec.xyz.z *= thruster_info.length.xyz.z)   -= 0.1f;

			// get magnitude, which we will use as the scaling reference
			magnitude = vm_vec_normalize(&scale_vec);

			// get absolute value
			if (magnitude < 0.0f)
				magnitude *= -1.0f;

			float scale = magnitude * (MAX_SCALE - MIN_SCALE) + MIN_SCALE;

			if (d > 0.0f){
				// Make glow bitmap fade in/out quicker from sides.
				d *= 3.0f;

				if (d > 1.0f)
					d = 1.0f;
			}

			float fog_int = 1.0f;

			// fade them in the nebula as well
			if (The_mission.flags & MISSION_FLAG_FULLNEB) {
				vm_vec_unrotate(&npnt, &gpt->pnt, orient);
				vm_vec_add2(&npnt, pos);

				fog_int = (1.0f - (neb2_get_fog_intensity(&npnt)));

				if (fog_int > 1.0f)
					fog_int = 1.0f;

				d *= fog_int;

				if (d > 1.0f)
					d = 1.0f;
			}

			float w = gpt->radius * (scale + thruster_info.glow_noise * NOISE_SCALE);

			// these lines are used by the tertiary glows, thus we will need to project this all of the time
			if (Cmdline_nohtl) {
				g3_rotate_vertex( &p, &world_pnt );
			} else {
				g3_transfer_vertex( &p, &world_pnt );
			}

			// start primary thruster glows
			if ( (thruster_info.primary_glow_bitmap >= 0) && (d > 0.0f) ) {
				p.r = p.g = p.b = p.a = (ubyte)(255.0f * d);
				batch_add_bitmap(
					thruster_info.primary_glow_bitmap, 
					TMAP_FLAG_GOURAUD | TMAP_FLAG_RGB | TMAP_FLAG_TEXTURED | TMAP_HTL_3D_UNLIT | TMAP_FLAG_SOFT_QUAD, 
					&p,
					0,
					(w * 0.5f * thruster_info.glow_rad_factor),
					1.0f,
					(w * 0.325f)
					);
			}

			// start tertiary thruster glows
			if (thruster_info.tertiary_glow_bitmap >= 0) {
				p.screen.xyw.w -= w;
				p.r = p.g = p.b = p.a = (ubyte)(255.0f * fog_int);
				batch_add_bitmap_rotated(
					thruster_info.tertiary_glow_bitmap,
					TMAP_FLAG_GOURAUD | TMAP_FLAG_RGB | TMAP_FLAG_TEXTURED | TMAP_HTL_3D_UNLIT | TMAP_FLAG_SOFT_QUAD,
					&p,
					(magnitude * 4),
					(w * 0.6f * thruster_info.tertiary_glow_rad_factor),
					1.0f,
					(-(D > 0) ? D : -D)
					);
			}

			// begin secondary glows
			if (thruster_info.secondary_glow_bitmap >= 0) {
				pnt = world_pnt;
				scale = magnitude * (MAX_SCALE - (MIN_SCALE / 2)) + (MIN_SCALE / 2);
				vm_vec_unrotate(&world_norm, &norm, orient);
				d = vm_vec_dot(&tempv, &world_norm);
				d += 0.75f;
				d *= 3.0f;

				if (d > 1.0f)
					d = 1.0f;

				if (d > 0.0f) {
					vm_vec_add(&norm2, &world_norm, &pnt);
					vm_vec_sub(&fvec, &norm2, &pnt);
					vm_vec_normalize(&fvec);

					float wVal = gpt->radius * scale * 2;

					vm_vec_scale_add(&norm2, &pnt, &fvec, wVal * 2 * thruster_info.glow_length_factor);

					if (The_mission.flags & MISSION_FLAG_FULLNEB) {
						vm_vec_add(&npnt, &pnt, pos);
						d *= fog_int;
					}

					batch_add_beam(thruster_info.secondary_glow_bitmap,
						TMAP_FLAG_GOURAUD | TMAP_FLAG_RGB | TMAP_FLAG_TEXTURED | TMAP_FLAG_CORRECT | TMAP_HTL_3D_UNLIT,
						&pnt, &norm2, wVal*thruster_info.secondary_glow_rad_factor*0.5f, d
						);
					if (Scene_framebuffer_in_frame && thruster_info.draw_distortion) {
						vm_vec_scale_add(&norm2, &pnt, &fvec, wVal * 2 * thruster_info.distortion_length_factor);
						int dist_bitmap;
						if (thruster_info.distortion_bitmap > 0) {
							dist_bitmap = thruster_info.distortion_bitmap;
						}
						else {
							dist_bitmap = thruster_info.secondary_glow_bitmap;
						}
						float mag = vm_vec_mag(&gpt->pnt); 
						mag -= (float)((int)mag);//Valathil - Get a fairly random but constant number to offset the distortion texture
						distortion_add_beam(dist_bitmap,
							TMAP_FLAG_GOURAUD | TMAP_FLAG_RGB | TMAP_FLAG_TEXTURED | TMAP_FLAG_CORRECT | TMAP_HTL_3D_UNLIT | TMAP_FLAG_DISTORTION_THRUSTER | TMAP_FLAG_SOFT_QUAD,
							&pnt, &norm2, wVal*thruster_info.distortion_rad_factor*0.5f, 1.0f, mag
							);
					}
				}
			}

			// begin particles
			if (shipp) {
				ship_info *sip = &Ship_info[shipp->ship_info_index];
				particle_emitter pe;
				thruster_particles *tp;
				size_t num_particles = 0;

				if (thruster_info.use_ab)
					num_particles = sip->afterburner_thruster_particles.size();
				else
					num_particles = sip->normal_thruster_particles.size();

				for (k = 0; k < num_particles; k++) {
					if (thruster_info.use_ab)
						tp = &sip->afterburner_thruster_particles[k];
					else
						tp = &sip->normal_thruster_particles[k];

					float v = vm_vec_mag_quick(&Objects[shipp->objnum].phys_info.desired_vel);

					vm_vec_unrotate(&npnt, &gpt->pnt, orient);
					vm_vec_add2(&npnt, pos);

					// Where the particles emit from
					pe.pos = npnt;
					// Initial velocity of all the particles
					pe.vel = Objects[shipp->objnum].phys_info.desired_vel;
					pe.min_vel = v * 0.75f;
					pe.max_vel =  v * 1.25f;
					// What normal the particle emit around
					pe.normal = orient->vec.fvec;
					vm_vec_negate(&pe.normal);

					// Lowest number of particles to create
					pe.num_low = tp->n_low;
					// Highest number of particles to create
					pe.num_high = tp->n_high;
					pe.min_rad = gpt->radius * tp->min_rad;
					pe.max_rad = gpt->radius * tp->max_rad;
					// How close they stick to that normal 0=on normal, 1=180, 2=360 degree
					pe.normal_variance = tp->variance;

					particle_emit( &pe, PARTICLE_BITMAP, tp->thruster_bitmap.first_frame);
				}
			}
		}
	}
}

void model_render_debug_children(polymodel *pm, int mn, int detail_level, uint flags)
{
	int i;

	if ( (mn < 0) || (mn >= pm->n_models) ) {
		Int3();
		return;
	}

	bsp_info *model = &pm->submodel[mn];

	if ( model->blown_off ) {
		return;
	}

	// Get submodel rotation data and use submodel orientation matrix
	// to put together a matrix describing the final orientation of
	// the submodel relative to its parent
	angles ang = model->angs;

	// Add barrel rotation if needed
	if ( model->gun_rotation ) {
		if ( pm->gun_submodel_rotation > PI2 ) {
			pm->gun_submodel_rotation -= PI2;
		} else if ( pm->gun_submodel_rotation < 0.0f ) {
			pm->gun_submodel_rotation += PI2;
		}

		ang.b += pm->gun_submodel_rotation;
	}

	// Compute final submodel orientation by using the orientation matrix
	// and the rotation angles.
	// By using this kind of computation, the rotational angles can always
	// be computed relative to the submodel itself, instead of relative
	// to the parent
	matrix rotation_matrix = model->orientation;
	vm_rotate_matrix_by_angles(&rotation_matrix, &ang);

	matrix inv_orientation;
	vm_copy_transpose_matrix(&inv_orientation, &model->orientation);

	matrix submodel_matrix;
	vm_matrix_x_matrix(&submodel_matrix, &rotation_matrix, &inv_orientation);

	g3_start_instance_matrix(&model->offset, &submodel_matrix, true);

// 	if ( flags & MR_SHOW_PIVOTS ) {
// 		model_draw_debug_points( pm, &pm->submodel[mn], flags );
// 	}

	i = model->first_child;

	while ( i >= 0 ) {
		model_render_debug_children( pm, i, detail_level, flags );

		i = pm->submodel[i].next_sibling;
	}

	g3_done_instance(true);
}

void model_render_debug(int model_num, matrix *orient, vec3d * pos, uint flags, uint debug_flags, int objnum, int detail_level_locked )
{
	ship *shipp = NULL;
	object *objp = NULL;

	if ( objnum >= 0 ) {
		objp = &Objects[objnum];

		if ( objp->type == OBJ_SHIP ) {
			shipp = &Ships[objp->instance];
		}
	}

	polymodel *pm = model_get(model_num);	
	
	g3_start_instance_matrix(pos, orient, true);

	if ( debug_flags & MR_DEBUG_RADIUS ) {
		if ( !( flags & MR_SHOW_OUTLINE_PRESET ) ) {
			gr_set_color(0,64,0);
			g3_draw_sphere_ez(&vmd_zero_vector,pm->rad);
		}
	}

	int detail_level = model_queue_render_determine_detail(objnum, model_num, orient, pos, flags, detail_level_locked);

	vec3d auto_back = ZERO_VECTOR;
	bool set_autocen = model_render_determine_autocenter(&auto_back, pm, detail_level, flags);
	
	if ( set_autocen ) {
		g3_start_instance_matrix(&auto_back, NULL, true);
	}

	uint save_gr_zbuffering_mode = gr_zbuffer_set(GR_ZBUFF_READ);

	int i = pm->submodel[pm->detail[detail_level]].first_child;

	while ( i >= 0 ) {
		model_render_debug_children( pm, i, detail_level, flags );

		i = pm->submodel[i].next_sibling;
	}

	if ( debug_flags & MR_DEBUG_PIVOTS ) {
		model_draw_debug_points( pm, NULL, flags );
		model_draw_debug_points( pm, &pm->submodel[pm->detail[detail_level]], flags );

		if ( pm->flags & PM_FLAG_AUTOCEN ) {
			gr_set_color(255, 255, 255);
			g3_draw_sphere_ez(&pm->autocenter, pm->rad / 4.5f);
		}
	}

	if ( debug_flags & MR_DEBUG_SHIELDS )	{
		model_render_shields(pm, flags);
	}	

	if ( debug_flags & MR_DEBUG_PATHS ) {
		if ( Cmdline_nohtl ) model_draw_paths(model_num, flags);
		else model_draw_paths_htl(model_num, flags);
	}

	if ( debug_flags & MR_DEBUG_BAY_PATHS ) {
		if ( Cmdline_nohtl ) model_draw_bay_paths(model_num);
		else model_draw_bay_paths_htl(model_num);
	}

	if ( (flags & MR_AUTOCENTER) && (set_autocen) ) {
		g3_done_instance(true);
	}

	g3_done_instance(true);

	gr_zbuffer_set(save_gr_zbuffering_mode);
}

void model_immediate_render(int model_num, matrix *orient, vec3d * pos, uint flags, int objnum, int lighting_skip, int *replacement_textures)
{

}

void model_immediate_render(model_render_params *render_info, int model_num, matrix *orient, vec3d * pos, int render)
{
	DrawList model_list;

	model_list.init();

	model_queue_render(render_info, &model_list, model_num, orient, pos);

	model_list.initRender();

	switch ( render ) {
	case MODEL_RENDER_OPAQUE:
		gr_zbuffer_set(ZBUFFER_TYPE_FULL);
		model_list.renderAll(GR_ALPHABLEND_NONE);
		break;
	case MODEL_RENDER_TRANS:
		gr_zbuffer_set(ZBUFFER_TYPE_READ);
		model_list.renderAll(GR_ALPHABLEND_FILTER);
		break;
	case MODEL_RENDER_ALL:
		gr_zbuffer_set(ZBUFFER_TYPE_FULL);
		model_list.renderAll(GR_ALPHABLEND_NONE);
		gr_zbuffer_set(ZBUFFER_TYPE_READ);
		model_list.renderAll(GR_ALPHABLEND_FILTER);
		break;
	}
	
	gr_zbias(0);
	gr_set_cull(0);
	gr_zbuffer_set(ZBUFFER_TYPE_READ);
	gr_set_fill_mode(GR_FILL_MODE_SOLID);

	gr_flush_data_states();
	gr_set_buffer(-1);

	gr_reset_lighting();
	gr_set_lighting(false, false);

	GL_state.Texture.DisableAll();

	model_render_debug(model_num, orient, pos, render_info->get_model_flags(), render_info->get_debug_flags(), render_info->get_object_number(), render_info->get_detail_level_lock());
}

void model_queue_render(model_render_params *interp, DrawList *scene, int model_num, matrix *orient, vec3d *pos)
{
	int i;
	int cull = 0;

	const int objnum = interp->get_object_number();
	const int model_flags = interp->get_model_flags();

	polymodel *pm = model_get(model_num);
	polymodel_instance * pmi = NULL;

	model_do_dumb_rotation(model_num);

	if ( interp->is_clip_plane_set() ) {
		scene->setClipPlane(interp->get_clip_plane_pos(), interp->get_clip_plane_normal());
	} else {
		scene->setClipPlane();
	}

	if ( interp->is_team_color_set() ) {
		scene->setTeamColor(interp->get_team_color());
	} else {
		scene->setTeamColor();
	}
		
	if ( model_flags & MR_FORCE_CLAMP ) {
		scene->setTextureAddressing(TMAP_ADDRESS_CLAMP);
	} else {
		scene->setTextureAddressing(TMAP_ADDRESS_WRAP);
	}

	model_queue_render_set_glow_points(pm, objnum);

	if ( !(model_flags & MR_NO_LIGHTING) ) {
		scene->setLightFilter( objnum, pos, pm->rad );
	}

	scene->setLightFactor(model_queue_render_determine_light(interp, pos, model_flags));

	ship *shipp = NULL;
	object *objp = NULL;
	
	int tmp_detail_level = Game_detail_level;
	
	// Set the flags we will pass to the tmapper
	uint tmap_flags = TMAP_FLAG_GOURAUD | TMAP_FLAG_RGB;

	// if we're in nebula mode, fog everything except for the warp holes and other non-fogged models
	if((The_mission.flags & MISSION_FLAG_FULLNEB) && (Neb2_render_mode != NEB2_RENDER_NONE) && !(model_flags & MR_NO_FOGGING)){
		tmap_flags |= TMAP_FLAG_PIXEL_FOG;
	}
	
	if ( !(model_flags & MR_NO_TEXTURING) )	{
		tmap_flags |= TMAP_FLAG_TEXTURED;

		if ( (pm->flags & PM_FLAG_ALLOW_TILING) && tiling)
			tmap_flags |= TMAP_FLAG_TILED;

		if ( !(model_flags & MR_NO_CORRECT) )	{
			tmap_flags |= TMAP_FLAG_CORRECT;
		}
	}

	if ( model_flags & MR_DESATURATED ) {
		tmap_flags |= TMAP_FLAG_DESATURATE;
	}

	if ( interp->get_animated_effect_num() >= 0 ) {
		tmap_flags |= TMAP_ANIMATED_SHADER;
		scene->setAnimatedEffect(interp->get_animated_effect_num());
		scene->setAnimatedTimer(interp->get_animated_effect_timer());
	}

	bool is_outlines_only = (model_flags & MR_NO_POLYS) && ((model_flags & MR_SHOW_OUTLINE_PRESET) || (model_flags & MR_SHOW_OUTLINE));
	bool is_outlines_only_htl = !Cmdline_nohtl && (model_flags & MR_NO_POLYS) && (model_flags & MR_SHOW_OUTLINE_HTL);
	bool use_api = !is_outlines_only_htl || (gr_screen.mode == GR_OPENGL);

	scene->pushTransform(pos, orient);

	int detail_level = model_queue_render_determine_detail(objnum, model_num, orient, pos, model_flags, interp->get_detail_level_lock());

// #ifndef NDEBUG
// 	if ( Interp_detail_level == 0 )	{
// 		MONITOR_INC( NumHiModelsRend, 1 );
// 	} else if ( Interp_detail_level == pm->n_detail_levels-1 ) {
// 		MONITOR_INC( NumLowModelsRend, 1 );
// 	}  else {
// 		MONITOR_INC( NumMedModelsRend, 1 );
// 	}
// #endif
	
	vec3d auto_back = ZERO_VECTOR;
	bool set_autocen = model_render_determine_autocenter(&auto_back, pm, detail_level, model_flags);
	
	if ( set_autocen ) {
		scene->pushTransform(&auto_back, NULL);
	}

	if ( tmap_flags & TMAP_FLAG_PIXEL_FOG ) {
		float fog_near = 10.0f, fog_far = 1000.0f;
		neb2_get_adjusted_fog_values(&fog_near, &fog_far, objp);

		unsigned char r, g, b;
		neb2_get_fog_color(&r, &g, &b);

		scene->setFog(GR_FOGMODE_FOG, r, g, b, fog_near, fog_far);
	} else {
		scene->setFog(GR_FOGMODE_NONE, 0, 0, 0);
	}

	if ( is_outlines_only_htl ) {
		scene->setFillMode(GR_FILL_MODE_WIRE);

		color outline_color = interp->get_outline_color();
		gr_set_color_fast( &outline_color );

		tmap_flags &= ~TMAP_FLAG_RGB;
	} else {
		scene->setFillMode(GR_FILL_MODE_SOLID);
	}
		
	if ( model_flags & MR_EDGE_ALPHA ) {
		scene->setCenterAlpha(-1);
	} else if ( model_flags & MR_CENTER_ALPHA ) {
		scene->setCenterAlpha(1);
	} else {
		scene->setCenterAlpha(0);
	}

	if ( ( model_flags & MR_NO_CULL ) || ( model_flags & MR_ALL_XPARENT ) || ( interp->get_warp_bitmap() >= 0 ) ) {
		scene->setCullMode(0);
	} else {
		scene->setCullMode(1);
	}

	if ( !(model_flags & MR_NO_LIGHTING) ) {
		scene->setLighting(true);
	} else {
		scene->setLighting(false);
	}
	
	if (is_outlines_only_htl || (!Cmdline_nohtl && !is_outlines_only)) {
		scene->setBuffer(pm->vertex_buffer_id);
	}

	if ( in_shadow_map ) {
		scene->setZBias(-1024);
	} else {
		scene->setZBias(0);
	}

	if ( GL_use_transform_buffer && !Cmdline_no_batching && !(model_flags & MR_NO_BATCH) ) {
		// always set batched rendering on if supported
		tmap_flags |= TMAP_FLAG_BATCH_TRANSFORMS;
	}

	if ( (tmap_flags & TMAP_FLAG_BATCH_TRANSFORMS) ) {
		scene->startModelDraw(pm->n_models);
		model_queue_render_buffers(scene, interp, pm, -1, detail_level, tmap_flags);
	}
		
	// Draw the subobjects
	bool draw_thrusters = false;
	i = pm->submodel[pm->detail[detail_level]].first_child;

	while( i >= 0 )	{
		if ( !pm->submodel[i].is_thruster ) {
			model_queue_render_children_buffers( scene, interp, pm, i, detail_level, tmap_flags );
		} else {
			draw_thrusters = true;
		}

		i = pm->submodel[i].next_sibling;
	}

	model_radius = pm->submodel[pm->detail[detail_level]].rad;

	//*************************** draw the hull of the ship *********************************************
	vec3d view_pos = scene->getViewPosition();

	if ( model_render_check_detail_box(&view_pos, pm, pm->detail[detail_level], model_flags) ) {
		model_queue_render_buffers(scene, interp, pm, pm->detail[detail_level], detail_level, tmap_flags);
	}
	
	// Draw the thruster subobjects
	if ( draw_thrusters ) {
		// always turn off batching when drawing thrusters
		tmap_flags &= ~TMAP_FLAG_BATCH_TRANSFORMS;

		i = pm->submodel[pm->detail[detail_level]].first_child;

		while( i >= 0 ) {
			if (pm->submodel[i].is_thruster) {
				model_queue_render_children_buffers( scene, interp, pm, i, detail_level, tmap_flags );
			}
			i = pm->submodel[i].next_sibling;
		}
	}

	if ( !( model_flags & MR_NO_TEXTURING ) ) {
		scene->addInsignia(pm, detail_level, interp->get_insignia_bitmap());
	}

	if ( (model_flags & MR_AUTOCENTER) && (set_autocen) ) {
		scene->popTransform();
	}

	scene->popTransform();

	// start rendering glow points -Bobboau
	if ( (pm->n_glow_point_banks) && !is_outlines_only && !is_outlines_only_htl && !Glowpoint_override ) {
		model_queue_render_glow_points(pm, shipp, orient, pos, Glowpoint_use_depth_buffer);
	}

	// Draw the thruster glow
	if ( !is_outlines_only && !is_outlines_only_htl ) {
		if ( ( model_flags & MR_AUTOCENTER ) && set_autocen ) {
			vec3d autoback_rotated;

			vm_vec_unrotate(&autoback_rotated, &auto_back, orient);
			vm_vec_add2(&autoback_rotated, pos);

			model_queue_render_thrusters( interp, pm, objnum, shipp, orient, &autoback_rotated );
		} else {
			model_queue_render_thrusters( interp, pm, objnum, shipp, orient, pos );
		}
	}
}