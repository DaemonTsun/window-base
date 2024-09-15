
#include "imgui_internal.h"
#include "shl/assert.hpp"
#include "shl/fixed_array.hpp"
#include "shl/string.hpp"
#include "shl/memory.hpp"
#include "ui/colorscheme.hpp"

// include all the colorschemes
#include "ui/colorschemes/light.hpp"
#include "ui/colorschemes/dark.hpp"

constexpr fixed_array _colorschemes
{
    ui::colorscheme{colorscheme_light_name, (ui::colorscheme_apply_function)colorscheme_light_apply},
    ui::colorscheme{colorscheme_dark_name,  (ui::colorscheme_apply_function)colorscheme_dark_apply}
};

static const ui::colorscheme *_find_colorscheme_by_name(const_string name)
{
    for_array(sc, &_colorschemes)
        if (to_const_string(sc->name) == name)
            return sc;

    return nullptr;
}

struct colorscheme_settings
{
    string colorscheme;
};

static void init(colorscheme_settings *settings)
{
    assert(settings != nullptr);
    fill_memory(settings, 0);
};

static void free(colorscheme_settings *settings)
{
    assert(settings != nullptr);
    free(&settings->colorscheme);
};

static colorscheme_settings _ini_settings;

static void _colorscheme_ClearAllFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler)
{
    (void)ctx;
    (void)handler;
    free(&_ini_settings);
    init(&_ini_settings);
}

static void *_colorscheme_ReadOpenFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name)
{
    (void)ctx;
    (void)handler;
    if (string_compare(name, "Preferences"_cs) == 0)
        return &_ini_settings;

    return (void*)nullptr;
}

static void _colorscheme_ReadLineFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* _entry, const char* _line)
{
    (void)ctx;
    (void)handler;
    (void)_entry;

    const_string line = to_const_string(_line);

    if (string_begins_with(line, "Name="_cs))
    {
        const_string schemename = line;
        schemename.c_str += 5;
        schemename.size  -= 5;

        if (string_is_blank(schemename))
            return;

        const ui::colorscheme *sc = _find_colorscheme_by_name(schemename);

        if (sc == nullptr)
            sc = _colorschemes.data;

        colorscheme_set(sc);
    }
}

static void _colorscheme_WriteAllFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    (void)ctx;

    if (string_is_blank(&_ini_settings.colorscheme))
        return;

    buf->appendf("[%s][Preferences]\n", handler->TypeName);
    buf->appendf("Name=%s\n", _ini_settings.colorscheme.data);

    buf->append("\n");
}

void ui::colorscheme_init()
{
    init(&_ini_settings);

    ImGuiSettingsHandler ini_handler{};
    ini_handler.TypeName = "Colorscheme";
    ini_handler.TypeHash = ImHashStr("Colorscheme");
    ini_handler.ClearAllFn = _colorscheme_ClearAllFn;
    ini_handler.ReadOpenFn = _colorscheme_ReadOpenFn;
    ini_handler.ReadLineFn = _colorscheme_ReadLineFn;
    ini_handler.WriteAllFn = _colorscheme_WriteAllFn;
    ImGui::AddSettingsHandler(&ini_handler);
}

void ui::colorscheme_free()
{
    free(&_ini_settings);
}

void ui::colorscheme_get_all(const ui::colorscheme **out, int *out_count)
{
    assert(out != nullptr);
    assert(out_count != nullptr);
    *out       = _colorschemes.data;
    *out_count = _colorschemes.size;
}

static const ui::colorscheme *_current_scheme = nullptr;

const ui::colorscheme *ui::colorscheme_get_current()
{
    return _current_scheme;
}

void ui::colorscheme_set(const ui::colorscheme *scheme)
{
    assert(scheme != nullptr);
    
    ImGuiStyle *st = &ImGui::GetStyle();
    scheme->apply(st);
    string_set(&_ini_settings.colorscheme, scheme->name);
    _current_scheme = scheme;
}

void ui::colorscheme_set_default()
{
    if (_current_scheme != nullptr)
        return;

    const ui::colorscheme *dark = _colorschemes.data + 1;
    ui::colorscheme_set(dark);
}

bool ui::ColorschemePicker(const ui::colorscheme **out)
{
    bool picked = false;

    if (ImGui::BeginCombo("Colorscheme", _current_scheme->name, 0))
    {
        for (s64 i = 0; i < _colorschemes.size; i++)
        {
            const ui::colorscheme *scheme = _colorschemes.data + i;

            if (ImGui::Selectable(scheme->name, scheme == _current_scheme))
            {
                ui::colorscheme_set(_colorschemes.data + i);
                picked = true;

                if (out != nullptr)
                    *out = scheme;
            }
        }
        ImGui::EndCombo();
    }

    return picked;
}

bool ui::ColorschemeMenu(const ui::colorscheme **out)
{
    bool picked = false;

    if (ImGui::BeginMenu("Colorscheme"))
    {
        static int selection = 0;

        for (int i = 0; i < (int)_colorschemes.size; i++)
        {
            const ui::colorscheme *scheme = _colorschemes.data + i;

            if (scheme == _current_scheme)
                selection = i;

            if (ImGui::RadioButton(scheme->name, &selection, i))
            {
                selection = i;
                ui::colorscheme_set(scheme);

                if (out != nullptr)
                    *out = scheme;
            }
        }

        ImGui::EndMenu();
    }

    return picked;
}
