/*
 * Copyright (C) Freespace Open 2013.  All rights reserved.
 *
 * All source code herein is the property of Freespace Open. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#include "graphics/shadows.h"
#include "globalincs/pstypes.h"
#include "math/vecmat.h"
#include "object/object.h"
#include "lighting/lighting.h"
#include "graphics/gropengltnl.h"
#include "cmdline/cmdline.h"
#include "render/3d.h"
#include "model/model.h"
#include "model/modelrender.h"
#include "debris/debris.h"
#include "asteroid/asteroid.h"
#include "graphics/gropengldraw.h"

extern vec3d check_offsets[8];

light_frustum_info shadow_frustums[4];

bool shadows_obj_in_frustum(object *objp, matrix *light_orient, light_frustum_info *shadow_frustum)
{
	int i;
	vec3d pts[8], tmp, pt, pos_lightspace, pos_viewspace;
	vec3d obj_min = ZERO_VECTOR;
	vec3d obj_max = ZERO_VECTOR;
	vec3d &min = shadow_frustum->min;
	vec3d &max = shadow_frustum->max;

// 	vm_vec_sub(&tmp, &objp->pos, &View_position);
// 	vm_vec_unrotate(&pos_lightspace, &tmp, light_orient);

	vec3d pos, pos_rot;

	vm_vec_sub(&pos, &objp->pos, &Eye_position);
	vm_vec_rotate(&pos_rot, &pos, light_orient);

	if((pos_rot.xyz.x - objp->radius) > max.xyz.x || (pos_rot.xyz.x + objp->radius) < min.xyz.x || (pos_rot.xyz.y - objp->radius) > max.xyz.y || (pos_rot.xyz.y + objp->radius) < min.xyz.y || (pos_rot.xyz.z - objp->radius) > max.xyz.z)
		return false;

// 	for ( i = 0; i < 8; i++ ) {
// 		vm_vec_scale_add(&pt, &pos_lightspace, &check_offsets[i], objp->radius);
// 
// 		// first point, set it as max and min
// 		if ( i == 0 ) {
// 			obj_min = pt;
// 			obj_max = pt;
// 		} else {
// 			if ( pt.xyz.x < obj_min.xyz.x ) {
// 				obj_min.xyz.x = pt.xyz.x;
// 			}
// 			
// 			if ( pt.xyz.x > obj_max.xyz.x ) {
// 				obj_max.xyz.x = pt.xyz.x;
// 			}
// 
// 			if ( pt.xyz.y < obj_min.xyz.y ) {
// 				obj_min.xyz.y = pt.xyz.y;
// 			}
// 			
// 			if ( pt.xyz.y > obj_max.xyz.y ) {
// 				obj_max.xyz.y = pt.xyz.y;
// 			}
// 
// 			if ( pt.xyz.z < obj_min.xyz.z ) {
// 				obj_min.xyz.z = pt.xyz.z;
// 			}
// 			
// 			if ( pt.xyz.z > obj_max.xyz.z ) {
// 				obj_max.xyz.z = pt.xyz.z;
// 			}
// 		}
// 	}
// 
// 	// check to see if the AABB of the object in light space intersects the light frustum
// 	if ( obj_min.xyz.x > max.xyz.x || obj_max.xyz.x < min.xyz.x ) return false;
// 	if ( obj_min.xyz.y > max.xyz.y || obj_max.xyz.y < min.xyz.y ) return false;
// 	if ( obj_min.xyz.z > max.xyz.z || obj_max.xyz.z < min.xyz.z ) return false;

	return true;
}

void shadows_construct_light_proj(light_frustum_info *shadow_data)
{
	memset(shadow_data->proj_matrix, 0, sizeof(float) * 16);

// 	shadow_data->proj_matrix[0] = 2.0f / ( -1.0f - 1.0f );
// 	shadow_data->proj_matrix[5] = 2.0f / ( 1.0f - -1.0f );
// 	shadow_data->proj_matrix[10] = -2.0f / ( Max_draw_distance - Min_draw_distance );
// 	shadow_data->proj_matrix[12] = -(-1.0f + 1.0f) / ( -1.0f - 1.0f );
// 	shadow_data->proj_matrix[13] = -(1.0f + -1.0f) / ( 1.0f - -1.0f  );
// 	shadow_data->proj_matrix[14] = -(Max_draw_distance + Min_draw_distance) / ( Max_draw_distance - Min_draw_distance );
// 	shadow_data->proj_matrix[15] = 1.0f;

// 	shadow_data->proj_matrix[0] = 1.0f;
// 	shadow_data->proj_matrix[5] = 1.0f;
// 	shadow_data->proj_matrix[10] = 1.0f;
// 	shadow_data->proj_matrix[12] = 1.0f;
// 	shadow_data->proj_matrix[13] = 1.0f;
// 	shadow_data->proj_matrix[14] = 1.0f;
// 	shadow_data->proj_matrix[15] = 1.0f;

	shadow_data->proj_matrix[0] = 2.0f / ( shadow_data->max.xyz.x - shadow_data->min.xyz.x );
	shadow_data->proj_matrix[5] = 2.0f / ( shadow_data->max.xyz.y - shadow_data->min.xyz.y );
	shadow_data->proj_matrix[10] = -2.0f / ( shadow_data->max.xyz.z - shadow_data->min.xyz.z );
	shadow_data->proj_matrix[12] = -(shadow_data->max.xyz.x + shadow_data->min.xyz.x) / ( shadow_data->max.xyz.x - shadow_data->min.xyz.x );
	shadow_data->proj_matrix[13] = -(shadow_data->max.xyz.y + shadow_data->min.xyz.y) / ( shadow_data->max.xyz.y - shadow_data->min.xyz.y );
	shadow_data->proj_matrix[14] = -(shadow_data->max.xyz.z + shadow_data->min.xyz.z) / ( shadow_data->max.xyz.z - shadow_data->min.xyz.z );
	shadow_data->proj_matrix[15] = 1.0f;
}

void shadows_debug_show_frustum(matrix* orient, vec3d *pos, float fov, float aspect, float z_near, float z_far)
{
	// find the widths and heights of the near plane and far plane to determine the points of this frustum
	float near_height = (float)tan((double)fov * 0.5) * z_near;
	float near_width = near_height * aspect;

	float far_height = (float)tan((double)fov * 0.5) * z_far;
	float far_width = far_height * aspect;

	vec3d up_scale = ZERO_VECTOR;
	vec3d right_scale = ZERO_VECTOR;
	vec3d forward_scale_near = orient->vec.fvec;
	vec3d forward_scale_far = orient->vec.fvec;

	vm_vec_scale(&forward_scale_near, z_near);
	vm_vec_scale(&forward_scale_far, z_far);

	// find the eight points using eye orientation and position
	vec3d near_top_left = ZERO_VECTOR;
	vec3d near_top_right = ZERO_VECTOR;
	vec3d near_bottom_left = ZERO_VECTOR;
	vec3d near_bottom_right = ZERO_VECTOR;

	// near top left
	up_scale = orient->vec.uvec;
	vm_vec_scale(&up_scale, -near_height);

	right_scale = orient->vec.rvec;
	vm_vec_scale(&right_scale, -near_width);

	vm_vec_add(&near_top_left, &up_scale, &right_scale);
	vm_vec_add2(&near_top_left, &forward_scale_near);

	// near top right
	up_scale = orient->vec.uvec;
	vm_vec_scale(&up_scale, -near_height);

	right_scale = orient->vec.rvec;
	vm_vec_scale(&right_scale, near_width);

	vm_vec_add(&near_top_right, &up_scale, &right_scale);
	vm_vec_add2(&near_top_right, &forward_scale_near);

	// near bottom left
	up_scale = orient->vec.uvec;
	vm_vec_scale(&up_scale, near_height);

	right_scale = orient->vec.rvec;
	vm_vec_scale(&right_scale, -near_width);

	vm_vec_add(&near_bottom_left, &up_scale, &right_scale);
	vm_vec_add2(&near_bottom_left, &forward_scale_near);

	// near bottom right
	up_scale = orient->vec.uvec;
	vm_vec_scale(&up_scale, near_height);

	right_scale = orient->vec.rvec;
	vm_vec_scale(&right_scale, near_width);

	vm_vec_add(&near_bottom_right, &up_scale, &right_scale);
	vm_vec_add2(&near_bottom_right, &forward_scale_near);

	vec3d far_top_left = ZERO_VECTOR;
	vec3d far_top_right = ZERO_VECTOR;
	vec3d far_bottom_left = ZERO_VECTOR;
	vec3d far_bottom_right = ZERO_VECTOR;

	// far top left
	up_scale = orient->vec.uvec;
	vm_vec_scale(&up_scale, -far_height);

	right_scale = orient->vec.rvec;
	vm_vec_scale(&right_scale, -far_width);

	vm_vec_add(&far_top_left, &up_scale, &right_scale);
	vm_vec_add2(&far_top_left, &forward_scale_far);

	// far top right
	up_scale = orient->vec.uvec;
	vm_vec_scale(&up_scale, -far_height);

	right_scale = orient->vec.rvec;
	vm_vec_scale(&right_scale, far_width);

	vm_vec_add(&far_top_right, &up_scale, &right_scale);
	vm_vec_add2(&far_top_right, &forward_scale_far);

	// far bottom left
	up_scale = orient->vec.uvec;
	vm_vec_scale(&up_scale, far_height);

	right_scale = orient->vec.rvec;
	vm_vec_scale(&right_scale, -far_width);

	vm_vec_add(&far_bottom_left, &up_scale, &right_scale);
	vm_vec_add2(&far_bottom_left, &forward_scale_far);

	// far bottom right
	up_scale = orient->vec.uvec;
	vm_vec_scale(&up_scale, far_height);

	right_scale = orient->vec.rvec;
	vm_vec_scale(&right_scale, far_width);

	vm_vec_add(&far_bottom_right, &up_scale, &right_scale);
	vm_vec_add2(&far_bottom_right, &forward_scale_far);

	// translate frustum
	vm_vec_add2(&near_bottom_left, pos);
	vm_vec_add2(&near_bottom_right, pos);
	vm_vec_add2(&near_top_right, pos);
	vm_vec_add2(&near_top_left, pos);
	vm_vec_add2(&far_top_left, pos);
	vm_vec_add2(&far_top_right, pos);
	vm_vec_add2(&far_bottom_right, pos);
	vm_vec_add2(&far_bottom_left, pos);

	gr_set_color(0, 255, 255);
	g3_draw_htl_line(&near_bottom_left, &near_bottom_right);
	g3_draw_htl_line(&near_bottom_right, &near_top_right);
	g3_draw_htl_line(&near_bottom_right, &near_top_left);
	g3_draw_htl_line(&near_top_right, &near_top_left);
	g3_draw_htl_line(&far_top_left, &far_top_right);
	g3_draw_htl_line(&far_top_right, &far_bottom_right);
	g3_draw_htl_line(&far_bottom_right, &far_bottom_left);
	g3_draw_htl_line(&far_bottom_left, &far_top_left);

	vec3d frustum_pts[8];

	// bring frustum points into light space
// 	vm_vec_unrotate(&frustum_pts[0], &near_bottom_left, light_matrix);
// 	vm_vec_unrotate(&frustum_pts[1], &near_bottom_right, light_matrix);
// 	vm_vec_unrotate(&frustum_pts[2], &near_top_right, light_matrix);
// 	vm_vec_unrotate(&frustum_pts[3], &near_top_left, light_matrix);
// 	vm_vec_unrotate(&frustum_pts[4], &far_top_left, light_matrix);
// 	vm_vec_unrotate(&frustum_pts[5], &far_top_right, light_matrix);
// 	vm_vec_unrotate(&frustum_pts[6], &far_bottom_right, light_matrix);
// 	vm_vec_unrotate(&frustum_pts[7], &far_bottom_left, light_matrix);

	frustum_pts[0] = near_bottom_left;
	frustum_pts[1] = near_bottom_right;
	frustum_pts[2] = near_top_right;
	frustum_pts[3] = near_top_left;
	frustum_pts[4] = far_top_left;
	frustum_pts[5] = far_top_right;
	frustum_pts[6] = far_bottom_right;
	frustum_pts[7] = far_bottom_left;

	vec3d min = ZERO_VECTOR;
	vec3d max = ZERO_VECTOR;

	// find min and max of frustum points
	for ( int i = 0; i < 8; ++i ) {
		if ( i == 0 ) {
			min = frustum_pts[i];
			max = frustum_pts[i];
		} else {
			if ( frustum_pts[i].xyz.x < min.xyz.x ) {
				min.xyz.x = frustum_pts[i].xyz.x;
			} 

			if ( frustum_pts[i].xyz.x > max.xyz.x ) {
				max.xyz.x = frustum_pts[i].xyz.x;
			}

			if ( frustum_pts[i].xyz.y < min.xyz.y ) {
				min.xyz.y = frustum_pts[i].xyz.y;
			} 

			if ( frustum_pts[i].xyz.y > max.xyz.y ) {
				max.xyz.y = frustum_pts[i].xyz.y;
			}

			if ( frustum_pts[i].xyz.z < min.xyz.z ) {
				min.xyz.z = frustum_pts[i].xyz.z;
			} 

			if ( frustum_pts[i].xyz.z > max.xyz.z ) {
				max.xyz.z = frustum_pts[i].xyz.z;
			}
		}
	}
	
	vec3d pnt;
	vec3d pnt2;

	pnt.xyz.x = min.xyz.x;
	pnt.xyz.y = min.xyz.y;
	pnt.xyz.z = min.xyz.z;

	pnt2.xyz.x = max.xyz.x;
	pnt2.xyz.y = min.xyz.y;
	pnt2.xyz.z = min.xyz.z;

// 	g3_draw_htl_line(&pnt, &pnt2);
// 
// 	pnt.xyz.x = max.xyz.x;
// 	pnt.xyz.y = max.xyz.y;
// 	pnt.xyz.z = min.xyz.z;
// 
// 	g3_draw_htl_line(&pnt2, &pnt);
// 
// 	pnt2.xyz.x = min.xyz.x;
// 	pnt2.xyz.y = max.xyz.y;
// 	pnt2.xyz.z = min.xyz.z;
// 
// 	g3_draw_htl_line(&pnt, &pnt2);
// 
// 	pnt.xyz.x = max.xyz.x;
// 	pnt.xyz.y = max.xyz.y;
// 	pnt.xyz.z = max.xyz.z;
// 
// 	g3_draw_htl_line(&pnt, &pnt2);
// 
// 	pnt2.xyz.x = min.xyz.x;
// 	pnt2.xyz.y = max.xyz.y;
// 	pnt2.xyz.z = max.xyz.z;
// 
// 	g3_draw_htl_line(&pnt, &pnt2);
// 
// 	pnt.xyz.x = min.xyz.x;
// 	pnt.xyz.y = min.xyz.y;
// 	pnt.xyz.z = max.xyz.z;
// 
// 	g3_draw_htl_line(&pnt, &pnt2);
// 
// 	pnt2.xyz.x = max.xyz.x;
// 	pnt2.xyz.y = min.xyz.y;
// 	pnt2.xyz.z = max.xyz.z;
// 
// 	g3_draw_htl_line(&pnt, &pnt2);
// 
// 	pnt.xyz.x = max.xyz.x;
// 	pnt.xyz.y = max.xyz.y;
// 	pnt.xyz.z = max.xyz.z;
// 
// 	g3_draw_htl_line(&pnt, &pnt2);
// 
// 	pnt2.xyz.x = max.xyz.x;
// 	pnt2.xyz.y = min.xyz.y;
// 	pnt2.xyz.z = min.xyz.z;
// 
// 	g3_draw_htl_line(&pnt, &pnt2);
// 
// 	pnt2.xyz.x = max.xyz.x;
// 	pnt2.xyz.y = max.xyz.y;
// 	pnt2.xyz.z = min.xyz.z;
// 
// 	pnt.xyz.x = max.xyz.x;
// 	pnt.xyz.y = min.xyz.y;
// 	pnt.xyz.z = max.xyz.z;
// 
// 	g3_draw_htl_line(&pnt, &pnt2);
// 
// 	pnt2.xyz.x = max.xyz.x;
// 	pnt2.xyz.y = max.xyz.y;
// 	pnt2.xyz.z = max.xyz.z;
// 
// 	pnt.xyz.x = max.xyz.x;
// 	pnt.xyz.y = min.xyz.y;
// 	pnt.xyz.z = max.xyz.z;
// 
// 	g3_draw_htl_line(&pnt, &pnt2);
// 
// 	pnt2.xyz.x = max.xyz.x;
// 	pnt2.xyz.y = max.xyz.y;
// 	pnt2.xyz.z = max.xyz.z;
// 
// 	pnt.xyz.x = min.xyz.x;
// 	pnt.xyz.y = min.xyz.y;
// 	pnt.xyz.z = min.xyz.z;
// 
// 	g3_draw_htl_line(&pnt, &pnt2);
}

void shadows_construct_light_frustum(light_frustum_info *shadow_data, matrix *light_matrix, matrix *orient, vec3d *pos, float fov, float aspect, float z_near, float z_far)
{
	// find the widths and heights of the near plane and far plane to determine the points of this frustum
	float near_height = (float)tan((double)fov * 0.5) * z_near;
	float near_width = near_height * aspect;

	float far_height = (float)tan((double)fov * 0.5f) * z_far;
	float far_width = far_height * aspect;

	vec3d up_scale = ZERO_VECTOR;
	vec3d right_scale = ZERO_VECTOR;
	vec3d forward_scale_near = orient->vec.fvec;
	vec3d forward_scale_far = orient->vec.fvec;

	vm_vec_scale(&forward_scale_near, z_near);
	vm_vec_scale(&forward_scale_far, z_far);

	// find the eight points using eye orientation and position
	vec3d near_top_left = ZERO_VECTOR;
	vec3d near_top_right = ZERO_VECTOR;
	vec3d near_bottom_left = ZERO_VECTOR;
	vec3d near_bottom_right = ZERO_VECTOR;

	// near top left
	up_scale = orient->vec.uvec;
	vm_vec_scale(&up_scale, -near_height);

	right_scale = orient->vec.rvec;
	vm_vec_scale(&right_scale, -near_width);

	vm_vec_add(&near_top_left, &up_scale, &right_scale);
	vm_vec_add2(&near_top_left, &forward_scale_near);

	// near top right
	up_scale = orient->vec.uvec;
	vm_vec_scale(&up_scale, -near_height);

	right_scale = orient->vec.rvec;
	vm_vec_scale(&right_scale, near_width);

	vm_vec_add(&near_top_right, &up_scale, &right_scale);
	vm_vec_add2(&near_top_right, &forward_scale_near);

	// near bottom left
	up_scale = orient->vec.uvec;
	vm_vec_scale(&up_scale, near_height);

	right_scale = orient->vec.rvec;
	vm_vec_scale(&right_scale, -near_width);

	vm_vec_add(&near_bottom_left, &up_scale, &right_scale);
	vm_vec_add2(&near_bottom_left, &forward_scale_near);

	// near bottom right
	up_scale = orient->vec.uvec;
	vm_vec_scale(&up_scale, near_height);

	right_scale = orient->vec.rvec;
	vm_vec_scale(&right_scale, near_width);

	vm_vec_add(&near_bottom_right, &up_scale, &right_scale);
	vm_vec_add2(&near_bottom_right, &forward_scale_near);

	vec3d far_top_left = ZERO_VECTOR;
	vec3d far_top_right = ZERO_VECTOR;
	vec3d far_bottom_left = ZERO_VECTOR;
	vec3d far_bottom_right = ZERO_VECTOR;

	// far top left
	up_scale = orient->vec.uvec;
	vm_vec_scale(&up_scale, -far_height);

	right_scale = orient->vec.rvec;
	vm_vec_scale(&right_scale, -far_width);

	vm_vec_add(&far_top_left, &up_scale, &right_scale);
	vm_vec_add2(&far_top_left, &forward_scale_far);

	// far top right
	up_scale = orient->vec.uvec;
	vm_vec_scale(&up_scale, -far_height);

	right_scale = orient->vec.rvec;
	vm_vec_scale(&right_scale, far_width);

	vm_vec_add(&far_top_right, &up_scale, &right_scale);
	vm_vec_add2(&far_top_right, &forward_scale_far);

	// far bottom left
	up_scale = orient->vec.uvec;
	vm_vec_scale(&up_scale, far_height);

	right_scale = orient->vec.rvec;
	vm_vec_scale(&right_scale, -far_width);

	vm_vec_add(&far_bottom_left, &up_scale, &right_scale);
	vm_vec_add2(&far_bottom_left, &forward_scale_far);

	// far bottom right
	up_scale = orient->vec.uvec;
	vm_vec_scale(&up_scale, far_height);

	right_scale = orient->vec.rvec;
	vm_vec_scale(&right_scale, far_width);

	vm_vec_add(&far_bottom_right, &up_scale, &right_scale);
	vm_vec_add2(&far_bottom_right, &forward_scale_far);

	// translate frustum
// 	vm_vec_add2(&near_bottom_left, pos);
// 	vm_vec_add2(&near_bottom_right, pos);
// 	vm_vec_add2(&near_top_right, pos);
// 	vm_vec_add2(&near_top_left, pos);
// 	vm_vec_add2(&far_top_left, pos);
// 	vm_vec_add2(&far_top_right, pos);
// 	vm_vec_add2(&far_bottom_right, pos);
// 	vm_vec_add2(&far_bottom_left, pos);
	
	vec3d frustum_pts[8];

	// bring frustum points into light space
// 	vm_vec_unrotate(&frustum_pts[0], &near_bottom_left, light_matrix);
// 	vm_vec_unrotate(&frustum_pts[1], &near_bottom_right, light_matrix);
// 	vm_vec_unrotate(&frustum_pts[2], &near_top_right, light_matrix);
// 	vm_vec_unrotate(&frustum_pts[3], &near_top_left, light_matrix);
// 	vm_vec_unrotate(&frustum_pts[4], &far_top_left, light_matrix);
// 	vm_vec_unrotate(&frustum_pts[5], &far_top_right, light_matrix);
// 	vm_vec_unrotate(&frustum_pts[6], &far_bottom_right, light_matrix);
// 	vm_vec_unrotate(&frustum_pts[7], &far_bottom_left, light_matrix);

	vm_vec_rotate(&frustum_pts[0], &near_bottom_left, light_matrix);
	vm_vec_rotate(&frustum_pts[1], &near_bottom_right, light_matrix);
	vm_vec_rotate(&frustum_pts[2], &near_top_right, light_matrix);
	vm_vec_rotate(&frustum_pts[3], &near_top_left, light_matrix);
	vm_vec_rotate(&frustum_pts[4], &far_top_left, light_matrix);
	vm_vec_rotate(&frustum_pts[5], &far_top_right, light_matrix);
	vm_vec_rotate(&frustum_pts[6], &far_bottom_right, light_matrix);
	vm_vec_rotate(&frustum_pts[7], &far_bottom_left, light_matrix);

// 	frustum_pts[0] = near_bottom_left;
// 	frustum_pts[1] = near_bottom_right;
// 	frustum_pts[2] = near_top_right;
// 	frustum_pts[3] = near_top_left;
// 	frustum_pts[4] = far_top_left;
// 	frustum_pts[5] = far_top_right;
// 	frustum_pts[6] = far_bottom_right;
// 	frustum_pts[7] = far_bottom_left;

	vec3d min = ZERO_VECTOR;
	vec3d max = ZERO_VECTOR;

	// find min and max of frustum points
	for ( int i = 0; i < 8; ++i ) {
		if ( i == 0 ) {
			min = frustum_pts[i];
			max = frustum_pts[i];
		} else {
			if ( frustum_pts[i].xyz.x < min.xyz.x ) {
				min.xyz.x = frustum_pts[i].xyz.x;
			} 

			if ( frustum_pts[i].xyz.x > max.xyz.x ) {
				max.xyz.x = frustum_pts[i].xyz.x;
			}

			if ( frustum_pts[i].xyz.y < min.xyz.y ) {
				min.xyz.y = frustum_pts[i].xyz.y;
			} 

			if ( frustum_pts[i].xyz.y > max.xyz.y ) {
				max.xyz.y = frustum_pts[i].xyz.y;
			}

			if ( frustum_pts[i].xyz.z < min.xyz.z ) {
				min.xyz.z = frustum_pts[i].xyz.z;
			} 

			if ( frustum_pts[i].xyz.z > max.xyz.z ) {
				max.xyz.z = frustum_pts[i].xyz.z;
			}
		}
	}

// 	vec3d light_pos;
// 	
// 	light_pos.xyz.x = (max.xyz.x - min.xyz.x)*0.5f;
// 	light_pos.xyz.y = (max.xyz.y - min.xyz.y)*0.5f;
// 	light_pos.xyz.z = min.xyz.z;
// 
// 	vm_vec_rotate(&shadow_data->view_position, &light_pos, light_matrix);
// 
// 	min.xyz.x = min.xyz.x - light_pos.xyz.x;
// 	max.xyz.x = max.xyz.x - light_pos.xyz.x;
// 
// 	min.xyz.y = min.xyz.y - light_pos.xyz.y;
// 	min.xyz.y = max.xyz.y - light_pos.xyz.y;
// 
// 	min.xyz.z = min.xyz.z - light_pos.xyz.z;
// 	max.xyz.z = max.xyz.z - light_pos.xyz.z;

// 	min.xyz.x = -1.0f;
// 	min.xyz.y = -1.0f;
// 	min.xyz.z = Min_draw_distance;
// 
// 	max.xyz.x = 1.0f;
// 	max.xyz.y = 1.0f;
// 	max.xyz.z = Max_draw_distance;

	shadow_data->min = min;
	shadow_data->max = max;

	shadows_construct_light_proj(shadow_data);
}

matrix shadows_start_render(matrix *eye_orient, vec3d *eye_pos, float fov, float aspect, float veryneardist, float neardist, float middist, float fardist)
{
	extern bool Glowpoint_override_save;
	extern bool Glowpoint_override;

	if(Static_light.empty())
		return vmd_identity_matrix; 
	
	light *lp = *(Static_light.begin());

	if ( lp == NULL ) {
		return vmd_identity_matrix;
	}

	vec3d light_dir;
	matrix light_matrix;

	vm_vec_copy_normalize(&light_dir, &lp->vec);
	vm_vector_2_matrix(&light_matrix, &light_dir, &eye_orient->vec.uvec, NULL);

	shadow_veryneardist = veryneardist;
	shadow_neardist = neardist;
	shadow_middist = middist;
	shadow_fardist = fardist;

	shadows_construct_light_frustum(&shadow_frustums[0], &light_matrix, eye_orient, eye_pos, fov, aspect, Min_draw_distance, veryneardist);

	shadows_construct_light_frustum(&shadow_frustums[1], &light_matrix, eye_orient, eye_pos, fov, aspect, veryneardist, neardist);

	shadows_construct_light_frustum(&shadow_frustums[2], &light_matrix, eye_orient, eye_pos, fov, aspect, neardist, middist);

	shadows_construct_light_frustum(&shadow_frustums[3], &light_matrix, eye_orient, eye_pos, fov, aspect, middist, fardist);
	
	gr_opengl_shadow_map_start(&light_matrix, &shadow_frustums[0], &shadow_frustums[1], &shadow_frustums[2], &shadow_frustums[3]);

	return light_matrix;
}

void shadows_end_render()
{
	gr_opengl_end_shadow_map();
}

void shadows_render_all(float fov, matrix *eye_orient, vec3d *eye_pos)
{
	if ( Static_light.empty() ) {
		return;
	}

	light *lp = *(Static_light.begin());

	if( Cmdline_nohtl || !Cmdline_shadow_quality || !lp ) {
		return;
	}

	//shadows_debug_show_frustum(&Player_obj->orient, &Player_obj->pos, fov, gr_screen.clip_aspect, Min_draw_distance, 3000.0f);

	gr_end_proj_matrix();
	gr_end_view_matrix();

	matrix light_matrix = shadows_start_render(eye_orient, eye_pos, fov, gr_screen.clip_aspect, 200.0f, 500.0f, 2000.0f, 10000.0f);

	DrawList scene;
	object *objp = Objects;

	for ( int i = 0; i <= Highest_object_index; i++, objp++ ) {
		bool cull = true;

		for ( int j = 0; j < 4; ++j ) {
			if ( shadows_obj_in_frustum(objp, &light_matrix, &shadow_frustums[j]) ) {
				cull = false;
				break;
			}
		}

		if ( cull ) {
			continue;
		}

		switch(objp->type)
		{
		case OBJ_SHIP:
			{
				obj_queue_render(objp, &scene);
			}
			break;
		case OBJ_ASTEROID:
			{
				interp_data interp;

				model_clear_instance( Asteroid_info[Asteroids[objp->instance].asteroid_type].model_num[Asteroids[objp->instance].asteroid_subtype]);
				model_queue_render(&interp, &scene, Asteroid_info[Asteroids[objp->instance].asteroid_type].model_num[Asteroids[objp->instance].asteroid_subtype], -1, &objp->orient, &objp->pos, MR_NORMAL | MR_IS_ASTEROID | MR_NO_TEXTURING | MR_NO_LIGHTING, OBJ_INDEX(objp), NULL );
			}
			break;

		case OBJ_DEBRIS:
			{
				debris *db;
				db = &Debris[objp->instance];

				if ( !(db->flags & DEBRIS_USED)){
					continue;
				}

				interp_data interp;

				objp = &Objects[db->objnum];
				submodel_queue_render(&interp, &scene, db->model_num, db->submodel_num, &objp->orient, &objp->pos, MR_NO_TEXTURING | MR_NO_LIGHTING, -1, NULL);
			}
			break; 
		}
	}

	scene.sortDraws();
	scene.renderAll();

	shadows_end_render();

	gr_zbias(0);
	gr_zbuffer_set(ZBUFFER_TYPE_READ);
	gr_set_cull(0);

	gr_flush_data_states();
	gr_set_buffer(-1);

	gr_set_proj_matrix(Proj_fov, gr_screen.clip_aspect, Min_draw_distance, Max_draw_distance);
	gr_set_view_matrix(&Eye_position, &Eye_matrix);
}