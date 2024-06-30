/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * View of this beautiful world :3
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

#include <iostream>
#include <cstdio>

#include "galogen.h"
#include "screen.hpp"
#include "profiler.hpp"
#include "clock.hpp"

#define GL_CALL(_CALL)      do { _CALL; GLenum gl_err = glGetError(); if (gl_err != 0) fprintf(stderr, "GL error 0x%x returned from '%s'.\n", gl_err, #_CALL); } while (0)  // Call with error check

namespace pb {

static const char *vertexShaderSource = 
R"a(#version 330
layout (location = 0) in vec2 Position;
layout (location = 1) in vec2 UV;
layout (location = 2) in vec4 Color;
uniform mat4 ProjMtx;
out vec3 vColor;
out vec2 vUV;
void main() {
	vColor = Color;
	vUV = UV;
	gl_Position = ProjMtx * vec4(aPos, 0.0, 1.0);
})a";

static const char *fragmentShaderSource = R"(#version 330
uniform sampler2D Texture;
in vec3 vColor;
in Vec2 vUV;
out vec4 FragColor;
void main() {
	FragColor = vec4(vColor, 1.0) * texture2D(Texture, vUV.st);
}
)"; 

static class WorldViewScreen : public screen::Screen {
 protected:
	GLuint shaderProgram = 0;
	GLuint VAO = 0;
	GLuint VBO = 0;
	GLuint AttribLocationProjMtx = 0;
	GLuint AttribLocationTex = 0;
 public:
	WorldViewScreen() {}

	void activate() override {
		// vertex shader
		unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
		glCompileShader(vertexShader);
		// check for shader compile errors
		int success;
		char infoLog[512];
		glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
			std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
		}
		// fragment shader
		unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
		glCompileShader(fragmentShader);
		// check for shader compile errors
		glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
			std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
		}
		// link shaders
		shaderProgram = glCreateProgram();
		glAttachShader(shaderProgram, vertexShader);
		glAttachShader(shaderProgram, fragmentShader);
		glLinkProgram(shaderProgram);
		// check for linking errors
		glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
		if (!success) {
			glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
			std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
		}
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		// stuff

		float vertices[] = {
        // first triangle
        -1, -1, 0.0f,  // left 
        1, -1, 0.0f,  // right
        1, 1, 0.0f,  // top 
        // second triangle
        -1, 1, 0.0f,  // left
				-1, -1, 0.0f,  // left 
    }; 
		GL_CALL(glGenBuffers(1, &VBO));

    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, VBO));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW));

    // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
	}

	void deactivate() override {
		glDeleteProgram(shaderProgram);
		glDeleteBuffers(1, &VBO);
	}

	void UpdateCamera(float x, float y, float w, float h) {
		//GL_CALL(glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height));
    float L = x;
    float R = x + w;
    float T = y;
    float B = y + h;
    const float ortho_projection[4][4] =
    {
        { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
        { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
        { 0.0f,         0.0f,        -1.0f,   0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
    };
    glUniform1i(AttribLocationTex, 0);
    glUniformMatrix4fv(AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);
	}

	void redraw() override {

		{
			PROFILING_SCOPE("glUseProgram + CreeateVertexArray")
			GL_CALL(glUseProgram(shaderProgram));
    	glGenVertexArrays(1, &VAO);
		}

		GL_CALL(glUniform2f(1, pb::window::width, pb::window::height));
		GL_CALL(glUniform1f(2, pb::__clocksource.time()));

		GL_CALL(glBindVertexArray(VAO)); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized
		GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0));
    GL_CALL(glEnableVertexAttribArray(0));
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, VBO));
    GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, 5)); // set the count to 6 since we're drawing 6 vertices now (2 triangles); not 3!

		GL_CALL(glBindVertexArray(0));
		glDeleteVertexArrays(1, &VAO);
		GL_CALL(glUseProgram(0));
	}
} bg;

static bool a = false;
static screen::Register aaasus([](int) {
	if (!a) {
		//screen::change(&bg);
		a = true;
	}
});

};	// namespace pb