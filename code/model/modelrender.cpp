/*
 * Copyright (C) Freespace Open 2013.  All rights reserved.
 *
 * All source code herein is the property of Freespace Open. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#include "Math/vecmat.h"
#include "model/modelrender.h"
#include "ship/ship.h"
#include "cmdline/cmdline.h"
#include "nebula/neb.h"
#include "graphics/tmapper.h"

DrawList::DrawList()
{
	set_clip_plane = -1;
}

void DrawList::setClipPlane(vec3d *position, vec3d normal)
{
	clip_plane_state clip_normal;

	clip_planes.push_back(clip_normal);

	current_render_state.clip_plane_handle = clip_planes.size() - 1;
}

void DrawList::addBufferDraw(matrix* orient, vec3d* pos, vertex_buffer *buffer, int texi, uint tmap_flags, interp_data *interp)
{
	uint sdr_flags = determineShaderFlags(&current_render_state, buffer, tmap_flags);

	current_render_state.light = interp->light;

	// need to do a check to see if the top render state matches the current.
	render_states.push_back(current_render_state);

	queued_buffer_draw draw_data;

	draw_data.render_state_handle = render_states.size() - 1;
	draw_data.orient = *orient;
	draw_data.pos = *pos;
	draw_data.buffer = buffer;
	draw_data.texi = texi;
	draw_data.flags = tmap_flags;
	draw_data.uniform_data_handle = uniforms.size() - 1;
	draw_data.sdr_flags = sdr_flags;

	uniform_data new_uniforms;

	// create uniform data here
	determineUniforms(&new_uniforms, &current_render_state, tmap_flags, sdr_flags);
	
	uniforms.push_back(new_uniforms);
	render_elements.push_back(draw_data);

	render_keys.push_back(render_elements.size() - 1);
}

void DrawList::determineUniforms(uniform_data *uniform_data_instance, render_state *render_state_instance, uint tmap_flags, uint sdr_flags)
{

	uniform_data_instance->anim_timer = opengl_shader_get_animated_timer();
	uniform_data_instance->effect_num = opengl_shader_get_animated_effect();
	uniform_data_instance->vp_width = 1.0f/gr_screen.max_w;
	uniform_data_instance->vp_width = 1.0f/gr_screen.max_h;

	uniform_data_instance->red = gr_screen.current_color.red/255.0f;
	uniform_data_instance->green = gr_screen.current_color.green/255.0f;
	uniform_data_instance->blue = gr_screen.current_color.blue/255.0f;
}

uint DrawList::determineShaderFlags(render_state *state, vertex_buffer *buffer, int flags)
{
	bool enable_lighting = state->lighting > 0;
	bool texture = (flags & TMAP_FLAG_TEXTURED) && (buffer->flags & VB_FLAG_UV1);
	
	bool fog = false;

	if ( state->fog_mode == GR_FOGMODE_FOG ) {
		fog = true;
	}

	gr_determine_shader_flags(enable_lighting, fog, texture, in_shadow_map, );
}

void DrawList::drawRenderElement(queued_buffer_draw *render_elements)
{
	// get the render state for this draw call
	int render_state_num = render_elements->render_state_handle;

	render_state *draw_state = &render_states[render_state_num];

	// set clip plane if necessary
	if ( draw_state->clip_plane_handle >= 0 && draw_state->clip_plane_handle != set_clip_plane ) {
		set_clip_plane = draw_state->clip_plane_handle;

		clip_plane_state *clip_plane = &clip_planes[set_clip_plane];

		G3_user_clip_normal = clip_plane->normal;
		G3_user_clip_point = clip_plane->point;

		gr_start_clip();
	} else ( draw_state->clip_plane_handle < 0 && set_clip_plane >= 0 ) {
		// draw call doesn't have a clip plane so check if the clip plane is currently set.
		gr_end_clip();
	}

	opengl_shader_set_animated_effect(draw_state->animated_effect);
	opengl_shader_set_animated_timer(draw_state->animated_timer);

	gr_set_texture_addressing(draw_state->texture_addressing);

	gr_zbuffer_set(draw_state->depth_mode);

	gr_set_cull(draw_state->cull_mode);

	gr_center_alpha(draw_state->center_alpha);

	gr_set_fill_mode(draw_state->fill_mode);

	gr_zbias(draw_state->zbias);

	gr_set_bitmap(draw_state->texture_maps[TM_BASE_TYPE], draw_state->blend_filter, GR_BITBLT_MODE_NORMAL, draw_state->alpha);

	GLOWMAP = draw_state->texture_maps[TM_GLOW_TYPE];
	SPECMAP = draw_state->texture_maps[TM_SPECULAR_TYPE];
	NORMMAP = draw_state->texture_maps[TM_NORMAL_TYPE];
	HEIGHTMAP = draw_state->texture_maps[TM_HEIGHT_TYPE];
	MISCMAP = draw_state->texture_maps[TM_MISC_TYPE];
}

void model_queue_render_lightning( DrawList *scene, interp_data* interp, polymodel *pm, bsp_info * sm )
{
	int i;
	float width = 0.9f;
	color primary, secondary;

	Assert( sm->num_arcs > 0 );

	if ( interp->flags & MR_SHOW_OUTLINE_PRESET ) {
		return;
	}

/*	if ( !Interp_lightning ) {
 		return;
 	}
*/
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
		scene->addArc(&Object_matrix, &Object_position, &sm->arc_pts[i][0], &sm->arc_pts[i][1], &primary, &secondary, width);
	}
}

int model_queue_render_determine_detail(int obj_num, int model_num, matrix* orient, vec3d* pos, int flags, int detail_level_locked)
{
	Assert( pm->n_detail_levels < MAX_MODEL_DETAIL_LEVELS );

	polymodel *pm = model_get(model_num);

	vec3d closest_pos;
	float depth = model_find_closest_point( &closest_pos, model_num, -1, orient, pos, &Eye_position );

	int i;

	if ( pm->n_detail_levels > 1 ) {
		if ( flags & MR_LOCK_DETAIL ) {
			i = detail_level_locked+1;
		} else {

#if MAX_DETAIL_LEVEL != 4
#error Code in modelInterp.cpp assumes MAX_DETAIL_LEVEL == 4
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

		// maybe force lower detail
		if ( flags & MR_FORCE_LOWER_DETAIL ) {
			i++;
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

void model_queue_render_buffers(DrawList* scene, interp_data* interp, polymodel *pm, int mn, bool is_child)
{
	if ( pm->vertex_buffer_id < 0 ) {
		return;
	}

	if ( (mn < 0) || (mn >= pm->n_models) ) {
		Int3();
		return;
	}

	bsp_info *model = &pm->submodel[mn];

	// if using detail boxes or spheres, check that we are valid for the range
	if ( !is_child && !( interp->flags & MR_FULL_DETAIL ) && model->use_render_box ) {
		vm_vec_copy_scale(&interp->render_box_min, &model->render_box_min, interp->box_scale);
		vm_vec_copy_scale(&interp->render_box_max, &model->render_box_max, interp->box_scale);

		if ( (-model->use_render_box + in_box(&interp->render_box_min, &interp->render_box_max, &model->offset)) )
			return;
	}
	if ( !is_child && !( interp->flags & MR_FULL_DETAIL ) && model->use_render_sphere ) {
		interp->render_sphere_radius = model->render_sphere_radius * interp->box_scale;

		// TODO: doesn't consider submodel rotations yet -zookeeper
		vec3d offset;
		model_find_submodel_offset(&offset, pm->id, mn);
		vm_vec_add2(&offset, &model->render_sphere_offset);

		if ( -model->use_render_sphere + in_sphere(&offset, interp->render_sphere_radius) ) {
			return;
		}
	}

	vec3d scale;

	if ( interp->thrust_scale_subobj ) {
		scale.xyz.x = 1.0f;
		scale.xyz.y = 1.0f;
		scale.xyz.z = interp->thrust_scale;
	} else {
		scale.xyz.x = interp->warp_scale_x;
		scale.xyz.y = interp->warp_scale_y;
		scale.xyz.z = interp->warp_scale_z;
	}

	texture_info tex_replace[TM_NUM_TYPES];

	int no_texturing = interp->flags & MR_NO_TEXTURING;

	int forced_texture = -2;
	float forced_alpha = 1.0f;
	int forced_blend_filter = GR_ALPHABLEND_NONE;

	if ( ( interp->flags & MR_FORCE_TEXTURE ) && ( interp->forced_bitmap >= 0 ) ) {
		forced_texture = interp->forced_bitmap;
	} else if ( interp->warp_bitmap >= 0 ) {
		forced_texture = interp->warp_bitmap;
		forced_alpha = interp->warp_alpha;
		forced_blend_filter = GR_ALPHABLEND_FILTER;
	} else if ( interp->thrust_scale_subobj ) {
		if ( ( interp->thrust_bitmap >= 0 ) && ( interp->thrust_scale > 0.0f ) ) {
			forced_texture = interp->thrust_bitmap;
		} else {
			forced_texture = -1;
		}

		forced_alpha = 1.2f;
		forced_blend_filter = GR_ALPHABLEND_FILTER;
	} else if ( interp->flags & MR_ALL_XPARENT ) {
		forced_alpha = interp->xparent_alpha;
		forced_blend_filter = GR_ALPHABLEND_FILTER;
	}

	if ( !interp->thrust_scale_subobj ) {
		scene->setScale(&scale);
	} else {
		scene->setScale();
	}

	int texture_maps[TM_NUM_TYPES] = {-1, -1, -1, -1, -1, -1};
	size_t buffer_size = model->buffer.tex_buf.size();

	for ( size_t i = 0; i < buffer_size; i++ ) {
		int tmap_num = model->buffer.tex_buf[i].texture;
		texture_map *tmap = &pm->maps[tmap_num];
		int rt_begin_index = tmap_num*TM_NUM_TYPES;
		float alpha = 1.0f;
		int blend_filter = GR_ALPHABLEND_NONE;

		if (forced_texture != -2) {
			texture_maps[TM_BASE_TYPE] = forced_texture;
			alpha = forced_alpha;
		} else if ( !no_texturing ) {
			// pick the texture, animating it if necessary
			if ( (interp->new_replacement_textures != NULL) && (interp->new_replacement_textures[rt_begin_index + TM_BASE_TYPE] == REPLACE_WITH_INVISIBLE) ) {
				// invisible textures aren't rendered, but we still have to skip assigning the underlying model texture
				texture_maps[TM_BASE_TYPE] = -1;
			} else if ( (interp->new_replacement_textures != NULL) && (interp->new_replacement_textures[rt_begin_index + TM_BASE_TYPE] >= 0) ) {
				// an underlying texture is replaced with a real new texture
				tex_replace[TM_BASE_TYPE] = texture_info(interp->new_replacement_textures[rt_begin_index + TM_BASE_TYPE]);
				texture_maps[TM_BASE_TYPE] = model_interp_get_texture(&tex_replace[TM_BASE_TYPE], interp->base_frametime);
			} else {
				// we just use the underlying texture
				texture_maps[TM_BASE_TYPE] = model_interp_get_texture(&tmap->textures[TM_BASE_TYPE], interp->base_frametime);
			}

			if ( texture_maps[TM_BASE_TYPE] < 0 ) {
				continue;
			}

			// doing glow maps?
			if ( !(interp->flags & MR_NO_GLOWMAPS) ) {
				texture_info *tglow = &tmap->textures[TM_GLOW_TYPE];

				if ( (interp->new_replacement_textures != NULL) && (interp->new_replacement_textures[rt_begin_index + TM_GLOW_TYPE] >= 0) ) {
					tex_replace[TM_GLOW_TYPE] = texture_info(interp->new_replacement_textures[rt_begin_index + TM_GLOW_TYPE]);
					texture_maps[TM_GLOW_TYPE] = model_interp_get_texture(&tex_replace[TM_GLOW_TYPE], interp->base_frametime);
				} else if (tglow->GetTexture() >= 0) {
					// shockwaves are special, their current frame has to come out of the shockwave code to get the timing correct
					if ( (interp->objnum >= 0) && (Objects[interp->objnum].type == OBJ_SHOCKWAVE) && (tglow->GetNumFrames() > 1) ) {
						texture_maps[TM_GLOW_TYPE] = tglow->GetTexture() + shockwave_get_framenum(Objects[interp->objnum].instance, tglow->GetNumFrames());
					} else {
						texture_maps[TM_GLOW_TYPE] = model_interp_get_texture(tglow, interp->base_frametime);
					}
				}
			}

			if ( (Detail.lighting > 2)  && (interp->detail_level < 2) ) {
				// likewise, etc.
				texture_info *spec_map = &tmap->textures[TM_SPECULAR_TYPE];
				texture_info *norm_map = &tmap->textures[TM_NORMAL_TYPE];
				texture_info *height_map = &tmap->textures[TM_HEIGHT_TYPE];
				texture_info *misc_map = &tmap->textures[TM_MISC_TYPE];

				if (interp->new_replacement_textures != NULL) {
					if (interp->new_replacement_textures[rt_begin_index + TM_SPECULAR_TYPE] >= 0) {
						tex_replace[TM_SPECULAR_TYPE] = texture_info(interp->new_replacement_textures[rt_begin_index + TM_SPECULAR_TYPE]);
						spec_map = &tex_replace[TM_SPECULAR_TYPE];
					}

					if (interp->new_replacement_textures[rt_begin_index + TM_NORMAL_TYPE] >= 0) {
						tex_replace[TM_NORMAL_TYPE] = texture_info(interp->new_replacement_textures[rt_begin_index + TM_NORMAL_TYPE]);
						norm_map = &tex_replace[TM_NORMAL_TYPE];
					}

					if (interp->new_replacement_textures[rt_begin_index + TM_HEIGHT_TYPE] >= 0) {
						tex_replace[TM_HEIGHT_TYPE] = texture_info(interp->new_replacement_textures[rt_begin_index + TM_HEIGHT_TYPE]);
						height_map = &tex_replace[TM_HEIGHT_TYPE];
					}

					if (interp->new_replacement_textures[rt_begin_index + TM_MISC_TYPE] >= 0) {
						tex_replace[TM_MISC_TYPE] = texture_info(interp->new_replacement_textures[rt_begin_index + TM_MISC_TYPE]);
						misc_map = &tex_replace[TM_MISC_TYPE];
					}
				}

				texture_maps[TM_SPECULAR_TYPE] = model_interp_get_texture(spec_map, interp->base_frametime);
				texture_maps[TM_NORMAL_TYPE] = model_interp_get_texture(norm_map, interp->base_frametime);
				texture_maps[TM_HEIGHT_TYPE] = model_interp_get_texture(height_map, interp->base_frametime);
				texture_maps[TM_MISC_TYPE] = model_interp_get_texture(misc_map, interp->base_frametime);
			}
		} else {
			alpha = forced_alpha;

			//Check for invisible or transparent textures so they don't show up in the shadow maps - Valathil
			if ( (interp->new_replacement_textures != NULL) && (interp->new_replacement_textures[rt_begin_index + TM_BASE_TYPE] >= 0) ) {
				tex_replace[TM_BASE_TYPE] = texture_info(interp->new_replacement_textures[rt_begin_index + TM_BASE_TYPE]);
				texture_maps[TM_BASE_TYPE] = model_interp_get_texture(&tex_replace[TM_BASE_TYPE], interp->base_frametime);
			} else {
				texture_maps[TM_BASE_TYPE] = model_interp_get_texture(&tmap->textures[TM_BASE_TYPE], interp->base_frametime);
			}

			if ( texture_maps[TM_BASE_TYPE] <= 0 ) {
				continue;
			}
		}

		if ( (texture_maps[TM_BASE_TYPE] == -1) && !no_texturing ) {
			continue;
		}

		// trying to get transparent textures-Bobboau
		if (tmap->is_transparent) {
			// for special shockwave/warp map usage
			alpha = (interp->warp_alpha != -1.0f) ? interp->warp_alpha : 0.8f;
			blend_filter = GR_ALPHABLEND_FILTER;

		}

		if (forced_blend_filter != GR_ALPHABLEND_NONE) {
			blend_filter = forced_blend_filter;
		}

		scene->setBlendFilter(blend_filter, alpha);

		scene->setTexture(TM_BASE_TYPE,	texture_maps[TM_BASE_TYPE]);
		scene->setTexture(TM_GLOW_TYPE,	texture_maps[TM_GLOW_TYPE]);
		scene->setTexture(TM_SPECULAR_TYPE, texture_maps[TM_SPECULAR_TYPE]);
		scene->setTexture(TM_NORMAL_TYPE, texture_maps[TM_NORMAL_TYPE]);
		scene->setTexture(TM_HEIGHT_TYPE, texture_maps[TM_HEIGHT_TYPE]);
		scene->setTexture(TM_MISC_TYPE,	texture_maps[TM_MISC_TYPE]);

		scene->addBufferDraw(&Object_matrix, &Object_position, &model->buffer, i, interp->tmap_flags);

		texture_maps[TM_BASE_TYPE] = -1;
		texture_maps[TM_GLOW_TYPE] = -1;
		texture_maps[TM_SPECULAR_TYPE] = -1;
		texture_maps[TM_NORMAL_TYPE] = -1;
		texture_maps[TM_HEIGHT_TYPE] = -1;
		texture_maps[TM_MISC_TYPE] = -1;
	}
}

void model_queue_render_children_buffers(DrawList* scene, interp_data* interp, polymodel* pm, int mn, int detail_level)
{
	int i;

	if ( (mn < 0) || (mn >= pm->n_models) ) {
		Int3();
		return;
	}

	bsp_info *model = &pm->submodel[mn];

	if (model->blown_off)
		return;

	uint fl = interp->flags;

	if (model->is_thruster) {
		if ( !( interp->flags & MR_SHOW_THRUSTERS ) ) {
			return;
		}

		interp->flags |= MR_NO_LIGHTING;
		interp->thrust_scale_subobj = 1;
		scene->setLighting(false);
	} else {
		interp->thrust_scale_subobj = 0;
	}

	// if using detail boxes or spheres, check that we are valid for the range
	if ( !( interp->flags & MR_FULL_DETAIL ) && model->use_render_box ) {
		vm_vec_copy_scale(&interp->render_box_min, &model->render_box_min, interp->box_scale);
		vm_vec_copy_scale(&interp->render_box_max, &model->render_box_max, interp->box_scale);

		if ( (-model->use_render_box + in_box(&interp->render_box_min, &interp->render_box_max, &model->offset)) ) {
			return;
		}
	}

	if ( !(interp->flags & MR_FULL_DETAIL) && model->use_render_sphere ) {
		interp->render_sphere_radius = model->render_sphere_radius * interp->box_scale;

		// TODO: doesn't consider submodel rotations yet -zookeeper
		vec3d offset;
		model_find_submodel_offset(&offset, pm->id, mn);
		vm_vec_add2(&offset, &model->render_sphere_offset);

		if ( (-model->use_render_sphere + in_sphere(&offset, interp->render_sphere_radius)) ) {
			return;
		}
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

	g3_start_instance_matrix(&model->offset, &submodel_matrix, false);

	model_queue_render_buffers(scene, interp, pm, mn, true);

	if ( model->num_arcs ) {
		model_queue_render_lightning( scene, interp, pm, &pm->submodel[mn] );
	}

	i = model->first_child;

	while ( i >= 0 ) {
		if ( !pm->submodel[i].is_thruster ) {
			model_queue_render_children_buffers( scene, interp, pm, i, detail_level );
		}

		i = pm->submodel[i].next_sibling;
	}

	interp->flags = fl;

	if ( interp->thrust_scale_subobj ) {
		interp->thrust_scale_subobj = 0;

		if ( !( interp->flags & MR_NO_LIGHTING ) ) {
			scene->setLighting(true);
		}
	}

	g3_done_instance(false);
}

float model_queue_render_determine_light(interp_data* interp, vec3d *pos, uint flags)
{
	if ( flags & MR_NO_LIGHTING ) {
		return 1.0f;
	} else if ( flags & MR_IS_ASTEROID ) {
		// Dim it based on distance
		float depth = vm_vec_dist_quick( pos, &Eye_position );
		if ( depth > interp->depth_scale )	{
			return interp->depth_scale/depth;
			// If it is too far, exit
			if ( interp->light < (1.0f/32.0f) ) {
				return 0.0f;
			} else if ( interp->light > 1.0f )	{
				return 1.0f;
			}
		} else {
			return 1.0f;
		}
	} else {
		return 1.0f;
	}
}

void model_queue_render_set_thrust(interp_data *interp, int model_num, mst_info *mst)
{
	if (mst == NULL) {
		Int3();
		return;
	}

	interp->thrust_scale = mst->length.xyz.z;
	interp->thrust_scale_x = mst->length.xyz.x;
	interp->thrust_scale_y = mst->length.xyz.y;

	CLAMP(Interp_thrust_scale, 0.1f, 1.0f);

	interp->thrust_bitmap = mst->primary_bitmap;
	interp->thrust_glow_bitmap = mst->primary_glow_bitmap;
	interp->secondary_thrust_glow_bitmap = mst->secondary_glow_bitmap;
	interp->tertiary_thrust_glow_bitmap = mst->tertiary_glow_bitmap;
	interp->distortion_thrust_bitmap = mst->distortion_bitmap;

	interp->thrust_glow_noise = mst->glow_noise;
	interp->afterburner = mst->use_ab;

	if (mst->rotvel != NULL) {
		interp->thrust_rotvel = *(mst->rotvel);
	} else {
		vm_vec_zero(&interp->thrust_rotvel);
	}

	interp->thrust_glow_rad_factor = mst->glow_rad_factor;
	interp->secondary_thrust_glow_rad_factor = mst->secondary_glow_rad_factor;
	interp->tertiary_thrust_glow_rad_factor = mst->tertiary_glow_rad_factor;
	interp->thrust_glow_len_factor = mst->glow_length_factor;
	interp->distortion_thrust_rad_factor = mst->distortion_rad_factor;
	interp->distortion_thrust_length_factor = mst->distortion_length_factor;

	interp->draw_distortion = mst->draw_distortion;
}

void submodel_queue_render(interp_data *interp, DrawList *scene, int model_num, int submodel_num, matrix *orient, vec3d * pos, uint flags, int objnum, int *replacement_textures, int render)
{
	// replacement textures - Goober5000
	interp->new_replacement_textures = replacement_textures;

	polymodel * pm;

	MONITOR_INC( NumModelsRend, 1 );	

	if ( !( Game_detail_flags & DETAIL_FLAG_MODELS ) )	return;

	// Turn off engine effect
	interp->thrust_scale_subobj = 0;

	if ( !Model_texturing ) {
		flags |= MR_NO_TEXTURING;
	}

	interp->flags = flags;
	interp->pos = *pos;

	pm = model_get(model_num);

	// Set the flags we will pass to the tmapper
	interp->tmap_flags = TMAP_FLAG_GOURAUD | TMAP_FLAG_RGB;

	// if we're in nebula mode
	if( ( The_mission.flags & MISSION_FLAG_FULLNEB ) && ( Neb2_render_mode != NEB2_RENDER_NONE ) ) {
		interp->tmap_flags |= TMAP_FLAG_PIXEL_FOG;
	}

	if ( !( interp->flags & MR_NO_TEXTURING ) )	{
		interp->tmap_flags |= TMAP_FLAG_TEXTURED;

		if ( ( pm->flags & PM_FLAG_ALLOW_TILING ) && tiling )
			interp->tmap_flags |= TMAP_FLAG_TILED;

		if ( !( interp->flags & MR_NO_CORRECT ) )	{
			interp->tmap_flags |= TMAP_FLAG_CORRECT;
		}
	}

	if ( interp->flags & MR_ANIMATED_SHADER )
		interp->tmap_flags |= TMAP_ANIMATED_SHADER;

	bool is_outlines_only_htl = !Cmdline_nohtl && (flags & MR_NO_POLYS) && (flags & MR_SHOW_OUTLINE_HTL);

	//set to true since D3d and OGL need the api matrices set
	g3_start_instance_matrix(pos, orient, false);

	bool set_autocen = false;
	vec3d auto_back = ZERO_VECTOR;

	if ( interp->flags & MR_AUTOCENTER ) {
		// standard autocenter using data in model
		if ( pm->flags & PM_FLAG_AUTOCEN ) {
			auto_back = pm->autocenter;
			vm_vec_scale(&auto_back, -1.0f);
			set_autocen = true;
		} else if ( interp->flags & MR_IS_MISSILE ) {
			// fake autocenter if we are a missile and don't already have autocen info
			auto_back.xyz.x = -( (pm->submodel[pm->detail[interp->detail_level]].max.xyz.x + pm->submodel[pm->detail[interp->detail_level]].min.xyz.x) / 2.0f );
			auto_back.xyz.y = -( (pm->submodel[pm->detail[interp->detail_level]].max.xyz.y + pm->submodel[pm->detail[interp->detail_level]].min.xyz.y) / 2.0f );
			auto_back.xyz.z = -( (pm->submodel[pm->detail[interp->detail_level]].max.xyz.z + pm->submodel[pm->detail[interp->detail_level]].min.xyz.z) / 2.0f );
			set_autocen = true;
		}

		if ( set_autocen ) {
			g3_start_instance_matrix(&auto_back, NULL, false);
		}
	}

	if (is_outlines_only_htl) {
		scene->setFillMode(GR_FILL_MODE_WIRE);

		// lines shouldn't be rendered with textures or special RGB colors (assuming preset colors)
		interp->flags |= MR_NO_TEXTURING;
		interp->tmap_flags &= ~TMAP_FLAG_TEXTURED;
		interp->tmap_flags &= ~TMAP_FLAG_RGB;
		// don't render with lighting either
		interp->flags |= MR_NO_LIGHTING;
	} else {
		scene->setFillMode(GR_FILL_MODE_SOLID);
	}

	if ( !( interp->flags & MR_NO_LIGHTING ) ) {
		interp->light = 1.0f;

		scene->pushLightFilter(-1, pos, pm->submodel[submodel_num].rad);

		scene->setLighting(true);
	}

	interp->base_frametime = 0;

	if (objnum >= 0) {
		object *objp = &Objects[objnum];

		if (objp->type == OBJ_SHIP) {
			interp->base_frametime = Ships[objp->instance].base_texture_anim_frametime;
		}
	}

	// fixes disappearing HUD in OGL - taylor
	scene->setCullMode(1);

	if (!Cmdline_nohtl) {

		// RT - Put this here to fog debris
		if( interp->tmap_flags & TMAP_FLAG_PIXEL_FOG ) {
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

		model_queue_render_buffers(scene, interp, pm, submodel_num);
	} else {
		model_interp_sub( pm->submodel[submodel_num].bsp_data, pm, &pm->submodel[submodel_num], 0 );
	}

	if ( !( interp->flags & MR_NO_LIGHTING ) ) {
		gr_set_lighting(false, false);
		gr_reset_lighting();
	}

	scene->setFillMode(GR_FILL_MODE_SOLID);

	if ( pm->submodel[submodel_num].num_arcs )	{
		model_queue_render_lightning( scene, interp, pm, &pm->submodel[submodel_num] );
	}

	if ( interp->flags & MR_SHOW_PIVOTS ) {
		model_draw_debug_points( pm, &pm->submodel[submodel_num] );
	}

	if ( !( interp->flags & MR_NO_LIGHTING ) )	{
		scene->popLightFilter();
	}

	if (set_autocen) {
		g3_done_instance(false);
	}

	g3_done_instance(false);
}

void model_queue_render(interp_data *interp, DrawList *scene, int model_num, matrix *orient, vec3d *pos, uint flags, int objnum, int *replacement_textures)
{
	int cull = 0;
	// replacement textures - Goober5000
	model_set_replacement_textures(replacement_textures);

	polymodel *pm = model_get(model_num);

	model_do_dumb_rotation(model_num);

	if ( flags & MR_FORCE_CLAMP ) {
		scene->setTextureAddressing(TMAP_ADDRESS_CLAMP);
	} else {
		scene->setTextureAddressing(TMAP_ADDRESS_WRAP);
	}

	if ( !(flags & MR_NO_LIGHTING) ) {
		scene->pushLightFilter( objnum, pos, pm->rad );
	}

	interp->objnum = objnum;

	interp->light = model_queue_render_determine_light(interp, pos, flags);

	ship *shipp = NULL;
	object *objp = NULL;

	// just to be on the safe side
	Assert( interp->objnum == objnum );

	if ( objnum >= 0 ) {
		objp = &Objects[objnum];

		if (objp->type == OBJ_SHIP) {
			shipp = &Ships[objp->instance];

			if (shipp->flags2 & SF2_GLOWMAPS_DISABLED)
				flags |= MR_NO_GLOWMAPS;
		}
	}

	interp->orient = *orient;
	interp->pos = *pos;

	int tmp_detail_level = Game_detail_level;

	// Turn off engine effect
	interp->thrust_scale_subobj = 0;

	if ( !Model_texturing )
		flags |= MR_NO_TEXTURING;

	if ( !Model_polys )	{
		flags |= MR_NO_POLYS;
	}

	interp->flags = flags;

	// Set the flags we will pass to the tmapper
	interp->tmap_flags = TMAP_FLAG_GOURAUD | TMAP_FLAG_RGB;

	// if we're in nebula mode, fog everything except for the warp holes and other non-fogged models
	if((The_mission.flags & MISSION_FLAG_FULLNEB) && (Neb2_render_mode != NEB2_RENDER_NONE) && !(flags & MR_NO_FOGGING)){
		interp->tmap_flags |= TMAP_FLAG_PIXEL_FOG;
	}

	if ( !(interp->flags & MR_NO_TEXTURING) )	{
		interp->tmap_flags |= TMAP_FLAG_TEXTURED;

		if ( (pm->flags & PM_FLAG_ALLOW_TILING) && tiling)
			interp->tmap_flags |= TMAP_FLAG_TILED;

		if ( !(interp->flags & MR_NO_CORRECT) )	{
			interp->tmap_flags |= TMAP_FLAG_CORRECT;
		}
	}

	if ( interp->flags & MR_ANIMATED_SHADER )
		interp->tmap_flags |= TMAP_ANIMATED_SHADER;

	if ( interp->desaturate ) {
		interp->tmap_flags |= TMAP_FLAG_DESATURATE;
	}

	bool is_outlines_only = (flags & MR_NO_POLYS) && ((flags & MR_SHOW_OUTLINE_PRESET) || (flags & MR_SHOW_OUTLINE));
	bool is_outlines_only_htl = !Cmdline_nohtl && (flags & MR_NO_POLYS) && (flags & MR_SHOW_OUTLINE_HTL);
	bool use_api = !is_outlines_only_htl || (gr_screen.mode == GR_OPENGL);

	g3_start_instance_matrix(pos, orient, false);

	interp->detail_level = model_queue_render_determine_detail(objnum, model_num, orient, pos, flags, interp->detail_level_locked);

#ifndef NDEBUG
	if ( Interp_detail_level == 0 )	{
		MONITOR_INC( NumHiModelsRend, 1 );
	} else if ( Interp_detail_level == pm->n_detail_levels-1 ) {
		MONITOR_INC( NumLowModelsRend, 1 );
	}  else {
		MONITOR_INC( NumMedModelsRend, 1 );
	}
#endif

	// scale the render box settings based on the "Model Detail" slider
	switch ( Detail.detail_distance ) {
	case 0:		// 1st dot is 20%
		interp->box_scale = 0.2f;
		break;
	case 1:		// 2nd dot is 50%
		interp->box_scale = 0.5f;
		break;
	case 2:		// 3rd dot is 80%
		interp->box_scale = 0.8f;
		break;
	case 3:		// 4th dot is 100% (this is the default setting for "High" and "Very High" settings)
		interp->box_scale = 1.0f;
		break;
	case 4:		// 5th dot (max) is 120%
		interp->box_scale = 1.2f;
		break;
	}

	vec3d auto_back = ZERO_VECTOR;
	bool set_autocen = false;

	if ( interp->flags & MR_AUTOCENTER ) {
		// standard autocenter using data in model
		if (pm->flags & PM_FLAG_AUTOCEN) {
			auto_back = pm->autocenter;
			vm_vec_scale(&auto_back, -1.0f);
			set_autocen = true;
		} else if ( interp->flags & MR_IS_MISSILE ) {
			// fake autocenter if we are a missile and don't already have autocen info
			auto_back.xyz.x = -( (pm->submodel[pm->detail[interp->detail_level]].max.xyz.x + pm->submodel[pm->detail[interp->detail_level]].min.xyz.x) / 2.0f );
			auto_back.xyz.y = -( (pm->submodel[pm->detail[interp->detail_level]].max.xyz.y + pm->submodel[pm->detail[interp->detail_level]].min.xyz.y) / 2.0f );
			auto_back.xyz.z = -( (pm->submodel[pm->detail[interp->detail_level]].max.xyz.z + pm->submodel[pm->detail[interp->detail_level]].min.xyz.z) / 2.0f );
			set_autocen = true;
		}

		if ( set_autocen ) {
			g3_start_instance_matrix(&auto_back, NULL, false);
		}
	}

	gr_zbias(1);

	if ( interp->tmap_flags & TMAP_FLAG_PIXEL_FOG ) {
		float fog_near = 10.0f, fog_far = 1000.0f;
		neb2_get_adjusted_fog_values(&fog_near, &fog_far, objp);

		unsigned char r, g, b;
		neb2_get_fog_color(&r, &g, &b);

		scene->setFog(GR_FOGMODE_FOG, r, g, b, fog_near, fog_far);
	}

	if ( is_outlines_only_htl ) {
		scene->setFillMode(GR_FILL_MODE_WIRE);
		gr_set_color_fast( &interp->outline_color );
		// lines shouldn't be rendered with textures or special RGB colors (assuming preset colors)
		interp->flags |= MR_NO_TEXTURING;
		interp->tmap_flags &= ~TMAP_FLAG_TEXTURED;
		interp->tmap_flags &= ~TMAP_FLAG_RGB;
		// don't render with lighting either
		interp->flags |= MR_NO_LIGHTING;
	} else {
		scene->setFillMode(GR_FILL_MODE_SOLID);
	}

	int zbuf_mode;

	if ( (interp->flags & MR_NO_ZBUFFER) || (interp->flags & MR_ALL_XPARENT) ) {
		zbuf_mode = GR_ZBUFF_NONE;
	} else {
		zbuf_mode = GR_ZBUFF_FULL;
	}

	gr_zbuffer_set(zbuf_mode);

	if ( interp->flags & MR_EDGE_ALPHA ) {
		gr_center_alpha(-1);
	} else if ( interp->flags & MR_CENTER_ALPHA ) {
		gr_center_alpha(1);
	} else {
		gr_center_alpha(0);
	}

	if ( ( interp->flags & MR_NO_CULL ) || ( interp->flags & MR_ALL_XPARENT ) || ( interp->warp_bitmap >= 0 ) ) {
		scene->setCullMode(0);
	} else {
		scene->setCullMode(1);
	}

	// Goober5000
	interp->base_frametime = 0;

	if ( (objp != NULL) && (objp->type == OBJ_SHIP) ) {
		interp->base_frametime = Ships[objp->instance].base_texture_anim_frametime;
	}

	if ( !(interp->flags & MR_NO_LIGHTING) ) {
		scene->setLighting(true);
	}
	
	if (is_outlines_only_htl || (!Cmdline_nohtl && !is_outlines_only)) {
		scene->setBuffer(pm->vertex_buffer_id);
	}

	if ( in_shadow_map ) {
		scene->setZBias(-1024);
	} else {
		scene->setZBias(0);
	}

	// Draw the subobjects
	i = pm->submodel[pm->detail[interp->detail_level]].first_child;

	while( i >= 0 )	{
		if ( !pm->submodel[i].is_thruster ) {
			// When in htl mode render with htl method unless its a jump node
			if (is_outlines_only_htl || (!Cmdline_nohtl && !is_outlines_only)) {
				model_queue_render_children_buffers( scene, interp, pm, i, interp->detail_level );
			} else {
				model_interp_subcall( pm, i, interp->detail_level );
			}
		} else {
			draw_thrusters = true;
		}

		i = pm->submodel[i].next_sibling;
	}

	model_radius = pm->submodel[pm->detail[interp->detail_level]].rad;

	//*************************** draw the hull of the ship *********************************************

	// When in htl mode render with htl method unless its a jump node
	if (is_outlines_only_htl || (!Cmdline_nohtl && !is_outlines_only)) {
		model_queue_render_buffers(scene, interp, pm, pm->detail[interp->detail_level]);
	} else {
		model_interp_subcall(pm, pm->detail[interp->detail_level], Interp_detail_level);
	}

	// Draw the thruster subobjects
	if ( draw_thrusters ) {
		i = pm->submodel[pm->detail[interp->detail_level]].first_child;

		while( i >= 0 ) {
			if (pm->submodel[i].is_thruster) {
				// When in htl mode render with htl method unless its a jump node
				if (is_outlines_only_htl || (!Cmdline_nohtl && !is_outlines_only)) {
					model_queue_render_children_buffers( scene, interp, pm, i, interp->detail_level );
				} else {
					model_interp_subcall( pm, i, interp->detail_level );
				}
			}
			i = pm->submodel[i].next_sibling;
		}
	}

	if ( (interp->flags & MR_AUTOCENTER) && (set_autocen) ) {
		g3_done_instance();
	}

	if ( !(flags & MR_NO_LIGHTING) ) {
		scene->popLightFilter();
	}
}