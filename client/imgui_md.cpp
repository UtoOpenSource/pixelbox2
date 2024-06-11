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

};	// namespace impl

void* MarkdownTree::allocate_raw(size_t size) {
	if (size == 0) return nullptr;
	if (bump_cap < size) {	// oh no
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


void ImGui::MarkdownTree::draw(const char* name) {}

void ImGui::MarkdownTree::clear() {
	// reset
	stack = {}; // reset
	root.nodes.clear(); // reset
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

void ImGui::MarkdownParser::init_parser() {
	impl.abi_version = 0;

	impl.flags = MD_FLAG_UNDERLINE | MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;

	impl.enter_block = [](MD_BLOCKTYPE t, void* detail, void* __u) -> int {
		auto& state = *((MarkdownParser*)__u);
		auto view = magic_enum::enum_name(t);
		LOG_DEBUG("ENTER %.*s", int(view.size()), view.data());
		switch (t) {
			case MD_BLOCK_DOC: {
				LOG_DEBUG("============================ START OF DOCUMENT ====================================");
				state.tree->push(&state.tree->root);
			}; break;

			case MD_BLOCK_HR: {	 // horisontal line
				LOG_DEBUG("HORIZONTAL LINE --------------------------");
			} break;

			case MD_BLOCK_QUOTE: {
			} break;

			case MD_BLOCK_UL: {	 // unordered list

			}; break;

			case MD_BLOCK_OL: {	 // unordered list

			}; break;

			case MD_BLOCK_LI: {	 // for each list item, inside list block

			}; break;

			case MD_BLOCK_H: {	// header
				auto* d = (MD_BLOCK_H_DETAIL*)detail;
				LOG_DEBUG("H LEVEL : %i", d->level);
			}; break;

			case MD_BLOCK_CODE: {	 // code multiline
				auto* d = (MD_BLOCK_CODE_DETAIL*)detail;
				LOG_DEBUG("CODEBLK : lang %.*s, info:%.*s fence:%c", d->lang.size, d->lang.text, d->info.size, d->info.text, d->fence_char);
			}; break;

			case MD_BLOCK_HTML: {
			} break;

			case MD_BLOCK_P: {
			} break;

			case MD_BLOCK_TABLE: {
				auto* d = (MD_BLOCK_TABLE_DETAIL*)detail;
				LOG_DEBUG("TABLE head_row_count:%i, body_row_count:%i, column_count:%i", d->head_row_count, d->body_row_count, d->col_count);
			}; break;

			case MD_BLOCK_THEAD: {
			}; break;

			case MD_BLOCK_TBODY: {
			}; break;

			case MD_BLOCK_TD: { // TABLE ROW-COLUMN DATA
				auto* d = (MD_BLOCK_TD_DETAIL*)detail;
				LOG_DEBUG("TD align : %i", d->align);
			};

			case MD_BLOCK_TR: { // TABLE ROW START/END
			}; break;

			case MD_BLOCK_TH: { // TABLE HEAD DATA
				auto* d = (MD_BLOCK_TD_DETAIL*)detail;	// yes, following the docs
				LOG_DEBUG("TD align : %i", d->align);
			}; break;
				break;

			default:
				break;
		};
		return 0;
	};

	impl.leave_block = [](MD_BLOCKTYPE t, void* d, void* __u) -> int {
		auto& state = *((MarkdownParser*)__u);
		auto view = magic_enum::enum_name(t);
		LOG_DEBUG("LEAVE %.*s", int(view.size()), view.data());
		if (t == MD_BLOCK_DOC) state.tree->pop();
		return 0;
	};

	impl.enter_span = [](MD_SPANTYPE t, void* detail, void* __u) -> int {
		auto& state = *((MarkdownParser*)__u);
		auto view = magic_enum::enum_name(t);
		LOG_DEBUG("enter %.*s", int(view.size()), view.data());
		switch (t) {
			case MD_SPAN_A: {
				auto* d = (MD_SPAN_A_DETAIL*)detail;
				LOG_DEBUG("A is_autolink:%i, href:%.*s title:%.*s", d->is_autolink, d->href.size, d->href.text, d->title.size, d->title.text);
			} break;
			case MD_SPAN_CODE:
				break;
			case MD_SPAN_DEL:

				break;
			case MD_SPAN_EM:

				break;
			case MD_SPAN_STRONG:

				break;
			case MD_SPAN_U:

				break;
			case MD_SPAN_IMG: {
				auto* d = (MD_SPAN_IMG_DETAIL*)detail;
				LOG_DEBUG("A href:%.*s title:%.*s", d->src.size, d->src.text, d->title.size, d->title.text);
			}
			break;
			default:
				break;
		};
		return 0;
	};

	impl.leave_span = [](MD_SPANTYPE t, void* d, void* __u) -> int {
		auto& state = *((MarkdownParser*)__u);
		auto view = magic_enum::enum_name(t);
		LOG_DEBUG("leave %.*s", int(view.size()), view.data());
		return 0;
	};

	impl.text = [](MD_TEXTTYPE t, const char* data, unsigned int size, void* __u) -> int {
		auto& state = *((MarkdownParser*)__u);
		std::string_view view(data, size);

		auto vv = magic_enum::enum_name(t);
		LOG_DEBUG("TEXT %.*s : %.*s", int(vv.size()), vv.data(), int(view.size()), view.data());
		volatile char* a = (nullptr);

		switch (t) {
			case MD_TEXT_NORMAL:
				break;
			case MD_TEXT_NULLCHAR:
				break;
			case MD_TEXT_BR:
				// ImGui::NewLine();
				break;
			case MD_TEXT_SOFTBR: {
				// raise(SIGTRAP);
			}
			// soft_break();
			break;
			case MD_TEXT_ENTITY:
				// if (!render_entity(str, str_end)) {
				//	render_text(str, str_end);
				// };
				break;
			case MD_TEXT_HTML:
				//	render_text(str, str_end);
				break;
			case MD_TEXT_CODE:
				break;
			default:
				break;
		}
		return 0;
	};

	impl.debug_log = [](const char* s, void*) { LOG_DEBUG("%s", s); };

	impl.syntax = nullptr;
}


void ImGui::MarkdownTree::push(impl::MarkdownItem* i, bool insert) {
	if (!i) {
		LOG_FATAL("nullptr NODE!");
	}
	if (insert) { // insert into the item at the top
		if (top() && !top()->is_block()) {
			LOG_FATAL("attempt to insert item into non-block node");
		}
		auto* p = (impl::MarkdownGroup*)top();
		p->nodes.emplace_back(i);
	}
	stack.push_back(i);
}

impl::MarkdownItem* ImGui::MarkdownTree::top() {
	return stack.size() ? stack.front() : nullptr;
}

void ImGui::MarkdownTree::pop() {
	if (!top()) {
		LOG_FATAL("STACK UNDERFLOW!");
	}
	stack.pop_back();
}

bool ImGui::MarkdownParser::parse() {
	init_parser();
	return md_parse(str.data(), str.size(), &impl, this) == 0;
	return true;
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