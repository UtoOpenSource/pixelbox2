/*
 * imgui_md: Markdown for Dear ImGui using MD4C
 * (http://https://github.com/mekhontsev/imgui_md)
 *
 * Copyright (c) 2021 Dmitry Mekhontsev
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
 */

#include "imgui_md.h"

#include <cstdio>
#include <string>
#include <string_view>

#include "SDL_misc.h"
#include "base.hpp"
#include "imgui.h"
#include "magic_enum.hpp"
#include "md4c.h"
#include "printf.h"


namespace ImGui {

namespace impl {

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

	MarkdownParser(MarkdownTree& t, std::string_view md_text, bool copy = true) {
		tree = &t;
		if (copy) {
			tree->str_copy = md_text;
			(void)tree->str_copy.c_str();	 // trigger container
			str = tree->str_copy;
		} else {
			t.str_copy.clear();
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

	void append_strings_of_node(impl::MarkdownItem* n, int level=0);

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
	printf_("{");
	for (auto* node : stack) {
		enum MarkdownItemType t = node->get_type();
		auto view = magic_enum::enum_name(t);
		printf_("%.*s, ", int(view.size()), view.data());
	}
	printf_("}\n");
}

void ImGui::MarkdownTree::clear() {
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

void MarkdownParser::push_string(std::string_view v) { 
	tmp_buffs.emplace_back(std::string{v.data(), v.size()});
}

void MarkdownParser::append_string(std::string_view v) {
	tmp_buffs.back() += v;
}

std::string_view MarkdownParser::build_string() {
	// allocated on bump heap
	size_t size = tmp_buffs.back().size();
	char* raw = (char*)tree->allocate_raw( size + 1);
	memcpy(raw, tmp_buffs.back().c_str(), size);
	return {raw, size};
}

void MarkdownParser::pop_string() {
	tmp_buffs.pop_back();
}

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
	switch(n->get_type()) {
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
			for (auto* i : node->nodes)
				append_strings_of_node(i, level+1);
		}
		break;
		case MS_ITALIC:
		case MS_BOLD:
		case MS_STRIKE:
		case MS_UNDERLINE:
		case MS_CODE:{
			auto* node = (impl::MSItalic*)n;
			for (auto* i : node->nodes)
				append_strings_of_node(i, level+1);
		}
		break;
		case MS_LINK:
		case MS_IMAGE:
		case MT_NULLCHAR: {
			append_string("\\0");
		}
		break;
		case MT_DATA: {
			auto* node = (impl::MTData*)n;
			append_string(node->data);
		}
		break;
		case MT_CODE: {
			auto* node = (impl::MTCode*)n;
			append_string(node->data);
		}
		break;
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
		//append_string("debug string --------\n weweeewowowowo");
		node->text = build_string();
		//LOG_DEBUG("BUILDED STRING CODE %.*s", int(node->text.size()), node->text.data());
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
		close_headers(0); // hehe
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
			// soft_break();
			auto* node = allocate_obj<MTData>();
			node->data = " "; // replace with space
			insert(node);
		}
		break;
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
					//auto* curr = (MBHTML*)top();
					//curr->text = data;
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
					//auto* curr = (MBCode*)top();
					//curr->text = data;
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

bool parseMarkdown(MarkdownTree& tree, std::string_view md_text) {
	impl::MarkdownParser p(tree, md_text, true);
	if (p.parse()) {
		return true;
	}
	tree.clear();	 // on parse errors
	return false;
}

///
/// RENDERING
///

#define MAX_LEVEL 10

extern ImFont* custom_fonts[];

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
	int row_id = -1; // for row drawing
};

static void open_url(std::string_view url, r_ctx& x) {}

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
			PushStyleColor(ImGuiCol_Text, s.Colors[ImGuiCol_ButtonHovered]);
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

				c = s.Colors[ImGuiCol_ButtonHovered];
				if (ImGui::IsMouseReleased(0)) {
					// open url
					open_url(x.url, x);
				}
			} else {
				c = s.Colors[ImGuiCol_Button];
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

	if (x.level > MAX_LEVEL) return;	// oh no
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
		case impl::MB_ULIST: {
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
			ImGui::PushID((size_t)node);
			for (auto* n : node->nodes) {
				x.list_char = node->mark;
				x.list_index = index++;
				DONODE(n);
			}
			x.list_index = -1;
			x.list_char = ' ';
			ImGui::PopID();
		} break;
		case impl::MB_LITEM: {
			auto* node = (impl::MBListItem*)_node;
			if (x.list_char > ' ') {
				ImGui::TreePush("node");
				if (x.list_index < 0) {
					ImGui::Bullet();
				} else {
					ImGui::Text("%i. ", x.list_index);
				}
				ImGui::SameLine();
				DOALL(node);
				ImGui::TreePop();
			} else {
				DOALL(node);
			}
		} break;

		// TABLES
		case impl::MB_TABLE: {
			auto* node = (impl::MBTable*)_node;
			int flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Reorderable | 
			ImGuiTableFlags_Hideable | ImGuiTableFlags_Resizable;
			if (ImGui::BeginTable("mdtable", node->columns, flags)) {
				ImGui::PushID((size_t)node);

				for (int index = 0; index < node->columns; index++) {
					auto* n = node->nodes[index];
					//char tmp[33];
					//snprintf_(tmp, 32, "r%i", index);
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
				}
				ImGui::TableHeadersRow();

				// column header rows
				for (int index = 0; index < int(node->nodes.size()); index++) {
					auto* n = (impl::MBTableRow*)node->nodes[index];
					x.row_id = index;
					if (index < node->columns) {
						ImGui::TableSetColumnIndex(index);
						//ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
						DONODE(n);
						//ImGui::PopStyleVar();
						ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
						char tmp[33];
						snprintf_(tmp, 32, "r%i", index);
						ImGui::TableHeader(tmp);
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
			bool stat = ImGui::TreeNodeEx("", ImGuiTreeNodeFlags_DefaultOpen |
			 ImGuiTreeNodeFlags_NoTreePushOnOpen, "%s ", std::string(node->level, '#').c_str());
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
			auto* node = (impl::MBHLine*)_node;
			ImGui::Separator();
		} break;

		// CODE
		case impl::MB_CODE: {
			auto* node = (impl::MBCode*)_node;
			ImGui::PushID((size_t)node);
			std::string tmp = {node->text.data(), node->text.size()};
			auto* font = ImGui::GetFont();
			const float scale = ImGui::GetIO().FontGlobalScale;

			ImVec2 size = font->CalcTextSizeA((font->FontSize+1) * scale, 
				ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x, 
				tmp.c_str());
			
			size.x += 10;
			size.y += 8;

			// with description now
			if (node->caption.size() > 0) {
					std::string lang = {node->caption.data(), node->caption.size()};
				if (ImGui::TreeNodeEx(lang.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
					PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
					ImGui::InputTextMultiline("", tmp.data(), tmp.length(), size, ImGuiInputTextFlags_ReadOnly);
					PopStyleColor();
				}
			} else { // nodesc
				ImGui::InputTextMultiline("", tmp.data(), tmp.length(), size, ImGuiInputTextFlags_ReadOnly);
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
		case impl::MS_ITALIC: {
			auto* node = (impl::MSItalic*)_node;
			ImGui::PushFont(custom_fonts[2]);
			DOALL(node);
			ImGui::PopFont();
		} break;
		case impl::MS_BOLD: {
			auto* node = (impl::MSBold*)_node;
			ImGui::PushFont(custom_fonts[1]);
			DOALL(node);
			ImGui::PopFont();
		} break;
		case impl::MS_STRIKE: {
			auto* node = (impl::MSStrike*)_node;
			x.is_strikethrough = true;
			DOALL(node);	// ingore
			x.is_strikethrough = false;
		} break;
		case impl::MS_UNDERLINE: {
			auto* node = (impl::MSUnderline*)_node;
			x.is_underline = true;
			DOALL(node);	// ignore
			x.is_underline = false;
		} break;
		case impl::MS_CODE: {
			auto* node = (impl::MSCode*)_node;
			ImGui::PushID((size_t)node);
			DOALL(node);
			ImGui::PopID();
		} break;
		case impl::MS_LINK: {
			auto* node = (impl::MSLink*)_node;
			x.url = node->url;
			x.title = node->title;
			//render_text(node->title, x);
			DOALL(node);
			x.title = {};
			x.url = {};
		} break;
		case impl::MS_IMAGE: {
			auto* node = (impl::MSImage*)_node;

		} break;
		case impl::MT_NULLCHAR: {
			auto* node = (impl::MTNullChar*)_node;
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

			ImVec2 size = font->CalcTextSizeA(font->FontSize * scale, 
				ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x, 
				tmp.c_str());
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

void impl::MarkdownTree::draw(const char* name) {
	if (ImGui::BeginTabItem(name)) {
		if (ImGui::BeginChild("child_md", ImGui::GetContentRegionAvail(), ImGuiChildFlags_Border, ImGuiWindowFlags_NoSavedSettings)) {
			// here we go
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
			auto x = r_ctx();
			drawNode(root, x);
			ImGui::PopStyleVar(3);
		}
		ImGui::EndChild();
		ImGui::EndTabItem();
	}
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