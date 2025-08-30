
#include "shl/defer.hpp"

#include "ui/styles/main_style.hpp"
#include "ui/colorscheme.hpp"
#include "window/find_font.hpp"
#include "window/find_font_fonts.hpp"
#include "window/window_imgui_util.hpp"

#include "ui/filepicker.hpp"

#include "project_info.hpp"

static ImFont *ui_font = nullptr;
static ImFont *monospace_font = nullptr;
const float font_size   = 20.f;
const int window_width  = 1600;
const int window_height = 900;

static void _template_settings_window()
{
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_MenuBar))
    {
        if (ImGui::BeginMenuBar())
        {
            ui::ColorschemeMenu();
            ImGui::EndMenuBar();
        }

        if (ImGui::CollapsingHeader("Style & Colors"))
            ui::ColorschemePicker();

    }
    ImGui::End();
}

static void _update(GLFWwindow *window, double dt)
{
    (void)window;
    (void)dt;

    imgui_new_frame();

    _template_settings_window();

    static char buf[256] = {0};

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

    ImGui::Begin("Filepicker After & Before text");
    {
        ImGui::Text("Lorem ipsum abcdef hello world wpakdfasdfnsdk askldfnslfs");
        const char *filter = "Any file|*.*|Office Files|*.docx;*.pptx;*.xlsx|INI Files|*.ini|Cool file|cool.file";

        ImGui::Text("No Flags");
        ui::Filepicker("File Label 1", buf, 255, filter);

        ImGui::Text("ui_FilepickerFlags_NoDirectories");
        ui::Filepicker("File Label 2", buf, 255, filter, ui_FilepickerFlags_NoDirectories);

        ImGui::Text("ui_FilepickerFlags_NoFiles");
        ui::Filepicker("File Label 3", buf, 255, filter, ui_FilepickerFlags_NoFiles);

        ImGui::Text("ui_FilepickerFlags_SelectionMustExist");
        ui::Filepicker("File Label 4", buf, 255, filter, ui_FilepickerFlags_SelectionMustExist);

        ImGui::Text("Lorem ipsum abcdef hello world wpakdfasdfnsdk askldfnslfs");
    }
    ImGui::End();

    ImGui::ShowDemoWindow();

    imgui_end_frame();
}

#include "shl/print.hpp"

static void _load_imgui_fonts()
{
    ff_cache *fc = ff_load_font_cache();
    defer { ff_unload_font_cache(fc); };

    for_font_cache(f, s, p, fc)
    {
        tprint("%; %; %\n", f, s, p);
    }

    const char *ui_font_path = ff_find_first_font_path(fc, (const char**)font_names_ui, font_names_ui_count * 2, nullptr);
    const char *monospace_font_path = ff_find_first_font_path(fc, (const char**)font_names_monospace, font_names_monospace_count * 2, nullptr);

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

    ui::colorscheme_set_default();
}

int main(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    window_init();
    defer { window_exit(); };

    GLFWwindow *window = window_create(window_template_NAME, window_width, window_height);
    defer { window_destroy(window); };

    imgui_init(window);
    defer { imgui_exit(window); };

    _load_imgui_fonts();
    _set_imgui_style_and_colors();

    window_event_loop(window, _update, default_render_function, 10.f);

    return 0;
}
