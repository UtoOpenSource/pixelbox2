/*
 * imgui_md: Markdown for Dear ImGui using MD4C
 * (http://https://github.com/mekhontsev/imgui_md) TODO FIX LINK
 *
 * Copyright (c) 2021 Dmitry Mekhontsev
 * Copyright (C) 2024 UtoECat 
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

#include "imgui_md.h"

#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>

#include "imgui.h"
#include "magic_enum.hpp"
#include "md4c.h"

#include "base.hpp"
#ifdef LOG_DEBUG
#define LOG_DEBUG(...) /*notjhing */
#endif

namespace ImGui {

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
			auto view = M_ENUM_NAME(t);
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

struct MBText : public MDAuto<MarkdownGroup, MB_TEXT> {};

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

// AND TREE TOO!
class MarkdownTree;

template <typename T>
static inline void move_and_zero(T&& dst, T&& src) {
  dst = std::move(src);
	typename std::remove_reference<T>::type v;
	src = v;
}

static const ImFont* NULL_FONTS[MarkdownFonts::MF_MAX_FONTS] = {nullptr};

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
	// root DOCUMENT node is here if parsed succesfully
	impl::MarkdownItem* root = nullptr;

	// callback, allowed flags and userdata stuff
	MarkdownDrawImageCallback image_callback = nullptr;
	void *image_userdata = nullptr;
	MarkdownOpenURLCallback url_callback = nullptr;
	void *url_userdata = nullptr;

	// CUSTOM FONTS
	const ImFont** fonts = NULL_FONTS;
	size_t         fonts_cnt = 0;

	MarkdownTree() = default;
	MarkdownTree(size_t prealloc) { // create with preallocated page
		bump_cap = prealloc;
		char* data = (char*)calloc(1, bump_cap);
		if (!data) throw std::bad_alloc();
		bumps.emplace_back(data);
		bump_pos = 0;
	}

	MarkdownTree(const MarkdownTree&) = delete;
	MarkdownTree(MarkdownTree&& src) { // yeah, this shit is huge... it was important one day, now it's not that much
		bumps = std::move(src.bumps);
		bump_cap = src.bump_cap;
		src.bump_cap = 0;
		bump_pos = src.bump_pos;
		destr_curr = src.destr_curr;
		src.destr_curr = nullptr;
		destr_list = src.destr_list;
		src.destr_list = nullptr;

		move_and_zero(str_copy, src.str_copy);
		move_and_zero(root, src.root);

		move_and_zero(image_callback,src.image_callback);
		move_and_zero(image_userdata, src.image_userdata);
		move_and_zero(url_callback,src.url_callback);
		move_and_zero(url_userdata, src.url_userdata);

		fonts = src.fonts;
		move_and_zero(fonts_cnt, src.fonts_cnt);
	}

	void* allocate_raw(size_t size);

	/// allocate objects on a bump heap
	/// for nontrivial objects allocate and add a destructor node in list of destruction nodes :)
	/// this really speedups shit a lot
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

		// insert delete node ONLY after succesful construction. don't care about "leak"
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

	void set_fonts(const ImFont* a[], MarkdownFonts lim) {
		if (lim > MarkdownFonts::MF_MAX_FONTS) lim = MarkdownFonts::MF_MAX_FONTS;

		// set empty one
		if (lim == 0 || a == nullptr) {
			fonts = NULL_FONTS;
			fonts_cnt = 0;
			return;
		}

		fonts = a;
		fonts_cnt = lim;
	}
};

/// allocate any amount of memory on a bump heap. no destruction is performed afterwards
void* MarkdownTree::allocate_raw(size_t size) {
	if (size == 0) return nullptr;
	if (size % 8 != 0) size = ((size + 8) / 8) * 8;
	if (bump_cap < bump_pos + size) {	 // oh no
		if (!bump_cap)
			bump_cap = 512;
		else
			bump_cap *= 2;
		if (bump_cap < size) {	// holy shit
			bump_cap = size * 2;
		}
		char* data = (char*)calloc(1, bump_cap);
		if (!data) throw std::bad_alloc();
		bumps.emplace_back(data);
		bump_pos = 0;
	}	 // else we hae enough capacity
	char* data = bumps.back() + bump_pos;
	bump_pos += size;
	return data;	// yipee
}

class MarkdownParser {
 public:
	MarkdownTree* tree;
	std::string_view str;

	MarkdownParser(MarkdownTree* t, std::string_view md_text, bool copy = true) {
		tree = t;
		if (copy) {
			tree->str_copy = md_text;
			(void)tree->str_copy.c_str();	 // trigger container
			str = tree->str_copy;
		} else {
			tree->str_copy.clear();
			str = md_text;	// user guarantees that string will live forever
		}
	}

	bool parse();	 // hehe

	virtual ~MarkdownParser(){};
	void init_parser();

	template <typename T>
	T* allocate_obj() {
		return tree->allocate_obj<T>();	 // yuipee
	}

	int proc_block(MD_BLOCKTYPE t, void* detail);
	int proc_block_end(MD_BLOCKTYPE t, void* detail);
	int proc_span(MD_SPANTYPE t, void* detail);
	int proc_span_end(MD_SPANTYPE t, void* detail);
	int proc_text(MD_TEXTTYPE t, std::string_view data);

	std::vector<impl::MarkdownItem*> stack;	 //
	void push(impl::MarkdownItem*);
	void insert(impl::MarkdownItem*);	 // insert at the end of the top item
	impl::MarkdownItem* top();
	void pop();

	void append_strings_of_node(impl::MarkdownItem* n, int level = 0);

	// temporary string buffers
	std::vector<std::string> tmp_buffs;
	void push_string(std::string_view v = {});
	void append_string(std::string_view v);
	std::string_view build_string();	// allocated on bump heap
	void pop_string();

	void close_headers(int level);
	void dump_stack();
	MD_PARSER impl;
};

void MarkdownParser::dump_stack() {
#ifdef DEBUG_MD
	printf("{");
	for (auto* node : stack) {
		enum MarkdownItemType t = node->get_type();
		auto view = magic_enum::enum_name(t);
		printf("%.*s, ", int(view.size()), view.data());
	}
	printf("}\n");
#endif
}

void ImGui::impl::MarkdownTree::clear() {
	str_copy.clear();	 // for good measure

	// destroy nontrivial objects, from the first to the last
	while (destr_list) {
		auto* node = destr_list;
		destr_list = destr_list->next;
		node->destroy();
	}
	destr_list = nullptr;
	destr_curr = nullptr;

	// free bumps
	for (char* data : bumps) {
		free(data);
	}
	bumps.clear();
	bump_pos = 0;
	bump_cap = 0;	 // IMPORTANT!
}

void MarkdownParser::push_string(std::string_view v) { tmp_buffs.emplace_back(std::string{v.data(), v.size()}); }

void MarkdownParser::append_string(std::string_view v) { tmp_buffs.back() += v; }

std::string_view MarkdownParser::build_string() {
	// allocated on bump heap
	size_t size = tmp_buffs.back().size();
	char* raw = (char*)tree->allocate_raw(size + 1);
	memcpy(raw, tmp_buffs.back().c_str(), size);
	return {raw, size};
}

void MarkdownParser::pop_string() { tmp_buffs.pop_back(); }

void MarkdownParser::init_parser() {
	stack = {};	 // reset
	impl.abi_version = 0;

	impl.flags = MD_FLAG_UNDERLINE | MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;

	impl.enter_block = [](MD_BLOCKTYPE t, void* detail, void* __u) -> int {
		auto& state = *((MarkdownParser*)__u);

		auto view = magic_enum::enum_name(t);
		LOG_DEBUG("ENTER %.*s", int(view.size()), view.data());

		return state.proc_block(t, detail);
	};

	impl.leave_block = [](MD_BLOCKTYPE t, void* detail, void* __u) -> int {
		auto& state = *((MarkdownParser*)__u);

		auto view = magic_enum::enum_name(t);
		LOG_DEBUG("LEAVE %.*s", int(view.size()), view.data());

		return state.proc_block_end(t, detail);
	};

	impl.enter_span = [](MD_SPANTYPE t, void* detail, void* __u) -> int {
		auto& state = *((MarkdownParser*)__u);
		auto view = magic_enum::enum_name(t);
		LOG_DEBUG("enter %.*s", int(view.size()), view.data());

		return state.proc_span(t, detail);
	};

	impl.leave_span = [](MD_SPANTYPE t, void* detail, void* __u) -> int {
		auto& state = *((MarkdownParser*)__u);

		auto view = magic_enum::enum_name(t);
		LOG_DEBUG("leave %.*s", int(view.size()), view.data());

		return state.proc_span_end(t, detail);
	};

	impl.text = [](MD_TEXTTYPE t, const char* data, unsigned int size, void* __u) -> int {
		auto& state = *((MarkdownParser*)__u);
		std::string_view view(data, size);

		auto vv = magic_enum::enum_name(t);
		LOG_DEBUG("TEXT %.*s : %.*s", int(vv.size()), vv.data(), int(view.size()), view.data());

		return state.proc_text(t, view);
	};

	impl.debug_log = [](const char* s, void*) { LOG_DEBUG("%s", s); };

	impl.syntax = nullptr;
}

void MarkdownParser::close_headers(int level) {
	while (top()->get_type() == MB_HEADER) {
		auto* node = (MBHeader*)top();
		if (node->level < level) break;	 // success
		pop();
	}
	if (top()->get_type() != MB_HEADER && top()->get_type() != MB_DOCUMENT && top()->get_type() != MB_QUOTE) {
		LOG_FATAL("Bad stack for header closing");
	}
}

int MarkdownParser::proc_block(MD_BLOCKTYPE t, void* detail) {
	switch (t) {
		case MD_BLOCK_DOC: {
			LOG_DEBUG("============================ START OF DOCUMENT ====================================");
			auto* node = allocate_obj<MBDocument>();
			if (top()) LOG_FATAL("stack must be empty at this point!");
			push(node);
		}; break;

		case MD_BLOCK_HR: {	 // horisontal line
			LOG_DEBUG("HORIZONTAL LINE --------------------------");
			auto* node = allocate_obj<MBHLine>();
			close_headers(0);	 // close headers BEFORE INSERT!
			if (top()->get_type() != MB_DOCUMENT && top()->get_type() != MB_QUOTE) LOG_FATAL("we are fucked");
			insert(node);	 // not push, we can't insert anything in HR!
		} break;

		case MD_BLOCK_QUOTE: {
			auto* node = allocate_obj<MBQuote>();
			insert(node);
			push(node);
		} break;

		case MD_BLOCK_UL: {	 // unordered list
			auto* d = (MD_BLOCK_UL_DETAIL*)detail;
			auto* node = allocate_obj<MBUList>();
			node->mark = d->mark;
			insert(node);
			push(node);
		}; break;

		case MD_BLOCK_OL: {	 // ordered list
			auto* d = (MD_BLOCK_OL_DETAIL*)detail;
			auto* node = allocate_obj<MBOList>();
			node->mark = d->mark_delimiter;
			node->start_index = d->start;
			insert(node);
			push(node);
		}; break;

		case MD_BLOCK_LI: {	 // for each list item, inside list block
			auto* d = (MD_BLOCK_LI_DETAIL*)detail;
			auto* node = allocate_obj<MBListItem>();
			node->mark = d->task_mark;
			if (top()->get_type() != MB_OLIST && top()->get_type() != MB_ULIST) LOG_FATAL("List item may be inserted ONLY into the lists!");
			insert(node);
			push(node);
			// tree.push(node);
		}; break;

		case MD_BLOCK_H: {	// header
			auto* d = (MD_BLOCK_H_DETAIL*)detail;
			LOG_DEBUG("H LEVEL : %i", d->level);
			auto* node = allocate_obj<MBHeader>();
			node->level = d->level;
			close_headers(d->level);	// close headers :3 + check for header
			insert(node);
			push(node);
			// insert caption
			auto* node2 = allocate_obj<MBText>();
			insert(node2);
			push(node2);
		}; break;

		case MD_BLOCK_CODE: {	 // code multiline
			auto* d = (MD_BLOCK_CODE_DETAIL*)detail;
			LOG_DEBUG("CODEBLK : lang %.*s, info:%.*s fence:%c", d->lang.size, d->lang.text, d->info.size, d->info.text, d->fence_char);

			auto* node = allocate_obj<MBCode>();
			node->caption = {d->lang.text, d->lang.size};
			node->text = {};
			insert(node);
			push(node);

			// code builder
			push_string();
		}; break;

		case MD_BLOCK_HTML: {
			auto* node = allocate_obj<MBHTML>();
			insert(node);
			push(node);

			// code builder
			push_string();
		} break;

		case MD_BLOCK_P: {
			auto* node = allocate_obj<MBText>();
			insert(node);
			push(node);
		} break;

		case MD_BLOCK_TABLE: {
			auto* d = (MD_BLOCK_TABLE_DETAIL*)detail;
			LOG_DEBUG("TABLE head_row_count:%i, body_row_count:%i, column_count:%i", d->head_row_count, d->body_row_count, d->col_count);
			auto* node = allocate_obj<MBTable>();
			node->columns = d->col_count * d->head_row_count;	 // ?
			node->rows = d->body_row_count;
			insert(node);
			push(node);
		}; break;

		case MD_BLOCK_THEAD: {
			if (top()->get_type() != MB_TABLE) LOG_FATAL("table expected at the top");
		}; break;

		case MD_BLOCK_TBODY: {
			if (top()->get_type() != MB_TABLE) LOG_FATAL("table expected at the top");
		}; break;

		case MD_BLOCK_TR: {	 // TABLE ROW START/END
			if (top()->get_type() != MB_TABLE) LOG_FATAL("table expected at the top");
			auto* node = (MBTable*)top();
			if (node->nodes.size() % node->columns != 0) {
				// not crash
				LOG_ERROR("TABLE ROW MISMATCH - NOT ROUNDED PROPERLY, DATA WILL BE SHIFTED!");
				// todo add empty text nodes in that case, if it will be relevant
			}
		}; break;

		case MD_BLOCK_TH:
		case MD_BLOCK_TD: {												// TABLE ROW-COLUMN DATA + TABLE HEAD DATA COMBINED
			auto* d = (MD_BLOCK_TD_DETAIL*)detail;	// yes, following the docs
			LOG_DEBUG("TD align : %i", d->align);
			auto* node = allocate_obj<MBTableRow>();
			node->alignment = d->align;
			if (top()->get_type() != MB_TABLE) LOG_FATAL("table expected at the top");
			insert(node);
			push(node);
		}; break;

		default:
			LOG_FATAL("unsupported node!");
			break;
	};
	return 0;
}

void MarkdownParser::append_strings_of_node(impl::MarkdownItem* n, int level) {
	if (level > 10) return;
	switch (n->get_type()) {
		case MB_DOCUMENT:
		case MB_QUOTE:
		case MB_ULIST:
		case MB_OLIST:
		case MB_TABLE:
		case MB_LITEM:
		case MB_ROW:
		case MB_HEADER:
		case MB_HLINE:
		case MB_CODE:
		case MB_HTML:
			break;
		case MB_TEXT: {
			auto* node = (impl::MBText*)n;
			for (auto* i : node->nodes) append_strings_of_node(i, level + 1);
		} break;
		case MS_ITALIC:
		case MS_BOLD:
		case MS_STRIKE:
		case MS_UNDERLINE:
		case MS_CODE: {
			auto* node = (impl::MSItalic*)n;
			for (auto* i : node->nodes) append_strings_of_node(i, level + 1);
		} break;
		case MS_LINK:
		case MS_IMAGE:
		case MT_NULLCHAR: {
			append_string("\\0");
		} break;
		case MT_DATA: {
			auto* node = (impl::MTData*)n;
			append_string(node->data);
		} break;
		case MT_CODE: {
			auto* node = (impl::MTCode*)n;
			append_string(node->data);
		} break;
		case MT_BRK:
		case MT_HTML:
		case MI_BASE_:
			break;
	}
}

int MarkdownParser::proc_block_end(MD_BLOCKTYPE t, void* detail) {
	// Header-specific exception
	if (t == MD_BLOCK_H) {
		if (top()->get_type() != MB_TEXT) LOG_FATAL("stack top is NOT header text");
		// combine all captions into tmp strng
		auto* node = (MBText*)top();
		// caption builder
		push_string();
		append_strings_of_node(node);
		pop();	// remove caption
		// insert combined_string
		auto* nodeh = (MBHeader*)top();
		nodeh->raw_title = build_string();
		pop_string();
		return 0;
	} else if (t == MD_BLOCK_HTML) {
		auto* node = (MBHTML*)top();
		node->text = build_string();
		pop_string();
	} else if (t == MD_BLOCK_CODE) {
		auto* node = (MBCode*)top();
		// append_string("debug string --------\n weweeewowowowo");
		node->text = build_string();
		// LOG_DEBUG("BUILDED STRING CODE %.*s", int(node->text.size()), node->text.data());
		pop_string();
	}

	// hr specific exception
	if (t == MD_BLOCK_HR) {
		return 0;	 // do not pop anything - we don't even push it
	}

	// table bloxck-specific
	if (t == MD_BLOCK_THEAD || t == MD_BLOCK_TBODY || t == MD_BLOCK_TR) {
		return 0;	 // do not pop anything - we don't even push it
	}

	// close headers
	if (t == MD_BLOCK_QUOTE) {
		close_headers(0);	 // hehe
	} else if (t == MD_BLOCK_DOC) {
		close_headers(0);
		tree->root = top();	 // yupee!
	}

	if (!top() || !top()->is_block()) LOG_FATAL("stack top is NOT block!");
	pop();
	return 0;
}

static enum MarkdownItemType hack_lookup(int t) {
	switch (t) {
		case MD_SPAN_CODE:
			return MS_CODE;
		case MD_SPAN_DEL:
			return MS_STRIKE;
		case MD_SPAN_EM:
			return MS_ITALIC;
		case MD_SPAN_STRONG:
			return MS_BOLD;
		case MD_SPAN_U:
			return MS_UNDERLINE;
	};
	LOG_FATAL("misuse");
	/*
	[MD_SPAN_CODE] = MS_CODE,
	[MD_SPAN_DEL] = MS_STRIKE,
	[MD_SPAN_EM] = MS_ITALIC,
	[MD_SPAN_STRONG] = MS_BOLD,
	[MD_SPAN_U] = MS_UNDERLINE, */
};

int MarkdownParser::proc_span(MD_SPANTYPE t, void* detail) {
	switch (t) {
		case MD_SPAN_A: {
			auto* d = (MD_SPAN_A_DETAIL*)detail;
			LOG_DEBUG("A is_autolink:%i, href:%.*s title:%.*s", d->is_autolink, d->href.size, d->href.text, d->title.size, d->title.text);

			auto* node = allocate_obj<MSLink>();
			node->url = {d->href.text, d->href.size};
			node->title = {d->title.text, d->title.size};
			insert(node);
			push(node);
		} break;
		case MD_SPAN_CODE:
		case MD_SPAN_DEL:
		case MD_SPAN_EM:
		case MD_SPAN_STRONG:
		case MD_SPAN_U: {
			// hack to write less code :)
			// assumes items above are same size and content
			auto* node = allocate_obj<MSItalic>();
			node->type_comb = hack_lookup(t);
			insert(node);
			push(node);
		} break;
		case MD_SPAN_IMG: {
			auto* d = (MD_SPAN_IMG_DETAIL*)detail;
			LOG_DEBUG("A href:%.*s title:%.*s", d->src.size, d->src.text, d->title.size, d->title.text);

			auto* node = allocate_obj<MSImage>();
			node->url = {d->src.text, d->src.size};
			node->title = {d->title.text, d->title.size};
			node->userdata = nullptr;	 // TODO: ADD LOADING FUNCTION
			insert(node);
			push(node);
		} break;
		default:
			break;
	};
	return 0;
}

int MarkdownParser::proc_span_end(MD_SPANTYPE t, void* detail) {
	if (!top() || !top()->is_span()) LOG_FATAL("stack top is NOT span!");
	pop();
	return 0;
}

int MarkdownParser::proc_text(MD_TEXTTYPE t, std::string_view data) {
	switch (t) {
		case MD_TEXT_NORMAL: {
			auto* node = allocate_obj<MTData>();
			node->data = data;
			insert(node);
		} break;
		case MD_TEXT_NULLCHAR: {
			auto* node = allocate_obj<MTNullChar>();
			insert(node);
		} break;
		case MD_TEXT_BR: {
			auto* node = allocate_obj<MTBrk>();
			insert(node);
		}	 // ImGui::NewLine();
		break;
		case MD_TEXT_SOFTBR: {
			//auto* node = allocate_obj<MTBrk>();
			//insert(node);
			//break;
			// soft_break();
			auto* node = allocate_obj<MTData>();
			node->data = " ";	 // replace with space
			insert(node);
		} break;
		case MD_TEXT_ENTITY: {
			// if (!render_entity(str, str_end)) {
			//	render_text(str, str_end);
			// };
			auto* node = allocate_obj<MTData>();
			node->data = data;
			insert(node);
		} break;

		case MD_TEXT_HTML: {
			if (top()) {
				// append to HTML block :)
				if (top()->get_type() == MB_HTML) {
					// auto* curr = (MBHTML*)top();
					// curr->text = data;
					append_string(data);
					break;
				}
			}

			auto* node = allocate_obj<MTHTML>();
			node->data = data;
			insert(node);
		} break;
		case MD_TEXT_CODE: {
			if (top()) {
				// append to code block :)
				if (top()->get_type() == MB_CODE) {
					// auto* curr = (MBCode*)top();
					// curr->text = data;
					append_string(data);
					break;
				}
				// inline code block is handled usually
			}

			auto* node = allocate_obj<MTCode>();
			node->data = data;
			insert(node);
		} break;
		default:	// ???
			LOG_FATAL("unknown TEXT");
			break;
	}
	return 0;
}

// big boi
void MarkdownParser::push(impl::MarkdownItem* i) {
	if (!i) LOG_FATAL("nullptr NODE!");
	if (!i->is_block() && !i->is_span()) LOG_FATAL("can't insert text node into stack!");
	stack.push_back(i);
	dump_stack();
}

void MarkdownParser::insert(impl::MarkdownItem* i) {
	if (!i) LOG_FATAL("nullptr NODE!");
	if (!top()) LOG_FATAL("STACK IS EMPTY _ NOWHERE TO INSERT!");

	if (!top()->is_container() && !top()->is_span() && i->is_span()) {
		dump_stack();
		LOG_FATAL("attempt to insert span data into non-block and non-span item");
	}

	if (!top()->is_container() && i->is_block()) LOG_FATAL("attempt to insert block into non-block item");

	if (!(top()->is_span() || top()->get_type() == MB_TEXT || top()->get_type() == MB_LITEM || top()->get_type() == MB_ROW) &&
			i->is_text_data()) {
		dump_stack();
		LOG_FATAL("Attempt to insert text data into non-span item");
	}

	((MarkdownGroup*)top())->nodes.emplace_back(i);
}

impl::MarkdownItem* MarkdownParser::top() { return stack.size() ? stack.back() : nullptr; }

void MarkdownParser::pop() {
	if (!top()) {
		LOG_FATAL("STACK UNDERFLOW!");
	}
	stack.pop_back();
	dump_stack();
}

bool MarkdownParser::parse() {
	init_parser();
	return md_parse(str.data(), str.size(), &impl, this) == 0;
	return true;
}

};	// namespace impl

///
/// RENDERING
///

#define MAX_LEVEL 20

//extern ImFont* custom_fonts[];

static void line(ImColor c, bool under) {
	ImVec2 mi = ImGui::GetItemRectMin();
	ImVec2 ma = ImGui::GetItemRectMax();

	if (!under) {
		ma.y -= ImGui::GetFontSize() / 2;
	}

	mi.y = ma.y;

	ImGui::GetWindowDrawList()->AddLine(mi, ma, c, 1.0f);
}

struct r_ctx {
	bool is_underline = false;
	bool is_strikethrough = false;
	int level;
	std::string_view title;
	std::string_view url;
	char list_char = ' ';
	int list_index = -1;
	bool text_drawn = false;
	int row_id = -1;	// for row drawing

	// yea...
	bool is_bold = false;
	bool is_italic = false;

	// for callback
	impl::MarkdownTree* tree;
};

// handle links
static void open_url(std::string_view url, r_ctx& x) {
	if (!url.size()) return; // ignore
	if (url.data()[0] == '#') { // local-document link
		// TODO: add search condition for header
		return;
	} else if (url.data()[0] == '.' || url.data()[0] == '/' || url.data()[0] == '\\') { // local path to files
		if (x.tree->url_callback) x.tree->url_callback(url, x.tree->url_userdata); // let user handle this :)
		return;
	} else if (url.data()[0] == '@' || url.data()[0] == '$' || url.data()[0] == '%' || url.data()[0] == ' ') { // seems weird : ignore
		return;
	} else { // it is likely a URL
		if (x.tree->url_callback) x.tree->url_callback(url, x.tree->url_userdata); // let user handle this :)
	}
}

static void change_font(r_ctx& x) {
	MarkdownFonts type = MF_NORMAL;

	if (x.is_bold && x.is_italic) {
		type = MF_BOLD_ITALIC;
	} else if (x.is_bold) {
		type = MF_BOLD;
	} else if (x.is_italic) {
		type = MF_ITALIC;
	}

	// custom fonts
	if (x.tree->fonts_cnt > type) { // font is valid to set
		ImGui::PushFont(const_cast<ImFont*>(x.tree->fonts[type]));
	} else { // use default font
		ImGui::PushFont(nullptr);
	}
}

static void render_text(std::string_view data, r_ctx& x) {
	const char* str = data.data();
	const char* str_end = data.data() + data.size();
	bool is_underline = x.is_underline;
	bool is_strikethrough = x.is_strikethrough;

	const float scale = ImGui::GetIO().FontGlobalScale;
	const ImGuiStyle& s = ImGui::GetStyle();
	bool is_lf = false;

	while (str < str_end) {
		const char* te = str_end;
		float wl = ImGui::GetContentRegionAvail().x;
		bool pop_color = false;

		te = ImGui::GetFont()->CalcWordWrapPositionA(scale, str, str_end, wl);

		if (!x.url.empty()) {
			PushStyleColor(ImGuiCol_Text, s.Colors[ImGuiCol_NavHighlight]);
			pop_color = true;
		}

		if (te == str) ++te;
		ImGui::TextUnformatted(str, te);

		if (te > str && *(te - 1) == '\n') {
			is_lf = true;
		}

		if (pop_color) PopStyleColor();

		if (!x.url.empty()) {
			ImVec4 c;
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%.*s\n%.*s", int(x.url.size()), x.url.data(), int(x.title.size()), x.title.data());

				c = s.Colors[ImGuiCol_PlotLinesHovered];
				if (ImGui::IsMouseReleased(0)) {
					// open url
					open_url(x.url, x);
				}
			} else {
				c = s.Colors[ImGuiCol_PlotLines];
			}
			line(c, true);
		}
		if (is_underline) {
			line(s.Colors[ImGuiCol_Text], true);
		}
		if (is_strikethrough) {
			line(s.Colors[ImGuiCol_Text], false);
		}

		str = te;
		while (str < str_end && *str == ' ') ++str;
	}

	if (!is_lf) ImGui::SameLine(0.0f, 0.0f);
	x.text_drawn = true;
}

static void opt_newline(r_ctx& x) {
	if (x.text_drawn) ImGui::NewLine();
	x.text_drawn = false;
}

static void drawNode(impl::MarkdownItem* _node, r_ctx& x) {
#define DONODE(n)   \
	do {              \
		x.level++;      \
		drawNode(n, x); \
		x.level--;      \
	} while (0)
#define DOALL(node) \
	for (auto* n : node->nodes) DONODE(n)

	if (x.level > MAX_LEVEL) { // oh no
		PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.1f, 0.15f, 1.0f));
		render_text("ERROR: recursion limit is reached!", x);
		PopStyleColor();
		return;
	}	
	if (!_node) return;								// bad

	if (!_node->is_text_data() && !_node->is_span()) {
		opt_newline(x);
	}

	switch (_node->get_type()) {
		case impl::MB_DOCUMENT: {
			auto* node = (impl::MBDocument*)_node;
			DOALL(node);
		} break;
		case impl::MB_QUOTE: {
			auto* node = (impl::MBQuote*)_node;
			ImGui::PushID((size_t)node);
			if (ImGui::TreeNodeEx("quote", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed)) {
				PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
				DOALL(node);
				PopStyleColor();
				ImGui::TreePop();
			}
			ImGui::PopID();
		} break;
		case impl::MB_ULIST: { // fine
			auto* node = (impl::MBUList*)_node;
			ImGui::PushID((size_t)node);
				for (auto* n : node->nodes) {
					x.list_char = node->mark;
					x.list_index = -1;
					DONODE(n);
				}
			x.list_char = ' ';
			ImGui::PopID();
		} break;
		case impl::MB_OLIST: {
			auto* node = (impl::MBOList*)_node;
			int index = node->start_index;
			//ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());

			ImGui::PushID((size_t)node);
			for (auto* n : node->nodes) {
				x.list_char = node->mark;
				x.list_index = index++;
				DONODE(n);
			}
			x.list_index = -1;
			x.list_char = ' ';
			ImGui::PopID();

			//ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
		} break;
		case impl::MB_LITEM: {
			auto* node = (impl::MBListItem*)_node;
			char tmp[32] = {0};
			int flags =  ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanTextWidth;
			bool take_indent_back = false;
			float indent_w = ImGui::GetTreeNodeToLabelSpacing();

			if (x.list_index < 0) { // Good
					flags |= ImGuiTreeNodeFlags_Bullet;
			} else {
				ImGui::Unindent(indent_w); // to get correct view of this text :)
				take_indent_back = true;
				snprintf(tmp, 32, "%i. ", x.list_index); // not quite
			}

			if (ImGui::TreeNodeEx(tmp, flags)) { 
				ImGui::SameLine();
				if (take_indent_back) ImGui::Indent(indent_w);
				DOALL(node);
				ImGui::TreePop();
			}
		} break;

		// TABLES
		case impl::MB_TABLE: {
			auto* node = (impl::MBTable*)_node;
			int flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Resizable;
			if (ImGui::BeginTable("mdtable", node->columns, flags)) {
				ImGui::PushID((size_t)node);

				for (int index = 0; index < node->columns; index++) {
					// auto* n = node->nodes[index];
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
				}
				ImGui::TableHeadersRow();

				// column header rows
				for (int index = 0; index < int(node->nodes.size()); index++) {
					auto* n = (impl::MBTableRow*)node->nodes[index];
					x.row_id = index;
					if (index < node->columns) {
						ImGui::TableSetColumnIndex(index);
						ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
						DONODE(n);
						ImGui::SameLine();
						char tmp[33] = "\0";
						snprintf(tmp, 32, "##r%i", index);
						ImGui::TableHeader(tmp);
						ImGui::PopStyleVar();
					} else {
						if (index % node->columns == 0) ImGui::TableNextRow();
						ImGui::TableNextColumn();
						DONODE(n);
					}
				}

				x.row_id = -1;
				ImGui::PopID();
				ImGui::EndTable();
			}
		} break;
		case impl::MB_ROW: {
			auto* node = (impl::MBTableRow*)_node;
			ImGui::PushID((size_t)node);
			DOALL(node);
			ImGui::PopID();
		} break;

		case impl::MB_HEADER: {
			auto* node = (impl::MBHeader*)_node;
			bool is_first = true;

			ImGui::PushID((size_t)node);
			bool stat = ImGui::TreeNodeEx("", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen, "%s ",
																		std::string(node->level, '#').c_str());
			for (auto* n : node->nodes) {
				if (is_first) {
					ImGui::SameLine();
					DONODE(n);
					if (!stat) break;
				} else
					DONODE(n);
				is_first = false;
			}
			ImGui::PopID();
		} break;
		case impl::MB_HLINE: {
			//auto* node = (impl::MBHLine*)_node;
			ImGui::Separator();
			x.text_drawn = false; // reset optional separator
		} break;

		// CODE
		case impl::MB_CODE: {
			auto* node = (impl::MBCode*)_node;
			ImGui::PushID((size_t)node);
			std::string tmp = {node->text.data(), node->text.size()};
			bool show_code = true;

			auto* font = ImGui::GetFont();
			const float scale = ImGui::GetIO().FontGlobalScale;
			ImVec2 size =
					font->CalcTextSizeA(font->FontSize * scale, ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x, tmp.c_str());

			// with description now
			if (node->caption.size() > 0) {
				std::string lang = {node->caption.data(), node->caption.size()};
				ImGui::PushItemWidth(size.x);
				show_code = ImGui::TreeNodeEx(lang.c_str(),
																			ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoTreePushOnOpen);
				ImGui::PopItemWidth();
			}

			size.x += 16;
			size.y += 8;

			if (show_code) {
				PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0.7f, 1.0f));
				ImGui::InputTextMultiline("", tmp.data(), tmp.length(), ImVec2{0, 0}, ImGuiInputTextFlags_ReadOnly);
				PopStyleColor();
			}

			ImGui::PopID();
		} break;

		case impl::MB_HTML: {
			auto* node = (impl::MBHTML*)_node;
			render_text(node->text, x);
		} break;
		case impl::MB_TEXT: {
			auto* node = (impl::MBText*)_node;
			DOALL(node);
		} break;

		// fonts
		case impl::MS_ITALIC: {
			auto* node = (impl::MSItalic*)_node;
			bool old = x.is_italic;
			x.is_italic = true;
			change_font(x);
			DOALL(node);
			x.is_italic = old;
			ImGui::PopFont();
		} break;
		case impl::MS_BOLD: {
			auto* node = (impl::MSBold*)_node;
			bool old = x.is_bold;
			x.is_bold = true;
			change_font(x);
			DOALL(node);
			x.is_bold = old;
			ImGui::PopFont();
		} break;
		// internally handled
		case impl::MS_STRIKE: {
			auto* node = (impl::MSStrike*)_node;
			x.is_strikethrough = true;
			DOALL(node);
			x.is_strikethrough = false;
		} break;
		case impl::MS_UNDERLINE: {
			auto* node = (impl::MSUnderline*)_node;
			x.is_underline = true;
			DOALL(node);
			x.is_underline = false;
		} break;
		case impl::MS_CODE: { // handle just like text?
			auto* node = (impl::MSCode*)_node;
			ImGui::PushID((size_t)node);
			DOALL(node);
			ImGui::PopID();
		} break;
		case impl::MS_LINK: {
			auto* node = (impl::MSLink*)_node;
			x.url = node->url;
			x.title = node->title;

			DOALL(node);

			x.title = {}; // links are usually cannot be nested...
			x.url = {};
		} break;
		case impl::MS_IMAGE: {
			auto* node = (impl::MSImage*)_node;
			if (x.tree->image_callback) x.tree->image_callback(node->url, node->title, x.tree->image_userdata);
		} break;
		case impl::MT_NULLCHAR: {
			//auto* node = (impl::MTNullChar*)_node;
			render_text("\\0", x);
		} break;
		case impl::MT_DATA: {
			auto* node = (impl::MTData*)_node;
			render_text(node->data, x);
		} break;
		case impl::MT_BRK: {
			auto* node = (impl::MTBrk*)_node;
			ImGui::NewLine();
		} break;
		case impl::MT_CODE: {
			auto* node = (impl::MTCode*)_node;
			std::string tmp = {node->data.data(), node->data.size()};
			auto* font = ImGui::GetFont();
			const float scale = ImGui::GetIO().FontGlobalScale;

			ImVec2 size =
					font->CalcTextSizeA(font->FontSize * scale, ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x, tmp.c_str());
			size.x += 8;
			size.y += 8;
			ImGui::PushItemWidth(size.x);
			ImGui::InputText("", tmp.data(), tmp.length(), ImGuiInputTextFlags_ReadOnly);
			ImGui::SameLine();
			ImGui::PopItemWidth();
		} break;
		case impl::MT_HTML: {
			auto* node = (impl::MTHTML*)_node;
			render_text(node->data, x);
		} break;
		// case impl::MI_BASE_:
		default:
			LOG_FATAL("corrupted tree! invalid node type");
			break;
	};
#undef DOALL
#undef DONODE
}

//
// PUBLIC API and PUBLIC MarkdownTree wrapper
//

MarkdownTree::MarkdownTree(size_t prealloc) {
	impl = new impl::MarkdownTree(prealloc);
}
MarkdownTree::~MarkdownTree() {
	delete impl;
}
bool MarkdownTree::parse(std::string_view md_text) {
	impl::MarkdownParser p(impl, md_text, true);
	if (p.parse()) { // tree will be cleared up
		return true;
	}
	impl->clear();	 // on parse errors
	return false;
}

void MarkdownTree::set_url_callback(MarkdownOpenURLCallback cb, void* userdata) {
	impl->url_callback = cb;
	impl->url_userdata = userdata;
}

void MarkdownTree::set_image_callback(MarkdownDrawImageCallback cb, void* userdata) {
	impl->image_callback = cb;
	impl->image_userdata = userdata;
}

void MarkdownTree::set_fonts(const ImFont* fonts[], MarkdownFonts count) {
	impl->set_fonts(fonts, count);
}

bool MarkdownTree::render() {
	bool retval = false;
	if (ImGui::BeginChild("child_md", ImGui::GetContentRegionAvail(), ImGuiChildFlags_Border, ImGuiWindowFlags_NoSavedSettings)) {
		// here we go
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
		auto x = r_ctx();
		x.tree = impl; // important

		if (impl->root) { // for invalidly parsed trees :)
			drawNode(impl->root, x);
		}
		ImGui::PopStyleVar(3);
		retval = true;
	}
	ImGui::EndChild();
	return retval;
}


};	// namespace ImGui

/*

DRAW TABLE WITH USTOM ROWS
const int COLUMNS_COUNT = 3;
				if (ImGui::BeginTable("table_custom_headers", COLUMNS_COUNT, ImGuiTableFlags_Borders | ImGuiTableFlags_Reorderable |
ImGuiTableFlags_Hideable))
				{
						ImGui::TableSetupColumn("Apricot");
						ImGui::TableSetupColumn("Banana");
						ImGui::TableSetupColumn("Cherry");

						// Dummy entire-column selection storage
						// FIXME: It would be nice to actually demonstrate full-featured selection using those checkbox.
						static bool column_selected[3] = {};

						// Instead of calling TableHeadersRow() we'll submit custom headers ourselves
						ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
						for (int column = 0; column < COLUMNS_COUNT; column++)
						{
								ImGui::TableSetColumnIndex(column);
								const char* column_name = ImGui::TableGetColumnName(column); // Retrieve name passed to TableSetupColumn()
								ImGui::PushID(column);
								ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
								ImGui::Checkbox("##checkall", &column_selected[column]);
								ImGui::PopStyleVar();
								ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
								ImGui::TableHeader(column_name);
								ImGui::PopID();
						}

						for (int row = 0; row < 5; row++)
						{
								ImGui::TableNextRow();
								for (int column = 0; column < 3; column++)
								{
										char buf[32];
										sprintf(buf, "Cell %d,%d", column, row);
										ImGui::TableSetColumnIndex(column);
										ImGui::Selectable(buf, column_selected[column]);
								}
						}
						ImGui::EndTable();
				}

void imgui_md::BLOCK_LI(const MD_BLOCK_LI_DETAIL*, bool e)
{
	if (e) {
		ImGui::NewLine();

		list_info& nfo = m_list_stack.back();
		if (nfo.is_ol) {
			ImGui::Text("%d%c", nfo.cur_ol++, nfo.delim);
			ImGui::SameLine();
		} else {
			if (nfo.delim == '*') {
				float cx = ImGui::GetCursorPosX();
				cx -= ImGui::GetStyle().FramePadding.x * 2;
				ImGui::SetCursorPosX(cx);
				ImGui::Bullet();
			} else {
				ImGui::Text("%c", nfo.delim);
				ImGui::SameLine();
			}
		}

		ImGui::Indent();
	} else {
		ImGui::Unindent();
	}
}

void imgui_md::BLOCK_HR(bool e)
{
	if (!e) {
		ImGui::NewLine();
		ImGui::Separator();

	}
}

void imgui_md::BLOCK_H(const MD_BLOCK_H_DETAIL* d, bool e)
{
	if (e) {
		m_hlevel = d->level;
		ImGui::NewLine();
	} else {
		m_hlevel = 0;
	}

	set_font(e);

	if (!e) {
		if (d->level <= 2) {
			ImGui::NewLine();
			ImGui::Separator();
		}
	}
}


namespace ImGui{
	extern ImFont* custom_fonts[];
};

void open_url() const
{
	//Example:

	if (!m_is_image && m_href[0] != '#') {
		SDL_OpenURL(m_href.c_str());
	} else if (m_href[0] == '#') {
		// hyperlink
	}
}

*/