/**
 * A global list of initialization functions in specific ordder!
 * Much better than undefined C++ global constructor shit!
 */

#include "_ui_list.hpp"
#include "profiler.hpp"

namespace pb {

extern void _init_engine();
extern void _free_engine();

void _init_client() {
    // GL and window and imgui are NOT ready yet...

    _init_engine();

    {
        using pb::screen::register_ui;
        using namespace pb::screen::ui;
        register_ui(demo_window);
        register_ui(help_window);
        register_ui(fps_overlay);
        register_ui(profiler);
        register_ui(world_ui);
    }

};

void _free_client() {
    _free_engine();
    // none yet...
}

};
