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

#include "galogen.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "SDL_events.h"
#include "base.hpp"
#include "clock.hpp"
#include "drawlist.hpp"
#include "galogen.h"
#include "printf.h"
#include "profiler.hpp"
#include "screen.hpp"
#include "shader.hpp"

#include "raymath.h"

#include "memedit.h"

namespace pb {

static const char *vertexShaderSource =
		R"a(#version 330

uniform mat4 ProjMtx;
layout (location = 0) in vec2 Position;
layout (location = 1) in vec2 UV;
layout (location = 2) in float Color;
out vec4 vColor;
out vec2 vUV; 

void main() {
	vColor = vec4(Color, Color, Color, 1.0);
	vUV = UV;
	gl_Position = ProjMtx * vec4(Position, 0.0, 1.0);
}

)a";

static const char *fragmentShaderSource = R"(#version 330

in vec4 vColor;
in vec2 vUV;

uniform float iTime;
uniform sampler2D Texture;

out vec4 FragColor;

void main() {
	FragColor = vColor + texture2D(Texture, vUV.st);
}

)";

static struct __gbuff {
	static constexpr size_t PER_VERTEX = 8;
  std::vector<float> buffdata = std::vector<float>(100*PER_VERTEX); // data to put in VBO

	void flush(GLuint VBO) {
		GL_CALL(glBufferData(GL_ARRAY_BUFFER, buffdata.size() * sizeof(float), buffdata.data(), GL_DYNAMIC_DRAW));
		GL_CALL(glDrawArrays(GL_TRIANGLES, 0, buffdata.size() / PER_VERTEX));
		buffdata.resize(0);
	}

	inline void vertex(float v[PER_VERTEX]) {
		for (size_t i = 0; i < PER_VERTEX; i++) {
			buffdata.push_back(v[i]);
		}
	}

} gbuff;



static class WorldViewScreen : public screen::Screen {
 protected:
	ShaderProgram prog;
	GLuint VBO = 0;
	GLuint EBO = 0;

	GLint AID_ProjMtx = 0;	// AttriblocationInDentifier
	GLint AID_Texture = 0;
	GLint AID_iTime = 0;
 public:
 	pb::VtxDrawList<sizeof(float) * 5> drawlist;
 public:
	// float offsets. They are "virtual" offsets in the boundaries of a chunk.
	float cam_offx = 0, cam_offy = 0, cam_scale = 1.0;
	//int32_t cam_hpos_x = 0, cam_chpos_y = 0; // THE CHUNK OFFSET

	// input
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
	}

 private:
	void UpdateMatUniform() {
		{
			GL_CALL(glUniformMatrix4fv(AID_ProjMtx, 1, GL_FALSE, MatrixToFloatV(curr_matrix).v)); // update Projection matrix
		}
	}

	/// YOU MUST BIND BUFFER BEFORE! AND VAO/VBO ETC.
	/*void DrawRect(float x, float y, float w, float h) {
		gbuff.vertex((float[]){x  , y  , 0.0f, 0.0f, 1.0f, .0f, .0f, 1.0f}); // LT
		gbuff.vertex((float[]){x+w, y  , 0.0f, 0.0f, .0f, 1.0f, .0f, 1.0f}); // RT
		gbuff.vertex((float[]){x+w, y+h, 0.0f, 0.0f, .0f, .0f, 1.0f, 1.0f}); // RB

		gbuff.vertex((float[]){x+w, y+h, 0.0f, 0.0f, .0f, .0f, 1.0f, 1.0f}); // RB
		gbuff.vertex((float[]){x  , y+h, 0.0f, 0.0f, 1.0f, .10f, 1.0f, 1.0f}); // LB
		gbuff.vertex((float[]){x  , y  , 0.0f, 0.0f, 1.0f, .0f, .0f, 1.0f}); // LT
		
	}*/
	void DrawRect(float x, float y, float w, float h, float bright) {
		// correct types are important!
		int LT = drawlist.add_unique_v(x,   y  , 0.0f, 0.0f, bright);
				 drawlist.add_unique_v(x+w, y  , 1.0f, 0.0f, bright);
		int RB = drawlist.add_unique_v(x+w, y+h, 1.0f, 1.0f, bright);
		         drawlist.add_same_vertex(RB);
				 drawlist.add_unique_v(x  , y+h, 0.0f, 1.0f, bright);
				 drawlist.add_same_vertex(LT);
	};

 public:
	void activate() override {
		prog.create();
		if (!CreateShaders(prog, vertexShaderSource, fragmentShaderSource)) {
			// oh no
			LOG_INFO("WE ARE SO FUCKED %i!", (GLint)prog);
			prog.destroy();
			return;
		}

		GL_CALL(glGenBuffers(1, &VBO));
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, VBO));
		GL_CALL(glGenBuffers(1, &EBO));
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, EBO));
		//GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW));
		//GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));

		AID_ProjMtx = prog.FindUniformID("ProjMtx");
		AID_Texture = prog.FindUniformID("Texture");
		AID_iTime = prog.FindUniformID("iTime");
		//AID_Position = prog.FindAttributeID("Position");
		//AID_TexCoord = prog.FindAttributeID("UV");
		//AID_Color = prog.FindAttributeID("Color");

		LOG_INFO("TIME : %i, MAT:%i, PROG : %i", AID_iTime, AID_ProjMtx, (GLint)prog);
		//assert(AID_Position == 0);
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
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, VBO));
		GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO));
		GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, drawlist.item_size(), (void *)0));
		GL_CALL(glEnableVertexAttribArray(0));
		GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, drawlist.item_size(), (void *)(sizeof(float)*2)));
		GL_CALL(glEnableVertexAttribArray(1));
		GL_CALL(glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, drawlist.item_size(), (void *)(sizeof(float)*4)));
		GL_CALL(glEnableVertexAttribArray(2));
		
		DrawRect(tbx, tby, 300, 300, 1.0f);

		for (int i = 0; i < /*30 * 30*/4; i++) {
			float x = (i % 30) * 100;
			float y = (int)(i / 30) * 100;
			DrawRect(x+0, y+0, 90, 90, 0.5f);
		}

		drawlist.flush(VBO, EBO);
		//drawlist.clear();

		GL_CALL(glBindVertexArray(0));
		GL_CALL(glUseProgram(0));
	}

	void deactivate() override {
		prog.destroy();
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);
		drawlist.clear();
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
	static MemoryEditor mem_edit, mem_edit2;
	mem_edit.DrawWindow("Memory Editor : Verticies", bg.drawlist.verticies.Data, bg.drawlist.verticies.size_in_bytes());
	mem_edit2.DrawWindow("Memory Editor : Indicies", bg.drawlist.indicies.Data, bg.drawlist.indicies.size_in_bytes());
	bg.drawlist.clear();
});

};	// namespace pb