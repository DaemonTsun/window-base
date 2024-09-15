
#pragma once

struct ImGuiStyle;

namespace ui
{
typedef void (*colorscheme_apply_function)(ImGuiStyle *dst);

struct colorscheme
{
    const char *name;
    colorscheme_apply_function apply;
};

void colorscheme_init();
void colorscheme_free();
void colorscheme_get_all(const ui::colorscheme **out, int *out_count);

const colorscheme *colorscheme_get_current();
void colorscheme_set(const ui::colorscheme *scheme);

// sets to a colorscheme if none is set
void colorscheme_set_default();

bool ColorschemePicker(const ui::colorscheme **out = nullptr);
bool ColorschemeMenu(const ui::colorscheme **out = nullptr);
}
