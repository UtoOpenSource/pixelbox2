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
	if (bump_cap < bump_pos + size) {	// oh no
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
	void init_parser();

	template <typename T>
	T* allocate_obj() {
		return tree->allocate_obj<T>(); // yuipee
	}

	int proc_block(MD_BLOCKTYPE t, void* detail);
	int proc_block_end(MD_BLOCKTYPE t, void* detail);
	int proc_span(MD_SPANTYPE t, void* detail);
	int proc_span_end(MD_SPANTYPE t, void* detail);
	int proc_text(MD_TEXTTYPE t, std::string_view data);

	std::vector<impl::MarkdownItem*> stack; //
	void push(impl::MarkdownItem*);
	void insert(impl::MarkdownItem*); // insert at the end of the top item
	impl::MarkdownItem* top();
	void pop();

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
	str_copy.clear(); // for good measure

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

void MarkdownParser::init_parser() {
	stack = {}; // reset
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
	while (top()->get_type() == MB_HEADER ) {
		auto* node = (MBHeader*)top();
		if (node->level < level) break; // success
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
				close_headers(0); // close headers BEFORE INSERT!
				if (top()->get_type() != MB_DOCUMENT && top()->get_type() != MB_QUOTE) 
					LOG_FATAL("we are fucked");
				insert(node); // not push, we can't insert anything in HR!
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
				if (top()->get_type() != MB_OLIST && top()->get_type() != MB_ULIST )
					LOG_FATAL("List item may be inserted ONLY into the lists!");
				insert(node);
				push(node);
				//tree.push(node);
			}; break;

			case MD_BLOCK_H: {	// header
				auto* d = (MD_BLOCK_H_DETAIL*)detail;
				LOG_DEBUG("H LEVEL : %i", d->level);
				auto* node = allocate_obj<MBHeader>();
				node->level = d->level;
				close_headers(d->level); // close headers :3 + check for header
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
			}; break;

			case MD_BLOCK_HTML: {
				auto* node = allocate_obj<MBHTML>();
				insert(node);
				push(node);
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
				node->columns = d->col_count * d->head_row_count; // ?
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

			case MD_BLOCK_TR: { // TABLE ROW START/END
				if (top()->get_type() != MB_TABLE) LOG_FATAL("table expected at the top");
				auto* node = (MBTable*)top();
				if (node->nodes.size() % node->columns != 0) {
					// not crash
					LOG_ERROR("TABLE ROW MISMATCH - NOT ROUNDED PROPERLY, DATA WILL BE SHIFTED!");
					// todo add empty text nodes in that case, if it will be relevant
				}
			}; break;

			case MD_BLOCK_TH:
			case MD_BLOCK_TD: { // TABLE ROW-COLUMN DATA + TABLE HEAD DATA COMBINED
				auto* d = (MD_BLOCK_TD_DETAIL*)detail;	// yes, following the docs
				LOG_DEBUG("TD align : %i", d->align);
				auto* node = allocate_obj<MBTableRow>();
				node->alignment = d->align;
				if (top()->get_type() != MB_TABLE) LOG_FATAL("table expected at the top");
				insert(node);
				push(node);
			};
			break;

			default:
				LOG_FATAL("unsupported node!");
				break;
		};
		return 0;
}

int MarkdownParser::proc_block_end(MD_BLOCKTYPE t, void* detail) {
	// Header-specific exception
	if (t == MD_BLOCK_H) {
		if (top()->get_type() != MB_TEXT) LOG_FATAL("stack top is NOT header text");
		// combine all captions into tmp strng
		// TODO
		pop(); // remove caption
		// TODO INSERT COMBINED STRING
		return 0;
	}

	// hr specific exception
	if (t == MD_BLOCK_HR) {
		return 0; // do not pop anything - we don't even push it
	}

	// table bloxck-specific
	if (t == MD_BLOCK_THEAD || t == MD_BLOCK_TBODY || t == MD_BLOCK_TR) {
		return 0; // do not pop anything - we don't even push it
	}

	if (t == MD_BLOCK_DOC) { // close headers too
		close_headers(0);
	}

	if (!top() || !top()->is_block()) LOG_FATAL("stack top is NOT block!");
	pop();
	return 0;
}

static enum MarkdownItemType hack_lookup(int t) {
	switch (t) {
		case MD_SPAN_CODE: return MS_CODE;
		case MD_SPAN_DEL: return MS_STRIKE;
		case MD_SPAN_EM: return MS_ITALIC;
		case MD_SPAN_STRONG: return MS_BOLD;
		case MD_SPAN_U: return MS_UNDERLINE;
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
			}
			break;
			case MD_SPAN_IMG: {
				auto* d = (MD_SPAN_IMG_DETAIL*)detail;
				LOG_DEBUG("A href:%.*s title:%.*s", d->src.size, d->src.text, d->title.size, d->title.text);
				
				auto* node = allocate_obj<MSImage>();
				node->url = {d->src.text, d->src.size};
				node->title = {d->title.text, d->title.size};
				node->userdata = nullptr; // TODO: ADD LOADING FUNCTION
				insert(node);
				push(node);
			}
			break;
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

int MarkdownParser::proc_text(MD_TEXTTYPE t, std::string_view data){
		switch (t) {
			case MD_TEXT_NORMAL: {
				auto* node = allocate_obj<MTData>();
				node->data = data;
				insert(node);
			}
			break;
			case MD_TEXT_NULLCHAR: {
				auto* node = allocate_obj<MTNullChar>();
				insert(node);
			}
			break;
			case MD_TEXT_BR:{
				auto* node = allocate_obj<MTBrk>();
				insert(node);
			} // ImGui::NewLine();
			break;
			case MD_TEXT_SOFTBR:
			// soft_break();
			break;
			case MD_TEXT_ENTITY: {
				// if (!render_entity(str, str_end)) {
				//	render_text(str, str_end);
				// };
				auto* node = allocate_obj<MTData>();
				node->data = data;
				insert(node);
			}
			break;
			case MD_TEXT_HTML: {
				if (top()) {
					// append to HTML block :)
					if (top()->get_type() == MB_HTML) {
						auto* curr = (MBHTML*)top();
						curr->text = data;
						break;
					}
				}

				auto* node = allocate_obj<MTHTML>();
				node->data = data;
				insert(node);
			}
			break;
			case MD_TEXT_CODE: {
				if (top()) {
					// append to code block :)
					if (top()->get_type() == MB_CODE) {
						auto* curr = (MBCode*)top();
						curr->text = data;
						break;
					}
					// inline code block is handled usually
				}

				auto* node = allocate_obj<MTCode>();
				node->data = data;
				insert(node);
			}
			break;
			default: // ???
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

	if (!top()->is_container() && i->is_block()) 
		LOG_FATAL("attempt to insert block into non-block item");

	if (!(top()->is_span() || top()->get_type() == MB_TEXT 
	|| top()->get_type() == MB_LITEM || top()->get_type() == MB_ROW) && i->is_text_data()) { 
		dump_stack();
		LOG_FATAL("Attempt to insert text data into non-span item");
	}

	((MarkdownGroup*)top())->nodes.emplace_back(i);
}

impl::MarkdownItem* MarkdownParser::top() {
	return stack.size() ? stack.back() : nullptr;
}

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

void MarkdownTree::draw(const char* name) {}

}; // namespace impl

bool parseMarkdown(MarkdownTree& tree, std::string_view md_text) {
	impl::MarkdownParser p(tree, md_text, true);
	if (p.parse()) {
		return true;
	}
	tree.clear(); // on parse errors
	return false;
}

};	// namespace ImGui

/*

DRAW TABLE WITH USTOM ROWS 
const int COLUMNS_COUNT = 3;
        if (ImGui::BeginTable("table_custom_headers", COLUMNS_COUNT, ImGuiTableFlags_Borders | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable))
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
		

void imgui_md::flush_code(const char*) {
		ImGui::PushID((size_t)m_code_id);
		std::string s = std::move(code_buff);
		auto *font = ImGui::GetFont();
		if (m_is_code) {
			if (s.find('\n') != s.npos) {
				ImVec2 size = font->CalcTextSizeA(font->FontSize, ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().x, s.data());
				size.x += 10;
				size.y += 8;
				ImGui::InputTextMultiline("", s.data(), s.length(), size, ImGuiInputTextFlags_ReadOnly);
			} else {
				ImGui::PushItemWidth(font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, s.data()).x + 8);
				ImGui::InputText("", s.data(), s.length(), ImGuiInputTextFlags_ReadOnly);
				ImGui::PopItemWidth();
			}
		}
		ImGui::SameLine();

		ImGui::PopID();
		//ImGui::NewLine();
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

int print(const char* str, const char* str_end)
{
	if (str >= str_end)return 0;
	m_quote_deep = 0;
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0 ,0));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0 ,0));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0 ,2));
	int v = md_parse(str, (MD_SIZE)(str_end - str), &m_md, this);
	ImGui::PopStyleVar(3);
	return v;
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