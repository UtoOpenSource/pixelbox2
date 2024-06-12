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

#include <cstddef>
#include <string_view>
#include "base.hpp"
#include "magic_enum.hpp"
#include "md4c.h"
#include "imgui.h"
#include <string>
#include <type_traits>
#include <vector>

namespace ImGui {

class MarkdownParser;

namespace impl {

enum MarkdownItemType {
	MI_BASE_  = 0, // must not exist!! above one too!

	// BLOCK NODE CONTAINERS
	// they are pushd onto stack and inserted into each other

	/// MD_BLOCK_DOC
	MB_DOCUMENT = 1, // first node awailable (root node)

	/// MD_BLOCK_QUOTE
	MB_QUOTE, // quote container

	/// MD_BLOCK_UL
	MB_ULIST, // unordered list. contains LITEM blocks

	/// MD_BLOCK_OL
	MB_OLIST, // ordered list. contains LITEM blocks

	/// MD_BLOCK_TABLE MD_BLOCK_THEAD MD_BLOCK_TBODY
	MB_TABLE, // a complex beast. Contains refs on Column names AND all the rows.

	//
	// Content Blocks - List Item and Table ROW
	// Table row is also used as a column - they are the same in markdown anyway :p

	/// MD_BLOCK_LI
	MB_LITEM, // LIST ITEM.

	/// MD_BLOCK_TR MD_BLOCK_TH MD_BLOCK_TD
	MB_ROW, // individual table ROW. all the rows are in one array

	//
	// Headers and Horisontal Line
	// Horisontal line contain nothing, but it stops all headers
	// Header stop another if level of the new header is less or same

	/// MD_BLOCK_H
	MB_HEADER, // header. HAS TWO ARRAYS :skull: . one for header, second for content

	/// MD_BLOCK_HR
	MB_HLINE, // horisontal line. 

	///
	// Block of code and block of HTML. SElf-contain string data, have no childs :)
	///

	/// MD_BLOCK_CODE
	MB_CODE,

	/// MD_BLOCK_HTML
	MB_HTML,

	///
	// EXPLICIT TEXT BLOCK!
	///

	/// MD_BLOCK_P
	MB_TEXT,

	// SPAN BLOCKS, can't contain BLOCK nodes
	// they are pushed onto stack :)
	MS_ITALIC = 20, // ITALIC MD_SPAN_EM
	MS_BOLD,        // BOLD MD_SPAN_STRONG
	MS_STRIKE,      // STRIKETROUGH OR MD_SPAN_DEL
	MS_UNDERLINE,   // MS_SPAN_U
	MS_CODE,        // STYLE SMALL INLINE CODE
	MS_LINK,        // URL or local link
	MS_IMAGE,       // IMAGE

	// TEXT BLOCKS. have no child nodes?
	// They are not pushed on the stack, but appended to current block on the stack
	MT_NULLCHAR = 30, // NULL CHARACTER
	MT_DATA,          // just a text data
	MT_BRK,           // newline
	MT_CODE,          // TEXT code inside INLINED code block (in multiline it is already combined)
	MT_HTML           // INLINE HTML
};

struct MarkdownItem { // base type
	unsigned char type_comb; // different item types + flags
	public:
	inline enum MarkdownItemType get_type() {return (MarkdownItemType)(type_comb);}
	MarkdownItem(MarkdownItemType t) : type_comb(t) {
		if (t <= MI_BASE_) {
			auto view = magic_enum::enum_name(t);
			LOG_FATAL("assertion! Bad MarkdownItem %.*s", int(view.size()), view.data());
		}
	}
	inline bool is_block() {
		return get_type() >= MB_DOCUMENT && get_type() <= MB_TEXT;
	}
	inline bool is_container() { // subset of blocks
		return get_type() == MB_TEXT ||
			(get_type() >= MB_DOCUMENT && get_type() <= MB_HEADER);
	}
	inline bool is_span() {
		return get_type() >= MS_ITALIC && get_type() <= MS_IMAGE;
	}
	inline bool is_text_data() {
		return get_type() >= MT_NULLCHAR && get_type() <= MT_HTML;
	}
	MarkdownItem() = delete;
	MarkdownItem(const MarkdownItem&) = default;
};

using MarkdownItems = std::vector<MarkdownItem*>;

struct MarkdownGroup : public MarkdownItem {
	MarkdownGroup(MarkdownItemType t) : MarkdownItem(t) {}
	MarkdownItems nodes; // child nodes, each of them allocated on bump heap 
};

template <typename T, enum MarkdownItemType type>
struct MDAuto : public T {
	MDAuto() : T(type) {}
};

struct MBDocument: public MDAuto<MarkdownGroup, MB_DOCUMENT> {};

//
// items
//

struct MBQuote: public MDAuto<MarkdownGroup, MB_QUOTE> {
	bool is_open = true;
};
struct MBUList: public MDAuto<MarkdownGroup, MB_ULIST> {
	char mark = '*';
};

struct MBOList: public MDAuto<MarkdownGroup, MB_OLIST> {
	char mark = '*';
	int start_index = 0;
};

struct MBTable: public MDAuto<MarkdownGroup, MB_TABLE> {
	int columns = 0;
	int rows = 0; // including header, total items = rows * columns == nodes.size()
};

struct MBListItem: public MDAuto<MarkdownGroup, MB_LITEM> {
	char mark = '\0'; // nonzero for task list
};

struct MBTableRow: public MDAuto<MarkdownGroup, MB_ROW> {
	int alignment = 0; // row content alignment
};

/// first TEXT BLOCK inside is threated as title
struct MBHeader : public MDAuto<MarkdownGroup, MB_HEADER> {
	int level = 0; // level of header
	bool is_open = true;
	std::string_view raw_title; // for search
};

struct MBHLine : public MDAuto<MarkdownItem, MB_HLINE> {};
struct MBCode : public MDAuto<MarkdownItem, MB_CODE> {
	std::string_view caption;	 // quote or php, js, etc.
	std::string_view text;		 // code text
};

struct MBHTML : public MDAuto<MarkdownItem, MB_HTML> {
	std::string_view text; // html
};

struct MBText : public MDAuto<MarkdownGroup, MB_TEXT> {
	std::string_view text; // html
};

struct MSItalic : public MDAuto<MarkdownGroup, MS_ITALIC> {};
struct MSBold : public MDAuto<MarkdownGroup, MS_BOLD> {};
struct MSStrike : public MDAuto<MarkdownGroup, MS_STRIKE> {};
struct MSUnderline : public MDAuto<MarkdownGroup, MS_UNDERLINE> {};
struct MSCode : public MDAuto<MarkdownGroup, MS_CODE> {};
struct MSLink : public MDAuto<MarkdownGroup, MS_LINK> {
	std::string_view title; // website tooltip desc (after URL)
	std::string_view url;
};

struct MSImage : public MDAuto<MarkdownGroup, MS_IMAGE> {
	std::string_view title; // website tooltip desc (after URL)
	std::string_view url;
	void* userdata;
};

struct MTNullChar : public MDAuto<MarkdownItem, MT_NULLCHAR> {};
struct MTData : public MDAuto<MarkdownItem, MT_DATA> {
	std::string_view data;
};
struct MTBrk : public MDAuto<MarkdownItem, MT_BRK> {};
struct MTCode : public MDAuto<MarkdownItem, MT_CODE> {
	std::string_view data;
};
struct MTHTML : public MDAuto<MarkdownItem, MT_HTML> {
	std::string_view data;
};

//
// PARSER IS PRIVATE NOW!
//
class MarkdownParser;

// Parsed Tree state
class MarkdownTree {
	friend class MarkdownParser;
	protected:
	struct destruction_node { // 3 pointers... :/
		public:
		void* ptr;
		struct destruction_node* next;
		virtual void destroy() {};
	};

	std::vector<char*> bumps;
	size_t bump_cap = 0; // capacity pof current bump allocator node
	size_t bump_pos = 0; // position in current bump allocator node

	destruction_node* destr_list = nullptr; // nodes to destroy
	destruction_node* destr_curr = nullptr; // most recent destruction node
	public:
	impl::MarkdownItem* root = nullptr;

	MarkdownTree() = default;
	MarkdownTree(const MarkdownTree&) = delete;
	MarkdownTree(MarkdownTree&& src) {
		bumps = std::move(src.bumps);
		bump_cap = src.bump_cap;
		src.bump_cap = 0;
		bump_pos = src.bump_pos;
		destr_curr = src.destr_curr;
		src.destr_curr = nullptr;
		destr_list = src.destr_list;
		src.destr_list = nullptr;
		str_copy = std::move(src.str_copy);
		root = src.root;
		src.root = nullptr;
	}

	void* allocate_raw(size_t size);

	template <typename T>
	T* allocate_obj() {
		void *data = allocate_raw(sizeof(T));
		if constexpr (std::is_trivially_destructible_v<T>) { // trivial - omit adding node
			return std::construct_at<T>((T*)data);
		}

		// nontrivial - add node
		struct dn : public destruction_node {
			virtual void destroy() override {
				std::destroy_at<T>((T*)ptr);
			}
		};

		static_assert(sizeof(destruction_node) == sizeof(dn), "oh no, something is wrong with your compiler");

		// allocate destruction node
			struct dn* node = (struct dn*)allocate_raw(sizeof(dn));
			if (!node) LOG_FATAL("can't allocate node?");
			node = std::construct_at<dn>(node); // construct properly

		//construct object
		data = std::construct_at<T>((T*)data);
		node->ptr = data;

		node->next = nullptr; // we are tail
		if (!destr_list) destr_list = node; // is first item in the list?
		if (destr_curr) destr_curr->next = node;
		destr_curr = node; // insert to tail

		// return
		return (T*)data;
	}

	void clear();

	~MarkdownTree() {
		clear();
	}

	protected:
	std::string str_copy; // may be unset. DO NOT CHANGE! THREAT AS CONST!
	public:
	void draw(const char* name); // draw markdown tree
};

};

using MarkdownTree = impl::MarkdownTree;
bool parseMarkdown(MarkdownTree& tree, std::string_view md_text);

};