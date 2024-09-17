#pragma once 
#include "screen.hpp"

namespace pb {
namespace screen {

namespace ui {

extern bool show_demo_window;
extern bool show_help_window;
extern bool show_profiler;
extern bool show_fps_overlay;

extern UIInstance* demo_window;
extern UIInstance* help_window;
extern UIInstance* fps_overlay;
extern UIInstance* profiler;

extern UIInstance* world_ui;

};
};
};