#include "graphics/material.h"

uniform_name_manager Uniform_name_lookup;

uniform_name_manager::uniform_name_manager(): 
Num_names(0)
{
}

uint uniform_name_manager::get_id(const SCP_string &name)
{
	name_hash_map::iterator iter = Name_table.find(name);

	if ( iter == Name_table.end() ) {
		return iter->second;		
	}

	Name_table[name] = Num_names;
	++Num_names;

	return Num_names - 1;
}

uniform_block::uniform_block(bool _compare = false, uniform_data* _data_store = NULL):
Compare(_compare), Data_store(_data_store)
{
	if ( Data_store != NULL ) {
		Local_data_store = false;
	} else {
		// allocate our own data storage
		Data_store = new uniform_data;
		Local_data_store = true;
	}
}

uniform_block::~uniform_block()
{
	if ( Local_data_store && Data_store != NULL ) {
		delete Data_store;
		Data_store = NULL;
	}
}

int uniform_block::find_uniform(const SCP_string& name)
{
	size_t count = Uniforms.size();

	for ( size_t i = 0; i < count; ++i ) {
		uniform& u = Uniforms[i];

		if ( u.name == name ) {
			return i;
		}
	}

	return -1;
}

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