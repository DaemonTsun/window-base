
#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui_internal.h"
#include "ui/utils.hpp"

void ui::TextSlice(const char *str, const char *str_end)
{
    ImGui::TextEx(str, str_end, 0);
}

void ui::TextSlice(const_string str)
{
    ImGui::TextEx(str.c_str, str.c_str + str.size, 0);
}

// why does ImGui not support this again...?
bool ui::ButtonSlice(const char* label, const char *label_end, const ImVec2& size_arg, ImGuiButtonFlags flags)
{
    using namespace ImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, label_end, true);

    ImVec2 pos = window->DC.CursorPos;
    if ((flags & ImGuiButtonFlags_AlignTextBaseLine) && style.FramePadding.y < window->DC.CurrLineTextBaseOffset) // Try to vertically align buttons that are smaller/have no padding so that text baseline matches (bit hacky, since it shouldn't be a flag)
        pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
    ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

    // Render
    const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    RenderNavHighlight(bb, id);
    RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);

    if (g.LogEnabled)
        LogSetNextTextDecoration("[", "]");
    RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, label_end, &label_size, style.ButtonTextAlign, &bb);
    
    return pressed;
}

bool ui::ButtonSlice(const_string str, const ImVec2& size_arg, ImGuiButtonFlags flags)
{
    return ui::ButtonSlice(str.c_str, str.c_str + str.size, size_arg, flags);
}
