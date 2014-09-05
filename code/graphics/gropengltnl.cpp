/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/


#ifdef _WIN32
#include <windows.h>
#endif

#include "globalincs/pstypes.h"
#include "globalincs/def_files.h"
#include "globalincs/alphacolors.h"
#include "globalincs/systemvars.h"

#include "graphics/2d.h"
#include "lighting/lighting.h"
#include "graphics/grinternal.h"
#include "graphics/gropengl.h"
#include "graphics/gropenglextension.h"
#include "graphics/gropengltexture.h"
#include "graphics/gropengllight.h"
#include "graphics/gropengltnl.h"
#include "graphics/gropengldraw.h"
#include "graphics/gropenglshader.h"
#include "graphics/gropenglstate.h"

#include "math/vecmat.h"
#include "render/3d.h"
#include "cmdline/cmdline.h"
#include "model/model.h"
#include "weapon/trails.h"
#include "graphics/shadows.h"

extern int GLOWMAP;
extern int CLOAKMAP;
extern int SPECMAP;
extern int NORMMAP;
extern int MISCMAP;
extern int HEIGHTMAP;
extern int G3_user_clip;
extern vec3d G3_user_clip_normal;
extern vec3d G3_user_clip_point;
extern int Interp_multitex_cloakmap;
extern int Interp_cloakmap_alpha;

extern bool Basemap_override;
extern bool Envmap_override;
extern bool Specmap_override;
extern bool Normalmap_override;
extern bool Heightmap_override;
extern bool GLSL_override;
extern bool Shadow_override;

const float uniform_handler::EPSILON = FLT_EPSILON;
uniform_handler Current_uniforms;

static int GL_modelview_matrix_depth = 1;
static int GL_htl_projection_matrix_set = 0;
static int GL_htl_view_matrix_set = 0;
static int GL_htl_2d_matrix_depth = 0;
static int GL_htl_2d_matrix_set = 0;

static GLfloat GL_env_texture_matrix[16] = { 0.0f };
static bool GL_env_texture_matrix_set = false;

static GLdouble eyex, eyey, eyez;
static GLdouble vmatrix[16];

static vec3d last_view_pos;
static matrix last_view_orient;

vec3d shadow_ref_point;

static bool use_last_view = false;
static GLfloat lmatrix[16];
static GLfloat lprojmatrix[4][16];
static GLfloat modelmatrix[16];

int GL_vertex_data_in = 0;

GLint GL_max_elements_vertices = 4096;
GLint GL_max_elements_indices = 4096;

float GL_thrust_scale = -1.0f;
team_color* Current_team_color;
team_color Current_temp_color;
bool Using_Team_Color = false;

bool GL_use_transform_buffer = false;
int GL_transform_buffer_offset = -1;

GLuint Shadow_map_texture = 0;
GLuint Shadow_map_depth_texture = 0;
GLuint shadow_fbo = 0;
vec3d saved_Eye;
float shadow_veryneardist = 0.0f, shadow_neardist = 0.0f, shadow_middist = 0.0f, shadow_fardist = 0.0f;
bool shadowers = false;
GLint saved_fb = 0;
bool in_shadow_map = false;
int parabolic = 0;

int Transform_buffer_handle = -1;

struct opengl_buffer_object {
	GLuint buffer_id;
	GLenum type;
	GLenum usage;
	uint size;

	GLuint texture;	// for texture buffer objects
};

struct opengl_vertex_buffer {
	GLfloat *array_list;	// interleaved array
	GLubyte *index_list;

	GLuint vbo;			// buffer for VBO
	GLuint ibo;

	uint vbo_size;
	uint ibo_size;

	opengl_vertex_buffer() :
		array_list(NULL), index_list(NULL), vbo(0), ibo(0),
		vbo_size(0), ibo_size(0)
	{
	}

	void clear();
};

void opengl_vertex_buffer::clear()
{
	if (array_list) {
		vm_free(array_list);
	}

	if (vbo) {
		vglDeleteBuffersARB(1, &vbo);
		GL_vertex_data_in -= vbo_size;
	}

	if (index_list) {
		vm_free(index_list);
	}

	if (ibo) {
		vglDeleteBuffersARB(1, &ibo);
		GL_vertex_data_in -= ibo_size;
	}
}

static SCP_vector<opengl_buffer_object> GL_buffer_objects;
static SCP_vector<opengl_vertex_buffer> GL_vertex_buffers;
static opengl_vertex_buffer *g_vbp = NULL;
static int GL_vertex_buffers_in_use = 0;

int opengl_create_buffer_object(GLenum type, GLenum usage)
{
	opengl_buffer_object buffer_obj;

	buffer_obj.usage = usage;
	buffer_obj.type = type;
	buffer_obj.size = 0;

	vglGenBuffersARB(1, &buffer_obj.buffer_id);

	GL_buffer_objects.push_back(buffer_obj);

	return GL_buffer_objects.size() - 1;
}

void gr_opengl_update_buffer_object(int handle, uint size, void* data)
{
	Assert(handle >= 0);
	Assert((size_t)handle < GL_buffer_objects.size());

	opengl_buffer_object &buffer_obj = GL_buffer_objects[handle];

	switch ( buffer_obj.type ) {
	case GL_ARRAY_BUFFER_ARB:
		GL_state.Array.BindArrayBuffer(buffer_obj.buffer_id);
		break;
	case GL_ELEMENT_ARRAY_BUFFER_ARB:
		GL_state.Array.BindElementBuffer(buffer_obj.buffer_id);
		break;
	case GL_TEXTURE_BUFFER_ARB:
		GL_state.Array.BindTextureBuffer(buffer_obj.buffer_id);
		break;
	case GL_UNIFORM_BUFFER:
		GL_state.Array.BindUniformBuffer(buffer_obj.buffer_id);
	default:
		Int3();
		return;
		break;
	}

	GL_vertex_data_in -= buffer_obj.size;
	buffer_obj.size = size;
	GL_vertex_data_in += buffer_obj.size;

	vglBufferDataARB(buffer_obj.type, size, data, buffer_obj.usage);

	if ( opengl_check_for_errors() ) {
		//Int3();
	}
}

void opengl_delete_buffer_object(int handle)
{
	Assert(handle >= 0);
	Assert((size_t)handle < GL_buffer_objects.size());

	opengl_buffer_object &buffer_obj = GL_buffer_objects[handle];

	if ( buffer_obj.type == GL_TEXTURE_BUFFER_ARB ) {
		glDeleteTextures(1, &buffer_obj.texture);
	}

	GL_vertex_data_in -= buffer_obj.size;

	vglDeleteBuffersARB(1, &buffer_obj.buffer_id);
}

int gr_opengl_create_stream_buffer_object()
{
	return opengl_create_buffer_object(GL_ARRAY_BUFFER_ARB, GL_STREAM_DRAW_ARB);
}

int opengl_create_texture_buffer_object()
{
	if ( Use_GLSL < 3 || !Is_Extension_Enabled(OGL_ARB_TEXTURE_BUFFER) || !Is_Extension_Enabled(OGL_ARB_FLOATING_POINT_TEXTURES) ) {
		return -1;
	}

	// create the buffer
	int buffer_object_handle = opengl_create_buffer_object(GL_TEXTURE_BUFFER_ARB, GL_DYNAMIC_DRAW_ARB);

	opengl_check_for_errors();

	opengl_buffer_object &buffer_obj = GL_buffer_objects[buffer_object_handle];

	// create the texture
	glGenTextures(1, &buffer_obj.texture);
	glBindTexture(GL_TEXTURE_BUFFER_ARB, buffer_obj.texture);

	opengl_check_for_errors();

	gr_opengl_update_buffer_object(buffer_object_handle, 100, NULL);

	vglTexBufferARB(GL_TEXTURE_BUFFER_ARB, GL_RGBA32F, buffer_obj.buffer_id);

	opengl_check_for_errors();

	return buffer_object_handle;
}

void gr_opengl_update_transform_buffer(void* data, uint size)
{
	if ( Transform_buffer_handle < 0 ) {
		return;
	}

	gr_opengl_update_buffer_object(Transform_buffer_handle, size, data);
}

GLuint opengl_get_transform_buffer_texture()
{
	if ( Transform_buffer_handle < 0 ) {
		return 0;
	}

	return GL_buffer_objects[Transform_buffer_handle].texture;
}

void gr_opengl_set_transform_buffer_offset(int offset)
{
	GL_transform_buffer_offset = offset;
}

static void opengl_gen_buffer(opengl_vertex_buffer *vbp)
{
	if ( !Use_VBOs ) {
		return;
	}

	if ( !vbp ) {
		return;
	}

	if ( !(vbp->vbo_size && vbp->ibo_size) ) {
		return;
	}

	// create vertex buffer
	{
		// clear any existing errors
		glGetError();

		vglGenBuffersARB(1, &vbp->vbo);

		// make sure we have one
		if (vbp->vbo) {
			GL_state.Array.BindArrayBuffer(vbp->vbo);
			vglBufferDataARB(GL_ARRAY_BUFFER_ARB, vbp->vbo_size, vbp->array_list, GL_STATIC_DRAW_ARB);

			// just in case
			if ( opengl_check_for_errors() ) {
				vglDeleteBuffersARB(1, &vbp->vbo);
				vbp->vbo = 0;
				return;
			}

			GL_state.Array.BindArrayBuffer(0);

			vm_free(vbp->array_list);
			vbp->array_list = NULL;	

			GL_vertex_data_in += vbp->vbo_size;
		}
	}

	// create index buffer
	{
		// clear any existing errors
		glGetError();

		vglGenBuffersARB(1, &vbp->ibo);

		// make sure we have one
		if (vbp->ibo) {
			GL_state.Array.BindElementBuffer(vbp->ibo);
			vglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, vbp->ibo_size, vbp->index_list, GL_STATIC_DRAW_ARB);

			// just in case
			if ( opengl_check_for_errors() ) {
				vglDeleteBuffersARB(1, &vbp->ibo);
				vbp->ibo = 0;
				return;
			}

			GL_state.Array.BindElementBuffer(0);

			vm_free(vbp->index_list);
			vbp->index_list = NULL;

			GL_vertex_data_in += vbp->ibo_size;
		}
	}
}

int gr_opengl_create_buffer()
{
	if (Cmdline_nohtl) {
		return -1;
	}

	opengl_vertex_buffer vbuffer;

	GL_vertex_buffers.push_back( vbuffer );
	GL_vertex_buffers_in_use++;

	return (int)(GL_vertex_buffers.size() - 1);
}

bool gr_opengl_config_buffer(const int buffer_id, vertex_buffer *vb, bool update_ibuffer_only)
{
	if (Cmdline_nohtl) {
		return false;
	}

	if (buffer_id < 0) {
		return false;
	}

	Assert( buffer_id < (int)GL_vertex_buffers.size() );

	if (vb == NULL) {
		return false;
	}

	if ( !(vb->flags & VB_FLAG_POSITION) ) {
		Int3();
		return false;
	}

	opengl_vertex_buffer *m_vbp = &GL_vertex_buffers[buffer_id];


	vb->stride = 0;

	// position
	Verify( update_ibuffer_only || vb->model_list->vert != NULL );

	vb->stride += (3 * sizeof(GLfloat));

	// normals
	if (vb->flags & VB_FLAG_NORMAL) {
		Verify( update_ibuffer_only || vb->model_list->norm != NULL );

		vb->stride += (3 * sizeof(GLfloat));
	}

	// uv coords
	if (vb->flags & VB_FLAG_UV1) {
		vb->stride += (2 * sizeof(GLfloat));
	}

	// tangent space data for normal maps (shaders only)
	if (vb->flags & VB_FLAG_TANGENT) {
		Assert( Cmdline_normal );

		Verify( update_ibuffer_only || vb->model_list->tsb != NULL );
		vb->stride += (4 * sizeof(GLfloat));
	}

	if (vb->flags & VB_FLAG_MODEL_ID) {
		Assert( Use_GLSL >= 3 );

		Verify( update_ibuffer_only || vb->model_list->submodels != NULL );
		vb->stride += (1 * sizeof(GLfloat));
	}

	// offsets for this chunk
	if ( update_ibuffer_only ) {
		vb->vertex_offset = 0;
	} else {
		vb->vertex_offset = m_vbp->vbo_size;
		m_vbp->vbo_size += vb->stride * vb->model_list->n_verts;
	}

	for (size_t idx = 0; idx < vb->tex_buf.size(); idx++) {
		buffer_data *bd = &vb->tex_buf[idx];

		bd->index_offset = m_vbp->ibo_size;
		m_vbp->ibo_size += bd->n_verts * ((bd->flags & VB_FLAG_LARGE_INDEX) ? sizeof(uint) : sizeof(ushort));
	}

	return true;
}

bool gr_opengl_pack_buffer(const int buffer_id, vertex_buffer *vb)
{
	if (Cmdline_nohtl) {
		return false;
	}

	if (buffer_id < 0) {
		return false;
	}

	Assert( buffer_id < (int)GL_vertex_buffers.size() );

	opengl_vertex_buffer *m_vbp = &GL_vertex_buffers[buffer_id];

	// NULL means that we are done with the buffer and can create the IBO/VBO
	// returns false here only for some minor error prevention
	if (vb == NULL) {
		opengl_gen_buffer(m_vbp);
		return false;
	}

	int i, n_verts = 0;
	size_t j;
	uint arsize = 0;


	if (m_vbp->array_list == NULL) {
		m_vbp->array_list = (GLfloat*)vm_malloc_q(m_vbp->vbo_size);

		// return invalid if we don't have the memory
		if (m_vbp->array_list == NULL) {
			return false;
		}

		memset(m_vbp->array_list, 0, m_vbp->vbo_size);
	}

	if (m_vbp->index_list == NULL) {
		m_vbp->index_list = (GLubyte*)vm_malloc_q(m_vbp->ibo_size);

		// return invalid if we don't have the memory
		if (m_vbp->index_list == NULL) {
			return false;
		}

		memset(m_vbp->index_list, 0, m_vbp->ibo_size);
	}

	// bump to our index in the array
	GLfloat *array = m_vbp->array_list + (vb->vertex_offset / sizeof(GLfloat));

	// generate the vertex array
	n_verts = vb->model_list->n_verts;
	for (i = 0; i < n_verts; i++) {
		vertex *vl = &vb->model_list->vert[i];

		// don't try to generate more data than what's available
		Assert( ((arsize * sizeof(GLfloat)) + vb->stride) <= (m_vbp->vbo_size - vb->vertex_offset) );

		// NOTE: UV->NORM->TSB->VERT, This array order *must* be preserved!!

		// tex coords
		if (vb->flags & VB_FLAG_UV1) {
			array[arsize++] = vl->texture_position.u;
			array[arsize++] = vl->texture_position.v;
		}

		// normals
		if (vb->flags & VB_FLAG_NORMAL) {
			vec3d *nl = &vb->model_list->norm[i];
			array[arsize++] = nl->xyz.x;
			array[arsize++] = nl->xyz.y;
			array[arsize++] = nl->xyz.z;
		}

		// tangent space data
		if (vb->flags & VB_FLAG_TANGENT) {
			tsb_t *tsb = &vb->model_list->tsb[i];
			array[arsize++] = tsb->tangent.xyz.x;
			array[arsize++] = tsb->tangent.xyz.y;
			array[arsize++] = tsb->tangent.xyz.z;
			array[arsize++] = tsb->scaler;
		}

		if (vb->flags & VB_FLAG_MODEL_ID) {
			array[arsize++] = (float)vb->model_list->submodels[i];
		}

		// verts
		array[arsize++] = vl->world.xyz.x;
		array[arsize++] = vl->world.xyz.y;
		array[arsize++] = vl->world.xyz.z;
	}

	// generate the index array
	for (j = 0; j < vb->tex_buf.size(); j++) {
		buffer_data* tex_buf = &vb->tex_buf[j];
		n_verts = tex_buf->n_verts;
		uint offset = tex_buf->index_offset;
		const uint *index = tex_buf->get_index();

		// bump to our spot in the buffer
		GLubyte *ibuf = m_vbp->index_list + offset;

		if (vb->tex_buf[j].flags & VB_FLAG_LARGE_INDEX) {
			memcpy(ibuf, index, n_verts * sizeof(uint));
		} else {
			ushort *mybuf = (ushort*)ibuf;

			for (i = 0; i < n_verts; i++) {
				mybuf[i] = (ushort)index[i];
			}
		}
	}
	
	return true;
}

void gr_opengl_set_buffer(int idx)
{
	if (Cmdline_nohtl) {
		return;
	}

	g_vbp = NULL;

	if (idx < 0) {
		if (Use_VBOs) {
			GL_state.Array.BindArrayBuffer(0);
			GL_state.Array.BindElementBuffer(0);
		}

		if ( (Use_GLSL > 1) && !GLSL_override ) {
			opengl_shader_set_current();
		}

		return;
	}

	Assert( idx < (int)GL_vertex_buffers.size() );

	g_vbp = &GL_vertex_buffers[idx];
}

void gr_opengl_destroy_buffer(int idx)
{
	if (Cmdline_nohtl) {
		return;
	}

	if (idx < 0) {
		return;
	}

	Assert( idx < (int)GL_vertex_buffers.size() );

	opengl_vertex_buffer *vbp = &GL_vertex_buffers[idx];

	vbp->clear();

	// we try to take advantage of the fact that there shouldn't be a lot of buffer
	// deletions/additions going on all of the time, so a model_unload_all() and/or
	// game_level_close() should pretty much keep everything cleared out on a
	// regular basis
	if (--GL_vertex_buffers_in_use <= 0) {
		GL_vertex_buffers.clear();
	}

	g_vbp = NULL;
}

void opengl_destroy_all_buffers()
{
	for (uint i = 0; i < GL_vertex_buffers.size(); i++) {
		gr_opengl_destroy_buffer(i);
	}

	for ( uint i = 0; i < GL_buffer_objects.size(); i++ ) {
		opengl_delete_buffer_object(i);
	}

	GL_vertex_buffers.clear();
	GL_vertex_buffers_in_use = 0;
}

void opengl_tnl_init()
{
	GL_vertex_buffers.reserve(MAX_POLYGON_MODELS);
	gr_opengl_deferred_light_sphere_init(16, 16);
	gr_opengl_deferred_light_cylinder_init(16);

	Transform_buffer_handle = opengl_create_texture_buffer_object();

	if ( Transform_buffer_handle >= 0 && !Cmdline_no_batching ) {
		GL_use_transform_buffer = true;
	}

	if(Cmdline_shadow_quality)
	{
		//Setup shadow map framebuffer
		vglGenFramebuffersEXT(1, &shadow_fbo);
		vglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shadow_fbo);

		glGenTextures(1, &Shadow_map_depth_texture);

		GL_state.Texture.SetActiveUnit(0);
		GL_state.Texture.SetTarget(GL_TEXTURE_2D_ARRAY_EXT);
//		GL_state.Texture.SetTarget(GL_TEXTURE_2D);
		GL_state.Texture.Enable(Shadow_map_depth_texture);
		
		glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

// 		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
// 		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
// 		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
// 		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
// 		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		//glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_REF_DEPTH_TO_TEXTURE_EXT);
		//glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);
		//glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_DEPTH_TEXTURE_MODE_ARB, GL_INTENSITY);
		int size = (Cmdline_shadow_quality == 2 ? 1024 : 512);
		vglTexImage3D(GL_TEXTURE_2D_ARRAY_EXT, 0, GL_DEPTH_COMPONENT32, size, size, 4, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		//glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

		vglFramebufferTextureEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, Shadow_map_depth_texture, 0);
		//vglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, Shadow_map_depth_texture, 0);

		glGenTextures(1, &Shadow_map_texture);

		GL_state.Texture.SetActiveUnit(0);
		GL_state.Texture.SetTarget(GL_TEXTURE_2D_ARRAY_EXT);
		//GL_state.Texture.SetTarget(GL_TEXTURE_2D);
		GL_state.Texture.Enable(Shadow_map_texture);

		glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
// 		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
// 		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
// 		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
// 		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
// 		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		vglTexImage3D(GL_TEXTURE_2D_ARRAY_EXT, 0, GL_RGB32F_ARB, size, size, 4, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
		//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, size, size, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);

		vglFramebufferTextureEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, Shadow_map_texture, 0);
		//vglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, Shadow_map_texture, 0);

		vglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

		bool rval = true;

		if ( opengl_check_for_errors("post_init_framebuffer()") ) {
			rval = false;
		}
	}
}

void opengl_tnl_shutdown()
{
	if ( Shadow_map_depth_texture ) {
		glDeleteTextures(1, &Shadow_map_depth_texture);
		Shadow_map_depth_texture = 0;
	}

	if ( Shadow_map_texture ) {
		glDeleteTextures(1, &Shadow_map_texture);
		Shadow_map_texture = 0;
	}

	opengl_destroy_all_buffers();
}

void mix_two_team_colors(team_color* dest, team_color* a, team_color* b, float mix_factor) {
	dest->base.r = a->base.r * (1.0f - mix_factor) + b->base.r * mix_factor;
	dest->base.g = a->base.g * (1.0f - mix_factor) + b->base.g * mix_factor;
	dest->base.b = a->base.b * (1.0f - mix_factor) + b->base.b * mix_factor;

	dest->stripe.r = a->stripe.r * (1.0f - mix_factor) + b->stripe.r * mix_factor;
	dest->stripe.g = a->stripe.g * (1.0f - mix_factor) + b->stripe.g * mix_factor;
	dest->stripe.b = a->stripe.b * (1.0f - mix_factor) + b->stripe.b * mix_factor;
}

void gr_opengl_set_team_color(team_color *colors)
{
	if ( colors == NULL ) {
		Using_Team_Color = false;
	} else {
		Using_Team_Color = true;
		Current_temp_color = *colors;
		Current_team_color = &Current_temp_color;
	}
}

void gr_opengl_disable_team_color() {
	Using_Team_Color = false;
}

void gr_opengl_set_thrust_scale(float scale)
{
	GL_thrust_scale = scale;
}

static void opengl_init_arrays(opengl_vertex_buffer *vbp, const vertex_buffer *bufferp)
{
	GLint offset = (GLint)bufferp->vertex_offset;
	GLubyte *ptr = NULL;

	if ( Is_Extension_Enabled(OGL_ARB_DRAW_ELEMENTS_BASE_VERTEX) ) {
		offset = 0;
	}

	// vertex buffer

	if (vbp->vbo) {
		GL_state.Array.BindArrayBuffer(vbp->vbo);
	} else {
		ptr = (GLubyte*)vbp->array_list;
	}

	if (bufferp->flags & VB_FLAG_UV1) {
		GL_state.Array.SetActiveClientUnit(0);
		GL_state.Array.EnableClientTexture();
		GL_state.Array.TexPointer(2, GL_FLOAT, bufferp->stride, ptr + offset);
		offset += (2 * sizeof(GLfloat));
	}

	if (bufferp->flags & VB_FLAG_NORMAL) {
		GL_state.Array.EnableClientNormal();
		GL_state.Array.NormalPointer(GL_FLOAT, bufferp->stride, ptr + offset);
		offset += (3 * sizeof(GLfloat));
	}

	if (bufferp->flags & VB_FLAG_TANGENT) {
		// we treat this as texture coords for ease of use
		// NOTE: this is forced on tex unit 1!!!
		GL_state.Array.SetActiveClientUnit(1);
		GL_state.Array.EnableClientTexture();
		GL_state.Array.TexPointer(4, GL_FLOAT, bufferp->stride, ptr + offset);
		offset += (4 * sizeof(GLfloat));
	}

	if (bufferp->flags & VB_FLAG_MODEL_ID) {
		int attrib_index = opengl_shader_get_attribute("attrib_model_id");
		if ( attrib_index >= 0 ) {
			GL_state.Array.EnableVertexAttrib(attrib_index);
			GL_state.Array.VertexAttribPointer(attrib_index, 1, GL_FLOAT, GL_FALSE, bufferp->stride, ptr + offset);
		}
		offset += (1 * sizeof(GLfloat));
	}

	Assert( bufferp->flags & VB_FLAG_POSITION );
	GL_state.Array.EnableClientVertex();
	GL_state.Array.VertexPointer(3, GL_FLOAT, bufferp->stride, ptr + offset);
	offset += (3 * sizeof(GLfloat));
}

#define DO_RENDER()	\
	if (Cmdline_drawelements) \
		glDrawElements(GL_TRIANGLES, count, element_type, ibuffer + (datap->index_offset + start)); \
	else \
		vglDrawRangeElements(GL_TRIANGLES, datap->i_first, datap->i_last, count, element_type, ibuffer + (datap->index_offset + start));

unsigned int GL_last_shader_flags = 0;
int GL_last_shader_index = -1;

static void opengl_render_pipeline_fixed(int start, const vertex_buffer *bufferp, const buffer_data *datap, int flags);

extern bool Scene_framebuffer_in_frame;
extern GLuint Framebuffer_fallback_texture_id;
extern int Interp_transform_texture;
extern matrix Object_matrix;
extern vec3d Object_position;
extern int Interp_thrust_scale_subobj;
extern float Interp_thrust_scale;
static void opengl_render_pipeline_program(int start, const vertex_buffer *bufferp, const buffer_data *datap, int flags)
{
	float u_scale, v_scale;
	int render_pass = 0;
	unsigned int shader_flags = 0;
	int sdr_index = -1;
	int r, g, b, a, tmap_type;
	GLubyte *ibuffer = NULL;

	int end = (datap->n_verts - 1);
	int count = (end - (start*3) + 1);

	GLenum element_type = (datap->flags & VB_FLAG_LARGE_INDEX) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

	opengl_vertex_buffer *vbp = g_vbp;
	Assert( vbp );

	int textured = ((flags & TMAP_FLAG_TEXTURED) && (bufferp->flags & VB_FLAG_UV1));

	// setup shader flags for the things that we want/need
	shader_flags = gr_determine_shader_flags(
		lighting_is_enabled, 
		GL_state.Fog(), 
		textured, 
		in_shadow_map,
		GL_thrust_scale > 0.0f,
		(flags & TMAP_FLAG_BATCH_TRANSFORMS) && (GL_transform_buffer_offset >= 0) && (bufferp->flags & VB_FLAG_MODEL_ID),
		Using_Team_Color, 
		flags, 
		SPECMAP, 
		GLOWMAP, 
		NORMMAP, 
		HEIGHTMAP, 
		ENVMAP, 
		MISCMAP
	);

	// find proper shader
	if (shader_flags == GL_last_shader_flags) {
		sdr_index = GL_last_shader_index;
	} else {
		sdr_index = gr_opengl_maybe_create_shader(shader_flags);

		if (sdr_index < 0) {
			opengl_render_pipeline_fixed(start, bufferp, datap, flags);
			return;
		}

		GL_last_shader_index = sdr_index;
		GL_last_shader_flags = shader_flags;
	}

	Assert( sdr_index >= 0 );

	opengl_shader_set_current( &GL_shader[sdr_index] );

	opengl_default_light_settings( !GL_center_alpha, (GL_light_factor > 0.25f) );
	gr_opengl_set_center_alpha(GL_center_alpha);

	opengl_setup_render_states(r, g, b, a, tmap_type, flags);
	GL_state.Color( (ubyte)r, (ubyte)g, (ubyte)b, (ubyte)a );


	render_pass = 0;

	GL_state.Texture.SetShaderMode(GL_TRUE);


	// basic setup of all data
	opengl_init_arrays(vbp, bufferp);

	if (vbp->ibo) {
		GL_state.Array.BindElementBuffer(vbp->ibo);
	} else {
		ibuffer = (GLubyte*)vbp->index_list;
	}

	opengl_tnl_set_material(flags, shader_flags, tmap_type);
	
	if(in_shadow_map) {
		vglDrawElementsInstancedBaseVertex(GL_TRIANGLES, count, element_type, ibuffer + (datap->index_offset + start), 4, (GLint)bufferp->vertex_offset/bufferp->stride);
		//vglDrawRangeElementsBaseVertex(GL_TRIANGLES, datap->i_first, datap->i_last, count, element_type, ibuffer + (datap->index_offset + start), (GLint)bufferp->vertex_offset/bufferp->stride);
	} else {
		if ( Is_Extension_Enabled(OGL_ARB_DRAW_ELEMENTS_BASE_VERTEX) ) {
			if (Cmdline_drawelements) {
				vglDrawElementsBaseVertex(GL_TRIANGLES, count, element_type, ibuffer + (datap->index_offset + start), (GLint)bufferp->vertex_offset/bufferp->stride);
			} else {
				vglDrawRangeElementsBaseVertex(GL_TRIANGLES, datap->i_first, datap->i_last, count, element_type, ibuffer + (datap->index_offset + start), (GLint)bufferp->vertex_offset/bufferp->stride);
			}
		} else {
			if (Cmdline_drawelements) {
				glDrawElements(GL_TRIANGLES, count, element_type, ibuffer + (datap->index_offset + start)); 
			} else {
				vglDrawRangeElements(GL_TRIANGLES, datap->i_first, datap->i_last, count, element_type, ibuffer + (datap->index_offset + start));
			}
		}
	}
/*
	int n_light_passes = (MIN(Num_active_gl_lights, GL_max_lights) - 1) / 3;

	if (lighting_is_enabled && n_light_passes > 0) {
		shader_flags = SDR_FLAG_LIGHT;

		if (textured) {
			if ( !Basemap_override ) {
				shader_flags |= SDR_FLAG_DIFFUSE_MAP;
			}

			if ( (SPECMAP > 0) && !Specmap_override ) {
				shader_flags |= SDR_FLAG_SPEC_MAP;
			}
		}

		opengl::shader_manager::get()->apply_main_shader(shader_flags);
		sdr = opengl::shader_manager::get()->get_main_shader();

		if (sdr) {
			GL_state.BlendFunc(GL_ONE, GL_ONE);

			int zbuf = gr_zbuffer_set(GR_ZBUFF_READ);

			static const float GL_light_zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			glMaterialfv( GL_FRONT, GL_AMBIENT, GL_light_zero );
			glMaterialfv( GL_FRONT, GL_EMISSION, GL_light_zero );

			GL_state.DepthMask(GL_FALSE);
			GL_state.DepthFunc(GL_LEQUAL);

			for (int i = 0; i < n_light_passes; i++) {
				int offset = 3 * (i+1) - 1;
				opengl_change_active_lights(0, offset);

				n_lights = MIN(Num_active_gl_lights, GL_max_lights) - offset;
				sdr->set_uniform(opengl::main_shader::n_lights, n_lights);

				DO_RENDER();
			}

			gr_zbuffer_set(zbuf);
		}
	}
*/
/*
	if (Num_active_gl_lights > 4) {
		opengl_change_active_lights(0, 4);

		int n_lights = MIN(Num_active_gl_lights, GL_max_lights) - 5;
		vglUniform1iARB( opengl_shader_get_uniform("n_lights"), n_lights );

		opengl_default_light_settings(0, 0, 0);

		opengl_shader_set_current( &GL_shader[2] );

		GL_state.SetAlphaBlendMode(ALPHA_BLEND_ADDITIVE);

		GL_state.DepthMask(GL_FALSE);
		GL_state.DepthFunc(GL_LEQUAL);

		DO_RENDER();
	}
*/
	GL_state.Texture.SetShaderMode(GL_FALSE);
}

static void opengl_render_pipeline_fixed(int start, const vertex_buffer *bufferp, const buffer_data *datap, int flags)
{
	float u_scale, v_scale;
	int render_pass = 0;
	int r, g, b, a, tmap_type;
	GLubyte *ibuffer = NULL;
	GLubyte *vbuffer = NULL;

	bool rendered_env = false;
	bool using_glow = false;
	bool using_spec = false;
	bool using_env = false;

	int end = (datap->n_verts - 1);
	int count = (end - start + 1);

	GLenum element_type = (datap->flags & VB_FLAG_LARGE_INDEX) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

	opengl_vertex_buffer *vbp = g_vbp;
	Assert( vbp );

	int textured = ((flags & TMAP_FLAG_TEXTURED) && (bufferp->flags & VB_FLAG_UV1));

	if (textured ) {
		if ( Cmdline_glow && (GLOWMAP > 0) ) {
			using_glow = true;
		}

		if (lighting_is_enabled) {
			GL_state.Normalize(GL_TRUE);

			if ( !GL_state.Fog() && (SPECMAP > 0) && !Specmap_override ) {
				using_spec = true;

				if ( (ENVMAP > 0) && !Envmap_override ) {
					using_env = true;
				}
			}
		}
	}

	render_pass = 0;

	opengl_default_light_settings( !GL_center_alpha, (GL_light_factor > 0.25f), (using_spec) ? 0 : 1 );
	gr_opengl_set_center_alpha(GL_center_alpha);

	opengl_setup_render_states(r, g, b, a, tmap_type, flags);
	GL_state.Color( (ubyte)r, (ubyte)g, (ubyte)b, (ubyte)a );

	// basic setup of all data
	opengl_init_arrays(vbp, bufferp);

	if (vbp->ibo) {
		GL_state.Array.BindElementBuffer(vbp->ibo);
	} else {
		ibuffer = (GLubyte*)vbp->index_list;
	}

	if ( !vbp->vbo ) {
		vbuffer = (GLubyte*)vbp->array_list;
	}

	#define BUFFER_OFFSET(off) (vbuffer+bufferp->vertex_offset+(off))

// -------- Begin 1st PASS (base texture, glow) ---------------------------------- //
	if (textured) {
		render_pass = 0;

		// base texture
		if ( !Basemap_override ) {
			if ( !Is_Extension_Enabled(OGL_ARB_DRAW_ELEMENTS_BASE_VERTEX) ) {
				GL_state.Array.SetActiveClientUnit(render_pass);
				GL_state.Array.EnableClientTexture();
				GL_state.Array.TexPointer( 2, GL_FLOAT, bufferp->stride, BUFFER_OFFSET(0) );
			}

			gr_opengl_tcache_set(gr_screen.current_bitmap, tmap_type, &u_scale, &v_scale, render_pass);

			// increment texture count for this pass
			render_pass++; // bump!
		}

		// glowmaps!
		if (using_glow) {
			GL_state.Array.SetActiveClientUnit(render_pass);
			GL_state.Array.EnableClientTexture();
			if ( Is_Extension_Enabled(OGL_ARB_DRAW_ELEMENTS_BASE_VERTEX) ) {
				GL_state.Array.TexPointer( 2, GL_FLOAT, bufferp->stride, 0 );
			} else {
				GL_state.Array.TexPointer( 2, GL_FLOAT, bufferp->stride, BUFFER_OFFSET(0) );
			}

			// set glowmap on relevant ARB
			gr_opengl_tcache_set(GLOWMAP, tmap_type, &u_scale, &v_scale, render_pass);

			opengl_set_additive_tex_env();

			render_pass++; // bump!
		}
	}

	// DRAW IT!!
	if ( Is_Extension_Enabled(OGL_ARB_DRAW_ELEMENTS_BASE_VERTEX) ) {
		if (Cmdline_drawelements) {
			vglDrawElementsBaseVertex(GL_TRIANGLES, count, element_type, ibuffer + (datap->index_offset + start), (GLint)bufferp->vertex_offset/bufferp->stride);
		} else {
			vglDrawRangeElementsBaseVertex(GL_TRIANGLES, datap->i_first, datap->i_last, count, element_type, ibuffer + (datap->index_offset + start), (GLint)bufferp->vertex_offset/bufferp->stride);
		}
	} else {
		if (Cmdline_drawelements) {
			glDrawElements(GL_TRIANGLES, count, element_type, ibuffer + (datap->index_offset + start)); 
		} else {
			vglDrawRangeElements(GL_TRIANGLES, datap->i_first, datap->i_last, count, element_type, ibuffer + (datap->index_offset + start));
		}
	}

// -------- End 2nd PASS --------------------------------------------------------- //


// -------- Begin 2nd pass (additional lighting) --------------------------------- //
/*	if ( (textured) && (lighting_is_enabled) && !(GL_state.Fog()) && (Num_active_gl_lights > GL_max_lights) ) {
		// the lighting code needs to do this better, may need some adjustment later since I'm only trying
		// to avoid rendering 7+ extra passes for lights which probably won't affect current object, but as
		// a performance hack I guess this will have to do for now...
		// restrict the number of extra lighting passes based on LOD:
		//  - LOD0:  only 2 extra passes (3 main passes total, rendering 24 light sources)
		//  - LOD1:  only 1 extra pass   (2 main passes total, rendering 16 light sources)
		//  - LOD2+: no extra passes     (1 main pass   total, rendering  8 light sources)
		extern int Interp_detail_level;
		int max_passes = (2 - Interp_detail_level);

		if (max_passes > 0) {
			int max_lights = (Num_active_gl_lights - 1) / GL_max_lights;

			if (max_lights > 0) {
				int i;

				opengl_set_state( TEXTURE_SOURCE_DECAL, ALPHA_BLEND_ALPHA_ADDITIVE, ZBUFFER_TYPE_READ );

				for (i = 1; i < render_pass; i++) {
					opengl_switch_arb(i, 0);
				}

				for (i = 1; (i < max_lights) && (i < max_passes); i++) {
					opengl_change_active_lights(i);

					// DRAW IT!!
					DO_RENDER();
				}

				// reset the active lights to the first set to render the spec related passes with
				// for performance and quality reasons they don't get special lighting passes
				opengl_change_active_lights(0);
			}
		}
	}*/
// -------- End 2nd PASS --------------------------------------------------------- //


// -------- Begin 3rd PASS (environment map) ------------------------------------- //
	if (using_env) {
		// turn all previously used arbs off before the specular pass
		// this fixes the glowmap multitexture rendering problem - taylor
		GL_state.Texture.DisableAll();

		render_pass = 0;

		// set specmap, for us to modulate against
		if ( !Is_Extension_Enabled(OGL_ARB_DRAW_ELEMENTS_BASE_VERTEX) ) {
			GL_state.Array.SetActiveClientUnit(render_pass);
			GL_state.Array.EnableClientTexture();
			GL_state.Array.TexPointer(2, GL_FLOAT, bufferp->stride, BUFFER_OFFSET(0) );
		}

		// set specmap on relevant ARB
		gr_opengl_tcache_set(SPECMAP, tmap_type, &u_scale, &v_scale, render_pass);

		GL_state.DepthMask(GL_TRUE);
		GL_state.DepthFunc(GL_LEQUAL);

		// as a crazy and sometimes useless hack, avoid using alpha when specmap has none
		if ( bm_has_alpha_channel(SPECMAP) ) {
			GL_state.Texture.SetEnvCombineMode(GL_COMBINE_RGB, GL_MODULATE);
			glTexEnvf( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE );
			glTexEnvf( GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_ALPHA );
			glTexEnvf( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB );
			glTexEnvf( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR );
			GL_state.Texture.SetRGBScale(1.0f);
			GL_state.Texture.SetAlphaScale(1.0f);
		} else {
			GL_state.Texture.SetEnvCombineMode(GL_COMBINE_RGB, GL_MODULATE);
			glTexEnvf( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE );
			glTexEnvf( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB );
			glTexEnvf( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR );
			glTexEnvf( GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR );
			GL_state.Texture.SetRGBScale(1.0f);
		}

		render_pass++; // bump!

		// now move the to the envmap
		if ( !Is_Extension_Enabled(OGL_ARB_DRAW_ELEMENTS_BASE_VERTEX) ) {
			GL_state.Array.SetActiveClientUnit(render_pass);
			GL_state.Array.EnableClientTexture();
			GL_state.Array.TexPointer(2, GL_FLOAT, bufferp->stride, BUFFER_OFFSET(0) );
		}

		gr_opengl_tcache_set(ENVMAP, TCACHE_TYPE_CUBEMAP, &u_scale, &v_scale, render_pass);

		GL_state.Texture.SetEnvCombineMode(GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvf(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
		glTexEnvf(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
		glTexEnvf(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
		glTexEnvf(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);

		GL_state.Texture.SetRGBScale(2.0f);

		GL_state.SetAlphaBlendMode(ALPHA_BLEND_ADDITIVE);

		GL_state.Texture.SetWrapS(GL_CLAMP_TO_EDGE);
		GL_state.Texture.SetWrapT(GL_CLAMP_TO_EDGE);
		GL_state.Texture.SetWrapR(GL_CLAMP_TO_EDGE);

		GL_state.Texture.SetTexgenModeS(GL_REFLECTION_MAP);
		GL_state.Texture.SetTexgenModeT(GL_REFLECTION_MAP);
		GL_state.Texture.SetTexgenModeR(GL_REFLECTION_MAP);

		GL_state.Texture.TexgenS(GL_TRUE);
		GL_state.Texture.TexgenT(GL_TRUE);
		GL_state.Texture.TexgenR(GL_TRUE);

		// set the matrix for the texture mode
		if (GL_env_texture_matrix_set) {
			glMatrixMode(GL_TEXTURE);
			glPushMatrix();
			glLoadMatrixf(GL_env_texture_matrix);
			// switch back to the default modelview mode
			glMatrixMode(GL_MODELVIEW);
		}

		render_pass++; // bump!

		GLfloat ambient_save[4];
		glGetMaterialfv( GL_FRONT, GL_AMBIENT, ambient_save );

		GLfloat ambient[4] = { 0.47f, 0.47f, 0.47f, 1.0f };
		glMaterialfv( GL_FRONT, GL_AMBIENT, ambient );

		// DRAW IT!!
		if ( Is_Extension_Enabled(OGL_ARB_DRAW_ELEMENTS_BASE_VERTEX) ) {
			if (Cmdline_drawelements) {
				vglDrawElementsBaseVertex(GL_TRIANGLES, count, element_type, ibuffer + (datap->index_offset + start), (GLint)bufferp->vertex_offset/bufferp->stride);
			} else {
				vglDrawRangeElementsBaseVertex(GL_TRIANGLES, datap->i_first, datap->i_last, count, element_type, ibuffer + (datap->index_offset + start), (GLint)bufferp->vertex_offset/bufferp->stride);
			}
		} else {
			if (Cmdline_drawelements) {
				glDrawElements(GL_TRIANGLES, count, element_type, ibuffer + (datap->index_offset + start)); 
			} else {
				vglDrawRangeElements(GL_TRIANGLES, datap->i_first, datap->i_last, count, element_type, ibuffer + (datap->index_offset + start));
			}
		}

		// disable and reset everything we changed
		GL_state.Texture.SetRGBScale(1.0f);

		// reset original ambient light value
		glMaterialfv( GL_FRONT, GL_AMBIENT, ambient_save );

		// pop off the texture matrix we used for the envmap
		if (GL_env_texture_matrix_set) {
			glMatrixMode(GL_TEXTURE);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
		}

		GL_state.Texture.TexgenS(GL_FALSE);
		GL_state.Texture.TexgenT(GL_FALSE);
		GL_state.Texture.TexgenR(GL_FALSE);

		opengl_set_texture_target();

		GL_state.Texture.SetActiveUnit(0);
		glTexEnvf(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);

		rendered_env = true;
	}
// -------- End 3rd PASS --------------------------------------------------------- //


// -------- Begin 4th PASS (specular/shine map) ---------------------------------- //
	if (using_spec) {
		// turn all previously used arbs off before the specular pass
		// this fixes the glowmap multitexture rendering problem - taylor
		GL_state.Texture.DisableAll();
		GL_state.Array.SetActiveClientUnit(1);
		GL_state.Array.DisableClientTexture();

		render_pass = 0;

		if ( !Is_Extension_Enabled(OGL_ARB_DRAW_ELEMENTS_BASE_VERTEX) ) {
			GL_state.Array.SetActiveClientUnit(0);
			GL_state.Array.EnableClientTexture();
			GL_state.Array.TexPointer( 2, GL_FLOAT, bufferp->stride, BUFFER_OFFSET(0) );
		}

		gr_opengl_tcache_set(SPECMAP, tmap_type, &u_scale, &v_scale, render_pass);

		// render with spec lighting only
		opengl_default_light_settings(0, 0, 1);

		GL_state.Texture.SetEnvCombineMode(GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvf(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
		glTexEnvf(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
		glTexEnvf(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
		glTexEnvf(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);

		GL_state.Texture.SetRGBScale( (rendered_env) ? 2.0f : 4.0f );

		GL_state.SetAlphaBlendMode(ALPHA_BLEND_ADDITIVE);

		GL_state.DepthMask(GL_TRUE);
		GL_state.DepthFunc(GL_LEQUAL);

		// DRAW IT!!
		if ( Is_Extension_Enabled(OGL_ARB_DRAW_ELEMENTS_BASE_VERTEX) ) {
			if (Cmdline_drawelements) {
				vglDrawElementsBaseVertex(GL_TRIANGLES, count, element_type, ibuffer + (datap->index_offset + start), (GLint)bufferp->vertex_offset/bufferp->stride);
			} else {
				vglDrawRangeElementsBaseVertex(GL_TRIANGLES, datap->i_first, datap->i_last, count, element_type, ibuffer + (datap->index_offset + start), (GLint)bufferp->vertex_offset/bufferp->stride);
			}
		} else {
			if (Cmdline_drawelements) {
				glDrawElements(GL_TRIANGLES, count, element_type, ibuffer + (datap->index_offset + start)); 
			} else {
				vglDrawRangeElements(GL_TRIANGLES, datap->i_first, datap->i_last, count, element_type, ibuffer + (datap->index_offset + start));
			}
		}

		opengl_default_light_settings();

		GL_state.Texture.SetRGBScale(1.0f);
	}
// -------- End 4th PASS --------------------------------------------------------- //

	// make sure everthing gets turned back off
	GL_state.Texture.DisableAll();
	GL_state.Normalize(GL_FALSE);
	GL_state.Array.SetActiveClientUnit(1);
	glTexEnvf(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvf(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
	glTexEnvf(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvf(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
	GL_state.Array.DisableClientTexture();
	GL_state.Array.SetActiveClientUnit(0);
	glTexEnvf(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvf(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
	glTexEnvf(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvf(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
	GL_state.Array.DisableClientTexture();
	GL_state.Array.DisableClientVertex();
	GL_state.Array.DisableClientNormal();
}

// start is the first part of the buffer to render, n_prim is the number of primitives, index_list is an index buffer, if index_list == NULL render non-indexed
void gr_opengl_render_buffer(int start, const vertex_buffer *bufferp, int texi, int flags)
{
	Assert( GL_htl_projection_matrix_set );
	Assert( GL_htl_view_matrix_set );

	Verify( bufferp != NULL );

	GL_CHECK_FOR_ERRORS("start of render_buffer()");

	if ( GL_state.CullFace() ) {
		GL_state.FrontFaceValue(GL_CW);
	}

	Assert( texi >= 0 );

	const buffer_data *datap = &bufferp->tex_buf[texi];

	if ( (Use_GLSL > 1) && !GLSL_override ) {
		opengl_render_pipeline_program(start, bufferp, datap, flags);
	} else {
		opengl_render_pipeline_fixed(start, bufferp, datap, flags);
	}

	GL_CHECK_FOR_ERRORS("end of render_buffer()");
}

int Stream_buffer_sdr = -1;
GLboolean Stream_cull;
GLboolean Stream_lighting;
int Stream_zbuff_mode;
void gr_opengl_render_stream_buffer_start(int buffer_id)
{
	Assert( buffer_id >= 0 );
	Assert( buffer_id < (int)GL_vertex_buffers.size() );

	GL_state.Array.BindArrayBuffer(GL_vertex_buffers[buffer_id].vbo);

	Stream_cull = GL_state.CullFace(GL_FALSE);
	Stream_lighting = GL_state.Lighting(GL_FALSE);
	Stream_zbuff_mode = gr_zbuffer_set(GR_ZBUFF_READ);

	opengl_shader_set_current();
	Stream_buffer_sdr = -1;
}

void gr_opengl_render_stream_buffer_end()
{
	GL_state.Array.BindArrayBuffer(0);

	gr_opengl_flush_data_states();

	opengl_shader_set_current();
	Stream_buffer_sdr = -1;

	GL_state.Texture.DisableAll();

	GL_state.CullFace(Stream_cull);
	GL_state.Lighting(Stream_lighting);
	gr_zbuffer_set(Stream_zbuff_mode);
}

extern GLuint Scene_depth_texture;
extern GLuint Scene_position_texture;
extern GLuint Distortion_texture[2];
extern int Distortion_switch;
void gr_opengl_render_stream_buffer(int offset, int n_verts, int flags)
{
	int alpha, tmap_type, r, g, b;
	float u_scale = 1.0f, v_scale = 1.0f;
	GLenum gl_mode = GL_TRIANGLE_FAN;
	int attrib_index = -1;
	int zbuff = ZBUFFER_TYPE_DEFAULT;
	GL_CHECK_FOR_ERRORS("start of render3d()");

	int stride = 0;

	GLubyte *ptr = NULL;
	int vert_offset = 0;

	int pos_offset = -1;
	int tex_offset = -1;
	int radius_offset = -1;
	int color_offset = -1;
	int up_offset = -1;
	int fvec_offset = -1;
	int alpha_offset = -1;

	// this shit is pretty ugly. i'll make a cleaner render function once i know this works (Swifty)
	if ( flags & TMAP_FLAG_VERTEX_GEN && flags & TMAP_FLAG_LINESTRIP ) {
		stride = sizeof(trail_shader_info);

		pos_offset = vert_offset;
		vert_offset += sizeof(vec3d);

		fvec_offset = vert_offset;
		vert_offset += sizeof(vec3d);

		alpha_offset = vert_offset;
		vert_offset += sizeof(float);

		radius_offset = vert_offset;
		vert_offset += sizeof(float);

		tex_offset = vert_offset;
		vert_offset += sizeof(uv_pair);
	} else if ( flags & TMAP_FLAG_VERTEX_GEN ) {	
		stride = sizeof(particle_pnt);

		pos_offset = vert_offset;
		vert_offset += sizeof(vec3d);

		radius_offset = vert_offset;
		vert_offset += sizeof(float);

		up_offset = vert_offset;
		//tex_offset = vert_offset;
		vert_offset += sizeof(vec3d);
	} else {
		stride = sizeof(effect_vertex);

		pos_offset = vert_offset;
		vert_offset += sizeof(vec3d);

		tex_offset = vert_offset;
		vert_offset += sizeof(uv_pair);

		radius_offset = vert_offset;
		vert_offset += sizeof(float);

		color_offset = vert_offset;
		vert_offset += sizeof(ubyte)*4;
	}

	opengl_setup_render_states(r, g, b, alpha, tmap_type, flags);

	if ( flags & TMAP_FLAG_TEXTURED ) {
		if ( flags & TMAP_FLAG_SOFT_QUAD ) {
			int sdr_index;

			if( (flags & TMAP_FLAG_DISTORTION) || (flags & TMAP_FLAG_DISTORTION_THRUSTER) ) {
				sdr_index = opengl_shader_get_effect_shader(SDR_EFFECT_DISTORTION);

				if ( sdr_index != Stream_buffer_sdr ) {
					glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);

					opengl_shader_set_current(&GL_effect_shaders[sdr_index]);
					Stream_buffer_sdr = sdr_index;

					vglUniform1iARB(opengl_shader_get_uniform("baseMap"), 0);
					vglUniform1iARB(opengl_shader_get_uniform("depthMap"), 1);
					vglUniform1fARB(opengl_shader_get_uniform("window_width"), (float)gr_screen.max_w);
					vglUniform1fARB(opengl_shader_get_uniform("window_height"), (float)gr_screen.max_h);
					vglUniform1fARB(opengl_shader_get_uniform("nearZ"), Min_draw_distance);
					vglUniform1fARB(opengl_shader_get_uniform("farZ"), Max_draw_distance);

					vglUniform1iARB(opengl_shader_get_uniform("frameBuffer"), 2);

					if ( radius_offset >= 0 ) {
						attrib_index = opengl_shader_get_attribute("offset_in");
						GL_state.Array.EnableVertexAttrib(attrib_index);
						GL_state.Array.VertexAttribPointer(attrib_index, 1, GL_FLOAT, GL_FALSE, stride, ptr + radius_offset);
					}
				}

				GL_state.Texture.SetActiveUnit(2);
				GL_state.Texture.SetTarget(GL_TEXTURE_2D);
				GL_state.Texture.Enable(Scene_effect_texture);
				
				if(flags & TMAP_FLAG_DISTORTION_THRUSTER) {
					vglUniform1iARB(opengl_shader_get_uniform("distMap"), 3);

					GL_state.Texture.SetActiveUnit(3);
					GL_state.Texture.SetTarget(GL_TEXTURE_2D);
					GL_state.Texture.Enable(Distortion_texture[!Distortion_switch]);
					vglUniform1fARB(opengl_shader_get_uniform("use_offset"), 1.0f);
				} else {
					vglUniform1iARB(opengl_shader_get_uniform("distMap"), 0);
					vglUniform1fARB(opengl_shader_get_uniform("use_offset"), 0.0f);
				}

				zbuff = gr_zbuffer_set(GR_ZBUFF_READ);

				Assert(Scene_depth_texture != 0);

				GL_state.Texture.SetActiveUnit(1);
				GL_state.Texture.SetTarget(GL_TEXTURE_2D);
				GL_state.Texture.Enable(Scene_depth_texture);
			} else if ( flags & TMAP_FLAG_VERTEX_GEN && flags & TMAP_FLAG_LINESTRIP ) {
				sdr_index = opengl_shader_get_effect_shader(SDR_EFFECT_TRAILS|SDR_EFFECT_GEOMETRY);

				if ( sdr_index != Stream_buffer_sdr ) {
					opengl_shader_set_current(&GL_effect_shaders[sdr_index]);
					Stream_buffer_sdr = sdr_index;

					vglUniform1iARB(opengl_shader_get_uniform("baseMap"), 0);

					if ( fvec_offset >= 0 ) {
						attrib_index = opengl_shader_get_attribute("fvec");
						GL_state.Array.EnableVertexAttrib(attrib_index);
						GL_state.Array.VertexAttribPointer(attrib_index, 3, GL_FLOAT, GL_FALSE, stride, ptr + fvec_offset);
					}

					if ( radius_offset >= 0 ) {
						attrib_index = opengl_shader_get_attribute("width");
						GL_state.Array.EnableVertexAttrib(attrib_index);
						GL_state.Array.VertexAttribPointer(attrib_index, 1, GL_FLOAT, GL_FALSE, stride, ptr + radius_offset);
					}

					if ( alpha_offset >= 0 ) {
						attrib_index = opengl_shader_get_attribute("intensity");
						GL_state.Array.EnableVertexAttrib(attrib_index);
						GL_state.Array.VertexAttribPointer(attrib_index, 1, GL_FLOAT, GL_FALSE, stride, ptr + alpha_offset);
					}
				}
			} else if ( Cmdline_softparticles ) {
				uint sdr_effect_flags = SDR_EFFECT_SOFT_QUAD;

				if ( flags & TMAP_FLAG_VERTEX_GEN ) {
					sdr_effect_flags |= SDR_EFFECT_GEOMETRY;
				}

				sdr_index = opengl_shader_get_effect_shader(sdr_effect_flags);

				if ( sdr_index != Stream_buffer_sdr ) {
					opengl_shader_set_current(&GL_effect_shaders[sdr_index]);
					Stream_buffer_sdr = sdr_index;

					vglUniform1iARB(opengl_shader_get_uniform("baseMap"), 0);
					vglUniform1iARB(opengl_shader_get_uniform("depthMap"), 1);
					vglUniform1fARB(opengl_shader_get_uniform("window_width"), (float)gr_screen.max_w);
					vglUniform1fARB(opengl_shader_get_uniform("window_height"), (float)gr_screen.max_h);
					vglUniform1fARB(opengl_shader_get_uniform("nearZ"), Min_draw_distance);
					vglUniform1fARB(opengl_shader_get_uniform("farZ"), Max_draw_distance);
					
					if ( Cmdline_no_deferred_lighting ) {
						vglUniform1iARB(opengl_shader_get_uniform("linear_depth"), 0);
					} else {
						vglUniform1iARB(opengl_shader_get_uniform("linear_depth"), 1);
					}

					if ( radius_offset >= 0 ) {
						attrib_index = opengl_shader_get_attribute("radius_in");
						GL_state.Array.EnableVertexAttrib(attrib_index);
						GL_state.Array.VertexAttribPointer(attrib_index, 1, GL_FLOAT, GL_FALSE, stride, ptr + radius_offset);
					}

					if ( up_offset >= 0 ) {
						attrib_index = opengl_shader_get_attribute("up");
						GL_state.Array.EnableVertexAttrib(attrib_index);
						GL_state.Array.VertexAttribPointer(attrib_index, 3, GL_FLOAT, GL_FALSE, stride, ptr + up_offset);
					}
				}

				zbuff = gr_zbuffer_set(GR_ZBUFF_NONE);

				if ( !Cmdline_no_deferred_lighting ) {
					Assert(Scene_position_texture != 0);

					GL_state.Texture.SetActiveUnit(1);
					GL_state.Texture.SetTarget(GL_TEXTURE_2D);
					GL_state.Texture.Enable(Scene_position_texture);
				} else {
					Assert(Scene_depth_texture != 0);

					GL_state.Texture.SetActiveUnit(1);
					GL_state.Texture.SetTarget(GL_TEXTURE_2D);
					GL_state.Texture.Enable(Scene_depth_texture);
				}
			}
		} else {
			GL_state.Array.ResetVertexAttribUsed();
			GL_state.Array.DisabledVertexAttribUnused();
		}

		if ( !gr_opengl_tcache_set(gr_screen.current_bitmap, tmap_type, &u_scale, &v_scale) ) {
			return;
		}

		if ( tex_offset >= 0 ) {
			GL_state.Array.SetActiveClientUnit(0);
			GL_state.Array.EnableClientTexture();
			GL_state.Array.TexPointer(2, GL_FLOAT, stride, ptr + tex_offset);
		}
	} else {
		GL_state.Array.SetActiveClientUnit(0);
		GL_state.Array.DisableClientTexture();
	}

	if (flags & TMAP_FLAG_TRILIST) {
		gl_mode = GL_TRIANGLES;
	} else if (flags & TMAP_FLAG_TRISTRIP) {
		gl_mode = GL_TRIANGLE_STRIP;
	} else if (flags & TMAP_FLAG_QUADLIST) {
		gl_mode = GL_QUADS;
	} else if (flags & TMAP_FLAG_QUADSTRIP) {
		gl_mode = GL_QUAD_STRIP;
	} else if (flags & TMAP_FLAG_POINTLIST) {
		gl_mode = GL_POINTS;
	} else if (flags & TMAP_FLAG_LINESTRIP) {
		gl_mode = GL_LINE_STRIP;
	}

	if ( (flags & TMAP_FLAG_RGB) && (flags & TMAP_FLAG_GOURAUD) && color_offset >= 0 ) {
		GL_state.Array.EnableClientColor();
		GL_state.Array.ColorPointer(4, GL_UNSIGNED_BYTE, stride, ptr + color_offset);
		GL_state.InvalidateColor();
	} else {
		// use what opengl_setup_render_states() gives us since this works much better for nebula and transparency
		GL_state.Array.DisableClientColor();
		GL_state.Color( (ubyte)r, (ubyte)g, (ubyte)b, (ubyte)alpha );
	}

	if ( pos_offset >= 0 ) {
		GL_state.Array.EnableClientVertex();
		GL_state.Array.VertexPointer(3, GL_FLOAT, stride, ptr + pos_offset);
	}

	glDrawArrays(gl_mode, offset, n_verts);

	if( (flags & TMAP_FLAG_DISTORTION) || (flags & TMAP_FLAG_DISTORTION_THRUSTER) ) {
		GLenum buffers[] = { GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT };
		vglDrawBuffers(2, buffers);
	}
	
	GL_CHECK_FOR_ERRORS("end of render3d()");
}

void gr_opengl_start_instance_matrix(vec3d *offset, matrix *rotation)
{
	if (Cmdline_nohtl) {
		return;
	}

	Assert( GL_htl_projection_matrix_set );
	Assert( GL_htl_view_matrix_set );

	if (offset == NULL) {
		offset = &vmd_zero_vector;
	}

	if (rotation == NULL) {
		rotation = &vmd_identity_matrix;	
	}

	GL_CHECK_FOR_ERRORS("start of start_instance_matrix()");

	glPushMatrix();

	vec3d axis;
	float ang;
	vm_matrix_to_rot_axis_and_angle(rotation, &ang, &axis);

	glTranslatef( offset->xyz.x, offset->xyz.y, offset->xyz.z );
	if (fl_abs(ang) > 0.0f) {
		glRotatef( fl_degrees(ang), axis.xyz.x, axis.xyz.y, axis.xyz.z );
	}

	GL_CHECK_FOR_ERRORS("end of start_instance_matrix()");

	GL_modelview_matrix_depth++;
}

void gr_opengl_start_instance_angles(vec3d *pos, angles *rotation)
{
	if (Cmdline_nohtl)
		return;

	Assert(GL_htl_projection_matrix_set);
	Assert(GL_htl_view_matrix_set);

	matrix m;
	vm_angles_2_matrix(&m, rotation);

	gr_opengl_start_instance_matrix(pos, &m);
}

void gr_opengl_end_instance_matrix()
{
	if (Cmdline_nohtl)
		return;

	Assert(GL_htl_projection_matrix_set);
	Assert(GL_htl_view_matrix_set);

	glPopMatrix();

	GL_modelview_matrix_depth--;
}

// the projection matrix; fov, aspect ratio, near, far
void gr_opengl_set_projection_matrix(float fov, float aspect, float z_near, float z_far)
{
	if (Cmdline_nohtl) {
		return;
	}

	GL_CHECK_FOR_ERRORS("start of set_projection_matrix()()");
	
	if (GL_rendering_to_texture) {
		glViewport(gr_screen.offset_x, gr_screen.offset_y, gr_screen.clip_width, gr_screen.clip_height);
	} else {
		glViewport(gr_screen.offset_x, (gr_screen.max_h - gr_screen.offset_y - gr_screen.clip_height), gr_screen.clip_width, gr_screen.clip_height);
	}
	

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	GLdouble clip_width, clip_height;

	clip_height = tan( (double)fov * 0.5 ) * z_near;
	clip_width = clip_height * (GLdouble)aspect;

	glFrustum( -clip_width, clip_width, -clip_height, clip_height, z_near, z_far );

	glMatrixMode(GL_MODELVIEW);

	GL_CHECK_FOR_ERRORS("end of set_projection_matrix()()");

	GL_htl_projection_matrix_set = 1;
}

void gr_opengl_end_projection_matrix()
{
	if (Cmdline_nohtl) {
		return;
	}

	GL_CHECK_FOR_ERRORS("start of end_projection_matrix()");

	glViewport(0, 0, gr_screen.max_w, gr_screen.max_h);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	// the top and bottom positions are reversed on purpose, but RTT needs them the other way
	if (GL_rendering_to_texture) {
		glOrtho(0, gr_screen.max_w, 0, gr_screen.max_h, -1.0, 1.0);
	} else {
		glOrtho(0, gr_screen.max_w, gr_screen.max_h, 0, -1.0, 1.0);
	}

	glMatrixMode(GL_MODELVIEW);

	GL_CHECK_FOR_ERRORS("end of end_projection_matrix()");

	GL_htl_projection_matrix_set = 0;
}

void gr_opengl_set_view_matrix(vec3d *pos, matrix *orient)
{
	if (Cmdline_nohtl)
		return;

	Assert(GL_htl_projection_matrix_set);
	Assert(GL_modelview_matrix_depth == 1);

	GL_CHECK_FOR_ERRORS("start of set_view_matrix()");

	glPushMatrix();

	// right now it depends on your settings as to whether this has any effect in-mission
	// not much good now, but should be a bit more useful later on
	if ( !memcmp(pos, &last_view_pos, sizeof(vec3d)) && !memcmp(orient, &last_view_orient, sizeof(matrix)) ) {
		use_last_view = true;
	} else {
		memcpy(&last_view_pos, pos, sizeof(vec3d));
		memcpy(&last_view_orient, orient, sizeof(matrix));

		use_last_view = false;
	}

	if ( !use_last_view ) {
		// should already be normalized
		eyex =  (GLdouble)pos->xyz.x;
		eyey =  (GLdouble)pos->xyz.y;
		eyez = -(GLdouble)pos->xyz.z;

		// should already be normalized
		GLdouble fwdx =  (GLdouble)orient->vec.fvec.xyz.x;
		GLdouble fwdy =  (GLdouble)orient->vec.fvec.xyz.y;
		GLdouble fwdz = -(GLdouble)orient->vec.fvec.xyz.z;

		// should already be normalized
		GLdouble upx =  (GLdouble)orient->vec.uvec.xyz.x;
		GLdouble upy =  (GLdouble)orient->vec.uvec.xyz.y;
		GLdouble upz = -(GLdouble)orient->vec.uvec.xyz.z;

		GLdouble mag;

		// setup Side vector (crossprod of forward and up vectors)
		GLdouble Sx = (fwdy * upz) - (fwdz * upy);
		GLdouble Sy = (fwdz * upx) - (fwdx * upz);
		GLdouble Sz = (fwdx * upy) - (fwdy * upx);

		// normalize Side
		mag = 1.0 / sqrt( (Sx*Sx) + (Sy*Sy) + (Sz*Sz) );

		Sx *= mag;
		Sy *= mag;
		Sz *= mag;

		// setup Up vector (crossprod of Side and forward vectors)
		GLdouble Ux = (Sy * fwdz) - (Sz * fwdy);
		GLdouble Uy = (Sz * fwdx) - (Sx * fwdz);
		GLdouble Uz = (Sx * fwdy) - (Sy * fwdx);

		// normalize Up
		mag = 1.0 / sqrt( (Ux*Ux) + (Uy*Uy) + (Uz*Uz) );

		Ux *= mag;
		Uy *= mag;
		Uz *= mag;

		// store the result in our matrix
		memset( vmatrix, 0, sizeof(vmatrix) );
		vmatrix[0]  = Sx;   vmatrix[1]  = Ux;   vmatrix[2]  = -fwdx;
		vmatrix[4]  = Sy;   vmatrix[5]  = Uy;   vmatrix[6]  = -fwdy;
		vmatrix[8]  = Sz;   vmatrix[9]  = Uz;   vmatrix[10] = -fwdz;
		vmatrix[15] = 1.0;
	}

	glLoadMatrixd(vmatrix);
	
	glTranslated(-eyex, -eyey, -eyez);
	glScalef(1.0f, 1.0f, -1.0f);


	if (Cmdline_env) {
		GL_env_texture_matrix_set = true;

		// if our view setup is the same as previous call then we can skip this
		if ( !use_last_view ) {
			// setup the texture matrix which will make the the envmap keep lined
			// up properly with the environment
			GLfloat mview[16];

			glGetFloatv(GL_MODELVIEW_MATRIX, mview);

			// r.xyz  <--  r.x, u.x, f.x
			GL_env_texture_matrix[0]  =  mview[0];
			GL_env_texture_matrix[1]  = -mview[4];
			GL_env_texture_matrix[2]  =  mview[8];
			// u.xyz  <--  r.y, u.y, f.y
			GL_env_texture_matrix[4]  =  mview[1];
			GL_env_texture_matrix[5]  = -mview[5];
			GL_env_texture_matrix[6]  =  mview[9];
			// f.xyz  <--  r.z, u.z, f.z
			GL_env_texture_matrix[8]  =  mview[2];
			GL_env_texture_matrix[9]  = -mview[6];
			GL_env_texture_matrix[10] =  mview[10];

			GL_env_texture_matrix[15] = 1.0f;
		}
	}

	GL_CHECK_FOR_ERRORS("end of set_view_matrix()");

	GL_modelview_matrix_depth = 2;
	GL_htl_view_matrix_set = 1;
}

void gr_opengl_end_view_matrix()
{
	if (Cmdline_nohtl)
		return;

	Assert(GL_modelview_matrix_depth == 2);

	glPopMatrix();
	glLoadIdentity();

	GL_modelview_matrix_depth = 1;
	GL_htl_view_matrix_set = 0;
	GL_env_texture_matrix_set = false;
}

// set a view and projection matrix for a 2D element
// TODO: this probably needs to accept values
void gr_opengl_set_2d_matrix(/*int x, int y, int w, int h*/)
{
	if (Cmdline_nohtl) {
		return;
	}

	// don't bother with this if we aren't even going to need it
	if ( !GL_htl_projection_matrix_set ) {
		return;
	}

	Assert( GL_htl_2d_matrix_set == 0 );
	Assert( GL_htl_2d_matrix_depth == 0 );

	glPushAttrib(GL_TRANSFORM_BIT);

	// the viewport needs to be the full screen size since glOrtho() is relative to it
	glViewport(0, 0, gr_screen.max_w, gr_screen.max_h);

	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
	glLoadIdentity();

	// the top and bottom positions are reversed on purpose, but RTT needs them the other way
	if (GL_rendering_to_texture) {
		glOrtho( 0, gr_screen.max_w, 0, gr_screen.max_h, -1, 1 );
	} else {
		glOrtho( 0, gr_screen.max_w, gr_screen.max_h, 0, -1, 1 );
	}

	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glLoadIdentity();

#ifndef NDEBUG
	// safety check to make sure we don't use more than 2 projection matrices
	GLint num_proj_stacks = 0;
	glGetIntegerv( GL_PROJECTION_STACK_DEPTH, &num_proj_stacks );
	Assert( num_proj_stacks <= 2 );
#endif

	GL_htl_2d_matrix_set++;
	GL_htl_2d_matrix_depth++;
}

// ends a previously set 2d view and projection matrix
void gr_opengl_end_2d_matrix()
{
	if (Cmdline_nohtl)
		return;

	if (!GL_htl_2d_matrix_set)
		return;

	Assert( GL_htl_2d_matrix_depth == 1 );

	// reset viewport to what it was originally set to by the proj matrix
	glViewport(gr_screen.offset_x, (gr_screen.max_h - gr_screen.offset_y - gr_screen.clip_height), gr_screen.clip_width, gr_screen.clip_height);

	glMatrixMode( GL_PROJECTION );
	glPopMatrix();
	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();

	glPopAttrib();

	GL_htl_2d_matrix_set = 0;
	GL_htl_2d_matrix_depth = 0;
}

static bool GL_scale_matrix_set = false;

void gr_opengl_push_scale_matrix(vec3d *scale_factor)
{
	if ( (scale_factor->xyz.x == 1) && (scale_factor->xyz.y == 1) && (scale_factor->xyz.z == 1) )
		return;

	GL_scale_matrix_set = true;
	glPushMatrix();

	GL_modelview_matrix_depth++;

	glScalef(scale_factor->xyz.x, scale_factor->xyz.y, scale_factor->xyz.z);
}

void gr_opengl_pop_scale_matrix()
{
	if (!GL_scale_matrix_set) 
		return;

	glPopMatrix();

	GL_modelview_matrix_depth--;
	GL_scale_matrix_set = false;
}

void gr_opengl_end_clip_plane()
{
	if (Cmdline_nohtl) {
		return;
	}

	if ( Use_GLSL > 1 ) {
		return;
	}

	GL_state.ClipPlane(0, GL_FALSE);
}

void gr_opengl_start_clip_plane()
{
	if (Cmdline_nohtl) {
		return;
	}

	if ( Use_GLSL > 1 ) {
		// bail since we're gonna clip in the shader
		return;
	}

	GLdouble clip_equation[4];

	clip_equation[0] = (GLdouble)G3_user_clip_normal.xyz.x;
	clip_equation[1] = (GLdouble)G3_user_clip_normal.xyz.y;
	clip_equation[2] = (GLdouble)G3_user_clip_normal.xyz.z;

	clip_equation[3] = (GLdouble)(G3_user_clip_normal.xyz.x * G3_user_clip_point.xyz.x)
						+ (GLdouble)(G3_user_clip_normal.xyz.y * G3_user_clip_point.xyz.y)
						+ (GLdouble)(G3_user_clip_normal.xyz.z * G3_user_clip_point.xyz.z);
	clip_equation[3] *= -1.0;


	glClipPlane(GL_CLIP_PLANE0, clip_equation);
	GL_state.ClipPlane(0, GL_TRUE);
}

//************************************State blocks************************************

//this is an array of reference counts for state block IDs
//static GLuint *state_blocks = NULL;
//static uint n_state_blocks = 0;
//static GLuint current_state_block;

//this is used for places in the array that a state block ID no longer exists
//#define EMPTY_STATE_BOX_REF_COUNT	0xffffffff

int opengl_get_new_state_block_internal()
{
/*	uint i;

	if (state_blocks == NULL) {
		state_blocks = (GLuint*)vm_malloc(sizeof(GLuint));
		memset(&state_blocks[n_state_blocks], 'f', sizeof(GLuint));
		n_state_blocks++;
	}

	for (i = 0; i < n_state_blocks; i++) {
		if (state_blocks[i] == EMPTY_STATE_BOX_REF_COUNT) {
			return i;
		}
	}

	// "i" should be n_state_blocks since we got here.
	state_blocks = (GLuint*)vm_realloc(state_blocks, sizeof(GLuint) * i);
	memset(&state_blocks[i], 'f', sizeof(GLuint));

	n_state_blocks++;

	return n_state_blocks-1;*/
	return -1;
}

void gr_opengl_start_state_block()
{
/*	gr_screen.recording_state_block = true;
	current_state_block = opengl_get_new_state_block_internal();
	glNewList(current_state_block, GL_COMPILE);*/
}

int gr_opengl_end_state_block()
{
/*	//sanity check
	if(!gr_screen.recording_state_block)
		return -1;

	//End the display list
	gr_screen.recording_state_block = false;
	glEndList();

	//now return
	return current_state_block;*/
	return -1;
}

void gr_opengl_set_state_block(int handle)
{
/*	if(handle < 0) return;
	glCallList(handle);*/
}

extern SCP_vector<light*> Static_light;
extern int opengl_check_framebuffer();
extern bool Glowpoint_override;
bool Glowpoint_override_save;
void gr_opengl_start_shadow_map(float neardist, float middist, float fardist)
{
	if(!Cmdline_shadow_quality)
		return;
	float minx = 0.0f, miny = 0.0f, minz = 0.0f, maxx = 0.0f, maxy = 0.0f, maxz = 0.0f;
	light *lp = *Static_light.begin(); 

	shadow_neardist = neardist;
	shadow_middist = middist;
	shadow_fardist = fardist;

	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &saved_fb);
	vglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shadow_fbo);

	//glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	GLenum buffers[] = { GL_COLOR_ATTACHMENT0_EXT};
	vglDrawBuffers(1, buffers);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	gr_opengl_set_lighting(false,false);
	if(lp)
	{
		in_shadow_map = true;
		Glowpoint_override_save = Glowpoint_override;
		Glowpoint_override = true;
		matrix orient;
		vec3d light_dir;
		vec3d frustum[8];
		vec3d bb_r; 
		vm_vec_copy_normalize(&light_dir, &lp->vec);
		vm_vector_2_matrix(&orient, &light_dir, &Eye_matrix.vec.uvec, NULL);
		
		GLdouble clip_width, clip_height;
		clip_height = tan( (double)Proj_fov * 0.5 );
		clip_width = clip_height * (GLdouble)gr_screen.clip_aspect;

		vm_vec_scale_add(&frustum[0], &Eye_matrix.vec.fvec, &Eye_matrix.vec.rvec, (float)clip_width);
		vm_vec_scale_add2(&frustum[0], &Eye_matrix.vec.uvec, (float)clip_height);

		vm_vec_scale_add(&frustum[1], &Eye_matrix.vec.fvec, &Eye_matrix.vec.rvec, (float)clip_width);
		vm_vec_scale_add2(&frustum[1], &Eye_matrix.vec.uvec, -(float)clip_height);

		vm_vec_scale_add(&frustum[2], &Eye_matrix.vec.fvec, &Eye_matrix.vec.rvec, -(float)clip_width);
		vm_vec_scale_add2(&frustum[2], &Eye_matrix.vec.uvec, (float)clip_height);

		vm_vec_scale_add(&frustum[3], &Eye_matrix.vec.fvec, &Eye_matrix.vec.rvec, -(float)clip_width);
		vm_vec_scale_add2(&frustum[3], &Eye_matrix.vec.uvec, -(float)clip_height);

		vm_vec_copy_scale(&frustum[4], &frustum[0], neardist);
		vm_vec_copy_scale(&frustum[5], &frustum[1], neardist);
		vm_vec_copy_scale(&frustum[6], &frustum[2], neardist);
		vm_vec_copy_scale(&frustum[7], &frustum[3], neardist);

		for(int i = 0; i < 8; i++)
		{
			
			vm_vec_rotate(&bb_r, &frustum[i], &orient);
			
			if(!i)
			{
				minx = bb_r.xyz.x;
				maxx = bb_r.xyz.x;
				miny = bb_r.xyz.y;
				maxy = bb_r.xyz.y;
				minz = bb_r.xyz.z;
				maxz = bb_r.xyz.z;
			}
			else
			{
				minx = MIN(bb_r.xyz.x, minx);
				maxx = MAX(bb_r.xyz.x, maxx);
				miny = MIN(bb_r.xyz.y, miny);
				maxy = MAX(bb_r.xyz.y, maxy);
				minz = MIN(bb_r.xyz.z, minz);
				maxz = MAX(bb_r.xyz.z, maxz);
			}
		}

		memset(lprojmatrix, 0, sizeof(GLfloat) * 3 * 16);

		lprojmatrix[0][0] = 2.0f / ( maxx - minx );
		lprojmatrix[0][5] = 2.0f / ( maxy - miny );
		lprojmatrix[0][10] = -2.0f / ( maxz - minz );
		lprojmatrix[0][12] = -(maxx + minx) / ( maxx - minx );
		lprojmatrix[0][13] = -(maxy + miny) / ( maxy - miny );
		lprojmatrix[0][14] = -(maxz + minz) / ( maxz - minz );
		lprojmatrix[0][15] = 1.0f;

		vm_vec_copy_scale(&frustum[0], &frustum[4], middist/neardist);
		vm_vec_copy_scale(&frustum[1], &frustum[5], middist/neardist);
		vm_vec_copy_scale(&frustum[2], &frustum[6], middist/neardist);
		vm_vec_copy_scale(&frustum[3], &frustum[7], middist/neardist);

		for(int i = 0; i < 8; i++)
		{
			
			vm_vec_rotate(&bb_r, &frustum[i], &orient);
			
			if(!i)
			{
				minx = bb_r.xyz.x;
				maxx = bb_r.xyz.x;
				miny = bb_r.xyz.y;
				maxy = bb_r.xyz.y;
				minz = bb_r.xyz.z;
				maxz = bb_r.xyz.z;
			}
			else
			{
				minx = MIN(bb_r.xyz.x, minx);
				maxx = MAX(bb_r.xyz.x, maxx);
				miny = MIN(bb_r.xyz.y, miny);
				maxy = MAX(bb_r.xyz.y, maxy);
				minz = MIN(bb_r.xyz.z, minz);
				maxz = MAX(bb_r.xyz.z, maxz);
			}
		}

		lprojmatrix[1][0] = 2.0f / ( maxx - minx );
		lprojmatrix[1][5] = 2.0f / ( maxy - miny );
		lprojmatrix[1][10] = -2.0f / ( maxz - minz );
		lprojmatrix[1][12] = -(maxx + minx) / ( maxx - minx );
		lprojmatrix[1][13] = -(maxy + miny) / ( maxy - miny );
		lprojmatrix[1][14] = -(maxz + minz) / ( maxz - minz );
		lprojmatrix[1][15] = 1.0f;

		vm_vec_copy_scale(&frustum[4], &frustum[0], fardist/middist);
		vm_vec_copy_scale(&frustum[5], &frustum[1], fardist/middist);
		vm_vec_copy_scale(&frustum[6], &frustum[2], fardist/middist);
		vm_vec_copy_scale(&frustum[7], &frustum[3], fardist/middist);

		for(int i = 0; i < 8; i++)
		{
			
			vm_vec_rotate(&bb_r, &frustum[i], &orient);
			
			if(!i)
			{
				minx = bb_r.xyz.x;
				maxx = bb_r.xyz.x;
				miny = bb_r.xyz.y;
				maxy = bb_r.xyz.y;
				minz = bb_r.xyz.z;
				maxz = bb_r.xyz.z;
			}
			else
			{
				minx = MIN(bb_r.xyz.x, minx);
				maxx = MAX(bb_r.xyz.x, maxx);
				miny = MIN(bb_r.xyz.y, miny);
				maxy = MAX(bb_r.xyz.y, maxy);
				minz = MIN(bb_r.xyz.z, minz);
				maxz = MAX(bb_r.xyz.z, maxz);
			}
		}

		lprojmatrix[2][0] = 2.0f / ( maxx - minx );
		lprojmatrix[2][5] = 2.0f / ( maxy - miny );
		lprojmatrix[2][10] = -2.0f / ( maxz - minz );
		lprojmatrix[2][12] = -(maxx + minx) / ( maxx - minx );
		lprojmatrix[2][13] = -(maxy + miny) / ( maxy - miny );
		lprojmatrix[2][14] = -(maxz + minz) / ( maxz - minz );
		lprojmatrix[2][15] = 1.0f;

		GL_htl_projection_matrix_set = 1;
		gr_set_view_matrix(&Eye_position, &orient);
		glGetFloatv(GL_MODELVIEW_MATRIX, lmatrix);
		int size = (Cmdline_shadow_quality == 2 ? 1024 : 512);
		glViewport(0, 0, size, size);
		//glDrawBuffer(GL_NONE);
		//glReadBuffer(GL_NONE);
	}
}

void gr_opengl_shadow_map_start(matrix *light_orient, light_frustum_info *verynear_frustum, light_frustum_info *near_frustum, light_frustum_info *mid_frustum, light_frustum_info *far_frustum)
{
	if(!Cmdline_shadow_quality)
		return;

	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &saved_fb);
	vglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shadow_fbo);

	//glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	GLenum buffers[] = { GL_COLOR_ATTACHMENT0_EXT};
	vglDrawBuffers(1, buffers);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	gr_opengl_set_lighting(false,false);
	
	in_shadow_map = true;
	Glowpoint_override_save = Glowpoint_override;
	Glowpoint_override = true;

	memcpy(lprojmatrix[0], verynear_frustum->proj_matrix, sizeof(float)*16);
	memcpy(lprojmatrix[1], near_frustum->proj_matrix, sizeof(float)*16);
	memcpy(lprojmatrix[2], mid_frustum->proj_matrix, sizeof(float)*16);
	memcpy(lprojmatrix[3], far_frustum->proj_matrix, sizeof(float)*16);

	GL_htl_projection_matrix_set = 1;
	//gr_set_view_matrix(&near_frustum->view_position, light_orient);
	//gr_set_view_matrix(&zero, light_orient);
	gr_set_view_matrix(&Eye_position, light_orient);
	//glOrtho(near_frustum->min.xyz.x, near_frustum->max.xyz.x, near_frustum->min.xyz.y, near_frustum->max.xyz.y, near_frustum->min.xyz.z, near_frustum->max.xyz.z);

// 	glMatrixMode(GL_PROJECTION);
// 	glLoadIdentity();
// 	glOrtho(-1.0f, 1.0f, -1.0f, 1.0f, Min_draw_distance, Max_draw_distance);
// 	glMatrixMode(GL_MODELVIEW);
	//gr_set_view_matrix(&zero, light_orient);
	//gr_set_proj_matrix(Proj_fov, 1.0, Min_draw_distance, Max_draw_distance);
	//gr_set_view_matrix(&Eye_position, &Eye_matrix);

	glGetFloatv(GL_MODELVIEW_MATRIX, lmatrix);

	int size = (Cmdline_shadow_quality == 2 ? 1024 : 512);
	glViewport(0, 0, size, size);
}

void gr_opengl_end_shadow_map()
{
		if(!in_shadow_map)
			return;

		gr_end_view_matrix();
		in_shadow_map = false;

		//gr_post_process_shadow_map();

		gr_zbuffer_set(ZBUFFER_TYPE_FULL);
		vglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, saved_fb);
		if(saved_fb)
		{
// 			GLenum buffers[] = { GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT };
// 			vglDrawBuffers(2, buffers);
		}

		GLenum buffers[] = { GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT };
		vglDrawBuffers(2, buffers);

		Glowpoint_override = Glowpoint_override_save;
		GL_htl_projection_matrix_set = 0;
		
		glViewport(gr_screen.offset_x, (gr_screen.max_h - gr_screen.offset_y - gr_screen.clip_height), gr_screen.clip_width, gr_screen.clip_height);
		glScissor(gr_screen.offset_x, (gr_screen.max_h - gr_screen.offset_y - gr_screen.clip_height), gr_screen.clip_width, gr_screen.clip_height);
}

void gr_opengl_clear_shadow_map()
{
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &saved_fb);
	vglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shadow_fbo);
	glClear(GL_DEPTH_BUFFER_BIT);
	vglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, saved_fb);
}

bool gr_opengl_set_shader_flag(uint shader_flags)
{
	int sdr_index;

	// find proper shader
	if (shader_flags == GL_last_shader_flags) {
		sdr_index = GL_last_shader_index;
	} else {
		sdr_index = gr_opengl_maybe_create_shader(shader_flags);

		if (sdr_index < 0) {
			return false;
		}

		GL_last_shader_index = sdr_index;
		GL_last_shader_flags = shader_flags;
	}

	Assert( sdr_index >= 0 );

	opengl_shader_set_current( &GL_shader[sdr_index] );

	return true;
}

void opengl_tnl_set_material(int flags, uint shader_flags, int tmap_type)
{
	float u_scale, v_scale;
	int render_pass = 0;

	Current_uniforms.setCurrentShader(Current_shader->program_id);
	Current_uniforms.setUniformsBegin();

	if ( flags & TMAP_ANIMATED_SHADER ) {
		Current_uniforms.queueUniformf("anim_timer", opengl_shader_get_animated_timer());
		Current_uniforms.queueUniformi("effect_num", opengl_shader_get_animated_effect());
		Current_uniforms.queueUniformf("vpwidth", 1.0f/gr_screen.max_w);
		Current_uniforms.queueUniformf("vpheight", 1.0f/gr_screen.max_h);
	}

	int num_lights = MIN(Num_active_gl_lights, GL_max_lights) - 1;
	Current_uniforms.queueUniformi("n_lights", num_lights);
	Current_uniforms.queueUniformf( "light_factor", GL_light_factor );
	
	if ( Using_Team_Color ) {
		Current_uniforms.setTeamColor(Current_team_color->base.r, Current_team_color->base.g, Current_team_color->base.b, 
			Current_team_color->stripe.r,  Current_team_color->stripe.g,  Current_team_color->stripe.b);
	}

	if ( shader_flags & SDR_FLAG_CLIP ) {
		Current_uniforms.queueUniformi("use_clip_plane", G3_user_clip);

		if ( G3_user_clip ) {
			vec3d normal, pos;

			vm_vec_unrotate(&normal, &G3_user_clip_normal, &Eye_matrix);
			vm_vec_normalize(&normal);

			vm_vec_unrotate(&pos, &G3_user_clip_point, &Eye_matrix);
			vm_vec_add2(&pos, &Eye_position);

			vec4 clip_plane_equation;

			clip_plane_equation.a1d[0] = normal.a1d[0];
			clip_plane_equation.a1d[1] = normal.a1d[1];
			clip_plane_equation.a1d[2] = normal.a1d[2];
			clip_plane_equation.a1d[3] = vm_vec_mag(&pos);

			matrix4 model_matrix;
			memset( &model_matrix, 0, sizeof(model_matrix) );

			model_matrix.a1d[0]  = Object_matrix.vec.rvec.xyz.x;   model_matrix.a1d[4]  = Object_matrix.vec.uvec.xyz.x;   model_matrix.a1d[8]  = Object_matrix.vec.fvec.xyz.x;
			model_matrix.a1d[1]  = Object_matrix.vec.rvec.xyz.y;   model_matrix.a1d[5]  = Object_matrix.vec.uvec.xyz.y;   model_matrix.a1d[9]  = Object_matrix.vec.fvec.xyz.y;
			model_matrix.a1d[2]  = Object_matrix.vec.rvec.xyz.z;   model_matrix.a1d[6]  = Object_matrix.vec.uvec.xyz.z;   model_matrix.a1d[10] = Object_matrix.vec.fvec.xyz.z;
			model_matrix.a1d[12] = Object_position.xyz.x;
			model_matrix.a1d[13] = Object_position.xyz.y;
			model_matrix.a1d[14] = Object_position.xyz.z;
			model_matrix.a1d[15] = 1.0f;

			Current_uniforms.queueUniform3f("clip_normal", normal);
			Current_uniforms.queueUniform3f("clip_position", pos);
			Current_uniforms.queueUniformMatrix4f("world_matrix", 0, model_matrix);
			//Current_uniforms.queueUniform4f("clip_plane", clip_plane_equation);
		}
	}

	if ( shader_flags & SDR_FLAG_DIFFUSE_MAP ) {
		Current_uniforms.queueUniformi("sBasemap", render_pass);
		
		int desaturate = 0;
		if ( flags & TMAP_FLAG_DESATURATE ) {
			desaturate = 1;
		}

		Current_uniforms.queueUniformi("desaturate", desaturate);
		Current_uniforms.queueUniformf("desaturate_r", gr_screen.current_color.red/255.0f);
		Current_uniforms.queueUniformf("desaturate_g", gr_screen.current_color.green/255.0f);
		Current_uniforms.queueUniformf("desaturate_b", gr_screen.current_color.blue/255.0f);

		gr_opengl_tcache_set(gr_screen.current_bitmap, tmap_type, &u_scale, &v_scale, render_pass);

		++render_pass;
	}

	if ( shader_flags & SDR_FLAG_GLOW_MAP ) {
		Current_uniforms.queueUniformi("sGlowmap", render_pass);

		gr_opengl_tcache_set(GLOWMAP, tmap_type, &u_scale, &v_scale, render_pass);

		++render_pass;
	}

	if ( shader_flags & SDR_FLAG_SPEC_MAP ) {
		Current_uniforms.queueUniformi("sSpecmap", render_pass);

		gr_opengl_tcache_set(SPECMAP, tmap_type, &u_scale, &v_scale, render_pass);

		++render_pass;

		if ( shader_flags & SDR_FLAG_ENV_MAP) {
			// 0 == env with non-alpha specmap, 1 == env with alpha specmap
			int alpha_spec = bm_has_alpha_channel(ENVMAP);

			matrix4 texture_mat;

			for ( int i = 0; i < 16; ++i ) {
				texture_mat.a1d[i] = GL_env_texture_matrix[i];
			}

			Current_uniforms.queueUniformi("alpha_spec", alpha_spec);
			Current_uniforms.queueUniformMatrix4fv("envMatrix", 1, GL_FALSE, &texture_mat);
			Current_uniforms.queueUniformi("sEnvmap", render_pass);

			gr_opengl_tcache_set(ENVMAP, TCACHE_TYPE_CUBEMAP, &u_scale, &v_scale, render_pass);

			++render_pass;
		}
	}

	if ( shader_flags & SDR_FLAG_NORMAL_MAP ) {
		Current_uniforms.queueUniformi("sNormalmap", render_pass);

		gr_opengl_tcache_set(NORMMAP, tmap_type, &u_scale, &v_scale, render_pass);

		++render_pass;
	}

	if ( shader_flags & SDR_FLAG_HEIGHT_MAP ) {
		Current_uniforms.queueUniformi("sHeightmap", render_pass);
		
		gr_opengl_tcache_set(HEIGHTMAP, tmap_type, &u_scale, &v_scale, render_pass);

		++render_pass;
	}

	if ( shader_flags & SDR_FLAG_MISC_MAP ) {
		Current_uniforms.queueUniformi("sMiscmap", render_pass);

		gr_opengl_tcache_set(MISCMAP, tmap_type, &u_scale, &v_scale, render_pass);

		++render_pass;
	}

	if ( shader_flags & SDR_FLAG_SHADOWS ) {
		matrix4 model_matrix;
		memset( &model_matrix, 0, sizeof(model_matrix) );

		model_matrix.a1d[0]  = Object_matrix.vec.rvec.xyz.x;   model_matrix.a1d[4]  = Object_matrix.vec.uvec.xyz.x;   model_matrix.a1d[8]  = Object_matrix.vec.fvec.xyz.x;
		model_matrix.a1d[1]  = Object_matrix.vec.rvec.xyz.y;   model_matrix.a1d[5]  = Object_matrix.vec.uvec.xyz.y;   model_matrix.a1d[9]  = Object_matrix.vec.fvec.xyz.y;
		model_matrix.a1d[2]  = Object_matrix.vec.rvec.xyz.z;   model_matrix.a1d[6]  = Object_matrix.vec.uvec.xyz.z;   model_matrix.a1d[10] = Object_matrix.vec.fvec.xyz.z;
		model_matrix.a1d[12] = Object_position.xyz.x;
		model_matrix.a1d[13] = Object_position.xyz.y;
		model_matrix.a1d[14] = Object_position.xyz.z;
		model_matrix.a1d[15] = 1.0f;

		matrix4 l_matrix;
		matrix4 l_proj_matrix[4];

		for ( int i = 0; i < 16; ++i ) {
			l_matrix.a1d[i] = lmatrix[i];
		}

		for ( int i = 0; i < 4; ++i ) {
			for ( int j = 0; j < 16; ++j ) {
				l_proj_matrix[i].a1d[j] = lprojmatrix[i][j];
			}
		}

		Current_uniforms.queueUniformMatrix4f("shadow_mv_matrix", GL_FALSE, l_matrix);
		Current_uniforms.queueUniformMatrix4fv("shadow_proj_matrix", 4, GL_FALSE, l_proj_matrix);
		Current_uniforms.queueUniformMatrix4f("model_matrix", GL_FALSE, model_matrix);
		Current_uniforms.queueUniformf("veryneardist", shadow_veryneardist);
		Current_uniforms.queueUniformf("neardist", shadow_neardist);
		Current_uniforms.queueUniformf("middist", shadow_middist);
		Current_uniforms.queueUniformf("fardist", shadow_fardist);
		Current_uniforms.queueUniformi("shadow_map", render_pass);
		
		GL_state.Texture.SetActiveUnit(render_pass);
		GL_state.Texture.SetTarget(GL_TEXTURE_2D_ARRAY_EXT);
		GL_state.Texture.Enable(Shadow_map_texture);

		++render_pass; // bump!
	}

	if ( shader_flags & SDR_FLAG_GEOMETRY && shader_flags & SDR_FLAG_SHADOW_MAP ) {
		matrix4 l_proj_matrix[4];

		for ( int i = 0; i < 4; ++i ) {
			for ( int j = 0; j < 16; ++j ) {
				l_proj_matrix[i].a1d[j] = lprojmatrix[i][j];
			}
		}

		Current_uniforms.queueUniformMatrix4fv("shadow_proj_matrix", 4, GL_FALSE, l_proj_matrix);
	}

	if ( shader_flags & SDR_FLAG_ANIMATED ) {
		Current_uniforms.queueUniformi("sFramebuffer", render_pass);
		
		GL_state.Texture.SetActiveUnit(render_pass);
		GL_state.Texture.SetTarget(GL_TEXTURE_2D);

		if ( Scene_framebuffer_in_frame ) {
			GL_state.Texture.Enable(Scene_effect_texture);
			glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		} else {
			GL_state.Texture.Enable(Framebuffer_fallback_texture_id);
		}

		++render_pass;
	}

	if ( shader_flags & SDR_FLAG_TRANSFORM ) {
		Current_uniforms.queueUniformi("transform_tex", render_pass);
		Current_uniforms.queueUniformi("buffer_matrix_offset", GL_transform_buffer_offset);
		
		GL_state.Texture.SetActiveUnit(render_pass);
		GL_state.Texture.SetTarget(GL_TEXTURE_BUFFER_ARB);
		GL_state.Texture.Enable(opengl_get_transform_buffer_texture());

		++render_pass;
	}

	// Team colors are passed to the shader here, but the shader needs to handle their application.
	// By default, this is handled through the r and g channels of the misc map, but this can be changed
	// in the shader; test versions of this used the normal map r and b channels
	if ( shader_flags & SDR_FLAG_TEAMCOLOR ) {
		vec3d stripe_color;
		vec3d base_color;

		stripe_color.xyz.x = Current_team_color->stripe.r;
		stripe_color.xyz.y = Current_team_color->stripe.g;
		stripe_color.xyz.z = Current_team_color->stripe.b;

		base_color.xyz.x = Current_team_color->base.r;
		base_color.xyz.y = Current_team_color->base.g;
		base_color.xyz.z = Current_team_color->base.b;

		Current_uniforms.queueUniform3f("stripe_color", stripe_color);
		Current_uniforms.queueUniform3f("base_color", base_color);
	}

	if ( shader_flags & SDR_FLAG_THRUSTER ) {
		Current_uniforms.queueUniformf("thruster_scale", GL_thrust_scale);
	}

	Current_uniforms.setUniformsFinish();
}

uniform_handler::uniform_handler()
{
	loaded_block = NULL;
}

void uniform_handler::resetTextures()
{
	for ( int i = 0; i < TEX_SLOT_MAX; ++i ) {
		current_textures[i] = -1;
	}
}

void uniform_handler::setTexture(int texture_slot, int texture_handle)
{
	Assert(texture_slot > -1);
	Assert(texture_slot < TEX_SLOT_MAX);

	current_textures[texture_slot] = texture_handle;
}

void uniform_handler::setOrientation(matrix *orient)
{
	orientation = *orient;
}

void uniform_handler::setPosition(vec3d *pos)
{
	position = *pos;
}

void uniform_handler::setThrusterScale(float scale)
{
	thruster_scale = scale;
}

void uniform_handler::setNumLights(int num_lights)
{
	n_lights = num_lights;
}

void uniform_handler::setLightFactor(float factor)
{
	light_factor = factor;
}

void uniform_handler::setTeamColor(float base_r, float base_g, float base_b, float stripe_r, float stripe_g, float stripe_b)
{
	base_color.xyz.x = base_r;
	base_color.xyz.y = base_g;
	base_color.xyz.z = base_b;

	stripe_color.xyz.x = stripe_r;
	stripe_color.xyz.y = stripe_g;
	stripe_color.xyz.z = stripe_b;
}

void uniform_handler::setTransformBufferOffset(int offset)
{
	transform_buffer_offset = offset;
}

void uniform_handler::loadUniformLookup(uniform_block *prev_block)
{
	uniform_lookup.clear();

	if ( prev_block == NULL ) {
		return;
	}

	// create a map of uniforms so we can do easy lookups for comparisons
	int i = prev_block->uniform_start_index;
	int end = i + prev_block->num_uniforms;

	for ( ; i < end; ++i ) {
		uniform_lookup[uniforms[i].name] = i;
	}
}

int uniform_handler::findUniform(SCP_string &name)
{
	SCP_map<SCP_string, int>::iterator iter;
	
	iter = uniform_lookup.find(name);

	if ( iter == uniform_lookup.end() ) {
		return -1;
	} else {
		return iter->second;
	}
}

void uniform_handler::queueUniformi(SCP_string name, int val)
{
	int uniform_index = findUniform(name);

	if ( uniform_index >= 0) {
		Assert( (size_t)uniform_index < uniforms.size() );

		uniform_bind *bind_info = &uniforms[uniform_index];

		if ( bind_info->type == uniform_bind::INT && uniform_data_ints[bind_info->index] == val ) {
			return;
		} else {
			uniform_data_ints[bind_info->index] = val;
			uniforms_to_set.push_back(uniform_index);
			return;
		}
	}

	// uniform doesn't exist in our previous uniform block so queue this new value
	uniform_data_ints.push_back(val);

	uniform_bind new_bind;

	new_bind.count = 1;
	new_bind.index = uniform_data_ints.size() - 1;
	new_bind.type = uniform_bind::INT;
	new_bind.name = name;
	new_bind.tranpose = false;

	uniforms.push_back(new_bind);
	uniforms_to_set.push_back(uniforms.size()-1);

	uniform_lookup[name] = uniforms.size()-1;
}

void uniform_handler::queueUniformf(SCP_string name, float val)
{
	int uniform_index = findUniform(name);

	if ( uniform_index >= 0) {
		Assert( (size_t)uniform_index < uniforms.size() );

		uniform_bind *bind_info = &uniforms[uniform_index];

		if ( bind_info->type == uniform_bind::FLOAT && (fl_abs(uniform_data_floats[bind_info->index] - val) < EPSILON) ) {
			// if the values are close enough, pass.
			return;
		} else {
			uniform_data_floats[bind_info->index] = val;
			uniforms_to_set.push_back(uniform_index);
			return;
		}
	}

	// uniform doesn't exist in our previous uniform block so queue this new value
	uniform_data_floats.push_back(val);

	uniform_bind new_bind;

	new_bind.count = 1;
	new_bind.index = uniform_data_floats.size() - 1;
	new_bind.type = uniform_bind::FLOAT;
	new_bind.name = name;
	new_bind.tranpose = false;

	uniforms.push_back(new_bind);
	uniforms_to_set.push_back(uniforms.size()-1);

	uniform_lookup[name] = uniforms.size()-1;
}

void uniform_handler::queueUniform3f(SCP_string name, vec3d &val)
{
	int uniform_index = findUniform(name);

	if ( uniform_index >= 0 ) {
		Assert( (size_t)uniform_index < uniforms.size() );

		uniform_bind *bind_info = &uniforms[uniform_index];

		if ( bind_info->type == uniform_bind::VEC3 
			&& fl_abs(uniform_data_vec3d[bind_info->index].a1d[0] - val.a1d[0]) < EPSILON 
			&& fl_abs(uniform_data_vec3d[bind_info->index].a1d[1] - val.a1d[1]) < EPSILON 
			&& fl_abs(uniform_data_vec3d[bind_info->index].a1d[2] - val.a1d[2]) < EPSILON 
			) {
				// if the values are close enough, pass.
				return;
		} else {
			uniform_data_vec3d[bind_info->index] = val;
			uniforms_to_set.push_back(uniform_index);
			return;
		}
	}

	// uniform doesn't exist in our previous uniform block so queue this new value
	uniform_data_vec3d.push_back(val);

	uniform_bind new_bind;

	new_bind.count = 1;
	new_bind.index = uniform_data_vec3d.size() - 1;
	new_bind.type = uniform_bind::VEC3;
	new_bind.name = name;
	new_bind.tranpose = false;

	uniforms.push_back(new_bind);
	uniforms_to_set.push_back(uniforms.size()-1);

	uniform_lookup[name] = uniforms.size()-1;
}

void uniform_handler::queueUniform4f(SCP_string name, vec4 &val)
{
	int uniform_index = findUniform(name);

	if ( uniform_index >= 0 ) {
		Assert( (size_t)uniform_index < uniforms.size() );

		uniform_bind *bind_info = &uniforms[uniform_index];

		if ( bind_info->type == uniform_bind::VEC4
			&& fl_abs(uniform_data_vec3d[bind_info->index].a1d[0] - val.a1d[0]) < EPSILON 
			&& fl_abs(uniform_data_vec3d[bind_info->index].a1d[1] - val.a1d[1]) < EPSILON 
			&& fl_abs(uniform_data_vec3d[bind_info->index].a1d[2] - val.a1d[2]) < EPSILON 
			&& fl_abs(uniform_data_vec3d[bind_info->index].a1d[3] - val.a1d[3]) < EPSILON 
			) {
				// if the values are close enough, pass.
				return;
		} else {
			uniform_data_vec4[bind_info->index] = val;
			uniforms_to_set.push_back(uniform_index);
			return;
		}
	}

	// uniform doesn't exist in our previous uniform block so queue this new value
	uniform_data_vec4.push_back(val);

	uniform_bind new_bind;

	new_bind.count = 1;
	new_bind.index = uniform_data_vec4.size() - 1;
	new_bind.type = uniform_bind::VEC4;
	new_bind.name = name;
	new_bind.tranpose = false;

	uniforms.push_back(new_bind);
	uniforms_to_set.push_back(uniforms.size()-1);

	uniform_lookup[name] = uniforms.size()-1;
}

void uniform_handler::queueUniformMatrix4f(SCP_string name, int transpose, matrix4 &val)
{
	int uniform_index = findUniform(name);

	if ( uniform_index >= 0) {
		Assert( (size_t)uniform_index < uniforms.size() );

		uniform_bind *bind_info = &uniforms[uniform_index];

		if ( bind_info->type == uniform_bind::MATRIX4 && bind_info->count == 1 ) {
			if ( compareMatrix4(uniform_data_matrix4[bind_info->index], val) ) {
				return;
			}

			uniform_data_matrix4[bind_info->index] = val;
			uniforms_to_set.push_back(uniform_index);
			return;
		}
	}

	// uniform doesn't exist in our previous uniform block so queue this new value
	//matrix_uniform_data[num_matrix_uniforms] = val;
	//memcpy(&(matrix_uniform_data[num_matrix_uniforms]), &val, sizeof(matrix4));
	uniform_data_matrix4.push_back(val);
//	num_matrix_uniforms += 1;

	uniform_bind new_bind;
	new_bind.count = 1;
	new_bind.index = uniform_data_matrix4.size() - 1;
//	new_bind.index = num_matrix_uniforms - 1;
	new_bind.type = uniform_bind::MATRIX4;
	new_bind.name = name;
	new_bind.tranpose = transpose;

	uniforms.push_back(new_bind);
	uniforms_to_set.push_back(uniforms.size()-1);

	uniform_lookup[name] = uniforms.size()-1;
}

void uniform_handler::queueUniformMatrix4fv(SCP_string name, int count, int transpose, matrix4 *val)
{
 	int uniform_index = findUniform(name);

	if ( uniform_index >= 0) {
		Assert( (size_t)uniform_index < uniforms.size() );

		uniform_bind *bind_info = &uniforms[uniform_index];

		if ( bind_info->type == uniform_bind::MATRIX4 && bind_info->count == count ) {
			bool equal = true;

			// if the values are close enough, pass.
			for ( int i = 0; i < count; ++i ) {
				if ( !compareMatrix4(val[i], uniform_data_matrix4[bind_info->index+i]) ) {
					equal = false;
					break;
				}
			}

			if ( !equal ) {
				for ( int i = 0; i < count; ++i ) {
					uniform_data_matrix4[bind_info->index+i] = val[i];
				}

				uniforms_to_set.push_back(uniform_index);
			}

			return;
		}
	}

	// uniform doesn't exist in our previous uniform block so queue this new value
	for ( int i = 0; i < count; ++i ) {
		uniform_data_matrix4.push_back(val[i]);
	}

	uniform_bind new_bind;
	new_bind.count = count;
	new_bind.index = uniform_data_matrix4.size() - count;
//	new_bind.index = num_matrix_uniforms - count;
	new_bind.type = uniform_bind::MATRIX4;
	new_bind.name = name;
	new_bind.tranpose = transpose;
	
	uniforms.push_back(new_bind);
	uniforms_to_set.push_back(uniforms.size()-1);

	uniform_lookup[name] = uniforms.size()-1;
}

void uniform_handler::resetAll()
{
	uniforms.clear();

	uniform_data_ints.clear();
	uniform_data_floats.clear();
	uniform_data_vec3d.clear();
	uniform_data_vec4.clear();
	uniform_data_matrix4.clear();

	uniform_lookup.clear();
	uniforms_to_set.clear();
}

bool uniform_handler::compareMatrix4(matrix4 &a, matrix4 &b)
{
	if ( fl_abs(a.a1d[0] - b.a1d[0]) < EPSILON 
		&& fl_abs(a.a1d[1] - b.a1d[1]) < EPSILON 
		&& fl_abs(a.a1d[2] - b.a1d[2]) < EPSILON 
		&& fl_abs(a.a1d[3] - b.a1d[3]) < EPSILON 
		&& fl_abs(a.a1d[4] - b.a1d[4]) < EPSILON 
		&& fl_abs(a.a1d[5] - b.a1d[5]) < EPSILON 
		&& fl_abs(a.a1d[6] - b.a1d[6]) < EPSILON 
		&& fl_abs(a.a1d[7] - b.a1d[7]) < EPSILON 
		&& fl_abs(a.a1d[8] - b.a1d[8]) < EPSILON 
		&& fl_abs(a.a1d[9] - b.a1d[9]) < EPSILON 
		&& fl_abs(a.a1d[10] - b.a1d[10]) < EPSILON 
		&& fl_abs(a.a1d[11] - b.a1d[11]) < EPSILON 
		&& fl_abs(a.a1d[12] - b.a1d[12]) < EPSILON 
		&& fl_abs(a.a1d[13] - b.a1d[13]) < EPSILON 
		&& fl_abs(a.a1d[14] - b.a1d[14]) < EPSILON 
		&& fl_abs(a.a1d[15] - b.a1d[15]) < EPSILON 
		) {
		return true;
	}

	return false;
}

void uniform_handler::setCurrentShader(GLhandleARB sdr_handle)
{
	if ( sdr_handle != current_sdr ) {
		resetAll();
	}

	current_sdr = sdr_handle;
}

void uniform_handler::setUniformsBegin()
{
	uniforms_to_set.clear();
}

void uniform_handler::setUniformsFinish()
{
	for ( size_t i = 0; i < uniforms_to_set.size(); ++i ) {
		int uniform_index = uniforms_to_set[i];

		const char* name = uniforms[uniform_index].name.c_str();
		int data_index = uniforms[uniform_index].index;

		switch ( uniforms[uniform_index].type ) {
			case uniform_bind::INT:
				vglUniform1iARB(opengl_shader_get_uniform(name), uniform_data_ints[data_index]);
				break;
			case uniform_bind::FLOAT:
				vglUniform1fARB(opengl_shader_get_uniform(name), uniform_data_floats[data_index]);
				break;
			case uniform_bind::VEC3:
				vglUniform3fARB(opengl_shader_get_uniform(name), uniform_data_vec3d[data_index].a1d[0], uniform_data_vec3d[data_index].a1d[1], uniform_data_vec3d[data_index].a1d[2]);
				break;
			case uniform_bind::VEC4:
				vglUniform4fARB(opengl_shader_get_uniform(name), uniform_data_vec4[data_index].a1d[0], uniform_data_vec4[data_index].a1d[1], uniform_data_vec4[data_index].a1d[2], uniform_data_vec4[data_index].a1d[3]);
				break;
			case uniform_bind::MATRIX4: {
				vglUniformMatrix4fvARB(opengl_shader_get_uniform(name), uniforms[uniform_index].count, (GLboolean)uniforms[uniform_index].tranpose, (const GLfloat*)&(uniform_data_matrix4[data_index].a1d[0]));
				break;
			} default:
				Int3();
				break;
		}
	}
}
