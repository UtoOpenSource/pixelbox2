/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * quick shader creation routines and various GL stuff
 * Copyright (C) 2024 UtoECat
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

#include <cassert>
#include <string_view>
#include "galogen.h"
#include "base.hpp"


#define GL_CALL(_CALL)      do { _CALL; GLenum gl_err = glGetError(); if (gl_err != 0) LOG_ERROR("GL error 0x%x returned from '%s'.", gl_err, #_CALL); } while (0)  // Call with error check

namespace pb {

	// vertex or fragment shader
	class ShaderObject {
		public:
		GLuint handle = 0;
		public:
		inline bool create(GLenum shaderType = GL_VERTEX_SHADER) {
			assert(handle == 0);
			handle = glCreateShader(shaderType);
			return handle;
		}
		inline void destroy() {
			if (!handle) return;
			glDeleteShader(handle);
			handle = 0;
		}
		bool compile(std::string_view code) {
			if (!handle) return false;

			const char* data = (code.data());
			const GLsizei size = code.size();
			glShaderSource(handle, 1, &data, &size);
			glCompileShader(handle);

			// check for shader compile errors
			int success;
			char infoLog[512];
			glGetShaderiv(handle, GL_COMPILE_STATUS, &success);
			if (!success) {
				glGetShaderInfoLog(handle, 512, NULL, infoLog);
				LOG_ERROR("ERROR::SHADER_COMPILATION_FAILED %s", infoLog);
			}
			return success;
		}
	};

	class ShaderProgram {
		public:
		GLuint handle = 0;
		bool link_ok = false;
		public:
		ShaderProgram() = default;
		~ShaderProgram() = default;

		bool create() {
			assert(handle == 0);
			handle = glCreateProgram();
			return handle;
		}

		void destroy() {
			if (!handle) return;
			glDeleteProgram(handle);
			handle = 0;
			link_ok = false;
		}

		void attach_object(const ShaderObject& shader) {
			GL_CALL(glAttachShader(handle, shader.handle));
		}

		bool link_program() {
			GL_CALL(glLinkProgram(handle));
			int success = false;
			// check for linking errors
			glGetProgramiv(handle, GL_LINK_STATUS, &success);
			char infoLog[512];
			if (!success) {
				glGetProgramInfoLog(handle, 512, NULL, infoLog);
				LOG_ERROR("Can't link shader program : %s", infoLog);
			}
			link_ok = success;
			return success;
		}

		/// -1 on failture
		GLint FindAttributeID(const char* name) {
    	return glGetAttribLocation(handle, name);
		};

		/// -1 on failture
		GLint FindUniformID(const char* name) {
		 return glGetUniformLocation(handle, name);
		};

		operator GLuint() {
			return handle;
		}

	};

	/// ALL in one routine. You must create program handle before, and you must destroy in manually later
	inline bool CreateShaders(ShaderProgram& dest, std::string_view vertex, std::string_view fragment) {
		if (!dest.handle) return false;

		ShaderObject vert, frag;
		if (!vert.create(GL_VERTEX_SHADER)) goto on_error;
		if (!frag.create(GL_FRAGMENT_SHADER)) goto on_error;

		if (!vert.compile(vertex)) goto on_error;
		if (!frag.compile(fragment)) goto on_error;

		dest.attach_object(vert);
		dest.attach_object(frag);

		if (!dest.link_program()) goto on_error;

		on_error:
		vert.destroy();
		frag.destroy();
		return dest.link_ok;
	}

	/// Scoped VAO
	/// We always create new VAO every frame, because it's most stable solution
	/// and CORE profile REQUIRES us to create VAO, there is no default one, so... yea
	class VAOScope {
		public:
		GLuint VAO = 0;
		// automaticly BEGINS vertex array
		VAOScope() {
			glGenVertexArrays(1, &VAO);
			glBindVertexArray(VAO);
		}
		~VAOScope() {
			glDeleteVertexArrays(1, &VAO);
		}
		operator GLuint() {return VAO;};
	};

};