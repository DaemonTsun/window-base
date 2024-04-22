
#include "imgui_internal.h"
#include "fs-ui/filepicker.hpp"

bool FsUi::Filepicker(const char *label, char *buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
    bool text_edited = ImGui::InputTextEx(label, NULL, buf, (int)buf_size, ImVec2(0, 0), flags, callback, user_data);
    ImGui::SameLine();
    bool picker_clicked = ImGui::Button("...");

    if (picker_clicked)
    {}

    return text_edited;
}
