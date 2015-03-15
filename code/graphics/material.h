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
} Uniform_name_lookup;

// base common class for our uniform data containers
class uniform
{
public:
	enum render_resource {
		RESOURCE_SHADOW_PROJ_MATRIX,
		RESOURCE_SHADOW_MODELVIEW_MATRIX,
		RESOURCE_SHADOW_DIST0,
		RESOURCE_SHADOW_DIST1,
		RESOURCE_SHADOW_DIST2,
		RESOURCE_SHADOW_DIST3,
		RESOURCE_ENV_TEXTURE_MATRIX,
		RESOURCE_NUM_LIGHTS,
		RESOURCE_INV_SCREEN_WIDTH,
		RESOURCE_INV_SCREEN_HEIGHT
	};

	// types to tell which type of uniform data is being held
	enum type
	{
		INT,
		FLOAT,
		VEC2,
		VEC3,
		VEC4,
		MATRIX4,
		RENDER_RESOURCE
	};

private:
	type data_type;
	bool array_type;
	bool dirty;
public:
	template<class T> uniform(T* value, bool _is_array = false) :
		data_type(determine_data_type(value)), array_type(_is_array), dirty(false) { }

	type get_data_type() {
		return data_type;
	}

	bool is_array() {
		return array_type;
	}

	bool is_dirty() {
		return dirty;
	}

	void set_dirty(bool val) {
		dirty = val;
	}

	// using function overloading to figure out the type being used in the child classes
	// if you want to add more supported uniform types, add a type to the enum and make a corresponding method here
	static type determine_data_type(int* value) { return INT; }
	static type determine_data_type(float* value) { return FLOAT; }
	static type determine_data_type(vec2d* value) { return VEC2; }
	static type determine_data_type(vec3d* value) { return VEC3; }
	static type determine_data_type(vec4* value) { return VEC4; }
	static type determine_data_type(matrix4* value) { return MATRIX4; }

	// kind of icky but this is just a stop gap solution until we get off our asses and start overloading operators
	// to add support for more uniform types, create a compare method for your type here
	static bool compare(int& a, int& b) { return a == b; }
	static bool compare(float& a, float& b) { return a == b; }
	static bool compare(vec2d& a, vec2d& b) { return vm_vec_equal(a, b); }
	static bool compare(vec3d& a, vec3d& b) { return vm_vec_equal(a, b); }
	static bool compare(vec4& a, vec4& b) { return vm_vec_equal(a, b); }
	static bool compare(matrix4& a, matrix4& b) { return vm_matrix_equal(a, b); }
};

template<class T>
class uniform_data : public uniform
{
	T data;
public:
	uniform_data(T& value) : uniform(&value), data(value) {}

	void set_data(const T& _data) {
		data = _data;

		set_dirty(false);
	}

	const T& get_data() {
		return data;
	}
};

template<class T>
class uniform_array : public uniform
{
	T* buffer;
	int size;
	int capacity;
public:
	uniform_array(const T* _buffer, const int _size) : buffer(NULL), size(0), capacity(0), uniform(_buffer, true) {
		set_data(_buffer, _size);
	}

	const T* get_data() {
		return buffer;
	}

	void set_data(const T* _buffer, const int _size) {
		Assert(_buffer != NULL);
		Assert(_size > 0);

		if (buffer == NULL || capacity < _size) {
			if (buffer != NULL) {
				delete buffer;
			}

			buffer = new T[_size];
			capacity = _size;
		}

		memcpy(buffer, _buffer, sizeof(T) * _size);
		size = _size;

		set_dirty(false);
	}

	void set_data(const T& _data, const int index) {
		Assert(index < size);

		buffer[index] = _data;

		set_dirty(false);
	}

	const int get_size() {
		return size;
	}

	const T& get_data(const int index) {
		Assert(index < size);

		return buffer[index];
	}
};

class uniform_handler
{
public:
	typedef SCP_map<SCP_string, uniform*> uniform_map;
private:
	uniform_map uniforms;

	uniform* find_uniform(SCP_string &name)
	{
		uniform_map::iterator iter = uniforms.find(name);

		if (iter == uniforms.end()) {
			return NULL;
		}

		return iter->second;
	}
public:
	~uniform_handler() {
		uniform_map::iterator iter;

		for (iter = uniforms.begin(); iter != uniforms.end(); ++iter) {
			uniform* container = iter->second;

			if (container != NULL) {
				delete container;
			}

			iter->second = NULL;
		}

		uniforms.clear();
	}

	void reset() {
		// set all uniforms to dirty so we'll always replace them
		uniform_map::iterator iter;

		for (iter = uniforms.begin(); iter != uniforms.end(); ++iter) {
			uniform* container = iter->second;

			container->set_dirty(true);
		}
	}

	uniform_map &get_uniforms() {
		return uniforms;
	}

	// assigns a value to a saved uniform based on name. returns false if nothing was changed.
	template <class T> bool set_value(const SCP_string &name, const T& value) {
		// figure out if this uniform is resident
		uniform* container = find_uniform(name);

		if (container == NULL) {
			// not resident; create a new one.
			uniforms[name] = new uniform_data<T>(value);
			return true;
		}
		else {
			// figure out if this is the same data type and make sure it's not an array uniform
			if (!container->array_type() && container->get_data_type() == uniform::determine_data_type(&value)) {
				uniform_data<T>* current_data = static_cast<uniform_data<T>*>(container);

				// kay, it's the same data type but does it have the same values?
				if (current_data->is_dirty() || !uniform::compare(current_data->get_data(), value)) {
					// nope, replace the stored values.
					current_data->set_data(value);
					return true;
				}
			}
			else {
				// not the same data type. trash and replace the entire container.
				delete container;

				uniforms[name] = new uniform_data<T>(value);
				return true;
			}
		}

		return false;
	}

	// assigns values to a uniform array based on name. returns false if nothing was changed.
	template <class T> bool set_array(const SCP_string &name, const T* buffer, const int size) {
		// figure out if this uniform is resident
		uniform* container = find_uniform(name);

		if (container == NULL) {
			// not resident; create a new one.
			uniforms[name] = new uniform_array<T>(buffer, size);
			return true;
		}
		else {
			// figure out if this is the same data type and make sure it's an array uniform
			if (container->array_type() && container->get_data_type() == uniform::determine_data_type(buffer)) {
				uniform_array<T>* current_data = static_cast<uniform_array<T>*>(container);

				bool equal = true;

				// first check if this uniform is dirty or not the same size
				if (current_data->is_dirty() || current_data->get_size() != size) {
					current_data->set_data(buffer, size);
					return true;
				}

				// it's the same size so compare each element in both buffers
				for (int i = 0; i < size, ++i) {
					const T& value = current_data->get_data(i);

					if (!uniform::compare(buffer[i], value)) {
						current_data->set_data(buffer[i], i);
						equal = false;
					}
				}

				if (!equal) {
					return true;
				}
			}
			else {
				// not the same data type. trash and replace the entire container.
				delete container;
				uniforms[name] = new uniform_array<T>(value);
				return true;
			}
		}

		return false;
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
	uniform_handler uniforms;

	void set_texture(uint slot_num, int bitmap_num, texture_type tex_type, const SCP_string &name);
public:
	material();

	void set_shader(int sdr_handle);

	void set_texture_bitmap(uint slot_num, int bitmap_num, const SCP_string &name);
	void set_texture_depth_buffer(uint slot_num, const SCP_string &name);
	void set_texture_transform_buffer(uint slot_num, const SCP_string &name);
	void set_texture_effect_texture(uint slot_num, const SCP_string &name);
	void set_texture_shadow_map(uint slot_num, const SCP_string &name);

	template <class T> void set_uniform(const SCP_string &name, const T &val) {
		uniforms.set_value(name, val);
	}

	template <class T> void set_uniform(const SCP_string &name, const int size, const T *val) {
		uniforms.set_array(name, size, val);
	}

	uniform_handler& get_uniforms();
};

#endif