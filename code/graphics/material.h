#ifndef _MATERIAL_H
#define _MATERIAL_H

#include "globalincs/pstypes.h"
#include "math/vecmat.h"

class uniform_name_manager
{
	typedef SCP_hash_map<SCP_string, uint> name_hash_map;

	uint Num_names;
	name_hash_map Name_table;
public:
	uniform_name_manager();

	uint get_id(const SCP_string &name);
};

extern uniform_name_manager Uniform_name_lookup;

struct uniform_data
{
	// when adding a new data type to support, create a vector for it here
	SCP_vector<int> int_data;
	SCP_vector<float> float_data;
	SCP_vector<vec2d> vec2_data;
	SCP_vector<vec3d> vec3_data;
	SCP_vector<vec4> vec4_data;
	SCP_vector<matrix4> matrix4_data;

	void clear()
	{
		int_data.clear();
		float_data.clear();
		vec2_data.clear();
		vec3_data.clear();
		vec4_data.clear();
		matrix4_data.clear();
	}

	template <class T> SCP_vector<T>& get_array();
	template <> SCP_vector<int>& get_array<int>() { return int_data; }
	template <> SCP_vector<float>& get_array<float>() { return float_data; }
	template <> SCP_vector<vec2d>& get_array<vec2d>() { return vec2_data; }
	template <> SCP_vector<vec3d>& get_array<vec3d>() { return vec3_data; }
	template <> SCP_vector<vec4>& get_array<vec4>() { return vec4_data; }
	template <> SCP_vector<matrix4>& get_array<matrix4>() { return matrix4_data; }

	// add a single value to the data pool
	template <class T> int set_value(const T& val)
	{
		SCP_vector<T>& data = get_array<T>();

		data.push_back(val);

		return (int)data.size() - 1;
	}

	// overwrite an existing single value to the data pool
	template <class T> void set_value(int location, const T& val)
	{
		SCP_vector<T>& data = get_array<T>();

		Assert(location < data.size());

		data[location] = val;
	}

	// add multiple values to the data pool
	template <class T> int set_values(T* val, int size)
	{
		SCP_vector<T>& data = get_array<T>();

		int start_index = data.size();

		for ( size_t i = 0; i < size; i++ ) {
			data.push_back(val[i]);
		}

		return start_index;
	}

	// overwrite multiple existing values to the data pool
	template <class T> void set_values(int location, T* val, int size)
	{
		SCP_vector<T>& data = get_array<T>();

		Assert(location < data.size());

		for (size_t i = 0; i < size; i++) {
			// check to make sure we don't go out of bounds
			if ( location + i < data.size() ) {
				data[location + i] = val[i];
			} else {
				data.push_back(val[i]);
			}
			
		}
	}
};

struct uniform
{
	SCP_string name;

	enum data_type {
		INT,
		FLOAT,
		VEC2,
		VEC3,
		VEC4,
		MATRIX4
	};

	data_type type;
	int index;

	int count;

	template <class T> data_type determine_type();
	template <> data_type determine_type<int>() { return INT; }
	template <> data_type determine_type<float>() { return FLOAT; }
	template <> data_type determine_type<vec2d>() { return VEC2; }
	template <> data_type determine_type<vec3d>() { return VEC3; }
	template <> data_type determine_type<vec4>() { return VEC4; }
	template <> data_type determine_type<matrix4>() { return MATRIX4; }
};

class uniform_block
{
	SCP_vector<uniform> Uniforms;

	uniform_data* Data_store;
	bool Local_data_store;

	bool Compare;

	int find_uniform(const SCP_string& name);

	template <class T> bool compare(uniform& u, const T& val);

	template <> bool compare<int>(uniform& u, const int& val)
	{
		Assert(Data_store != NULL);

		if ( u.type != uniform::INT || u.count != 1) {
			return false;
		}

		return Data_store->int_data[u.index] == val;
	}

	template <> bool compare<float>(uniform& u, const float& val)
	{
		Assert(Data_store != NULL);
		
		if ( u.type != uniform::FLOAT || u.count != 1 ) {
			return false;
		}

		return Data_store->float_data[u.index] == val;
	}

	template <> bool compare<vec2d>(uniform& u, const vec2d& val)
	{
		Assert(Data_store != NULL);
		
		if ( u.type != uniform::VEC2 || u.count != 1 ) {
			return false;
		}

		return vm_vec_equal(Data_store->vec2_data[u.index], val);
	}

	template <> bool compare<vec3d>(uniform& u, const vec3d& val)
	{
		Assert(Data_store != NULL);
		
		if ( u.type == uniform::VEC3 || u.count != 1 ) {
			return false;
		}

		return vm_vec_equal(Data_store->vec3_data[u.index], val);
	}

	template <> bool compare<vec4>(uniform& u, const vec4& val)
	{
		Assert(Data_store != NULL);
		
		if ( u.type == uniform::VEC4 || u.count != 1 ) {
			return false;
		}

		return vm_vec_equal(Data_store->vec4_data[u.index], val);
	}

	template <> bool compare<matrix4>(uniform& u, const matrix4& val)
	{
		Assert(Data_store != NULL);
		
		if ( u.type == uniform::MATRIX4 || u.count != 1 ) {
			return false;
		}

		return vm_matrix_equal(Data_store->matrix4_data[u.index], val);
	}

	template <class T> bool compare(uniform& u, T* val, int count);

	template <> bool compare<matrix4>(uniform& u, matrix4* val, int count)
	{
		Assert(Data_store != NULL);
		
		if ( u.type != uniform::MATRIX4 || u.count != count ) {
			return false;
		}

		for (int i = 0; i < u.count; ++i) {
			if (!vm_matrix_equal(Data_store->matrix4_data[u.index + i], val[i])) {
				return false;
			}
		}

		return true;
	}
public:
	uniform_block(bool compare = false, uniform_data* data_store = NULL);
	~uniform_block();

	template <class T> bool set_value(const SCP_string& name, const T& val)
	{
		Assert(Data_store != NULL);

		uniform::data_type uniform_type = uniform::determine_type<T>();
		int index = find_uniform(name);

		if (index >= 0) {
			uniform& info = Uniforms[index];

			if ( Compare && compare(info, val) ) {
				return false;
			}

			info.type = uniform_type;
			info.count = 1;

			Data_store->set_value(info.index, val);

			return true;
		}

		uniform u;

		u.count = 1;
		u.type = uniform_type;
		u.name = name;
		u.index = Data_store->set_value(val);

		Uniforms.push_back(u);

		return true;
	}

	template <class T> bool set_values(const SCP_string& name, T* val, int size)
	{
		Assert(Data_store != NULL);

		uniform::data_type uniform_type = uniform::determine_type<T>();
		int index = find_uniform(name);

		if (index >= 0) {
			uniform& info = Uniforms[index];

			if ( Compare && compare(info, val, size) ) {
				return false;
			}

			info.type = uniform_type;

			if ( info.count < size ) {
				// we're going to overflow so append instead of replace
				info.count = size;
				info.index = Data_store->set_values(val, size);
			} else {
				info.count = size;
				Data_store->set_values(info.index, val, size);
			}

			return true;
		}

		uniform u;

		u.count = size;
		u.type = uniform_type;
		u.name = name;
		u.index = Data_store->set_values(val, size);

		Uniforms.push_back(u);

		return true;
	}

	int num_uniforms() 
	{ 
		return (int)Uniforms.size(); 
	}

	uniform::data_type get_type(int i) 
	{
		Assert(i < (int)Uniforms.size());

		return Uniforms[i].type;
	}

	const SCP_string& get_name(int i)
	{
		Assert(i < (int)Uniforms.size());

		return Uniforms[i].name;
	}

	template <class T> T& get_value(int i)
	{
		int index = Uniforms[i].index;

		Assert(Data_store != NULL);
		Assert(Uniforms[i].type == uniform::determine_type<T>());

		SCP_vector<T>& data = Data_store->get_array<T>();

		return data[index];
	}

	template <class T> T& get_value(int i, int offset)
	{
		int index = Uniforms[i].index;

		Assert(Data_store != NULL);
		Assert(Uniforms[i].type == uniform::determine_type<T>());
		Assert(offset < Uniforms[i].count);

		SCP_vector<T>& data = Data_store->get_array<T>();

		Assert(index + offset < (int)data.size());

		return data[index + offset];
	}
};

class material
{
public:
	enum texture_type {
		TEX_BITMAP_TCACHE,
		TEX_RESOURCE_DEPTH_BUFFER,
		TEX_RESOURCE_TRANSFORM_BUFFER,
		TEX_RESOURCE_EFFECT_TEXTURE,
		TEX_RESOURCE_SHADOW_MAP
	};

	struct texture_unit
	{
		uint slot;
		int bitmap_num;
		texture_type type;

		texture_unit(uint _slot, int _bitmap_num, texture_type _type): 
			slot(_slot), bitmap_num(_bitmap_num), type(_type) {}
	};
private:
	int shader_handle;
	SCP_vector<material::texture_unit> textures;
	uniform_block uniforms;

	void set_texture(uint slot_num, int bitmap_num, texture_type tex_type, const SCP_string &name);
public:
	material();
	material(uniform_data* data_storage);

	void set_shader(int sdr_handle);

	void set_texture_bitmap(uint slot_num, int bitmap_num, const SCP_string &name);
	void set_texture_depth_buffer(uint slot_num, const SCP_string &name);
	void set_texture_transform_buffer(uint slot_num, const SCP_string &name);
	void set_texture_effect_texture(uint slot_num, const SCP_string &name);
	void set_texture_shadow_map(uint slot_num, const SCP_string &name);

	void set_uniform(const SCP_string &name, const int& val);
	void set_uniform(const SCP_string &name, const float& val);
	void set_uniform(const SCP_string &name, const vec2d& val);
	void set_uniform(const SCP_string &name, const vec3d& val);
	void set_uniform(const SCP_string &name, const vec4& val);
	void set_uniform(const SCP_string &name, const matrix4& val);

	uniform_block& get_uniforms();
};

#endif