/*
 * Copyright (C) Freespace Open 2013.  All rights reserved.
 *
 * All source code herein is the property of Freespace Open. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#include "globalincs/pstypes.h"
#include "object/object.h"

#ifndef _SHADOWS_H
#define _SHADOWS_H

struct light_frustum_info
{
	float proj_matrix[16];
	matrix view_matrix;
	vec3d view_position;

	vec3d min;
	vec3d max;
};

void shadows_construct_light_frustum(vec3d *min_out, vec3d *max_out, vec3d light_vec, matrix *orient, vec3d *pos, float fov, float aspect, float z_near, float z_far);
bool shadows_obj_in_frustum(object *objp, vec3d *min, vec3d *max, matrix *light_orient);
void shadows_render_all(float fov, matrix *eye_orient, vec3d *eye_pos);

matrix shadows_start_render(matrix *eye_orient, vec3d *eye_pos, float fov, float aspect, float veryneardist, float neardist, float middist, float fardist);
void shadows_end_render();

#endif