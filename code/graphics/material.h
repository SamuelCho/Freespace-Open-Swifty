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

	template <class T> bool compare(int index, const T& val);
	template <class T> bool compare(int index, T* val, int count);

	template <> bool compare<int>(int index, const int& val)
	{
		return int_data[index] == val;
	}

	template <> bool compare<float>(int index, const float& val)
	{
		return fl_equal(float_data[index], val);
	}

	template <> bool compare<vec2d>(int index, const vec2d& val)
	{
		return vm_vec_equal(vec2_data[index], val);
	}

	template <> bool compare<vec3d>(int index, const vec3d& val)
	{
		return vm_vec_equal(vec3_data[index], val);
	}

	template <> bool compare<vec4>(int index, const vec4& val)
	{
		return vm_vec_equal(vec4_data[index], val);
	}

	template <> bool compare<matrix4>(int index, const matrix4& val)
	{
		return vm_matrix_equal(matrix4_data[index], val);
	}

	template <> bool compare<matrix4>(int index, matrix4* val, int count)
	{
		for (int i = 0; i < count; ++i) {
			if (!vm_matrix_equal(matrix4_data[index + i], val[i])) {
				return false;
			}
		}

		return true;
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

	uniform_data *data_src;

	uniform(): data_src(NULL)
	{

	}

	uniform(uniform_data* _data_src):
		data_src(_data_src)
	{

	}

	template <class T> data_type determine_type();
	template <> data_type determine_type<int>() { return INT; }
	template <> data_type determine_type<float>() { return FLOAT; }
	template <> data_type determine_type<vec2d>() { return VEC2; }
	template <> data_type determine_type<vec3d>() { return VEC3; }
	template <> data_type determine_type<vec4>() { return VEC4; }
	template <> data_type determine_type<matrix4>() { return MATRIX4; }

	template <class T>
	void init(const SCP_string& init_name, const T& val)
	{
		name = init_name;
		type = determine_type<T>();
		count = 1;
		index = data_src->set_value(val);
	}

	template <class T>
	void init(const SCP_string &name, T* val, int size)
	{
		name = init_name;
		type = determine_type<T>();
		count = size;
		index = data_src->set_value(val, size);
	}

	template <class T>
	bool update(const T& val)
	{
		data_type uniform_type = determine_type<T>();

		if ( uniform_type == type ) {
			if ( count == 1 && data_src->compare(index, val) ) {
				return false;
			}

			count = 1;

			data_src->set_value(index, val);

			return true;
		}

		count = 1;
		uniform_type = type;
		index = data_src->set_value(val);

		return true;
	}

	template <class T>
	bool update(T* val, int size)
	{
		data_type uniform_type = determine_type<T>();

		if (uniform_type == type) {
			if (count == size && data_src->compare(index, val, size)) {
				return false;
			}

			if (count < size) {
				// we're going to overflow so append instead of replace
				count = size;
				index = data_src->set_values(val, size);
			} else {
				count = size;
				data_src->set_values(index, val, size);
			}

			return true;
		}

		count = size;
		uniform_type = type;
		index = data_src->set_value(val, size);

		return true;
	}
};

class uniform_block
{
	SCP_vector<uniform> Uniforms;

	uniform_data* Data_store;
	bool Local_data_store;

	int find_uniform(const SCP_string& name);
public:
	uniform_block(bool compare = false, uniform_data* data_store = NULL);
	~uniform_block();

	template <class T> bool set_value(const SCP_string& name, const T& val)
	{
		Assert(Data_store != NULL);

		uniform::data_type uniform_type = uniform::determine_type<T>();
		int index = find_uniform(name);

		if (index >= 0) {
			return Uniforms[index].update(val);
		}

		uniform u(Data_store);

		u.init(name, val);

		Uniforms.push_back(u);

		return true;
	}

	template <class T> bool set_values(const SCP_string& name, T* val, int size)
	{
		Assert(Data_store != NULL);

		uniform::data_type uniform_type = uniform::determine_type<T>();
		int index = find_uniform(name);

		if (index >= 0) {
			return Uniforms[index].update(val, size);
		}

		uniform u(Data_store);

		u.init(name, val, size);

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
	material(uniform_data* uniform_data_pool);

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
	void set_uniform(const SCP_string &name, matrix4* val, int size);

	uniform_block& get_uniforms();
};

#endif