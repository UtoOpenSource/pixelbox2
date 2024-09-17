/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Help manager
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

#include "base.hpp"
#include "screen.hpp"
#include "imgui.h"
#include "imgui_md.h"
#include "galogen.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include "hashmap.hpp"
#include "stb_image.h"

namespace fs = std::filesystem;

namespace ImGui {
	extern const ImFont* custom_fonts[3];
};

namespace pb {

namespace screen {

struct ImageInfo {
	GLuint handle = -1;
	ImVec2 size = {};
};

static std::string sanitize_path(std::string_view vvp) {
	std::string res;
	res.reserve(vvp.size());

	int num_dots = 0;
	for (char c : vvp) {
		if (c == '.') num_dots++;
		else num_dots = 0; // not a dot anymore

		if (num_dots > 1) continue; // skip more dots
		res += c;
	}
	return res;
}

static struct HelpManager {
	struct HelpTab {
		std::string name;
		bool is_closed = false;
		std::string content;
		ImGui::MarkdownTree tree;

		void draw() {
			tree.render();
		}
	};
	public:
	std::vector<HelpTab> tabs;

	private:
	static const int IMAGE_MARKED = 210;
	struct LoadedImage {
		GLuint handle = 0;
		ImVec2 size = {};
		int refcnt = IMAGE_MARKED;
	};
	pb::HashMap<std::string, LoadedImage> loaded_images; // GL textures

	public:
	// returns zero size if image not loaded AND DOES NOT ATEMPTS TO LOAD ANYTHING
	ImVec2 get_opt_size(std::string_view path) {
		auto v = loaded_images.find(std::string(path));
		if (v != loaded_images.end()) { // found
			v->second.refcnt = IMAGE_MARKED;
			return v->second.size;
		}
		return ImVec2(); // zero
	}

	// load image or return existent one
	ImageInfo load_image(std::string_view path) {
		auto v = loaded_images.find(std::string(path));
		if (v != loaded_images.end()) { // found
			v->second.refcnt = IMAGE_MARKED;
			return {v->second.handle, v->second.size};
		}

		/// GENERATE TEXTURE
		// push state
		GLint last_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);

		// gen
		GLuint handle; // GL texture
		LoadedImage res; // result
		std::string file = sanitize_path(path); // \0 string of path
		// fix double dots

		file.insert(0, "./assets/");
		int x=0,y=0,n=0; unsigned char *data; // image

    glGenTextures(1, &handle);
		if (!handle) { // error
			LOG_ERROR("Too many textures created or GPU error! %i", glGetError());
			goto end_of_all; // YOU CANNOT SIMPLY RETURN!
		}

    glBindTexture(GL_TEXTURE_2D, handle);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef GL_UNPACK_ROW_LENGTH // Not on WebGL/ES
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

		// LOAD FILE
		data = stbi_load(file.c_str(), &x, &y, &n, 0);
		// ... x = width, y = height, n = # 8-bit components per pixel ...
		// ... replace '0' with '1'..'4' to force that many components per pixel
		// ... but 'n' will always be the number that it would have been if you said 0
		static constexpr GLenum tx_format[] = {
			0,
			GL_RED,
			GL_RG,
			GL_RGB,
			GL_RGBA
		};

		// error
		if (!data || n < 0 || n > 4) {
			LOG_ERROR("CAN'T LOAD IMAGE %s", file.c_str());
			glDeleteTextures(1, &handle);
			handle = 0; // to make empty texture and not try to load it every fuckin frame
			goto end_of_all;
		}
		LOG_INFO("LOADED IMAGE %s", file.c_str());

		// LAOD FILE INTO TEXTURE
    glTexImage2D(GL_TEXTURE_2D, 0, tx_format[n], x, y, 0, tx_format[n], GL_UNSIGNED_BYTE, data);
		stbi_image_free(data);

		end_of_all:
    // Restore state
    glBindTexture(GL_TEXTURE_2D, last_texture);

		res.handle = handle; // save handle for later
		res.size = ImVec2(x, y);
		loaded_images.insert_or_assign(std::string(path), std::move(res));
		return {res.handle, res.size}; // 0 - error
	}

	void unload_all_now() {
		for (auto& v : loaded_images) {
			auto h = v.second.handle;
			if (h) glDeleteTextures(1, &h);
		}
		loaded_images.clear();
	}

	void collect_images() {
		auto it = loaded_images.begin();
		while (it != loaded_images.end()) {
			auto &h = it->second;
			h.refcnt--;
			if (h.refcnt < 0) { // free texture NOW
				if (h.handle) glDeleteTextures(1, &h.handle);
				it = loaded_images.erase(it); // delete from cache
			} else { // keep alive
				it++;
			}
		}
	}

	// hehe
	static void image_handler(std::string_view url, std::string_view title, void* ud) {
		HelpManager& self = *(HelpManager*)ud;
		ImVec2 v = self.get_opt_size(url);
		if (!ImGui::IsRectVisible(v)) return; // do not load invisible images

		auto res = self.load_image(url);

		if (!res.handle) return;
		ImGui::Image((void*)res.handle, res.size);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%.*s\n%.*s", int(url.size()), url.data(), int(title.size()), title.data());
		}
	}
} man;

static bool loaded = false;

static Register r2([](int) {
	if (!show_help_window) return;
	man.collect_images(); // hehe

	if (!loaded) {
		std::string path = "./assets/help";
		std::stringstream buffer;
		for (const auto & entry : fs::directory_iterator(path)) {
   		std::ifstream f(entry.path());
			buffer << f.rdbuf();
			auto& v = man.tabs.emplace_back(HelpManager::HelpTab{.name=entry.path().filename(), .is_closed=false, .content=buffer.str(), .tree={}});
			//v = man.tabs.back();
			v.tree.parse(v.content); // yupee
			v.tree.set_image_callback(man.image_handler, &man);
			v.tree.set_fonts(ImGui::custom_fonts, ImGui::MarkdownFonts::MF_ITALIC);
			buffer = {};
			// LOG_INFO("wtf %s", entry.path().c_str());
		}
		loaded=true;
	}

	if (ImGui::Begin("User Guide", &show_help_window, ImGuiWindowFlags_Modal)) {
		if (ImGui::BeginTabBar("tabs")) {
			if (ImGui::BeginTabItem("Controls")) {
				if (ImGui::BeginChild("child_md", ImGui::GetContentRegionAvail(), ImGuiChildFlags_Border, 0)) {
					ImGui::SeparatorText("USER GUIDE:");
					ImGui::ShowUserGuide();
					ImGui::SeparatorText("DEBUG SHORTCUTS :");
					ImGui::BulletText("F1  - Open this Help");
					ImGui::BulletText("F7  - Open ImGUI Demo");
					ImGui::BulletText("F8  - Show Profiler");
					ImGui::BulletText("F10 - Show FPS Overlay");
				}
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			for (auto& t : man.tabs) {
				if(ImGui::BeginTabItem(t.name.c_str())) {
					t.draw();
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
});

RegisterDestructor sus([](){
	man.unload_all_now();
});

};

};