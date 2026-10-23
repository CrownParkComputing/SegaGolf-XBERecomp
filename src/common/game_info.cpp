#include "app/app.h"
#include "image_symbols.h"
#include "game/touch.h"

namespace {
void HostInit() {
    auto& settings = app::Get();
    if (!settings.width && !settings.height) {
        settings.width = 1280;
        settings.height = 960;
        settings.display_mode = app::DisplayMode::Windowed;
    }
}
}

const app::GameInfo app::kGame = {
    .title = L"Sega Golf Club",
    .short_title = L"SegaGolf",
    .name = L"SegaGolf",
    .log = "segagolf",
    .settings = "settings.toml",
    .splash = L"splash",
    .missing = L"Put the matching extracted Sega Golf files in assets.",
    .images = {{IMAGE_XBE, "SegaGolf"}},
    .init = HostInit,
    .window = golf::touch::Window,
    .ui = golf::touch::Draw,
    .keyboard_pad = true,
    .toggle_menu = golf::touch::ToggleMenu,
};
