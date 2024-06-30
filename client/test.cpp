/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Graphics And screen management Core
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
#include "imgui.h"
#include "screen.hpp"
#include "profiler.hpp"
#include "clock.hpp"

//define GL_CALL(_CALL)      do { _CALL; GLenum gl_err = glGetError(); if (gl_err != 0) fprintf(stderr, "GL error 0x%x returned from '%s'.\n", gl_err, #_CALL); } while (0)  // Call with error check
#include "shader.hpp"

namespace pb {

const char *vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "}\0";

const char *fragmentShaderSource = R"(#version 330 core
#extension GL_ARB_explicit_uniform_location : enable
#extension GL_ARB_explicit_attrib_location : enable

out vec4 fragColor;
layout(location = 2) uniform float iTime;
layout(location = 1) uniform vec2 iResolution;

#define S(a,b,t) smoothstep(a,b,t)

mat2 Rot(float a){
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

// Created by inigo quilez - iq/2014
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
vec2 hash( vec2 p ){
    p = vec2( dot(p,vec2(2127.1,81.17)), dot(p,vec2(1269.5,283.37)) );
	return fract(sin(p)*43758.5453);
}

float noise( in vec2 p ){
    vec2 i = floor( p );
    vec2 f = fract( p );
	
	vec2 u = f*f*(3.0-2.0*f);

    float n = mix( mix( dot( -1.0+2.0*hash( i + vec2(0.0,0.0) ), f - vec2(0.0,0.0) ), 
                        dot( -1.0+2.0*hash( i + vec2(1.0,0.0) ), f - vec2(1.0,0.0) ), u.x),
                   mix( dot( -1.0+2.0*hash( i + vec2(0.0,1.0) ), f - vec2(0.0,1.0) ), 
                        dot( -1.0+2.0*hash( i + vec2(1.0,1.0) ), f - vec2(1.0,1.0) ), u.x), u.y);
	return 0.5 + 0.5*n;
}


void main(){
		vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord/iResolution.xy;
    float ratio = iResolution.x / iResolution.y;

    vec2 tuv = uv;
    tuv -= .5;

    // rotate with Noise
    float degree = noise(vec2(iTime*.1, tuv.x*tuv.y));

    tuv.y *= 1./ratio;
    tuv *= Rot(radians((degree-.5)*720.+180.));
	  tuv.y *= ratio;
    
    // Wave warp with sin
    float frequency = 5.;
    float amplitude = 30.;
    float speed = iTime * 2.;
    tuv.x += sin(tuv.y*frequency+speed)/amplitude;
   	tuv.y += sin(tuv.x*frequency*1.5+speed)/(amplitude*.5);
    
    // draw the image
    vec3 colorYellow = vec3(.957, .824, .623);
    vec3 colorDeepBlue = vec3(.192, .354, .933);
    vec3 layer1 = mix(colorYellow, colorDeepBlue, S(-.3, .2, (tuv*Rot(radians(-5.))).x));
    
    vec3 colorRed = vec3(.910, .310, .8);
    vec3 colorBlue = vec3(0.350, .71, .953);
    vec3 layer2 = mix(colorRed, colorBlue, S(-.3, .2, (tuv*Rot(radians(-5.))).x));
    
    vec3 finalComp = mix(layer1, layer2, S(.5, -.3, tuv.y));
    
    vec3 col = finalComp;
    
    fragColor = vec4(col,1.0);
}
)";

static class Background : public screen::Screen {
 protected:
	//GLuint shaderProgram = 0;
	ShaderProgram prog;
	GLuint VAO = 0;
	GLuint VBO = 0;
 public:
	Background() {}

	void activate() override {
		prog.create();
		if (!CreateShaders(prog, vertexShaderSource, fragmentShaderSource)) {
			// fuck
			prog.destroy();
		}

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
		prog.destroy();
		glDeleteBuffers(1, &VBO);
	}

	void redraw() override {
		{
			PROFILING_SCOPE("glUseProgram")
			GL_CALL(glUseProgram(prog));
		}
		VAOScope VAO; // VAO is autobinded

		GL_CALL(glUniform2f(1, pb::window::width, pb::window::height));
		GL_CALL(glUniform1f(2, pb::__clocksource.time()));

		GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0));
    GL_CALL(glEnableVertexAttribArray(0));
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, VBO));
    GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, 5)); // set the count to 6 since we're drawing 6 vertices now (2 triangles); not 3!

		GL_CALL(glBindVertexArray(0));
		GL_CALL(glUseProgram(0));
	}
} bg;

static bool a = false;
static screen::Register aaa([](int) {
	if (!a) {
		screen::change(&bg);
		a = true;
	}
});

};	// namespace pb