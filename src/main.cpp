
#include "shl/defer.hpp"

#include "window/styles/main_style.hpp"
#include "window/colorscheme.hpp"
#include "window/find_font.hpp"
#include "window/fonts.hpp"
#include "window/window_imgui_util.hpp"

#include "project_info.hpp"

static ImFont *ui_font = nullptr;
static ImFont *monospace_font = nullptr;
const float font_size = 20.f;
const int window_width  = 1600;
const int window_height = 900;

static void _basic_window()
{
    ImGui::Begin("Window");

    ImGui::Text("Hello");

    ImGui::End();
}

static void _template_settings_window()
{
    ImGui::Begin("Settings");

    if (ImGui::CollapsingHeader("Style & Colors"))
    {
        static const colorscheme *schemes = nullptr;
        static int count = 0;
        static int selection = 0;

        if (schemes == nullptr)
        {
            colorscheme_get_all(&schemes, &count);

            for (int i = 0; i < count; ++i)
                if (schemes + i == colorscheme_get_current())
                    selection = i;
        }

        if (ImGui::BeginCombo("Colorscheme", schemes[selection].name, 0))
        {
            for (int i = 0; i < count; i++)
            {
                if (ImGui::Selectable(schemes[i].name, selection == i))
                {
                    selection = i;
                    colorscheme_set(schemes + i);
                }
            }
            ImGui::EndCombo();
        }
    }

    ImGui::End();
}

static void _update(GLFWwindow *window, double dt)
{
    imgui_new_frame();

    _basic_window();
    _template_settings_window();

    imgui_end_frame();
}

static void _load_imgui_fonts()
{
    ff_cache *fc = ff_load_font_cache();
    defer { ff_unload_font_cache(fc); };

    const char *ui_font_path = ff_find_first_font_path(fc, (const char**)font_paths_ui, font_paths_ui_count * 2, nullptr);
    const char *monospace_font_path = ff_find_first_font_path(fc, (const char**)font_paths_monospace, font_paths_monospace_count * 2, nullptr);

    assert(ui_font_path != nullptr);
    assert(monospace_font_path != nullptr);

    ImGuiIO &io = ImGui::GetIO(); (void)io;
    ui_font = io.Fonts->AddFontFromFileTTF(ui_font_path, font_size);
    monospace_font = io.Fonts->AddFontFromFileTTF(monospace_font_path, font_size);
}

static void _set_imgui_style_and_colors()
{
    ImGuiIO &io = ImGui::GetIO(); (void)io;
    ImGuiStyle &st = ImGui::GetStyle();
    set_style(&st);
}

int main(int argc, const char *argv[])
{
    window_init();
    defer { window_exit(); };

    GLFWwindow *window = window_create(window_template_NAME, window_width, window_height);
    defer { window_destroy(window); };

    imgui_init(window);
    defer { imgui_exit(window); };

    _load_imgui_fonts();
    _set_imgui_style_and_colors();

    window_event_loop(window, _update);

    return 0;
}
