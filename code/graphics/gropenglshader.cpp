/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/


#include "globalincs/pstypes.h"
#include "globalincs/def_files.h"

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
#include "graphics/gropenglpostprocessing.h"
#include "graphics/gropenglstate.h"

#include "math/vecmat.h"
#include "render/3d.h"
#include "cmdline/cmdline.h"
#include "mod_table/mod_table.h"


SCP_vector<opengl_shader_t> GL_shader;
opengl_shader_t Deferred_light_shader;
opengl_shader_t Deferred_clear_shader;

opengl_shader_t Ambient_occlusion_shader;

SCP_vector<opengl_shader_t> GL_effect_shaders;

static char *GLshader_info_log = NULL;
static const int GLshader_info_log_size = 8192;
GLuint Framebuffer_fallback_texture_id = 0;

static int GL_anim_effect_num = 0;
static float GL_anim_timer = 0.0f;

geometry_sdr_params Geo_transform = {GL_TRIANGLES, GL_TRIANGLE_STRIP, 3};
geometry_sdr_params Particle_billboards = {GL_POINTS, GL_TRIANGLE_STRIP, 4};
geometry_sdr_params Trail_billboards = {GL_LINES, GL_TRIANGLE_STRIP, 4};

geometry_sdr_params *Current_geo_sdr_params = NULL;

/**
 * Static lookup reference for main shader uniforms
 * When adding a new SDR_ flag, list all associated uniforms and attributes here
 */
static opengl_shader_uniform_reference_t GL_Uniform_Reference_Main[] = {
	{ SDR_FLAG_LIGHT,		2, {"n_lights", "light_factor"}, 0, {}, 0, {}, "Lighting" },
	{ SDR_FLAG_FOG,			0, { NULL }, 0, { NULL }, 0, {}, "Fog Effect" },
	{ SDR_FLAG_DIFFUSE_MAP, 5, {"sBasemap", "desaturate", "desaturate_r", "desaturate_g", "desaturate_b"}, 0, { NULL }, 0, {}, "Diffuse Mapping"},
	{ SDR_FLAG_GLOW_MAP,	1, {"sGlowmap"}, 0, { NULL }, 0, {}, "Glow Mapping" },
	{ SDR_FLAG_SPEC_MAP,	1, {"sSpecmap"}, 0, { NULL }, 0, {}, "Specular Mapping" },
	{ SDR_FLAG_NORMAL_MAP,	1, {"sNormalmap"}, 0, { NULL }, 0, {}, "Normal Mapping" },
	{ SDR_FLAG_HEIGHT_MAP,	1, {"sHeightmap"}, 0, { NULL }, 0, {}, "Parallax Mapping" },
	{ SDR_FLAG_ENV_MAP,		3, {"sEnvmap", "alpha_spec", "envMatrix"}, 0, { NULL }, 0, {}, "Environment Mapping" },
	{ SDR_FLAG_ANIMATED,	5, {"sFramebuffer", "effect_num", "anim_timer", "vpwidth", "vpheight"}, 0, { NULL }, 0, {}, "Animated Effects" },
	{ SDR_FLAG_MISC_MAP,	1, {"sMiscmap"}, 0, { NULL }, 0, {}, "Utility mapping" },
	{ SDR_FLAG_TEAMCOLOR,	2, {"stripe_color", "base_color"}, 0, { NULL }, 0, {}, "Team Colors" },
	{ SDR_FLAG_DEFERRED,	0, { NULL }, 0, { NULL} , 0, {}, "Deferred lighting" },
	{ SDR_FLAG_GEOMETRY,	1, { "shadow_proj_matrix" }, 0, { NULL }, 0, {}, "Geometry Transformation" },
	{ SDR_FLAG_SHADOW_MAP,	2, { "shadow_map_num", "shadow_proj_matrix" }, 0, { NULL }, 0, {}, "Shadow Mapping" },
	{ SDR_FLAG_SHADOWS,		8, { "shadow_map", "shadow_mv_matrix", "shadow_proj_matrix", "model_matrix", "veryneardist", "neardist", "middist", "fardist" }, 0, { NULL }, 0, {}, "Shadows" },
	{ SDR_FLAG_THRUSTER,	1, {"thruster_scale"}, 0, { NULL }, 0, {}, "Thruster scaling" },
	{ SDR_FLAG_TRANSFORM,	2, {"transform_tex", "buffer_matrix_offset"}, 1, {"attrib_model_id"}, 0, { NULL }, "Submodel Transforms" },
	{ SDR_FLAG_CLIP,		4, {"use_clip_plane", "world_matrix", "clip_normal", "clip_position"}, 0, { NULL }, 0, { NULL }, "Clip Plane" }
};

static const int Main_shader_flag_references = sizeof(GL_Uniform_Reference_Main) / sizeof(opengl_shader_uniform_reference_t);

/**
 * Static lookup reference for effect shader uniforms
 */
static opengl_shader_file_t GL_effect_shader_files[] = {

	// soft particles
	{ "effect-v.sdr", "effect-particle-f.sdr", 0, SDR_EFFECT_SOFT_QUAD, 7, {"baseMap", "depthMap", "window_width", "window_height", "nearZ", "farZ", "linear_depth"}, 1, {"radius_in"} },

	// geometry shader soft particles
	{ "effect-v.sdr", "effect-particle-f.sdr", "effect-screen-g.sdr", SDR_EFFECT_SOFT_QUAD | SDR_EFFECT_GEOMETRY, 7, 
		{"baseMap", "depthMap", "window_width", "window_height", "nearZ", "farZ", "linear_depth"}, 2, {"radius_in", "up"}, },

	// distortion effect
	{ "effect-v.sdr", "effect-distort-f.sdr", 0, SDR_EFFECT_DISTORTION, 6, {"baseMap", "window_width", "window_height", "distMap", "frameBuffer", "use_offset"}, 
		1, { "offset_in" } },

	// geometry shader trails
	{ "effect-v.sdr", "effect-f.sdr", "effect-ribbon-g.sdr", SDR_EFFECT_TRAILS | SDR_EFFECT_GEOMETRY, 1, {"baseMap"}, 3, { "fvec", "intensity", "width" } }

	// { char *vert, char *frag, char *geo, int flags, int num_uniforms, char* uniforms[MAX_SHADER_UNIFORMS], int num_attributes, char* attributes[MAX_SDR_ATTRIBUTES] }
};

static const unsigned int Num_effect_shaders = sizeof(GL_effect_shader_files) / sizeof(opengl_shader_file_t);

opengl_shader_t *Current_shader = NULL;


void opengl_shader_check_info_log(GLhandleARB shader_object);

/**
 * Set the currently active shader 
 * @param shader_obj	Pointer to an opengl_shader_t object. This function calls glUseProgramARB with parameter 0 if shader_obj is NULL or if function is called without parameters, causing OpenGL to revert to fixed-function processing 
 */
void opengl_shader_set_current(opengl_shader_t *shader_obj)
{
	if (shader_obj != NULL) {
		if(!Current_shader || (Current_shader->program_id != shader_obj->program_id)) {
			Current_shader = shader_obj;
			vglUseProgramObjectARB(Current_shader->program_id);

#ifndef NDEBUG
			if ( opengl_check_for_errors("shader_set_current()") ) {
				vglValidateProgramARB(Current_shader->program_id);

				GLint obj_status = 0;
				vglGetObjectParameterivARB(Current_shader->program_id, GL_OBJECT_VALIDATE_STATUS_ARB, &obj_status);

				if ( !obj_status ) {
					opengl_shader_check_info_log(Current_shader->program_id);
	
					mprintf(("VALIDATE INFO-LOG:\n"));

					if (strlen(GLshader_info_log) > 5) {
						mprintf(("%s\n", GLshader_info_log));
					} else {
						mprintf(("<EMPTY>\n"));
					}
				}
			}
#endif
		}
	} else {
		Current_shader = NULL;
		vglUseProgramObjectARB(0);
	}
}

/**
 * Given a set of flags, determine whether a shader with these flags exists within the GL_shader vector. If no shader with the requested flags exists, attempt to compile one.
 *
 * @param flags	Integer variable, holding a combination of SDR_* flags
 * @return 		Index into GL_shader, referencing a valid shader, or -1 if shader compilation failed
 */
int gr_opengl_maybe_create_shader(unsigned int flags)
{
	if (Use_GLSL < 2)
		return -1;

	size_t idx;
	size_t max = GL_shader.size();

	for (idx = 0; idx < max; idx++) {
		if (GL_shader[idx].flags == flags) {
			return idx;
		}
	}

	// If we are here, it means we need to compile a new shader
	opengl_compile_main_shader(flags);
	if (GL_shader.back().flags == flags)
		return (int)GL_shader.size() - 1;

	// If even that has failed, bail
	return -1;
}

int opengl_shader_get_effect_shader(uint flags)
{
	if (Use_GLSL < 2)
		return -1;

	size_t max = GL_effect_shaders.size();

	for ( size_t i = 0; i < max; ++i ) {
		if ( GL_effect_shaders[i].flags == flags ) {
			return i;
		}
	}

	return -1;
}

/**
 * Go through GL_shader and call glDeleteObject() for all created shaders, then clear GL_shader
 */
void opengl_shader_shutdown()
{
	size_t i;

	if ( !Use_GLSL ) {
		return;
	}

	for (i = 0; i < GL_shader.size(); i++) {
		if (GL_shader[i].program_id) {
			vglDeleteObjectARB(GL_shader[i].program_id);
			GL_shader[i].program_id = 0;
		}

		GL_shader[i].uniforms.clear();
		GL_shader[i].attributes.clear();
		GL_shader[i].uniform_blocks.clear();
	}

	for (i = 0; i < GL_effect_shaders.size(); i++) {
		if (GL_effect_shaders[i].program_id) {
			vglDeleteObjectARB(GL_effect_shaders[i].program_id);
			GL_effect_shaders[i].program_id = 0;
		}

		GL_effect_shaders[i].uniforms.clear();
		GL_effect_shaders[i].attributes.clear();
	}

	GL_shader.clear();
	GL_effect_shaders.clear();

	if (GLshader_info_log != NULL) {
		vm_free(GLshader_info_log);
		GLshader_info_log = NULL;
	}
}

/**
 * Load a shader file from disc or from the builtin defaults in def_files.cpp if none can be found.
 * This function will also create a list of preprocessor defines for the GLSL compiler based on the shader flags
 * and the supported GLSL version as reported by the GPU driver.
 *
 * @param filename	C-string holding the filename (with extension) of the shader file
 * @param flags		integer variable holding a combination of SDR_* flags
 * @return			C-string holding the complete shader source code
 */
static char *opengl_load_shader(char *filename, int flags)
{
	SCP_string sflags;
    
    if (Use_GLSL >= 4) {
		sflags += "#define SHADER_MODEL 4\n";
	} else if (Use_GLSL == 3) {
		sflags += "#define SHADER_MODEL 3\n";
	} else {
		sflags += "#define SHADER_MODEL 2\n";
	}

#ifdef __APPLE__
	sflags += "#define APPLE\n";
#endif

	if (flags & SDR_FLAG_DIFFUSE_MAP) {
		sflags += "#define FLAG_DIFFUSE_MAP\n";
	}

	if (flags & SDR_FLAG_ENV_MAP) {
		sflags += "#define FLAG_ENV_MAP\n";
	}

	if (flags & SDR_FLAG_FOG) {
		sflags += "#define FLAG_FOG\n";
	}

	if (flags & SDR_FLAG_GLOW_MAP) {
		sflags += "#define FLAG_GLOW_MAP\n";
	}

	if (flags & SDR_FLAG_HEIGHT_MAP) {
		sflags += "#define FLAG_HEIGHT_MAP\n";
	}

	if (flags & SDR_FLAG_LIGHT) {
		sflags += "#define FLAG_LIGHT\n";
	}

	if (flags & SDR_FLAG_NORMAL_MAP) {
		sflags += "#define FLAG_NORMAL_MAP\n";
	}

	if (flags & SDR_FLAG_SPEC_MAP) {
		sflags += "#define FLAG_SPEC_MAP\n";
	}

	if (flags & SDR_FLAG_ANIMATED) {
		sflags += "#define FLAG_ANIMATED\n";
	}
	
	if (flags & SDR_FLAG_TRANSFORM) {
		sflags += "#define FLAG_TRANSFORM\n";
	}

	if (flags & SDR_FLAG_MISC_MAP) {
		sflags += "#define FLAG_MISC_MAP\n";
	}

	if (flags & SDR_FLAG_DEFERRED) {
		sflags += "#define FLAG_DEFERRED\n";
	}

	if (flags & SDR_FLAG_TEAMCOLOR) {
		sflags += "#define FLAG_TEAMCOLOR\n";
	}
    
    if(flags & SDR_FLAG_GEOMETRY) {
        sflags += "#define FLAG_GEOMETRY\n";
    }
    
	if (flags & SDR_FLAG_SHADOW_MAP) {
		sflags += "#define FLAG_SHADOW_MAP\n";
	}

	if (flags & SDR_FLAG_SHADOWS) {
		sflags += "#define FLAG_SHADOWS\n";
	}

	if (flags & SDR_FLAG_THRUSTER) {
		sflags += "#define FLAG_THRUSTER\n";
	}

	if (flags & SDR_FLAG_CLIP) {
		sflags += "#define FLAG_CLIP\n";
	}

	const char *shader_flags = sflags.c_str();
	int flags_len = strlen(shader_flags);

	if (Enable_external_shaders) {
		CFILE *cf_shader = cfopen(filename, "rt", CFILE_NORMAL, CF_TYPE_EFFECTS);
	
		if (cf_shader != NULL) {
			int len = cfilelength(cf_shader);
			char *shader = (char*) vm_malloc(len + flags_len + 1);

			strcpy(shader, shader_flags);
			memset(shader + flags_len, 0, len + 1);
			cfread(shader + flags_len, len + 1, 1, cf_shader);
			cfclose(cf_shader);

			return shader;	
		}
	}

	//If we're still here, proceed with internals
	mprintf(("   Loading built-in default shader for: %s\n", filename));
	char* def_shader = defaults_get_file(filename);
	size_t len = strlen(def_shader);
	char *shader = (char*) vm_malloc(len + flags_len + 1);

	strcpy(shader, shader_flags);
	strcat(shader, def_shader);

	return shader;
}

/**
 * Compiles a new shader, and creates an opengl_shader_t that will be put into the GL_shader vector
 * if compilation is successful.
 * This function is used for main (i.e. model rendering) and particle shaders, post processing shaders use their own infrastructure
 *
 * @param flags		Combination of SDR_* flags
 */
void opengl_compile_main_shader(unsigned int flags) {
	char *vert = NULL, *frag = NULL, *geom = NULL;

	mprintf(("Compiling new shader:\n"));

	bool in_error = false;
	opengl_shader_t new_shader;

	// choose appropriate files
	char vert_name[NAME_LENGTH];
	char geom_name[NAME_LENGTH];
	char frag_name[NAME_LENGTH];

	strcpy_s( vert_name, "main-v.sdr");
	strcpy_s( frag_name, "main-f.sdr");

	// read vertex shader
	if ( (vert = opengl_load_shader(vert_name, flags)) == NULL ) {
		in_error = true;
		goto Done;
	}

	// read fragment shader
	if ( (frag = opengl_load_shader(frag_name, flags)) == NULL ) {
		in_error = true;
		goto Done;
	}

	if( flags & SDR_FLAG_GEOMETRY ) {
		strcpy_s( geom_name, "main-g.sdr");
		Current_geo_sdr_params = &Geo_transform;

		// read geometry shader
		geom = opengl_load_shader(geom_name, flags);
	} else {
		Current_geo_sdr_params = NULL;
	}

	Verify( vert != NULL );
	Verify( frag != NULL );
	
	new_shader.program_id = opengl_shader_create(vert, frag, geom);

	if ( !new_shader.program_id ) {
		in_error = true;
		goto Done;
	}

	new_shader.flags = flags;

	opengl_shader_set_current( &new_shader );
	
	mprintf(("Shader features:\n"));

	//Init all the uniforms
	for (int j = 0; j < Main_shader_flag_references; j++) {
		if (new_shader.flags & GL_Uniform_Reference_Main[j].flag) {
			if (GL_Uniform_Reference_Main[j].num_uniforms > 0) {
				for (int k = 0; k < GL_Uniform_Reference_Main[j].num_uniforms; k++) {
					opengl_shader_init_uniform( GL_Uniform_Reference_Main[j].uniforms[k] );
				}
			}

			if (GL_Uniform_Reference_Main[j].num_attributes > 0) {
				for (int k = 0; k < GL_Uniform_Reference_Main[j].num_attributes; k++) {
					opengl_shader_init_attribute( GL_Uniform_Reference_Main[j].attributes[k] );
				}
			}

			if (GL_Uniform_Reference_Main[j].num_uniform_blocks > 0) {
				for (int k = 0; k < GL_Uniform_Reference_Main[j].num_uniform_blocks; k++) {
					opengl_shader_init_uniform_block( GL_Uniform_Reference_Main[j].uniform_blocks[k] );
				}
			}

			mprintf(("   %s\n", GL_Uniform_Reference_Main[j].name));
		}
	}

	opengl_shader_set_current();

	// add it to our list of embedded shaders
	GL_shader.push_back( new_shader );

Done:
	if (vert != NULL) {
		vm_free(vert);
		vert = NULL;
	}

	if (frag != NULL) {
		vm_free(frag);
		frag = NULL;
	}

	if (in_error) {
		// shut off relevant usage things ...
		bool dealt_with = false;

		if (flags & SDR_FLAG_HEIGHT_MAP) {
			mprintf(("  Shader in_error!  Disabling height maps!\n"));
			Cmdline_height = 0;
			dealt_with = true;
		}

		if (flags & SDR_FLAG_NORMAL_MAP) {
			mprintf(("  Shader in_error!  Disabling normal maps and height maps!\n"));
			Cmdline_height = 0;
			Cmdline_normal = 0;
			dealt_with = true;
		}

		if (!dealt_with) {
			if (flags == 0) {
				mprintf(("  Shader in_error!  Disabling GLSL!\n"));

				Use_GLSL = 0;
				Cmdline_height = 0;
				Cmdline_normal = 0;

				GL_shader.clear();
			} else {
				// We died on a lighting shader, probably due to instruction count.
				// Drop down to a special var that will use fixed-function rendering
				// but still allow for post-processing to work
				mprintf(("  Shader in_error!  Disabling GLSL model rendering!\n"));
				Use_GLSL = 1;
				Cmdline_height = 0;
				Cmdline_normal = 0;
			}
		}
	}
}

static char *opengl_load_effect_shader(char *filename, int flags)
{
	SCP_string sflags;

	if (Use_GLSL >= 4) {
		sflags += "#define SHADER_MODEL 4\n";
	} else if (Use_GLSL == 3) {
		sflags += "#define SHADER_MODEL 3\n";
	} else {
		sflags += "#define SHADER_MODEL 2\n";
	}

	if ( flags & SDR_EFFECT_GEOMETRY ) {
		sflags += "#define FLAG_EFFECT_GEOMETRY\n";
	}

	if ( flags & SDR_EFFECT_TRAILS ) {
		sflags += "#define FLAG_EFFECT_TRAILS\n";
	}

	if ( flags & SDR_EFFECT_LINEAR_DEPTH ) {
		sflags += "#define FLAG_EFFECT_LINEAR_DEPTH\n";
	}

	if ( flags & SDR_EFFECT_SOFT_QUAD ) {
		sflags += "#define FLAG_EFFECT_SOFT_QUAD\n";
	}

	if ( flags & SDR_EFFECT_DISTORTION ) {
		sflags += "#define FLAG_EFFECT_DISTORTION\n";
	}

	const char *shader_flags = sflags.c_str();
	int flags_len = strlen(shader_flags);

	if ( Enable_external_shaders ) {
		CFILE *cf_shader = cfopen(filename, "rt", CFILE_NORMAL, CF_TYPE_EFFECTS);

		if (cf_shader != NULL  ) {
			int len = cfilelength(cf_shader);
			char *shader = (char*) vm_malloc(len + flags_len + 1);

			strcpy(shader, shader_flags);
			memset(shader + flags_len, 0, len + 1);
			cfread(shader + flags_len, len + 1, 1, cf_shader);
			cfclose(cf_shader);

			return shader;
		} 
	}

	mprintf(("   Loading built-in default shader for: %s\n", filename));
	char* def_shader = defaults_get_file(filename);
	size_t len = strlen(def_shader);
	char *shader = (char*) vm_malloc(len + flags_len + 1);

	strcpy(shader, shader_flags);
	strcat(shader, def_shader);

	return shader;
}

void opengl_shader_init_effects()
{
	char *vert = NULL, *frag = NULL, *geo = NULL;

	for ( int i = 0; i < Num_effect_shaders; ++i ) {
		bool in_error = false;
		opengl_shader_t new_shader;
		opengl_shader_file_t *shader_file = &GL_effect_shader_files[i];

		// choose appropriate files
		char *vert_name = shader_file->vert;
		char *frag_name = shader_file->frag;
		char *geo_name = shader_file->geo;

		mprintf(("  Compiling effect shader %d ... \n", i+1));

		// read vertex shader
		if ( (vert = opengl_load_effect_shader(vert_name, shader_file->flags) ) == NULL ) {
			in_error = true;
			goto Done;
		}

		// read fragment shader
		if ( (frag = opengl_load_effect_shader(frag_name, shader_file->flags) ) == NULL ) {
			in_error = true;
			goto Done;
		}

		// read geometry shader
		if ( geo_name != NULL ) {
			if ( !Is_Extension_Enabled(OGL_EXT_GEOMETRY_SHADER4) ) {
				goto Done;
			}

			if ( (geo = opengl_load_effect_shader(geo_name, shader_file->flags) ) == NULL ) {
				in_error = true;
				goto Done;
			}

			// hack in order to get the proper geometry shader parameters using the EXT extension
			if ( shader_file->flags & SDR_EFFECT_TRAILS ) {
				Current_geo_sdr_params = &Trail_billboards;
			} else {
				Current_geo_sdr_params = &Particle_billboards;
			}
		}

		Verify( vert != NULL );
		Verify( frag != NULL );

		new_shader.program_id = opengl_shader_create(vert, frag, geo);

		if ( !new_shader.program_id ) {
			in_error = true;
			goto Done;
		}

		new_shader.flags = shader_file->flags;

		opengl_shader_set_current( &new_shader );

		new_shader.uniforms.reserve(shader_file->num_uniforms);

		for (int j = 0; j < shader_file->num_uniforms; j++) {
			opengl_shader_init_uniform( shader_file->uniforms[j] );
		}

		for (int j = 0; j < shader_file->num_attributes; j++) {
			opengl_shader_init_attribute( shader_file->attributes[j] );
		}

		opengl_shader_set_current();

		GL_effect_shaders.push_back(new_shader);

	Done:
		if (vert != NULL) {
			vm_free(vert);
			vert = NULL;
		}

		if (frag != NULL) {
			vm_free(frag);
			frag = NULL;
		}

		if (geo != NULL) {
			vm_free(geo);
			geo = NULL;
		}
	}
}

/**
 * Initializes the shader system. Creates a 1x1 texture that can be used as a fallback texture when framebuffer support is missing.
 * Also compiles the shaders used for particle rendering.
 */
void opengl_shader_init()
{
	if ( !Use_GLSL ) {
		return;
	}

	glGenTextures(1,&Framebuffer_fallback_texture_id);
	GL_state.Texture.SetActiveUnit(0);
	GL_state.Texture.SetTarget(GL_TEXTURE_2D);
	GL_state.Texture.Enable(Framebuffer_fallback_texture_id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	GLuint pixels[4] = {0,0,0,0};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, &pixels);

	if (Cmdline_no_glsl_model_rendering) {
		Use_GLSL = 1;
	}

	GL_shader.clear();
	
	// Reserve 32 shader slots. This should cover most use cases in real life.
	GL_shader.reserve(32);

	opengl_shader_init_effects();

	opengl_shader_compile_ambient_occlusion_shader();
	opengl_shader_compile_deferred_light_shader();
	opengl_shader_compile_deferred_light_clear_shader();
	mprintf(("\n"));
}

/**
 * Retrieve the compilation log for a given shader object, and store it in the GLshader_info_log global variable
 *
 * @param shader_object		OpenGL handle of a shader object
 */
void opengl_shader_check_info_log(GLhandleARB shader_object)
{
	if (GLshader_info_log == NULL) {
		GLshader_info_log = (char *) vm_malloc(GLshader_info_log_size);
	}

	memset(GLshader_info_log, 0, GLshader_info_log_size);

	vglGetInfoLogARB(shader_object, GLshader_info_log_size-1, 0, GLshader_info_log);
}

/**
 * Pass a GLSL shader source to OpenGL and compile it into a usable shader object.
 * Prints compilation errors (if any) to the log.
 * Note that this will only compile shaders into objects, linking them into executables happens later
 *
 * @param shader_source		GLSL sourcecode for the shader
 * @param shader_type		OpenGL ID for the type of shader being used, like GL_FRAGMENT_SHADER_ARB, GL_VERTEX_SHADER_ARB
 * @return 					OpenGL handle for the compiled shader object
 */
GLhandleARB opengl_shader_compile_object(const GLcharARB *shader_source, GLenum shader_type)
{
	GLhandleARB shader_object = 0;
	GLint status = 0;

	shader_object = vglCreateShaderObjectARB(shader_type);

	vglShaderSourceARB(shader_object, 1, &shader_source, NULL);
	vglCompileShaderARB(shader_object);

	// check if the compile was successful
	vglGetObjectParameterivARB(shader_object, GL_OBJECT_COMPILE_STATUS_ARB, &status);

	opengl_shader_check_info_log(shader_object);

	// we failed, bail out now...
	if (status == 0) {
		// basic error check
		mprintf(("%s shader failed to compile:\n%s\n", (shader_type == GL_VERTEX_SHADER_ARB) ? "Vertex" : ((shader_type == GL_GEOMETRY_SHADER_EXT) ? "Geometry" : "Fragment"), GLshader_info_log));

		// this really shouldn't exist, but just in case
		if (shader_object) {
			vglDeleteObjectARB(shader_object);
		}

		return 0;
	}

	// we succeeded, maybe output warnings too
	if (strlen(GLshader_info_log) > 5) {
		nprintf(("SHADER-DEBUG", "%s shader compiled with warnings:\n%s\n", (shader_type == GL_VERTEX_SHADER_ARB) ? "Vertex" : ((shader_type == GL_GEOMETRY_SHADER_EXT) ? "Geometry" : "Fragment"), GLshader_info_log));
	}

	return shader_object;
}

/**
 * Link a vertex shader object and a fragment shader object into a usable shader executable.
 * Prints linker errors (if any) to the log.
 * 
 * @param vertex_object		Compiled vertex shader object
 * @param fragment_object	Compiled fragment shader object
 * @return					Shader executable
 */
GLhandleARB opengl_shader_link_object(GLhandleARB vertex_object, GLhandleARB fragment_object, GLhandleARB geometry_object)
{
	GLhandleARB shader_object = 0;
	GLint status = 0;

	shader_object = vglCreateProgramObjectARB();

	if (vertex_object) {
		vglAttachObjectARB(shader_object, vertex_object);
	}

	if (fragment_object) {
		vglAttachObjectARB(shader_object, fragment_object);
	}

	if (geometry_object) {
		vglAttachObjectARB(shader_object, geometry_object);
		
		if ( Current_geo_sdr_params != NULL) {
			vglProgramParameteriEXT((GLuint)shader_object, GL_GEOMETRY_INPUT_TYPE_EXT, Current_geo_sdr_params->input_type);
			vglProgramParameteriEXT((GLuint)shader_object, GL_GEOMETRY_OUTPUT_TYPE_EXT, Current_geo_sdr_params->output_type);
			vglProgramParameteriEXT((GLuint)shader_object, GL_GEOMETRY_VERTICES_OUT_EXT, Current_geo_sdr_params->vertices_out);
		}
	}
	vglLinkProgramARB(shader_object);

	// check if the link was successful
	vglGetObjectParameterivARB(shader_object, GL_OBJECT_LINK_STATUS_ARB, &status);

	opengl_shader_check_info_log(shader_object);

	// we failed, bail out now...
	if (status == 0) {
		mprintf(("Shader failed to link:\n%s\n", GLshader_info_log));

		if (shader_object) {
			vglDeleteObjectARB(shader_object);
		}

		return 0;
	}

	// we succeeded, maybe output warnings too
	if (strlen(GLshader_info_log) > 5) {
		nprintf(("SHADER-DEBUG", "Shader linked with warnings:\n%s\n", GLshader_info_log));
	}

	return shader_object;
}

/**
 * Creates an executable shader.
 *
 * @param vs	Vertex shader source code
 * @param fs	Fragment shader source code
 * @return 		Internal ID of the compiled and linked shader as generated by OpenGL
 */
GLhandleARB opengl_shader_create(const char *vs, const char *fs, const char *gs)
{
	GLhandleARB vs_o = 0;
	GLhandleARB fs_o = 0;
	GLhandleARB gs_o = 0;
	GLhandleARB program = 0;

	if (vs) {
		vs_o = opengl_shader_compile_object( (const GLcharARB*)vs, GL_VERTEX_SHADER_ARB );

		if ( !vs_o ) {
			mprintf(("ERROR! Unable to create vertex shader!\n"));
			goto Done;
		}
	}

	if (fs) {
		fs_o = opengl_shader_compile_object( (const GLcharARB*)fs, GL_FRAGMENT_SHADER_ARB );

		if ( !fs_o ) {
			mprintf(("ERROR! Unable to create fragment shader!\n"));
			goto Done;
		}
	}

	if (gs) {
		gs_o = opengl_shader_compile_object( (const GLcharARB*)gs, GL_GEOMETRY_SHADER_EXT );

		if ( !gs_o ) {
			mprintf(("ERROR! Unable to create fragment shader!\n"));
			goto Done;
		}
	}

	program = opengl_shader_link_object(vs_o, fs_o, gs_o);

	if ( !program ) {
		mprintf(("ERROR! Unable to create shader program!\n"));
	}

Done:
	if (vs_o) {
		vglDeleteObjectARB(vs_o);
	}

	if (fs_o) {
		vglDeleteObjectARB(fs_o);
	}

	if (gs_o) {
		vglDeleteObjectARB(gs_o);
	}

	return program;
}

/**
 * Initialize a shader attribute. Requires that the Current_shader global variable is valid.
 *
 * @param attribute_text	Name of the attribute to be initialized
 */
void opengl_shader_init_attribute(const char *attribute_text)
{
	opengl_shader_uniform_t new_attribute;

	if ( ( Current_shader == NULL ) || ( attribute_text == NULL ) ) {
		Int3();
		return;
	}

	new_attribute.text_id = attribute_text;
	new_attribute.location = vglGetAttribLocationARB(Current_shader->program_id, attribute_text);

	if ( new_attribute.location < 0 ) {
		nprintf(("SHADER-DEBUG", "WARNING: Unable to get shader attribute location for \"%s\"!\n", attribute_text));
		return;
	}

	Current_shader->attributes.push_back( new_attribute );
}

/**
 * Get the internal OpenGL location for a given attribute. Requires that the Current_shader global variable is valid
 *
 * @param attribute_text	Name of the attribute
 * @return					Internal OpenGL location for the attribute
 */
GLint opengl_shader_get_attribute(const char *attribute_text)
{
	if ( (Current_shader == NULL) || (attribute_text == NULL) ) {
		Int3();
		return -1;
	}

	SCP_vector<opengl_shader_uniform_t>::iterator attribute;

	for (attribute = Current_shader->attributes.begin(); attribute != Current_shader->attributes.end(); ++attribute) {
		if ( !attribute->text_id.compare(attribute_text) ) {
			return attribute->location;
		}
	}

	return -1;
}

/**
 * Initialize a shader uniform. Requires that the Current_shader global variable is valid.
 *
 * @param uniform_text		Name of the uniform to be initialized
 */
void opengl_shader_init_uniform(const char *uniform_text)
{
	opengl_shader_uniform_t new_uniform;

	if ( (Current_shader == NULL) || (uniform_text == NULL) ) {
		Int3();
		return;
	}

	new_uniform.text_id = uniform_text;
	new_uniform.location = vglGetUniformLocationARB(Current_shader->program_id, uniform_text);

	if (new_uniform.location < 0) {
		nprintf(("SHADER-DEBUG", "WARNING: Unable to get shader uniform location for \"%s\"!\n", uniform_text));
		return;
	}

	Current_shader->uniforms.push_back( new_uniform );
}

/**
 * Get the internal OpenGL location for a given uniform. Requires that the Current_shader global variable is valid
 *
 * @param uniform_text	Name of the uniform
 * @return				Internal OpenGL location for the uniform
 */
GLint opengl_shader_get_uniform(const char *uniform_text)
{
	if ( (Current_shader == NULL) || (uniform_text == NULL) ) {
		Int3();
		return -1;
	}

	SCP_vector<opengl_shader_uniform_t>::iterator uniform;
	SCP_vector<opengl_shader_uniform_t>::iterator uniforms_end = Current_shader->uniforms.end();
	
	for (uniform = Current_shader->uniforms.begin(); uniform != uniforms_end; ++uniform) {
		if ( !uniform->text_id.compare(uniform_text) ) {
			return uniform->location;
		}
	}

	return -1;
}

/**
 * Initialize a shader uniform. Requires that the Current_shader global variable is valid.
 *
 * @param uniform_text		Name of the uniform to be initialized
 */
void opengl_shader_init_uniform_block(const char *uniform_text)
{
	opengl_shader_uniform_t new_uniform_block;

	if ( (Current_shader == NULL) || (uniform_text == NULL) ) {
		Int3();
		return;
	}

	new_uniform_block.text_id = uniform_text;
	new_uniform_block.location = vglGetUniformBlockIndexARB(Current_shader->program_id, uniform_text);

	if (new_uniform_block.location < 0) {
		nprintf(("SHADER-DEBUG", "WARNING: Unable to get shader uniform block location for \"%s\"!\n", uniform_text));
		return;
	}

	Current_shader->uniform_blocks.push_back( new_uniform_block );
}

/**
 * Get the internal OpenGL location for a given uniform. Requires that the Current_shader global variable is valid
 *
 * @param uniform_text	Name of the uniform
 * @return				Internal OpenGL location for the uniform
 */
GLint opengl_shader_get_uniform_block(const char *uniform_text)
{
	if ( (Current_shader == NULL) || (uniform_text == NULL) ) {
		Int3();
		return -1;
	}

	SCP_vector<opengl_shader_uniform_t>::iterator uniform_block;
	
	for (uniform_block = Current_shader->uniform_blocks.begin(); uniform_block != Current_shader->uniform_blocks.end(); ++uniform_block) {
		if ( !uniform_block->text_id.compare(uniform_text) ) {
			return uniform_block->location;
		}
	}

	return -1;
}

/**
 * Sets the currently active animated effect.
 *
 * @param effect	Effect ID, needs to be implemented and checked for in the shader
 * @param timer		Timer value to be passed to the shader
 */
void gr_opengl_shader_set_animated_effect(int effect, float timer)
{
	GL_anim_effect_num = effect;
	GL_anim_timer = timer;
}

/**
 * Returns the currently active animated effect ID.
 *
 * @return		Currently active effect ID
 */
int opengl_shader_get_animated_effect()
{
	return GL_anim_effect_num;
}

/**
 * Get the timer for animated effects.
 */
float opengl_shader_get_animated_timer()
{
	return GL_anim_timer;
}

/**
 * Compile the deferred light shader.
 */
void opengl_shader_compile_deferred_light_shader()
{
	char *vert = NULL, *frag = NULL;

	mprintf(("Compiling deferred light shader...\n"));

	bool in_error = false;

	// choose appropriate files
	char vert_name[NAME_LENGTH];
	char frag_name[NAME_LENGTH];

	strcpy_s( vert_name, "deferred-v.sdr");
	strcpy_s( frag_name, "deferred-f.sdr");

	// read vertex shader
	if ( (vert = opengl_load_shader(vert_name, 0)) == NULL ) {
		in_error = true;
		goto Done;
	}

	// read fragment shader
	if ( (frag = opengl_load_shader(frag_name, 0)) == NULL ) {
		in_error = true;
		goto Done;
	}

	Verify( vert != NULL );
	Verify( frag != NULL );

	Deferred_light_shader.program_id = opengl_shader_create(vert, frag, NULL);

	if ( !Deferred_light_shader.program_id ) {
		in_error = true;
		goto Done;
	}
	opengl_shader_set_current( &Deferred_light_shader );
	
	//Hardcoded Uniforms
	opengl_shader_init_uniform( "Scale" );
	opengl_shader_init_uniform( "NormalBuffer" );
	opengl_shader_init_uniform( "PositionBuffer" );
	opengl_shader_init_uniform( "SpecBuffer" );
	opengl_shader_init_uniform( "vpwidth" );
	opengl_shader_init_uniform( "vpheight" );
	opengl_shader_init_uniform( "lighttype" );
	opengl_shader_init_uniform( "lightradius" );
	opengl_shader_init_uniform( "diffuselightcolor" );
	opengl_shader_init_uniform( "speclightcolor" );
	opengl_shader_init_uniform( "dual_cone" );
	opengl_shader_init_uniform( "coneDir" );
	opengl_shader_init_uniform( "cone_angle" );
	opengl_shader_init_uniform( "cone_inner_angle" );
	opengl_shader_init_uniform( "spec_factor" );

	vglUniform1iARB( opengl_shader_get_uniform("NormalBuffer"), 0 );
	vglUniform1iARB( opengl_shader_get_uniform("PositionBuffer"), 1 );
	vglUniform1iARB( opengl_shader_get_uniform("SpecBuffer"), 2 );
	vglUniform1fARB( opengl_shader_get_uniform("vpwidth"), 1.0f/gr_screen.max_w );
	vglUniform1fARB( opengl_shader_get_uniform("vpheight"), 1.0f/gr_screen.max_h );
	vglUniform1fARB( opengl_shader_get_uniform("spec_factor"), Cmdline_ogl_spec );


	opengl_shader_set_current();

Done:
	if (vert != NULL) {
		vm_free(vert);
		vert = NULL;
	}

	if (frag != NULL) {
		vm_free(frag);
		frag = NULL;
	}

	if (in_error) {
		// We died on a lighting shader, probably due to instruction count.
		// Drop down to a special var that will use fixed-function rendering
		// but still allow for post-processing to work
		mprintf(("  Shader in_error!  Disabling GLSL model rendering!\n"));
		Use_GLSL = 1;
		Cmdline_height = 0;
		Cmdline_normal = 0;
	}
}

/**
 * Compile the deferred light clear shader.
 */
void opengl_shader_compile_deferred_light_clear_shader()
{
	char *vert = NULL, *frag = NULL;

	mprintf(("Compiling deferred light clear shader...\n"));

	bool in_error = false;

	// choose appropriate files
	char vert_name[NAME_LENGTH];
	char frag_name[NAME_LENGTH];

	strcpy_s( vert_name, "deferred-clear-v.sdr");
	strcpy_s( frag_name, "deferred-clear-f.sdr");

	// read vertex shader
	if ( (vert = opengl_load_shader(vert_name, 0)) == NULL ) {
		in_error = true;
		goto Done;
	}

	// read fragment shader
	if ( (frag = opengl_load_shader(frag_name, 0)) == NULL ) {
		in_error = true;
		goto Done;
	}

	Verify( vert != NULL );
	Verify( frag != NULL );

	Deferred_clear_shader.program_id = opengl_shader_create(vert, frag, NULL);

	if ( !Deferred_clear_shader.program_id ) {
		in_error = true;
		goto Done;
	}
	opengl_shader_set_current( &Deferred_clear_shader );

	opengl_shader_set_current();

Done:
	if (vert != NULL) {
		vm_free(vert);
		vert = NULL;
	}

	if (frag != NULL) {
		vm_free(frag);
		frag = NULL;
	}

	if (in_error) {
		// We died on a lighting shader, probably due to instruction count.
		// Drop down to a special var that will use fixed-function rendering
		// but still allow for post-processing to work
		mprintf(("  Shader in_error!  Disabling GLSL model rendering!\n"));
		Use_GLSL = 1;
		Cmdline_height = 0;
		Cmdline_normal = 0;
	}
}

void opengl_shader_compile_ambient_occlusion_shader()
{
	char *vert = NULL, *frag = NULL;

	mprintf(("Compiling ambient occlusion shader...\n"));

	bool in_error = false;

	// choose appropriate files
	char vert_name[NAME_LENGTH];
	char frag_name[NAME_LENGTH];

	strcpy_s( vert_name, "ao-v.sdr");
	strcpy_s( frag_name, "ao-f.sdr");

	// read vertex shader
	if ( (vert = opengl_load_shader(vert_name, 0)) == NULL ) {
		in_error = true;
		goto Done;
	}

	// read fragment shader
	if ( (frag = opengl_load_shader(frag_name, 0)) == NULL ) {
		in_error = true;
		goto Done;
	}

	Verify( vert != NULL );
	Verify( frag != NULL );

	Ambient_occlusion_shader.program_id = opengl_shader_create(vert, frag, NULL);

	if ( !Ambient_occlusion_shader.program_id ) {
		in_error = true;
		goto Done;
	}
	opengl_shader_set_current( &Ambient_occlusion_shader );

	//Hardcoded Uniforms
	opengl_shader_init_uniform( "normalMap" );
	opengl_shader_init_uniform( "positionMap" );
	opengl_shader_init_uniform( "projScale" );
	opengl_shader_init_uniform( "worldRadius" );
	opengl_shader_init_uniform( "intensityDivR6" );
	opengl_shader_init_uniform( "bias" );

	float intensity = 1.0f;
	float radius = 2.0f;
	
	vglUniform1iARB( opengl_shader_get_uniform("normalMap"), 0 );
	vglUniform1iARB( opengl_shader_get_uniform("positionMap"), 1 );
	//vglUniform1fARB( opengl_shader_get_uniform("projScale"), proj_scale ); // height in pixels of a 1 unit length object viewed from 1 unit away.
	vglUniform1fARB( opengl_shader_get_uniform("worldRadius"), radius ); // world-space AO radius in scene units
	vglUniform1fARB( opengl_shader_get_uniform("intensityDivR6"), intensity / pow(radius, 6) ); // intensity / radius^6
	vglUniform1fARB( opengl_shader_get_uniform("bias"), 0.01f ); // Bias to avoid AO in smooth corners. e.g. 0.01m

	opengl_shader_set_current();

Done:
	if (vert != NULL) {
		vm_free(vert);
		vert = NULL;
	}

	if (frag != NULL) {
		vm_free(frag);
		frag = NULL;
	}

	if (in_error) {
		mprintf(("  Shader in_error!  Disabling ambient occlusion!\n"));
	}
}