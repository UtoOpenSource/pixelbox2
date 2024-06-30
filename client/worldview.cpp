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

#include <cstdio>
#include <iostream>

#include "SDL_events.h"
#include "clock.hpp"
#include "galogen.h"
#include "profiler.hpp"
#include "screen.hpp"
#include "shader.hpp"

namespace pb {

static const char *vertexShaderSource =
		R"a(#version 330

uniform mat4 ProjMtx;
layout (location = 0) in vec3 Position;
//layout (location = 1) in vec2 UV;
//layout (location = 2) in vec3 Color;
out vec3 vColor;
out vec2 vUV;

void main() {
	vColor = vec3(0.5, 1, 0);//Color;
	vUV = vec2(0, 0);//UV;
	gl_Position = ProjMtx * vec4(Position, 1.0);
}

)a";

static const char *fragmentShaderSource = R"(#version 330

in vec3 vColor;
in vec2 vUV;

uniform float iTime;
uniform sampler2D Texture;

out vec4 FragColor;

void main() {
	FragColor = vec4(vColor, 1.0) + texture2D(Texture, vUV.st);
}

)";

typedef struct Matrix {
    float m0, m4, m8, m12;
    float m1, m5, m9, m13; 
    float m2, m6, m10, m14;
    float m3, m7, m11, m15;
} Matrix;

// Taken from raylib https://github.com/raysan5/raylib/blob/master/src/raymath.h
static Matrix MatrixInvert(Matrix mat) {
    Matrix result = {};

    float a00 = mat.m0, a01 = mat.m1, a02 = mat.m2, a03 = mat.m3;
    float a10 = mat.m4, a11 = mat.m5, a12 = mat.m6, a13 = mat.m7;
    float a20 = mat.m8, a21 = mat.m9, a22 = mat.m10, a23 = mat.m11;
    float a30 = mat.m12, a31 = mat.m13, a32 = mat.m14, a33 = mat.m15;

    float b00 = a00*a11 - a01*a10;
    float b01 = a00*a12 - a02*a10;
    float b02 = a00*a13 - a03*a10;
    float b03 = a01*a12 - a02*a11;
    float b04 = a01*a13 - a03*a11;
    float b05 = a02*a13 - a03*a12;
    float b06 = a20*a31 - a21*a30;
    float b07 = a20*a32 - a22*a30;
    float b08 = a20*a33 - a23*a30;
    float b09 = a21*a32 - a22*a31;
    float b10 = a21*a33 - a23*a31;
    float b11 = a22*a33 - a23*a32;

    float invDet = 1.0f/(b00*b11 - b01*b10 + b02*b09 + b03*b08 - b04*b07 + b05*b06);

    result.m0 = (a11*b11 - a12*b10 + a13*b09)*invDet;
    result.m1 = (-a01*b11 + a02*b10 - a03*b09)*invDet;
    result.m2 = (a31*b05 - a32*b04 + a33*b03)*invDet;
    result.m3 = (-a21*b05 + a22*b04 - a23*b03)*invDet;
    result.m4 = (-a10*b11 + a12*b08 - a13*b07)*invDet;
    result.m5 = (a00*b11 - a02*b08 + a03*b07)*invDet;
    result.m6 = (-a30*b05 + a32*b02 - a33*b01)*invDet;
    result.m7 = (a20*b05 - a22*b02 + a23*b01)*invDet;
    result.m8 = (a10*b10 - a11*b08 + a13*b06)*invDet;
    result.m9 = (-a00*b10 + a01*b08 - a03*b06)*invDet;
    result.m10 = (a30*b04 - a31*b02 + a33*b00)*invDet;
    result.m11 = (-a20*b04 + a21*b02 - a23*b00)*invDet;
    result.m12 = (-a10*b09 + a11*b07 - a12*b06)*invDet;
    result.m13 = (a00*b09 - a01*b07 + a02*b06)*invDet;
    result.m14 = (-a30*b03 + a31*b01 - a32*b00)*invDet;
    result.m15 = (a20*b03 - a21*b01 + a22*b00)*invDet;

    return result;
}

static void VectorTransform(float vec[3], Matrix mat)
{
	float x = vec[0];
	float y = vec[1];
	float z = vec[2];

	vec[0] = mat.m0*x + mat.m4*y + mat.m8*z + mat.m12;
	vec[1] = mat.m1*x + mat.m5*y + mat.m9*z + mat.m13;
	vec[2] = mat.m2*x + mat.m6*y + mat.m10*z + mat.m14;
}

static class WorldViewScreen : public screen::Screen {
 protected:
	ShaderProgram prog;
	GLuint VBO = 0;
	GLint AID_ProjMtx = 0;	// AttriblocationInDentifier
	GLint AID_Texture = 0;
	GLint AID_iTime = 0;
	GLint AID_Position = 0;

 public:
	float cam_offx = 0, cam_offy = 0, cam_scale = 1.0;
	bool is_clicked = false;
	int click_x = 0, click_y = 0;
	Matrix curr_matrix;

 public:
	WorldViewScreen() {}

	void UpdateMatrix() {
		float WW = window::width/2.0f *cam_scale;
		float HH = window::height/2.0f *cam_scale;

		// camera points to the center!
		float L = cam_offx - WW;
		float R = (cam_offx + WW);
		float T = cam_offy - HH;
		float B = (cam_offy + HH);
		const Matrix ortho_projection = {
				2.0f / (R - L), 0.0f, 0.0f, 0.0f,
				0.0f, 2.0f / (T - B), 0.0f, 0.0f,
				0.0f, 0.0f, -1.0f, 0.0f,
				(R + L) / (L - R), (T + B) / (B - T), 0.0f, 1.0f,
		};
		curr_matrix = ortho_projection;
	}

 private:
	void UpdateMatUniform() {
		{
			GL_CALL(glUniformMatrix4fv(AID_ProjMtx, 1, GL_FALSE, &curr_matrix.m0));	 // update Projection matrix
		}
	}

	/// YOU MUST BIND BUFFER BEFORE! AND VAO/VBO ETC.
	void DrawRect(float x, float y, float w, float h) {
		float vertices[] = {
				// first triangle
				x,     y, 0.0f,	 // left
				x+w,   y, 0.0f,	 // right
				x+w, y+h, 0.0f,	 // top
				// second triangle
				x,   y+h, 0.0f,	 // left
				x,     y, 0.0f,	 // left
		};
		GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW));
		GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, 5));
	}

 public:
	void activate() override {
		prog.create();
		if (!CreateShaders(prog, vertexShaderSource, fragmentShaderSource)) {
			// oh no
			prog.destroy();
		}

		// stuff
		float st = 30;
		float ed = 100;
		float vertices[] = {
				// first triangle
				st, st, 0.0f,	 // left
				ed, st, 0.0f,	 // right
				ed, ed, 0.0f,	 // top
				// second triangle
				st, ed, 0.0f,	 // left
				st, st, 0.0f,	 // left
		};

		GL_CALL(glGenBuffers(1, &VBO));
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, VBO));
		GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW));
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));

		AID_ProjMtx = prog.FindUniformID("ProjMtx");
		AID_Texture = prog.FindUniformID("Texture");
		AID_iTime = prog.FindUniformID("iTime");
		AID_Position = prog.FindAttributeID("Position");

		LOG_INFO("TIME : %i, PROG : %i", AID_iTime, (GLint)prog);
		assert(AID_Position == 0);
		// if (AID_Texture > 0) {GL_CALL(glUniform1i(AID_Texture, 0));}; // once

		UpdateMatrix(); // set new defaults
	}

	void redraw() override {
		{
			PROFILING_SCOPE("glUseProgram 2")
			GL_CALL(glUseProgram(prog));
		}
		VAOScope VAO;
		UpdateMatrix(); // request update
		UpdateMatUniform();

		// GL_CALL(glUniform2f(1, pb::window::width, pb::window::height));
		if (AID_iTime > 0) {
			GL_CALL(glUniform1f(AID_iTime, pb::__clocksource.time()));
		};

		/// GET READY FOR DRAWRECT!
		GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0));
		GL_CALL(glEnableVertexAttribArray(0));
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, VBO));
		
		DrawRect(100, 100, 300, 300);

		for (int i = 0; i < 30 * 30; i++) {
			float x = (i % 30) * 100;
			float y = (int)(i / 30) * 100;
			DrawRect(x+500, y+500, 90, 90);
		}

		GL_CALL(glBindVertexArray(0));
		GL_CALL(glUseProgram(0));
	}

	void deactivate() override {
		prog.destroy();
		glDeleteBuffers(1, &VBO);
	}

	void input(SDL_Event &e) override {
		if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == 1) {
			is_clicked = true;
			click_x = e.button.x;
			click_y = e.button.y;
			LOG_INFO("CLICK");
		}
		if (e.type == SDL_MOUSEBUTTONUP && e.button.button == 1) {
			is_clicked = false;
			LOG_INFO("UNCKICK");
		}
		if (e.type == SDL_MOUSEMOTION && is_clicked) {
			cam_offx += (click_x - e.button.x) * cam_scale;
			click_x = e.button.x;
			cam_offy += (click_y - e.button.y) * cam_scale;
			click_y = e.button.y;
		}
		if (e.type == SDL_MOUSEWHEEL) {
			cam_scale -= e.wheel.preciseY * 0.15 * cam_scale;
		}
	}
} bg;

static bool a = false;
static screen::Register aaasus([](int) {
	if (!a) {
		screen::change(&bg);
		a = true;
	}
});

};	// namespace pb