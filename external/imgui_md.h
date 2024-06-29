/*
 * imgui_md_stateful: Stateful Markdown for Dear ImGui using MD4C
 * (http://https://github.com/mekhontsev/imgui_md) TODO FIX LINK
 *
 * Copyright (c) 2021 Dmitry Mekhontsev
 * COpyright (C) 2024 UtoECat 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * This one chnges parsing process completely - text is parsed into tree, which is will be recursively rendered.
 * This allows us to collapse elements, provide search capabilities, and etc. - good stuff
 */

#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstddef>
#include <string_view>
#include "md4c.h"
#include "imgui.h"
#include <string>
#include <type_traits>
#include <vector>
#include <memory>


#include "magic_enum.hpp"
#define M_ENUM_NAME(v) magic_enum::enum_name(v)

namespace ImGui {

// the real tree. implementation defined, structure may change
namespace impl {
	class MarkdownTree;
};

/** rendeering callback : open url or draw stuff
 * They are passed into MarkdownCallback, but ONY when user allowed specific of them using bit-or on interesting enums and passing them to set_callback()!
 * DO NOT set unnecesarry bits on, because thiis may slow down things, and also break future compatability
 */
enum MarkdownCallbackAction {
	MCA_URL = 1, // open URL (in web browser?)
	MCA_FILE = 1<<2, /// open file (likely markdown? started with a forward slash, yaou may want to append extension in your callback)
	          /// @warning DO NOT EDIT TREE THAT IS DRAWN AT THE CURRENT MOMENT! WAIT FOR DRAWING TO FINISH BEFORE SWITCH AND CLEARUP!
						/// In other case behaviour is undefined.
	MCA_IMAGE = 1<<3, // DRAW image (and open it). Called for visibble images ONLY. Use ImGui functioss for that.
						// You may also want to properly cache loaded images, provide or not provide fallback image, unload all of the images from cache later
	MCA_RESERVED = 1<<8 // Except MORE actions added in the future. For this and greater enum value just do nothing.
};

typedef void(*MarkdownCallback)(MarkdownCallbackAction action, std::string_view url, void* userdata);

// rendering fonts
enum MarkdownFonts {
	MF_NORMAL = 0,
	MF_BOLD,
	MF_ITALIC,
	MF_BOLD_ITALIC,
	MF_MAX_FONTS
};

/**
 * Tree is a REUSABLE storage for markdown AST.
 * At any time you can reparse new markdown into this tree, and draw it.
 * IT IS RECOMMENDED AND ENCOURAGED to parse your document ONCE, and then draw 
 * tree representation many times to reduce parsing cost.
 *
 * Implementation detal : Intyernally tree uses Bump allocator to allocate nodes in a
 * semi-linear way to improve emory usage and reduce traversing cost due to cache misses.
 * 
 * Once again : you REALLY WANT to REUSE trees.
 * This structure is not threadsafe, but you can use as many as you want in independent
 * threads as long as multiple threads don't access the same one at the same time.
 * Also don't switch ImGui Content in thread while drawing or something like that.
 */
class MarkdownTree {
	private:
	 impl::MarkdownTree* impl;
	public:
	// implemented separrately, does dynamic allocation of the tree
	MarkdownTree(size_t prealloc = 0);
	~MarkdownTree(); 
	MarkdownTree(MarkdownTree&& src) {impl = src.impl; src.impl = nullptr;}
	public:
	bool parse(std::string_view md_text);
	// callback on different actions
	void set_callback(MarkdownCallback callback = nullptr, unsigned int callback_allowed_action_flags = 0, void* userdata = nullptr);

	/// different fonts for different text
	/// count must be the LAST font + 1, that was implemented. Leave on 0 to reset fonts and use default one
	/// Set as NULL fonts in between to use default font :)
	/// @warning Fonts array MUST BE VALID UNTIL END OF TREE LIFETIME OR CALL TO set_fonts()! Same goes for all items in the array.
	/// => array is not copied for perfomance reasons
	void set_fonts(const ImFont* fonts[], MarkdownFonts count);

	bool render();
};

};