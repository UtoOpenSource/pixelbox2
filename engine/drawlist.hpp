/**
 
 */

#pragma once
#include <stdlib.h>
#include <cassert>
#include "imgui.h"
#include "shader.hpp"
#include "hashmap.hpp" // overkill, but whatever

namespace pb {

namespace typeporn {

// insert rawgod representation of arguments into specified memory
template <class ... Ts>
void _bin_concat(char * begin, char* end, Ts && ... inputs) {
    ([&] {
			char* src = (char*)&inputs; // yep, it is unpacked... i love c++ syntax
      constexpr ptrdiff_t size = sizeof(inputs);

			if(ptrdiff_t(end - begin) < size) // error condition
				assert(0 && "we are out of bounds now");

      memcpy(begin, src, size);
			begin += size; // increment destination position
    } (), ...);
}

};

// i thinked about it an hashmap WILL be overkill. I can optionally add it separatelyy later if there will be a need in it
template <size_t SIZE_PER_VERTEX = sizeof(float) * 8, size_t ALIGNMENT = alignof(float)>
struct VtxDrawList {
	using item_t = struct alignas(ALIGNMENT) {char data[SIZE_PER_VERTEX];}; // any data
  ImVector<item_t> verticies; // "unique" verticies. all per-vertex data goes here
	ImVector<unsigned int> indicies; // index of unique verticies

	void flush(GLuint VBO, GLuint EBO) {
		assert(VBO != 0 && EBO != 0);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		GL_CALL(glBufferData(GL_ARRAY_BUFFER, verticies.size() * SIZE_PER_VERTEX, verticies.Data, GL_DYNAMIC_DRAW));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicies.size() * sizeof(unsigned int), indicies.Data, GL_DYNAMIC_DRAW));
		GL_CALL(glDrawElements(GL_TRIANGLES, indicies.size(), GL_UNSIGNED_INT, 0));
	}

	void clear() {
		verticies.resize(0);
		indicies.resize(0);
	}

	inline unsigned int add_unique_vertex(char data[SIZE_PER_VERTEX]) {
		unsigned int index = (unsigned int)(verticies.size());
		indicies.push_back(index); // add new indicies

		item_t d; 
		memcpy(&d, data, sizeof(SIZE_PER_VERTEX));
		verticies.push_back(d);

		return index; // my index
	}

	template <class... VtxData>
	inline unsigned int _add_unique_v(VtxData&&... args) {
		unsigned int index = (unsigned int)(verticies.size());
		indicies.push_back(index); // add new indicies

		item_t d; 
		typeporn::_bin_concat((char*)&d, (char*)&d + SIZE_PER_VERTEX, std::forward<VtxData>(args)...);
		verticies.push_back(d);

		return index; // my index
	}

	template <class... VtxData>
	inline unsigned int add_unique_v(VtxData&&... v) {
		return _add_unique_v(std::forward<VtxData>(v)...);
	}

	inline void add_same_vertex(unsigned int index) {
		assert(index < (unsigned int)indicies.size()); // sanity check
		indicies.push_back(index); // add new vertex by index
	}

	template <class... VtxData>
	inline void add_rect(float x, float y, float w, float h, VtxData... v) {
		// correct types are important!
		int LT = add_unique_v(x,   y  , 0.0f, 0.0f, v...);
				 add_unique_v(x+w, y  , 1.0f, 0.0f, v...);
		int RB = add_unique_v(x+w, y+h, 1.0f, 1.0f, v...);
		         add_same_vertex(RB);
				 add_unique_v(x  , y+h, 0.0f, 1.0f, v...);
				 add_same_vertex(LT);
	}

	static inline size_t item_size() {
		return SIZE_PER_VERTEX;
	}

};

};

//pb::vtxdrawlist<sizeof(float)*8> a;