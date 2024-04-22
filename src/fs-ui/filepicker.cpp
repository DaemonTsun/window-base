
#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui_internal.h"

#include "fs-ui/filepicker.hpp"

bool FsUi::Filepicker(const char *label, char *buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* callback_user_data)
{
    ImGuiStyle &st = ImGui::GetStyle();
    ImGui::PushID(label);
    bool text_edited = ImGui::InputTextEx("", NULL, buf, (int)buf_size, ImVec2(0, 0), flags, callback, callback_user_data);
    ImGui::SameLine();
    ImGui::Dummy({-st.FramePadding.x * 2, 0});
    ImGui::SameLine();
    bool picker_clicked = ImGui::Button("...");
    ImGui::SameLine();
    ImGui::Text(label);

    if (picker_clicked)
    {}

    ImGui::PopID();

    return text_edited;
}
