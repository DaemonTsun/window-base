
// included by filepicker.cpp
#pragma once

#include "shl/string.hpp"

namespace ui
{
void TextSlice(const char *label, const char *label_end);
void TextSlice(const_string str);

// why does ImGui not support this again...?
bool ButtonSlice(const char* label, const char *label_end, const ImVec2& size_arg, ImGuiButtonFlags flags);
bool ButtonSlice(const_string str, const ImVec2 &size_arg, ImGuiButtonFlags flags);

void ScrollWhenDraggingOnVoid(const ImVec2 &delta, ImGuiMouseButton mouse_button);
}
