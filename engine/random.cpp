/*
 * Comtains a lot of thirdparty MIT Licensed/Public domain code
 */

#include "stdbool.h"

/*
 * Original random generator was taken from Luau programming language
 * Copyright (c) 2019-2022 Roblox Corporation
 * Copyright (c) 1994–2019 Lua.org, PUC-Rio.
 * And was highy modified by me, to fit in my purposes

 * Permission is hereby granted, free of charge, to any person
 obtaining a copy of
 * this software and associated documentation files (the "Software"),
 to deal in
 * the Software without restriction, including without limitation the
 rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or
 sell copies
 * of the Software, and to permit persons to whom the Software is
 furnished to do
 * so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be
 included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
 SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE
 * SOFTWARE.
 */

#include <math.h>
#include <time.h>

#include "random.h"
#include "doctest.h"

namespace pb {

uint64_t RNG::next() {
	int64_t old = state;
	state = old * 6364136223846793005ULL + (105 | 1);
	return old;
}

int32_t RNG::get(void) {
	uint64_t old = next();
	uint32_t sft = (((old >> 18u) ^ old) >> 27u);
	uint32_t rot = (old >> 59u);
	return (sft >> rot) | (sft << ((-(int32_t)rot) & 31));
}

double RNG::getn() {
	int32_t v = get();
	uint64_t v2 = (uint64_t)get();
	double rd = ldexp(double(v | (uint64_t(v2) << 32)), -64);
	return rd;
}

void RNG::seed(uint64_t seed) {
	state = 0;
	next();
	state = state ^ seed;
	next();
}

RNG::RNG() { seed((uint64_t)time(NULL) * clock()); }

RNG::RNG(uint64_t s) { seed(s); }

TEST_CASE("RNG") {
	RNG a;
	CHECK(a.get() != a.get());
}

/*
 * END OF MIT-LICENSED CODE!!!!
 */

// noise1234
//
// Author: Stefan Gustavson, 2003-2005
// Contact: stefan.gustavson@liu.se
//
// This code was GPL licensed until February 2011.
// As the original author of this code, I hereby
// release it into the public domain.
// Please feel free to use it for whatever you want.
// Credit is appreciated where appropriate, and I also
// appreciate being told where this code finds any use,
// but you may do as you like.

// This is the new and improved, C(2) continuous interpolant
#define FADE(t) (t * t * t * (t * (t * 6 - 15) + 10))

#define FASTFLOOR(x) (((int)(x) < (x)) ? ((int)x) : ((int)x - 1))
#define LERP(t, a, b) ((a) + (t) * ((b) - (a)))

/*
 * Permutation table. This is just a random jumble of all numbers
 * 0-255, repeated twice to avoid wrapping the index at 255 for each
 * lookup. This needs to be exactly the same for all instances on all
 * platforms, so it's easiest to just keep it as static explicit data.
 * This also removes the need for any initialisation of this class.
 *
 * Note that making this an int[] instead of a char[] might make the
 * code run faster on platforms with a high penalty for unaligned
 * single byte addressing. Intel x86 is generally
 * single-byte-friendly, but some other CPUs are faster with 4-aligned
 * reads. However, a char[] is smaller, which avoids cache trashing,
 * and that is probably the most important aspect on most
 * architectures. This array is accessed a *lot* by the noise
 * functions. A vector-valued noise over 3D accesses it 96 times, and
 * a float-valued 4D noise 64 times. We want this to fit in the cache!
 *
static unsigned char perm[] = {
		151, 160, 137, 91,	90,	 15,	131, 13,	201, 95,	96,	 53,	194,
		233, 7,		225, 140, 36,	 103, 30,	 69,	142, 8,		99,	 37,	240,
		21,	 10,	23,	 190, 6,	 148, 247, 120, 234, 75,	0,	 26,	197,
		62,	 94,	252, 219, 203, 117, 35,	 11,	32,	 57,	177, 33,	88,
		237, 149, 56,	 87,	174, 20,	125, 136, 171, 168, 68,	 175, 74,
		165, 71,	134, 139, 48,	 27,	166, 77,	146, 158, 231, 83,	111,
		229, 122, 60,	 211, 133, 230, 220, 105, 92,	 41,	55,	 46,	245,
		40,	 244, 102, 143, 54,	 65,	25,	 63,	161, 1,		216, 80,	73,
		209, 76,	132, 187, 208, 89,	18,	 169, 200, 196, 135, 130, 116,
		188, 159, 86,	 164, 100, 109, 198, 173, 186, 3,		64,	 52,	217,
		226, 250, 124, 123, 5,	 202, 38,	 147, 118, 126, 255, 82,	85,
		212, 207, 206, 59,	227, 47,	16,	 58,	17,	 182, 189, 28,	42,
		223, 183, 170, 213, 119, 248, 152, 2,		44,	 154, 163, 70,	221,
		153, 101, 155, 167, 43,	 172, 9,	 129, 22,	 39,	253, 19,	98,
		108, 110, 79,	 113, 224, 232, 178, 185, 112, 104, 218, 246, 97,
		228, 251, 34,	 242, 193, 238, 210, 144, 12,	 191, 179, 162, 241,
		81,	 51,	145, 235, 249, 14,	239, 107, 49,	 192, 214, 31,	181,
		199, 106, 157, 184, 84,	 204, 176, 115, 121, 50,	45,	 127, 4,
		150, 254, 138, 236, 205, 93,	222, 114, 67,	 29,	24,	 72,	243,
		141, 128, 195, 78,	66,	 215, 61,	 156, 180, 151, 160, 137, 91,
		90,	 15,	131, 13,	201, 95,	96,	 53,	194, 233, 7,	 225, 140,
		36,	 103, 30,	 69,	142, 8,		99,	 37,	240, 21,	10,	 23,	190,
		6,	 148, 247, 120, 234, 75,	0,	 26,	197, 62,	94,	 252, 219,
		203, 117, 35,	 11,	32,	 57,	177, 33,	88,	 237, 149, 56,	87,
		174, 20,	125, 136, 171, 168, 68,	 175, 74,	 165, 71,	 134, 139,
		48,	 27,	166, 77,	146, 158, 231, 83,	111, 229, 122, 60,	211,
		133, 230, 220, 105, 92,	 41,	55,	 46,	245, 40,	244, 102, 143,
		54,	 65,	25,	 63,	161, 1,		216, 80,	73,	 209, 76,	 132, 187,
		208, 89,	18,	 169, 200, 196, 135, 130, 116, 188, 159, 86,	164,
		100, 109, 198, 173, 186, 3,		64,	 52,	217, 226, 250, 124, 123,
		5,	 202, 38,	 147, 118, 126, 255, 82,	85,	 212, 207, 206, 59,
		227, 47,	16,	 58,	17,	 182, 189, 28,	42,	 223, 183, 170, 213,
		119, 248, 152, 2,		44,	 154, 163, 70,	221, 153, 101, 155, 167,
		43,	 172, 9,	 129, 22,	 39,	253, 19,	98,	 108, 110, 79,	113,
		224, 232, 178, 185, 112, 104, 218, 246, 97,	 228, 251, 34,	242,
		193, 238, 210, 144, 12,	 191, 179, 162, 241, 81,	51,	 145, 235,
		249, 14,	239, 107, 49,	 192, 214, 31,	181, 199, 106, 157, 184,
		84,	 204, 176, 115, 121, 50,	45,	 127, 4,	 150, 254, 138, 236,
		205, 93,	222, 114, 67,	 29,	24,	 72,	243, 141, 128, 195, 78,
		66,	 215, 61,	 156, 180}; */

void NoiseGen::randomize(uint64_t seed) {	 // my extension
	bool done[256] = {0};

	RNG random;
	random.seed(seed);

	for (int i = 0; i < 256; i++) {
		uint8_t val = random();
		while (done[val]) {	 // unique random generation
			val = random();
		}
		perm[i] = val;
		perm[i + 256] = val;
		done[val] = true;
	}
}

//---------------------------------------------------------------------

/*
 * Helper functions to compute gradients-dot-residualvectors (1D to
 * 4D) Note that these generate gradients of more than unit length. To
 * make a close match with the value range of classic Perlin noise,
 * the final noise values need to be rescaled. To match the RenderMan
 * noise in a statistical sense, the approximate scaling values
 * (empirically determined from test renderings) are: 1D noise needs
 * rescaling with 0.188 2D noise needs rescaling with 0.507 3D noise
 * needs rescaling with 0.936 4D noise needs rescaling with 0.87 Note
 * that these noise functions are the most practical and useful signed
 * version of Perlin noise. To return values according to the
 * RenderMan specification from the SL noise() and pnoise() functions,
 * the noise values need to be scaled and offset to [0,1], like this:
 * float SLnoise = (noise3(x,y,z) + 1.0) * 0.5;
 */

float NoiseGen::grad1(int hash, float x) {
	int h = hash & 15;
	float grad = 1.0 + (h & 7);	 // Gradient value 1.0, 2.0, ..., 8.0
	if (h & 8) grad = -grad;		 // and a random sign for the gradient
	return (grad * x);	// Multiply the gradient with the distance
}

float NoiseGen::grad2(int hash, float x, float y) {
	int h = hash & 7;					// Convert low 3 bits of hash code
	float u = h < 4 ? x : y;	// into 8 simple gradient directions,
	float v = h < 4 ? y : x;	// and compute the dot product with (x,y).
	return ((h & 1) ? -u : u) + ((h & 2) ? -2.0 * v : 2.0 * v);
}

float grad3(int hash, float x, float y, float z) {
	int h =
			hash & 15;	// Convert low 4 bits of hash code into 12 simple
	float u =
			h < 8 ? x : y;	// gradient directions, and compute dot product.
	float v = h < 4								 ? y
						: h == 12 || h == 14 ? x
																 : z;	 // Fix repeats at h = 12 to 15
	return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float grad4(int hash, float x, float y, float z, float t) {
	int h =
			hash & 31;	// Convert low 5 bits of hash code into 32 simple
	float u = h < 24
								? x
								: y;	// gradient directions, and compute dot product.
	float v = h < 16 ? y : z;
	float w = h < 8 ? z : t;
	return ((h & 1) ? -u : u) + ((h & 2) ? -v : v) + ((h & 4) ? -w : w);
}

//---------------------------------------------------------------------
/** 1D float Perlin noise, SL "noise()"
 */
float NoiseGen::noise1(float x) {
	int ix0, ix1;
	float fx0, fx1;
	float s, n0, n1;

	ix0 = FASTFLOOR(x);	 // Integer part of x
	fx0 = x - ix0;			 // Fractional part of x
	fx1 = fx0 - 1.0f;
	ix1 = (ix0 + 1) & 0xff;
	ix0 = ix0 & 0xff;	 // Wrap to 0..255

	s = FADE(fx0);

	n0 = grad1(perm[ix0], fx0);
	n1 = grad1(perm[ix1], fx1);
	return 0.188f * (LERP(s, n0, n1));
}

//---------------------------------------------------------------------
/** 1D float Perlin periodic noise, SL "pnoise()"
 */
float NoiseGen::pnoise1(float x, int px) {
	int ix0, ix1;
	float fx0, fx1;
	float s, n0, n1;

	ix0 = FASTFLOOR(x);	 // Integer part of x
	fx0 = x - ix0;			 // Fractional part of x
	fx1 = fx0 - 1.0f;
	ix1 = ((ix0 + 1) % px) &
				0xff;								// Wrap to 0..px-1 *and* wrap to 0..255
	ix0 = (ix0 % px) & 0xff;	// (because px might be greater than 256)

	s = FADE(fx0);

	n0 = grad1(perm[ix0], fx0);
	n1 = grad1(perm[ix1], fx1);
	return 0.188f * (LERP(s, n0, n1));
}

//---------------------------------------------------------------------
/** 2D float Perlin noise.
 */
float NoiseGen::noise2(float x, float y) {
	int ix0, iy0, ix1, iy1;
	float fx0, fy0, fx1, fy1;
	float s, t, nx0, nx1, n0, n1;

	ix0 = FASTFLOOR(x);	 // Integer part of x
	iy0 = FASTFLOOR(y);	 // Integer part of y
	fx0 = x - ix0;			 // Fractional part of x
	fy0 = y - iy0;			 // Fractional part of y
	fx1 = fx0 - 1.0f;
	fy1 = fy0 - 1.0f;
	ix1 = (ix0 + 1) & 0xff;	 // Wrap to 0..255
	iy1 = (iy0 + 1) & 0xff;
	ix0 = ix0 & 0xff;
	iy0 = iy0 & 0xff;

	t = FADE(fy0);
	s = FADE(fx0);

	nx0 = grad2(perm[ix0 + perm[iy0]], fx0, fy0);
	nx1 = grad2(perm[ix0 + perm[iy1]], fx0, fy1);
	n0 = LERP(t, nx0, nx1);

	nx0 = grad2(perm[ix1 + perm[iy0]], fx1, fy0);
	nx1 = grad2(perm[ix1 + perm[iy1]], fx1, fy1);
	n1 = LERP(t, nx0, nx1);

	return 0.507f * (LERP(s, n0, n1));
}

//---------------------------------------------------------------------
/** 2D float Perlin periodic noise.
 */
float NoiseGen::pnoise2(float x, float y, int px, int py) {
	int ix0, iy0, ix1, iy1;
	float fx0, fy0, fx1, fy1;
	float s, t, nx0, nx1, n0, n1;

	ix0 = FASTFLOOR(x);	 // Integer part of x
	iy0 = FASTFLOOR(y);	 // Integer part of y
	fx0 = x - ix0;			 // Fractional part of x
	fy0 = y - iy0;			 // Fractional part of y
	fx1 = fx0 - 1.0f;
	fy1 = fy0 - 1.0f;
	ix1 =
			((ix0 + 1) % px) & 0xff;	// Wrap to 0..px-1 and wrap to 0..255
	iy1 =
			((iy0 + 1) % py) & 0xff;	// Wrap to 0..py-1 and wrap to 0..255
	ix0 = (ix0 % px) & 0xff;
	iy0 = (iy0 % py) & 0xff;

	t = FADE(fy0);
	s = FADE(fx0);

	nx0 = grad2(perm[ix0 + perm[iy0]], fx0, fy0);
	nx1 = grad2(perm[ix0 + perm[iy1]], fx0, fy1);
	n0 = LERP(t, nx0, nx1);

	nx0 = grad2(perm[ix1 + perm[iy0]], fx1, fy0);
	nx1 = grad2(perm[ix1 + perm[iy1]], fx1, fy1);
	n1 = LERP(t, nx0, nx1);

	return 0.507f * (LERP(s, n0, n1));
}

};	// namespace pb