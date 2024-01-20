/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * World chunk visualizer
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
namespace pb {

static const char* fragment_shader_code = R"(#version 330
// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;  
out vec4 finalColor; 

vec4 effect(vec4 color, sampler2D tex, vec2 texture_coords) {
	int val = int(texture(tex, texture_coords).r*255.1);
	float kind = float(val & 3)/6.0;
	int type = (val >> 2) & 63;
	float r  = float(type & 3) + kind;
	float g  = float((type >> 2) & 3) + kind;
	float b  = float((type >> 4) & 3) + kind;
	return vec4(r/4.0, g/4.0, b/4.0, 1);
}

void main() {
	finalColor = effect(fragColor, texture0, fragTexCoord);
})";

static const char* vertex_shader_code =
R"(#version 300
precision mediump float;
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragTexCoord;
out vec4 fragColor;
void main()
{
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp*vec4(vertexPosition, 1.0);
})";



};