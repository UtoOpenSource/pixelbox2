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
#include "base.hpp"
#include "clock.hpp"
#include "galogen.h"
#include "profiler.hpp"
#include "screen.hpp"
#include "shader.hpp"

#include "raymath.h"

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

	float tbx = 0, tby = 0;

 public:
	WorldViewScreen() {}

	Matrix GetModelMatrix() {
			float WW = window::width;
		float HH = window::height;
		return GetCameraMatrix2D(cam_offx, cam_offy, 0, 1/cam_scale, WW/2, HH/2);
	}

	void UpdateMatrix() {
		float WW = window::width;
		float HH = window::height;

		// camera points to the center!
		float L = 0;
		float R = (WW);
		float T = 0;
		float B = (HH);

		curr_matrix = MatrixOrtho(L, R, B, T, -1, 1);
		curr_matrix = MatrixMultiply(GetModelMatrix(), curr_matrix);
		//curr_matrix = GetCameraMatrix2D(0, 0, 0, cam_scale, cam_offx, cam_offy);
		static int cc = 0;
		auto arr = MatrixToFloatV(curr_matrix);

		if (cc++ < 400) return;
		cc = 0; 
		printf_("MATRIX:");
		int cnt = 0;
		for (float i : arr.v) {
			printf_("%+.6f, ", i);
			cnt++;
			if (cnt % 4 == 0) printf_("\n\t");
		};
		printf_("\n");
	}

 private:
	void UpdateMatUniform() {
		{
			GL_CALL(glUniformMatrix4fv(AID_ProjMtx, 1, GL_FALSE, MatrixToFloatV(curr_matrix).v)); // update Projection matrix
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
		
		DrawRect(tbx, tby, 300, 300);

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
		if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == 3) {
			Vector3 vec = {(e.button.x)*1.0f, e.button.y*1.0f, 0};
			Matrix inv = MatrixInvert(GetModelMatrix());
			vec = Vector3Transform(vec, inv);
			tbx = vec.x;
			tby = vec.y;
			LOG_INFO("CLICK MID %f %f %f %f", 0.0, 0.0, tbx, tby);
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