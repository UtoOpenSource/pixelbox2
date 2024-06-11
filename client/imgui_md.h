/*
 * imgui_md_stateful: Stateful Markdown for Dear ImGui using MD4C
 * (http://https://github.com/mekhontsev/imgui_md)
 *
 * Copyright (c) 2021 Dmitry Mekhontsev
 * COpyright (C) 2024 UtoiECat 
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
	MI_GROUP_ = -1, // virtual. group tree node, has array of BLOCKS.
	MI_BASE_  = 0, // must not exist!! above one too!

	// BLOCK NODE CONTAINERS
	MI_DOCUMENT = 1, // first node awailable (root node)
	MI_QUOTE, // quote container.

	MI_ULIST, // unordered list. contains LITEM blocks
	MI_OLIST, // ordered list. contains LITEM blocks

	MI_HEADER, // header. in cnntrast to other markdown viewers, headers are collapseable here. (So they contain all child elments inside)

	MI_TABLE, // a complex beast. Contains refs on Column names AND all the rows.

	// unilitary items
	MI_LITEM, // LIST ITEM.
	MI_ROW, // individual table ROW. all the rows are in one array

	// LEAF NODES
	MI_SPAN, // Leaf node - array of SPAN nodes. Text and everythiing. MD_BLOCK_P
	MI_HLINE, // horisontal line. Leaf node. BLOCK_HR
	MI_CODE, // big Source code block. Leaf node
	MI_HTML, // rawdog html. render as text, but mode stupidly and straightforwardly

	MI_BRK, // newline

	// SPAN TYPES. have no child nodes at all
	MS_REGULAR=30, // regular text
	MS_BOLD,       // bold text SPAN_STRONG
	MS_ITALIC,     // italic text SPAN_EM
	MS_UNDERLINE,  // underline text
	MS_STRIKE,     // text is ~deleted~ or striketrough
	MS_CODE,       // oneliner code in ` `
	MS_LINK,       // link (local, to file or into internet)
	MS_IMAGE,      // yep
};

struct MarkdownItem { // base type
	unsigned char type_comb; // different item types + flags
	public:
	inline enum MarkdownItemType get_type() {return (MarkdownItemType)(type_comb & 15);} // 4 bits
	MarkdownItem(MarkdownItemType t) : type_comb(t) {
		if (t <= MI_BASE_) {
			auto view = magic_enum::enum_name(t);
			LOG_FATAL("assertion! Bad MarkdownItem %.*s", int(view.size()), view.data());
		}
	}
	inline bool is_block() {

	}
	MarkdownItem() = delete;
	MarkdownItem(const MarkdownItem&) = default;
};

using MarkdownItems = std::vector<MarkdownItem*>;

struct MarkdownGroup : public MarkdownItem {
	MarkdownGroup(MarkdownItemType t = MI_GROUP_) : MarkdownItem(t) {}
	MarkdownItems nodes; // child nodes, each of them allocated on bump heap 
};

struct MarkdownDocument: public MarkdownGroup {
	MarkdownDocument() : MarkdownGroup(MI_DOCUMENT) {}
	MarkdownItems nodes; // child nodes, each of them allocated on bump heap 
};

//
// items
//

// quote has no extra stuff
struct MarkdownQuote: public MarkdownGroup {
	MarkdownQuote() : MarkdownGroup(MI_QUOTE) {}
};

// lists too
struct MarkdownUnorderedList: public MarkdownGroup {
	MarkdownUnorderedList() : MarkdownGroup(MI_ULIST) {}
};
struct MarkdownOrderedList: public MarkdownGroup {
	MarkdownOrderedList() : MarkdownGroup(MI_OLIST) {}
};

struct MarkdownListItem: public MarkdownGroup {
	MarkdownListItem() : MarkdownGroup(MI_LITEM) {}
};

struct MarkdownHead : public MarkdownGroup {
	MarkdownHead() : MarkdownGroup(MI_HEADER) {}
	unsigned char level;					 // header level
	std::string_view raw_caption;	 // raw text for search
};

struct MarkdownTable : public MarkdownGroup {

};

// leaf node
struct MarkdownCode : public MarkdownItem {
	MarkdownCode() : MarkdownItem(MI_CODE) {}
	std::string_view caption;	 // quote or php, js, etc.
	std::string_view text;		 // code text
};

// leaf node
struct MarkdownBrk : public MarkdownItem {
	MarkdownBrk() : MarkdownItem(MI_BRK) {}
	int count;	// count of newlines
};


// separate guys

// MS_REGULAR .. MS_STRIKE + MS_CODE
struct MarkdownSpan : public MarkdownItem { // all of them have text
	MarkdownSpan(enum MarkdownItemType t) : MarkdownItem(t) {
		if (t <= MS_REGULAR) {
			auto view = magic_enum::enum_name(t);
			LOG_FATAL("assertion! Bad MarkdownSpan %.*s", int(view.size()), view.data());
		}
	}
	std::string_view text;
};

struct MSRegular : public MarkdownItem {MSRegular() : MarkdownItem(MS_REGULAR) {}; };
struct MSBold : public MarkdownItem {MSBold() : MarkdownItem(MS_BOLD) {}; };
struct MSItalic : public MarkdownItem {MSItalic() : MarkdownItem(MS_ITALIC) {}; };
struct MSUnderline : public MarkdownItem {MSUnderline() : MarkdownItem(MS_UNDERLINE) {}; };
struct MSStrike : public MarkdownItem {MSStrike() : MarkdownItem(MS_STRIKE) {}; };
struct MSCode : public MarkdownItem {MSCode() : MarkdownItem(MS_CODE) {}; };

struct MSLink : public MarkdownSpan { // all of them have text
	// text is visible link name
	std::string_view title; // website tooltip desc (after URL)
	std::string_view url;
};

struct MSImage : public MarkdownSpan { // all of them have text
	// text is visible image name
	std::string_view title; // image tooltip desc (after URL)
	std::string_view url; // image URL

	void* udata; // image handle to draw?
};

};

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

	void* allocate_raw(size_t size);

	template <typename T>
	T* allocate_obj() {
		void *data = allocate_raw(sizeof(T));
		if constexpr (std::is_trivially_destructible_v<T>) { // trivial - omit adding node
			return std::construct_at<T>(data);
		}

		// nontrivial - add node
		struct dn : public destruction_node {
			virtual void destroy() override {
				std::destroy_at<T>(ptr);
			}
		};

		static_assert(sizeof(destruction_node) == sizeof(dn), "oh no, something is wrong with your compiler");

		// allocate destruction node
		struct dn* node = (struct dn*)allocate_raw(sizeof(dn));
		std::construct_at<dn>(node); // construct properly
		node->ptr = data;

		//construct object
		std::construct_at<T>(data);

		node->next = nullptr; // we are tail
		if (!destr_list) destr_list = node; // is first item in the list?
		if (destr_curr) destr_curr->next = node;
		destr_curr = node; // insert to tail

		// return
		return data;
	}

	void clear();

	~MarkdownTree() {
		clear();
	}

	protected:
	impl::MarkdownDocument root;
	std::vector<impl::MarkdownItem*> stack; // current tree
	std::string str_copy; // may be unset. DO NOT CHANGE! THREAT AS CONST!
	void push(impl::MarkdownItem*, bool insert=true);
	impl::MarkdownItem* top();
	void pop();
	public:

	void draw(const char* name); // draw markdown tree
};

class MarkdownParser {
	public:
	MarkdownTree* tree;
	std::string_view str;

	MarkdownParser(MarkdownTree& t, std::string_view md_text, bool copy=true) {
		tree = &t;
		if (copy) {
			tree->str_copy = md_text;
			(void) tree->str_copy.c_str(); // trigger container
			str = tree->str_copy;
		} else {
			t.str_copy.clear();
			str = md_text; // user guarantees that string will live forever
		}
	}

	bool parse(); // hehe
	virtual ~MarkdownParser() {};
	protected:
	void init_parser();
	MD_PARSER impl;
};

};