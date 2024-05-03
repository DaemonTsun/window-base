
#include "shl/defer.hpp"

#include "window/colors/light.hpp"
#include "window/styles/main_style.hpp"
#include "window/find_font.hpp"
#include "window/fonts.hpp"
#include "window/window_imgui_util.hpp"

#include "fs-ui/filepicker.hpp"

#include "project_info.hpp"

static ImFont *ui_font = nullptr;
static ImFont *monospace_font = nullptr;
const float font_size = 20.f;
const int window_width  = 1600;
const int window_height = 900;

static void _update(GLFWwindow *window, double dt)
{
    static char buf[256] = {0};
    ui_new_frame();

    ImGui::SetNextWindowSize({450, 200});
    ImGui::Begin("InputText After & Before text");
    {
        ImGui::Text("Lorem ipsum abcdef hello world wpakdfasdfnsdk askldfnslfs");

        ImGui::InputText("Input Label 1", buf, 255);

        ImGui::Text("Lorem ipsum abcdef hello world wpakdfasdfnsdk askldfnslfs");
    }
    ImGui::End();

    ImGui::SetNextWindowSize({600, 200});
    ImGui::Begin("InputText SameLine");
    {
        ImGui::Text("Lorem ipsum");
        ImGui::SameLine();

        ImGui::PushItemWidth(-200);
        ImGui::InputText("Input Label 2", buf, 255);
        ImGui::SameLine();

        ImGui::Text("Lorem ipsum abcdef hello world");
    }
    ImGui::End();

    ImGui::SetNextWindowSize({450, 200});
    ImGui::Begin("Filepicker After & Before text");
    {
        ImGui::Text("Lorem ipsum abcdef hello world wpakdfasdfnsdk askldfnslfs");

        FsUi::Filepicker("File Label 1", buf, 255, "Any file|*.*|Office Files|*.docx;*.pptx;*.xlsx|Cool file|cool.file");

        ImGui::Text("Lorem ipsum abcdef hello world wpakdfasdfnsdk askldfnslfs");
    }
    ImGui::End();

    ImGui::ShowDemoWindow();

    ui_end_frame();
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
    set_colors(&st);
}

int main(int argc, const char *argv[])
{
    window_init();
    defer { window_exit(); };

    GLFWwindow *window = create_window(window_template_NAME, window_width, window_height);
    defer { destroy_window(window); };

    ui_init(window);
    defer { ui_exit(window); };

    _load_imgui_fonts();
    _set_imgui_style_and_colors();

    window_event_loop(window, _update);

    return 0;
}
