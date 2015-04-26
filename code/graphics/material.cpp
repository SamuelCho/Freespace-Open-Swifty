#include "graphics/material.h"

material::material():
shader_handle(-1) 
{
}

void material::set_shader(int sdr_handle)
{
	shader_handle = sdr_handle;
}

void material::set_texture(uint slot_num, int bitmap_num, texture_type tex_type, const SCP_string &name)
{
	texture_unit t_unit(slot_num, bitmap_num, tex_type);

	set_uniform(name, (int)slot_num);

	for ( size_t i = 0; i < textures.size(); ++i ) {
		if ( slot_num == textures[i].slot ) {
			textures[i] = t_unit;
			return;
		}
	}

	textures.push_back(t_unit);
}

void material::set_texture_bitmap(uint slot_num, int bitmap_num, const SCP_string &name)
{
	set_texture(slot_num, bitmap_num, TEX_BITMAP_TCACHE, name);
}

void material::set_texture_depth_buffer(uint slot_num, const SCP_string &name)
{
	set_texture(slot_num, -1, TEX_RESOURCE_DEPTH_BUFFER, name);
}

void material::set_texture_transform_buffer(uint slot_num, const SCP_string &name)
{
	set_texture(slot_num, -1, TEX_RESOURCE_TRANSFORM_BUFFER, name);
}

void material::set_texture_effect_texture(uint slot_num, const SCP_string &name)
{
	set_texture(slot_num, -1, TEX_RESOURCE_EFFECT_TEXTURE, name);
}

void material::set_texture_shadow_map(uint slot_num, const SCP_string &name)
{
	set_texture(slot_num, -1, TEX_RESOURCE_SHADOW_MAP, name);
}

void material::set_uniform(const SCP_string &name, const int& val)
{
	uniforms.set_value(name, val);
}

void material::set_uniform(const SCP_string &name, const float& val)
{
	uniforms.set_value(name, val);
}

void material::set_uniform(const SCP_string &name, const vec2d& val)
{
	uniforms.set_value(name, val);
}

void material::set_uniform(const SCP_string &name, const vec3d& val)
{
	uniforms.set_value(name, val);
}

void material::set_uniform(const SCP_string &name, const vec4& val)
{
	uniforms.set_value(name, val);
}

void material::set_uniform(const SCP_string &name, const matrix4& val)
{
	uniforms.set_value(name, val);
}

void material::set_uniform(const SCP_string &name, matrix4* val, int size)
{
	uniforms.set_values(name, val, size);
}

uniform_block& material::get_uniforms() 
{
	return uniforms;
}