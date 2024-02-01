/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Graphics IMGUI/SDL Setup Wrappers
 * Copyright (C) 2023-2024 UtoECat
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include "base.hpp"
#include "base/string.hpp"
#include "external/imgui.h"
#include "galogen.h"
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <initializer_list>
#include <type_traits>

namespace pb {

namespace backend {

class BackendRAII {
	public:
	BackendRAII() {}
	BackendRAII(const BackendRAII&) = delete;
	BackendRAII(const BackendRAII&&) {};
	virtual ~BackendRAII() = 0;
	virtual bool tick() = 0;
	virtual void clear() = 0;
	virtual void flush() = 0;
};

std::unique_ptr<BackendRAII> init(); // yep

/* Render stuff */

class OpenGLError : public std::runtime_error {
	public:
	using Base = std::runtime_error;
	OpenGLError(const char* a, int b = glGetError()) : Base(format_r("%s (code %i)", a, b)) {}
	OpenGLError(const std::string a, int b = glGetError()) : Base(format_r("%s (code %i)", a.c_str(), b)) {}
};

static inline void GLCheckError() {
	int v = glGetError();
	if (v != 0) OpenGLError("OpenGL error detected!", v);
}

/** Basic OpenGL Object wrapper */
class GLObject {
	protected:
	bool owned = true;
	GLuint object = 0;
	public:
	 GLObject(const GLObject &src) {
		object = src.object;
		owned = false;
	 };
	 GLObject(GLObject && src) {
		object = src.object;
		src.object = 0;
		owned = true;
	 }
	 GLObject &operator=(const GLObject &src) {
		if (this == &src) return *this;
		object = src.object;
		owned = false;
		return *this;
	 }
	 GLObject &operator=(GLObject && src) {
		if (this == &src) return *this;
		object = src.object;
		src.object = 0;
		owned = true;
		return *this;
	 }
	 GLObject();
	 GLObject(std::nullptr_t) {}
	 operator bool() const {return !!object;}
	 GLuint get() const {return object;}
	 public:
	 GLObject(GLuint src) : object(src) {}
	 virtual void release() = 0;
	 ~GLObject();
	 /** destroy opengl object. You can't use any object's methods after that*/
};

enum ShaderType {
	VERTEX = GL_VERTEX_SHADER,
	FRAGMENT = GL_FRAGMENT_SHADER
};

class Shader : public GLObject {
	public:
	 Shader(const Shader&) = default;
	 Shader(Shader&&) = default;
	 Shader& operator=(const Shader&) = default;
	 Shader& operator=(Shader&&) = default;

	 Shader(ShaderType type) : GLObject(glCreateShader(type)) {
		if (!object) OpenGLError("Error creating shader object");
	 }

	void release() {if (object) glDeleteShader(object); object = 0;}
	~Shader() {if (owned) release();}

	std::string errmsg() {
		GLint logSize = 0;
		glGetShaderiv(object, GL_INFO_LOG_LENGTH, &logSize);
		std::string res;
		res.resize(logSize);
		glGetShaderInfoLog(object, logSize, &logSize, &res[0]);
		return res;
	}

	bool compile(const char* src) {
		glShaderSource(object, 1, &src, NULL);
		GLCheckError();
		glCompileShader(object);

		int v;
		glGetShaderiv(object, GL_COMPILE_STATUS, &v);
		return v == GL_TRUE;
	}

	Shader(ShaderType t, const char* src) : Shader(t) {
		if (!compile(src)) {
			OpenGLError(errmsg());
		}
	}
	public:
};

class ShaderProgram : public  GLObject {
	ShaderProgram() : GLObject(glCreateProgram()) {
		if (!object) OpenGLError("Error creating shader program object");
	}
	void release() {if (object) glDeleteProgram(object); object = 0;}
	~ShaderProgram() {if (owned) release();}

	void attach(const Shader& s) {
		glAttachShader(object, s.get());
		GLCheckError();
	}

	std::string errmsg() {
		GLint logSize = 0;
		glGetProgramiv(object, GL_INFO_LOG_LENGTH, &logSize);
		std::string res;
		res.resize(logSize);
		glGetProgramInfoLog(object, logSize, &logSize, &res[0]);
		return res;
	}

	bool link() {
		glLinkProgram(object);
		int v;
		glGetProgramiv(object, GL_LINK_STATUS, &v);
		return v == GL_TRUE;
	}

	void detach(const Shader& s) {
		glDetachShader(object, s.get());
		GLCheckError();
	}

	void use() {
		glUseProgram(object);
	}

	static void unuse() {
		glUseProgram(0);
	}

	ShaderProgram(std::initializer_list<Shader> v) : ShaderProgram() {
		for (auto& i : v) {
			attach(i);
		}
		if (!link()) {
			OpenGLError(errmsg());
		}
		// success
		for (auto& i : v) {
			detach(i);
		}
	}

 public:
	ShaderProgram(const ShaderProgram&) = default;
	ShaderProgram(ShaderProgram&&) = default;
	ShaderProgram& operator=(const ShaderProgram&) = default;
	ShaderProgram& operator=(ShaderProgram&&) = default;
};

enum TextureFormat {
	TF_RED = GL_RED,
	TF_RG  = GL_RG,
	TF_RGB = GL_RGB,
	TF_RGBA = GL_RGBA 
};

class Texture : public GLObject {
	int u_w = 0, u_h = 0; TextureFormat u_f;
	public:
	Texture(const Texture&) = default;
	Texture(Texture&&) = default;
	Texture& operator=(const Texture&) = default;
	Texture& operator=(Texture&&) = default;
	public:
	Texture() : GLObject() {
		glGenTextures(1, &object);
		if (!object) OpenGLError("Error creating texture object");
	}
	void release() {if (object) glDeleteTextures(1, &object); object = 0;}
	~Texture() {if (owned) release();}

	void bind() {
		glBindTexture(GL_TEXTURE_2D, object);
	}

	static void unbind() {
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	static void default_params() {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// no filtering
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	void create(int w, int h, TextureFormat fmt = TF_RGB) {
		bind();
		default_params();
		glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, GL_RG, GL_UNSIGNED_BYTE, nullptr);
		GLCheckError();
		u_w = w; u_h = h; u_f = fmt;
	}

	void create(const char* src, int w, int h, TextureFormat fmt = TF_RGB) {
		bind();
		default_params();
		glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, GL_RG, GL_UNSIGNED_BYTE, src);
		GLCheckError();
		u_w = w; u_h = h; u_f = fmt;
	}

	void subset(const char* src, int x, int y, int w, int h, TextureFormat fmt= TF_RGB) {
		bind();
		if (x + w > u_w || y + h > u_h || u_f != fmt) create(w, h, fmt); // hehe
		glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, fmt, GL_UNSIGNED_BYTE, src);
	}

	int width() const {return u_w;}
	int hieght() const {return u_h;}
	TextureFormat format() const {return u_f;}
};

enum GLType {
	BYTE = GL_UNSIGNED_BYTE,
	CHAR = GL_BYTE,
	SHORT = GL_SHORT,
	USHORT = GL_UNSIGNED_SHORT,
	INT = GL_INT,
	UINT = GL_UNSIGNED_INT,
	FLOAT = GL_FLOAT
};

template <typename T> 
struct gltype_map {
	using has_value = std::false_type;
};

#define _mapping(T, V) \
template <> \
struct gltype_map<T> {\
	using has_value = std::true_type;\
	static constexpr GLType value = V;\
};\

_mapping(char, CHAR);
_mapping(unsigned char, BYTE);
_mapping(short, SHORT);
_mapping(unsigned short, USHORT);
_mapping(int, INT);
_mapping(unsigned int, UINT);
_mapping(float, FLOAT);
#undef _mapping

struct GLTypeEx {
	GLType type;
	unsigned char vec;
	public:
	template <typename T, int n = 1>
	static constexpr GLTypeEx make() {
		static_assert(n > 0 && n <= 4, "incorrent size of a vector!");
		using v = gltype_map<T>;
		static_assert(v::has_value, "type is not supported to be converted int GLtype!");
		return GLTypeEx{v::value, n};
	}
};

class VertexBuffer : public GLObject {
	protected:
	size_t item_size = 0;
	public:
	VertexBuffer(const VertexBuffer&) = default;
	VertexBuffer(VertexBuffer&&) = default;
	VertexBuffer& operator=(const VertexBuffer&) = default;
	VertexBuffer& operator=(VertexBuffer&&) = default;
	public:
	VertexBuffer() : GLObject() {
		glGenBuffers(1, &object);
		glBindBuffer(GL_ARRAY_BUFFER, object);
	}
	void release() {if (object) glDeleteBuffers(1, &object); object = 0;}
	~VertexBuffer() {if (owned) release();}

	void bind() {
		glBindBuffer(GL_ARRAY_BUFFER, object);
	}

	/** @argument isz size of one item 
	  * @argument n   count of items */
	void load(const char* src, size_t isz, size_t n) {
		item_size = isz;
		bind();
		glBufferData(GL_ARRAY_BUFFER, item_size*n, src, GL_STATIC_DRAW);
		GLCheckError();
	}

	size_t get_item_size() const {
		return item_size;
	}

};

class AttributeBuffer : public GLObject{
	public:
	AttributeBuffer(const AttributeBuffer&) = default;
	AttributeBuffer(AttributeBuffer&&) = default;
	AttributeBuffer& operator=(const AttributeBuffer&) = default;	
	AttributeBuffer& operator=(AttributeBuffer&&) = default;
	public:
	AttributeBuffer() : GLObject() {
		glGenVertexArrays(1, &object);
		glBindVertexArray(object);
	}
	void release() {if (object) glDeleteVertexArrays(1, &object); object = 0;}
	~AttributeBuffer() {if (owned) release();}

	void bind() {
		glBindVertexArray(object);
		GLCheckError();
	}

	static void unbind() {
		glBindVertexArray(0);
	}

	void enable(int index) {
		bind();
		glEnableVertexAttribArray(index);
		GLCheckError();
	}

	void disable(int index) {
		bind();
		glDisableVertexAttribArray(index);
	}

	/** @ argumennt spacing offset in bytes in item. Size of item is specidied in load()! 
	  * You MUST bind a VAO before doing this call AND bind VBO(this class) AND load() data to the VBO!
		*/
	void attribute(VertexBuffer& buff, int index, size_t spacing, GLTypeEx type) {
		size_t item_size = buff.get_item_size();
		if (!item_size) OpenGLError("misuse : setting attribute with an empty buffer!", -1);
		buff.bind();
		bind();
		glVertexAttribPointer(index, type.vec, type.type, GL_FALSE, item_size, (void*)(spacing));
		GLCheckError();
	}

};

};

};