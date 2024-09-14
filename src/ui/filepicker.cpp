
// future features:
// TODO: multiple selection
// TODO: jump when typing in result table

#include <time.h> // how sad, used for modified/created timestamp formatting
#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui_internal.h"

#include "shl/print.hpp"
#include "shl/sort.hpp"
#include "shl/hash_table.hpp"
#include "shl/memory.hpp"
#include "shl/string.hpp"
#include "shl/time.hpp"
#include "fs/path.hpp"

#include "ui/filepicker.hpp"
#include "ui/utils.hpp"

#define ui_Ini_Name "ui"
#define ui_Ini_Preferences "Preferences"
#define ui_Ini_Preferences_EditBar    "EditBar"
#define ui_Ini_Preferences_ShowHidden "ShowHidden"
#define ui_Ini_Preferences_Pin        "Pin"

#define ui_Ini_Dialog_Preferences "FileDialog"
#define ui_Ini_Dialog_Preferences_LastDirectory "LastDirectory"

#define ui_Dialog_Navbar_Size 4096


static int lexicoraphical_compare(const char *s1, const char *s2)
{
    for (; *s1 && *s2; s1++, s2++)
    {
        if (*s1 < *s2) return -1;
        if (*s1 > *s2) return  1;
    }

    if (*s2 != '\0') return -1;
    if (*s1 != '\0') return  1;

    return 0;
}

static int timespan_compare(const timespan *lhs, const timespan *rhs)
{
    if (lhs->seconds     < rhs->seconds)     return -1;
    if (lhs->seconds     > rhs->seconds)     return  1;
    if (lhs->nanoseconds < rhs->nanoseconds) return -1;
    if (lhs->nanoseconds > rhs->nanoseconds) return  1;

    return 0;
}

// FILTERS
struct ui_fs_dialog_filter_item
{
    const_string filename;  // most likely "*"
    const_string extension; // maybe ".*"
};

struct ui_fs_dialog_filter
{
    const_string label;
    array<ui_fs_dialog_filter_item> items;
};

static void init(ui_fs_dialog_filter *f)
{
    init(&f->items);
}

static void free(ui_fs_dialog_filter *f)
{
    free(&f->items);
}

inline static s64 _skip_whitespace(const_string str, s64 i)
{
    while (i >= 0 && i < str.size && is_space(str[i]))
        i += 1;

    return i;
}

/*
https://learn.microsoft.com/en-us/dotnet/api/microsoft.win32.filedialog.filter?view=windowsdesktop-8.0
general filter structure is

<label>"|"<name+extensions>["|"<label>"|"<name+extensions>]*

<name+extensions> = <name>"."<extension>[";"<name>"."<extension>]*

<label>, <name> and <extension> are strings.
*/
static s64 ui_fs_parse_filter(const_string str, s64 i, array<ui_fs_dialog_filter> *out_filters)
{
    i = _skip_whitespace(str, i);

    s64 label_start = i;

    while (i >= 0 && i < str.size && str[i] != '|')
        i += 1;

    assert(i >= 0 && i < str.size && str[i] == '|' && "parse error, expected |, got end or invalid char");

    s64 label_end = i;

    ui_fs_dialog_filter *filter = add_at_end(out_filters);
    init(filter);
    filter->label.c_str = str.c_str + label_start;
    filter->label.size  = label_end - label_start;

    i += 1;

    // names and extensions
    while (i >= 0 && i < str.size && str[i] != '|')
    {
        i = _skip_whitespace(str, i);
        s64 name_start = i;
        s64 name_end = -1;
        s64 ext_start = -1;
        s64 ext_end = -1;
        char c = '\0';

        while (i >= 0 && i < str.size)
        {
            c = str[i];

            if (c == '.' || c == ';' || c == '|')
                break;

            i += 1;
        }

        assert(i >= 0 && (i >= str.size || (c == '.' || c == ';' || c == '|')) && "parse error, unexpected character");

        name_end = i;
        assert(name_start != name_end);

        ui_fs_dialog_filter_item *item = add_at_end(&filter->items);
        fill_memory(item, 0);
        item->filename.c_str = str.c_str + name_start;
        item->filename.size  = name_end - name_start;

        if (i >= str.size || c == '|')
            break;

        if (c == '.')
        {
            ext_start = i;
            i += 1;

            while (i >= 0 && i < str.size)
            {
                c = str[i];

                if (c == ';' || c == '|')
                    break;

                i += 1;
            }
            
            assert(i >= 0 && (i >= str.size || (c == ';' || c == '|')) && "parse error, unexpected character");

            ext_end = i;
        }

        if (ext_start != -1 && ext_end != -1)
        {
            assert(ext_start != ext_end);
            item->extension.c_str = str.c_str + ext_start;
            item->extension.size  = ext_end - ext_start;
        }

        if (c == '|')
            break;

        i += 1;
    }

    if (i < str.size)
        i += 1;

    return i;
}

static void ui_fs_parse_filters(const_string str, array<ui_fs_dialog_filter> *out_filters)
{
    assert(out_filters != nullptr);

    ::clear(out_filters);

    if (::string_is_blank(str))
        str = to_const_string(ui_DefaultDialogFilter);

    s64 i = 0;

    while (i >= 0 && i < str.size)
        i = ui_fs_parse_filter(str, i, out_filters);

    assert(i >= str.size && "parse error, there is remaining input");
}

static bool ui_fs_matches_filter(const_string str, ui_fs_dialog_filter *f)
{
    const_string filename{};
    const_string extension{};

    s64 found = ::string_index_of(str, '.');

    if (found < 0)
        found = str.size;

    filename = str;
    filename.size = found;

    if (found < str.size)
    {
        extension.c_str = str.c_str + found;
        extension.size  = str.size - found;
    }

    for_array(i, &f->items)
    {
        bool any_filename = i->filename == "*"_cs;
        bool any_extension = (i->extension.size == 0 || i->extension == ".*"_cs);

        if (any_filename && any_extension)
            return true;

        if ((!any_filename) && filename != i->filename)
            continue;

        if ((!any_extension) && extension != i->extension)
            continue;

        return true;
    }
    
    return false;
}

struct ui_fs_dialog_settings
{
    ImGuiID id;
    string last_directory;
};

static void free(ui_fs_dialog_settings *settings)
{
    settings->id = 0;
    free(&settings->last_directory);
}

struct ui_fs_pin
{
    const_string name;
    string path;
};

static void init(ui_fs_pin *pin)
{
    fill_memory(pin, 0);
}

static void free(ui_fs_pin *pin)
{
    free(&pin->path);
}

static s64 ui_fs_path_pin_index(const_string str, array<ui_fs_pin> *pins)
{
    for_array(i, pin, pins)
        if (to_const_string(pin->path) == str)
            return i;

    return -1;
}

struct ui_fs_ini_settings
{
    ImGuiID id;
    hash_table<ImGuiID, ui_fs_dialog_settings> dialog_settings;
    bool show_hidden;
    bool edit_bar;

    array<ui_fs_pin> pins;
};

static ui_fs_ini_settings _ini_settings;

static void init(ui_fs_ini_settings *settings)
{
    fill_memory(settings, 0);
    settings->id = (ImGuiID)-1;
    init_for_n_items(&settings->dialog_settings, 32);
    init(&settings->pins);
}

static void free(ui_fs_ini_settings *settings)
{
    free<false, true>(&settings->dialog_settings);
    free<true>(&settings->pins);
}

static void _ui_fs_ClearAllFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler)
{
    (void)ctx;
    (void)handler;
    // tprint("ClearAll\n");
    free(&_ini_settings);
    init(&_ini_settings);
}

static void *_ui_fs_ReadOpenFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name)
{
    (void)ctx;
    (void)handler;
    // tprint("ReadOpen %\n", name);
    ImGuiID id = 0;

    if (string_compare(name, ui_Ini_Preferences) == 0)
        return &_ini_settings;

    if (sscanf(name, ui_Ini_Dialog_Preferences ",%08x", &id) < 1)
        return nullptr;

    ui_fs_dialog_settings *settings = search_or_insert(&_ini_settings.dialog_settings, &id);

    if (settings == nullptr)
        return nullptr;

    fill_memory(settings, 0);
    settings->id = id;

    return (void*)settings;
}

static void _ui_fs_ReadLineFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* _entry, const char* _line)
{
    (void)ctx;
    (void)handler;
    // tprint("ReadLine\n");

    ImGuiID *id = (ImGuiID*)_entry;
    const_string line = to_const_string(_line);

    if (*id == (ImGuiID)-1)
    {
        // global settings have id -1
        if      (line == ui_Ini_Preferences_EditBar "=1"_cs)    _ini_settings.edit_bar = true;
        else if (line == ui_Ini_Preferences_ShowHidden "=1"_cs) _ini_settings.show_hidden = true;
        else if (string_begins_with(line, ui_Ini_Preferences_Pin "="_cs))
        {
            const_string pinpath = line;
            pinpath.c_str += string_length(ui_Ini_Preferences_Pin) + 1;
            pinpath.size  -= string_length(ui_Ini_Preferences_Pin) + 1;

            if (!::string_is_blank(pinpath))
            {
                ui_fs_pin *pin = add_at_end(&_ini_settings.pins);

                fs::path tmp{};
                defer { fs::free(&tmp); };
                fs::path_set(&tmp, pinpath);
                
                s64 fname_length = fs::filename(&tmp).size;
                pin->path = string_copy(tmp.data);
                pin->name.c_str = pin->path.data + (pin->path.size - fname_length);
                pin->name.size = fname_length;
            }
        }
    }
    else
    {
        ui_fs_dialog_settings *settings = (ui_fs_dialog_settings*)_entry;

        if (string_begins_with(line, ui_Ini_Dialog_Preferences_LastDirectory "="_cs))
        {
            const_string dir = substring(line, 14);

            if (!::string_is_blank(dir))
                settings->last_directory = string_copy(dir);
        }
    }
}

static void _ui_fs_WriteAllFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    (void)ctx;
    // tprint("WriteAll\n");

    buf->appendf("[%s][" ui_Ini_Preferences "]\n", handler->TypeName);

    buf->appendf(ui_Ini_Preferences_EditBar    "=%s\n", _ini_settings.edit_bar    ? "1" : "0");
    buf->appendf(ui_Ini_Preferences_ShowHidden "=%s\n", _ini_settings.show_hidden ? "1" : "0");

    for_array(pin, &_ini_settings.pins)
        buf->appendf(ui_Ini_Preferences_Pin "=%s\n", pin->path.data);

    buf->append("\n");

    for_hash_table(v, &_ini_settings.dialog_settings)
    {
        buf->appendf("[%s][" ui_Ini_Dialog_Preferences ",%08x]\n", handler->TypeName, v->id);

        if (!::string_is_blank(v->last_directory))
            buf->appendf(ui_Ini_Dialog_Preferences_LastDirectory "=%s\n", v->last_directory.data);

        buf->append("\n");
    }

    buf->append("\n");
}

void ui::filepicker_init()
{
    init(&_ini_settings);
    ImGuiSettingsHandler ini_handler{};
    ini_handler.TypeName = ui_Ini_Name;
    ini_handler.TypeHash = ImHashStr(ui_Ini_Name);
    ini_handler.ClearAllFn = _ui_fs_ClearAllFn;
    ini_handler.ReadOpenFn = _ui_fs_ReadOpenFn;
    ini_handler.ReadLineFn = _ui_fs_ReadLineFn;
    ini_handler.WriteAllFn = _ui_fs_WriteAllFn;
    ImGui::AddSettingsHandler(&ini_handler);
}

void ui::filepicker_exit()
{
    free(&_ini_settings);
}

#define ui_fs_dialog_label_size 32
struct ui_fs_dialog_item
{
    // properties
    fs::path path;
    fs::filesystem_type type;
    fs::filesystem_type symlink_target_type; // only used by symlinks, obviously

    s64  size; // item count for directories, bytes for files / symlinks
    timespan modified;
    timespan created;

    char size_label[ui_fs_dialog_label_size];
    char size_accurate_label[ui_fs_dialog_label_size];
    char modified_label[ui_fs_dialog_label_size];
    char created_label[ui_fs_dialog_label_size];

    // settings / interaction data
    // bool multi_selected; // when multiple items can be selected, this is set per item
};

static void free(ui_fs_dialog_item *item)
{
    fs::free(&item->path);
}

struct ui_fs_dialog
{
    // ImGuiID id;
    // navigation
    fs::path current_dir;
    array<fs::const_fs_string> current_dir_segments;
    bool current_dir_ok;
    string navigation_error_message;
    array<fs::path> back_stack;
    array<fs::path> forward_stack;

    // settings
    array<ui_fs_dialog_filter> filters;
    s64 selected_filter_index;
    string selected_filter_label; // I am deeply upset that ImGui doesn't support string slices (or only for very specific parts)
    bool allow_directories;

    // items, display and sorting
    array<ui_fs_dialog_item> items;
    s64 single_selection_index;
    int  last_sort_criteria;
    bool last_sort_ascending;

    // etc
    fs::path _it_path; // used during iteration, constantly overwritten
    char selection_buffer[255];
    bool selection_exists;
    fs::filesystem_type selection_type;
    fs::filesystem_type selection_symlink_target_type;
    bool selection_matches_filter;
};

static void init(ui_fs_dialog *diag)
{
    fill_memory(diag, 0);
    diag->single_selection_index = -1;
    diag->last_sort_ascending = true;

    fs::init(&diag->current_dir);
    init(&diag->current_dir_segments);
    init(&diag->navigation_error_message);
    init(&diag->back_stack);
    init(&diag->forward_stack);

    init(&diag->filters);
    init(&diag->selected_filter_label);

    init(&diag->items);

    fs::init(&diag->_it_path);
}

static void free(ui_fs_dialog *diag)
{
    fs::free(&diag->current_dir);
    free(&diag->current_dir_segments);
    free(&diag->navigation_error_message);
    free<true>(&diag->back_stack);
    free<true>(&diag->forward_stack);

    free<true>(&diag->filters);
    free(&diag->selected_filter_label);

    free<true>(&diag->items);
    fs::free(&diag->_it_path);
}

// must be same order as the table
enum ui_fs_dialog_sort_criteria
{
    ui_Sort_Type,
    ui_Sort_Name,
    ui_Sort_Size,
    ui_Sort_Modified,
    ui_Sort_Created,
};

static int _compare_item_type_ascending(const ui_fs_dialog_item *lhs, const ui_fs_dialog_item *rhs)
{
    return compare_ascending((int)lhs->type, (int)rhs->type);
}

static int _compare_item_type_descending(const ui_fs_dialog_item *lhs, const ui_fs_dialog_item *rhs)
{
    return -_compare_item_type_ascending(lhs, rhs);
}

#define _is_directory_type(Type, Symtype) ((Type) == fs::filesystem_type::Directory || ((Type) == fs::filesystem_type::Symlink && (Symtype) == fs::filesystem_type::Directory))
#define _is_directory(X) _is_directory_type((X)->type, (X)->symlink_target_type)

#define sort_directories_first(lhs, rhs)\
    if      (_is_directory(lhs) && !_is_directory(rhs)) return -1;\
    else if (_is_directory(rhs) && !_is_directory(lhs)) return 1;

static int _compare_item_name_ascending(const ui_fs_dialog_item *lhs, const ui_fs_dialog_item *rhs)
{
    sort_directories_first(lhs, rhs);
    return lexicoraphical_compare(lhs->path.data, rhs->path.data);
}

static int _compare_item_name_descending(const ui_fs_dialog_item *lhs, const ui_fs_dialog_item *rhs)
{
    sort_directories_first(lhs, rhs);
    return -lexicoraphical_compare(lhs->path.data, rhs->path.data);
}

static int _compare_item_size_ascending(const ui_fs_dialog_item *lhs, const ui_fs_dialog_item *rhs)
{
    sort_directories_first(lhs, rhs);
    return compare_ascending(lhs->size, rhs->size);
}

static int _compare_item_size_descending(const ui_fs_dialog_item *lhs, const ui_fs_dialog_item *rhs)
{
    sort_directories_first(lhs, rhs);
    return -compare_ascending(lhs->size, rhs->size);
}

static int _compare_item_modified_ascending(const ui_fs_dialog_item *lhs, const ui_fs_dialog_item *rhs)
{
    sort_directories_first(lhs, rhs);
    return timespan_compare(&lhs->modified, &rhs->modified);
}

static int _compare_item_modified_descending(const ui_fs_dialog_item *lhs, const ui_fs_dialog_item *rhs)
{
    sort_directories_first(lhs, rhs);
    return -timespan_compare(&lhs->modified, &rhs->modified);
}

static int _compare_item_created_ascending(const ui_fs_dialog_item *lhs, const ui_fs_dialog_item *rhs)
{
    sort_directories_first(lhs, rhs);
    return timespan_compare(&lhs->created, &rhs->created);
}

static int _compare_item_created_descending(const ui_fs_dialog_item *lhs, const ui_fs_dialog_item *rhs)
{
    sort_directories_first(lhs, rhs);
    return -timespan_compare(&lhs->created, &rhs->created);
}

static void _ui_fs_dialog_sort_items(ui_fs_dialog *diag, int criteria, bool ascending = true)
{
    ui_fs_dialog_item *items = diag->items.data;
    s64 count = diag->items.size;

    if (count <= 0)
        return;

    compare_function_p<ui_fs_dialog_item> comp = nullptr;

    switch (criteria)
    {
    case ui_Sort_Type:
        if (ascending)  comp = _compare_item_type_ascending;
        else            comp = _compare_item_type_descending;
        break;
    case ui_Sort_Name:
        if (ascending)  comp = _compare_item_name_ascending;
        else            comp = _compare_item_name_descending;
        break;
    case ui_Sort_Size:
        if (ascending)  comp = _compare_item_size_ascending;
        else            comp = _compare_item_size_descending;
        break;
    case ui_Sort_Modified:
        if (ascending)  comp = _compare_item_modified_ascending;
        else            comp = _compare_item_modified_descending;
        break;
    case ui_Sort_Created:
        if (ascending)  comp = _compare_item_created_ascending;
        else            comp = _compare_item_created_descending;
        break;
    }

    if (comp != nullptr)
        sort(items, count, comp);

    diag->last_sort_criteria  = criteria;
    diag->last_sort_ascending = ascending;
}

static void _ui_fs_sort_by_imgui_spec(ui_fs_dialog *diag, ImGuiTableSortSpecs *specs)
{
    for (int i = 0; i < specs->SpecsCount; ++i)
    {
        const ImGuiTableColumnSortSpecs *s = specs->Specs + i;

        _ui_fs_dialog_sort_items(diag, s->ColumnIndex, s->SortDirection == ImGuiSortDirection_Ascending);
    }
}

static inline ImVec2 floor(float x, float y)
{
    return ImVec2((float)(int)x, (float)(int)y);
}

static inline ImVec2 floor(ImVec2 v)
{
    return ImVec2((float)(int)v.x, (float)(int)v.y);
}

static void _ui_fs_render_filesystem_type(ImDrawList *lst, fs::filesystem_type type, float size, ImU32 color)
{
    ImVec2 p0 = ImGui::GetCursorScreenPos() + ImVec2(0.5f, 0.5f);
    const float factor = 0.2f;
    float thickness = (float)(int)(size / 20.f);

    if (thickness < 1.f)
        thickness = 1.f;

    switch (type)
    {
    case fs::filesystem_type::Directory:
    {
        p0 += ImVec2((float)(int)(-size * factor * 0.25f), (float)(int)(size * factor * 0.3f));
        // Draw a folder
        const auto tl  = p0 + floor(size * (factor),            size * (factor * 2.f));
        const auto m1  = p0 + floor(size * (factor * 2.f),      size * (factor * 2.f));
        const auto m2  = p0 + floor(size * (factor * 3.f),      size * (factor * 1.0f));
        const auto tr  = p0 + floor(size * (1 - factor * 0.5f), size * (factor * 1.0f));
        const auto tr2 = p0 + floor(size * (1 - factor * 0.5f), size * (factor * 2.0f));
        const auto br  = p0 + floor(size * (1 - factor * 0.5f), size * (1 - factor * 0.8f));
        const auto bl  = p0 + floor(size * (factor),            size * (1 - factor * 0.8f));

        lst->PathLineTo(tl);
        lst->PathLineTo(tr2);
        lst->PathLineTo(br);
        lst->PathLineTo(bl);
        lst->PathLineTo(tl);
        lst->PathStroke(color, 0, thickness);

        lst->PathLineTo(m1);
        lst->PathLineTo(m2);
        lst->PathLineTo(tr);
        lst->PathLineTo(tr2);
        lst->PathLineTo(m1);
        lst->PathFillConvex(color);

        // extra "file"
        const auto ftl = p0 + floor(size * (factor),            size * (factor * 0.5f));
        const auto ftr = p0 + floor(size * (factor * 2.5f),     size * (factor * 0.5f));

        lst->PathLineTo(tl);
        lst->PathLineTo(ftl);
        lst->PathLineTo(ftr);
        lst->PathLineTo(m2);
        lst->PathStroke(color, 0, thickness);
        ImGui::Dummy(ImVec2(size, 0));
        break;
    }
    case fs::filesystem_type::Symlink: 
        // TODO: draw an arrow or something
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("->"); 
        ImGui::PopStyleColor();
        break;
    case fs::filesystem_type::File:
    {
        // Draw a file icon
        const auto tl  = p0 + floor(size * (factor),         size * (factor * 0.5f));
        const auto tr1 = p0 + floor(size * (1 - factor * 2), size * (factor * 0.5f));
        const auto tr2 = p0 + floor(size * (1 - factor),     size * (factor * 1.5f));
        const auto trc = p0 + floor(size * (1 - factor * 2), size * (factor * 1.5f));
        const auto br  = p0 + floor(size * (1 - factor),     size * (1 - factor * 0.5f));
        const auto bl  = p0 + floor(size * (factor),         size * (1 - factor * 0.5f));

        lst->PathLineTo(tr1 + ImVec2(0, 0.5f));
        lst->PathLineTo(tr2);
        lst->PathStroke(color, 0, thickness);

        lst->PathLineTo(tr2);
        lst->PathLineTo(br);
        lst->PathLineTo(bl);
        lst->PathLineTo(tl);
        lst->PathLineTo(tr1);
        lst->PathLineTo(trc);
        lst->PathLineTo(tr2);
        lst->PathStroke(color, 0, thickness);
        ImGui::Dummy(ImVec2(size, 0));
        break;
    }
    case fs::filesystem_type::CharacterFile:
    case fs::filesystem_type::Pipe:
    case fs::filesystem_type::Unknown:
    default: 
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("?");
        ImGui::PopStyleColor();
        break;
    }
}

static void _format_date(char *buf, s64 buf_size, timespan *sp)
{
    if (sp == nullptr || sp->seconds < 0)
    {
        string_copy("?", buf);
        return;
    }

#if Windows
    SYSTEMTIME tutc{};
    SYSTEMTIME t{};

    if (!FileTimeToSystemTime((const FILETIME *)sp, &tutc))
    {
        string_copy("?", buf);
        return;
    }

    if (!SystemTimeToTzSpecificLocalTime(nullptr, &tutc, &t))
    {
        string_copy("?", buf);
        return;
    }

    format(buf, buf_size, "%d-%02d-%02d %02d:%02d:%02d",
           t.wYear,
           t.wMonth,
           t.wDay,
           t.wHour,
           t.wMinute,
           t.wSecond);
#else
    struct tm t;

    tzset(); // oof

    if (localtime_r((time_t*)sp, &t) == nullptr)
    {
        string_copy("?", buf);
        return;
    }

    if (strftime(buf, buf_size, "%F %T", &t) <= 0)
    {
        string_copy("?", buf);
        return;
    }
#endif
}

static bool _ui_fs_dialog_load_path(ui_fs_dialog *diag)
{
    diag->current_dir.data[diag->current_dir.size] = '\0';
    fs::path *it = &diag->_it_path;
    fs::path_set(it, &diag->current_dir);

    fs::path_segments(&diag->current_dir, &diag->current_dir_segments);
    diag->single_selection_index = -1;

    // TODO: reuse item memory
    free<true>(&diag->items);
    init(&diag->items);

    error _err{};

    const s64 base_size = it->size;
    for_path(item, &diag->current_dir, fs::iterate_option::QueryType, &_err)
    {
        it->size = base_size;
        it->data[base_size] = '\0';
        fs::path_append(it, item->path);

        ui_fs_dialog_item *ditem = add_at_end(&diag->items);
        fill_memory(ditem, 0);

        fs::path_set(&ditem->path, item->path);
        ditem->type = item->type;

        // Gather filesystem information
        if (item->type == fs::filesystem_type::Symlink)
            fs::get_filesystem_type(it, &ditem->symlink_target_type);

        // _is_directory also checks for symlinks targeting directories
        if (_is_directory(ditem))
            ditem->size = fs::get_children_count(it);

#if Windows
        fs::filesystem_info info{};

        if (!_is_directory(ditem))
        {
            if (!fs::query_filesystem(it, &info, true, fs::query_flag::Size))
            {
                free(ditem);
                remove_from_end(&diag->items);
                continue;
            }

            ditem->size = fs::get_file_size(&info);
        }

        if (!fs::query_filesystem(it, &info, true, fs::query_flag::FileTimes))
        {
            free(ditem);
            remove_from_end(&diag->items);
            continue;
        }

        ditem->modified.seconds = info.detail.file_times.last_write_time;
        ditem->modified.nanoseconds = 0;
        ditem->created.seconds = info.detail.file_times.creation_time;
        ditem->created.nanoseconds = 0;
#else // Linux
        fs::filesystem_info info{};

        if (!fs::query_filesystem(it, &info, true, fs::query_flag_default))
        {
            free(ditem);
            remove_from_end(&diag->items);
            continue;
        }

        if (!_is_directory(ditem))
            ditem->size = info.stx_size;

        ditem->modified.seconds     = info.stx_mtime.tv_sec;
        ditem->modified.nanoseconds = info.stx_mtime.tv_nsec;
        ditem->created.seconds      = info.stx_btime.tv_sec;
        ditem->created.nanoseconds  = info.stx_btime.tv_nsec;
#endif

        // Prepare strings to display

        // File size
        if (ditem->size < 0)
            string_copy("?", ditem->size_label);
        else
        {
            if (_is_directory(ditem))
                format(ditem->size_label, ui_fs_dialog_label_size, "% items", ditem->size);
            else
            {
                constexpr const s64 EiB = S64_LIT(1) << S64_LIT(60);
                constexpr const s64 PiB = S64_LIT(1) << S64_LIT(50);
                constexpr const s64 TiB = S64_LIT(1) << S64_LIT(40);
                constexpr const s64 GiB = S64_LIT(1) << S64_LIT(30);
                constexpr const s64 MiB = S64_LIT(1) << S64_LIT(20);
                constexpr const s64 KiB = S64_LIT(1) << S64_LIT(10);

#define _FSUI_Format_Unit(Unit)\
    else if (ditem->size >= Unit)\
        format(ditem->size_label, ui_fs_dialog_label_size, "%.1f " #Unit, (double)ditem->size / (double)Unit);

                format(ditem->size_accurate_label, ui_fs_dialog_label_size, "% bytes", ditem->size);

                if (false) {}
                _FSUI_Format_Unit(EiB)
                _FSUI_Format_Unit(PiB)
                _FSUI_Format_Unit(TiB)
                _FSUI_Format_Unit(GiB)
                _FSUI_Format_Unit(MiB)
                _FSUI_Format_Unit(KiB)
                else
                    format(ditem->size_label, ui_fs_dialog_label_size, "% bytes", ditem->size);

#undef _FSUI_Format_Unit
            }
        }

        // Modified & Created date
        _format_date(ditem->modified_label, ui_fs_dialog_label_size, &ditem->modified);
        _format_date(ditem->created_label,  ui_fs_dialog_label_size, &ditem->created);
    }

    diag->current_dir_ok = _err.error_code == 0;

    if (_err.error_code != 0)
    {
        string_set(&diag->navigation_error_message, _err.what);
        return false;
    }

    _ui_fs_dialog_sort_items(diag, diag->last_sort_criteria, diag->last_sort_ascending);

    return true;
}

static void _history_push(array<fs::path> *stack, fs::path *path)
{
    if (path == nullptr || path->data == nullptr || ::string_is_blank(path->data))
        return;

    if (stack->size > 0 && string_compare(path->data, (end(stack)-1)->data) == 0)
        // don't add if its already at the end of the stack
        return;

    fs::path *e = add_at_end(stack);
    init(e);
    fs::path_set(e, path);
}

static void _history_pop(array<fs::path> *stack, fs::path *out)
{
    if (stack->size <= 0)
        return;

    fs::path *ret = end(stack) - 1;
    fs::path_set(out, ret);
    fs::free(ret);
    remove_from_end(stack);
}

static void _history_clear(array<fs::path> *stack)
{
    for_array(p, stack)
        fs::free(p);

    clear(stack);
}

namespace ui
{
bool FileDialog(const char *label, char *out_filebuf, size_t filebuf_size, const char *filter, ui_FilepickerFlags flags)
{
    ImGuiContext &g = *GImGui; (void)g;
    ImGuiWindow *window = g.CurrentWindow; (void)window;
    ImGuiStorage *storage = ImGui::GetStateStorage(); (void)storage;
    ImGuiStyle *style = &g.Style;

    bool alt  = ImGui::GetIO().KeyMods & ImGuiMod_Alt;
    bool ctrl = ImGui::GetIO().KeyMods & ImGuiMod_Ctrl;

    (void)label;
    (void)out_filebuf;
    (void)filebuf_size;
    (void)filter;
    (void)flags;
    // ImGuiSettingsHandler

    const ImGuiID id = window->GetID(label);
    static char navbar_content[ui_Dialog_Navbar_Size] = {0};
    static char quicksearch_content[256] = {0};

    bool selection_changed = false;

    // SETUP
    ui_fs_dialog *diag = (ui_fs_dialog*)storage->GetVoidPtr(id);

    if (diag == nullptr)
    {
        // tprint("Alloc'd\n");
        diag = alloc<ui_fs_dialog>();
        storage->SetVoidPtr(id, (void*)diag);

        init(diag);

        ui_fs_parse_filters(to_const_string(filter), &diag->filters);
        assert(diag->filters.size > 0);
        string_set(&diag->selected_filter_label, diag->filters[0].label);

        ui_fs_dialog_settings *settings = search(&_ini_settings.dialog_settings, &id);

        if (settings == nullptr)
        {
            settings = add_element_by_key(&_ini_settings.dialog_settings, &id);
            settings->id = id;
            init(&settings->last_directory);
        }

        _history_push(&diag->back_stack, &diag->current_dir);
        _history_clear(&diag->forward_stack);

        if (::string_is_blank(settings->last_directory))
            fs::get_current_path(&diag->current_dir);
        else
            fs::path_set(&diag->current_dir, to_const_string(settings->last_directory));

        _ui_fs_dialog_load_path(diag);
        string_copy(diag->current_dir.data, navbar_content, ui_Dialog_Navbar_Size - 1);
        quicksearch_content[0] = '\0';
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F5))
    {
        // refresh
        _ui_fs_dialog_load_path(diag);
        string_copy(diag->current_dir.data, navbar_content, ui_Dialog_Navbar_Size - 1);
    }

    // HOME
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    if (ImGui::Button("@"))
    {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Home");

        _history_push(&diag->back_stack, &diag->current_dir);
        _history_clear(&diag->forward_stack);
        fs::get_home_path(&diag->current_dir);
        _ui_fs_dialog_load_path(diag);
        string_copy(diag->current_dir.data, navbar_content, ui_Dialog_Navbar_Size - 1);
    }

    ImGui::SameLine();

    // HISTORY
    bool can_back = diag->back_stack.size > 0;
    ImGui::BeginDisabled(!can_back);
    if (ImGui::ArrowButton("##history_back", ImGuiDir_Left) || (can_back && alt && ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)))
    {
        _history_push(&diag->forward_stack, &diag->current_dir);
        _history_pop(&diag->back_stack, &diag->current_dir);

        _ui_fs_dialog_load_path(diag);
        string_copy(diag->current_dir.data, navbar_content, ui_Dialog_Navbar_Size - 1);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    bool can_forward = diag->forward_stack.size > 0;
    ImGui::BeginDisabled(!can_forward);
    if (ImGui::ArrowButton("##history_forward", ImGuiDir_Right) || (can_forward && alt && ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)))
    {
        _history_push(&diag->back_stack, &diag->current_dir);
        _history_pop(&diag->forward_stack, &diag->current_dir);

        _ui_fs_dialog_load_path(diag);
        string_copy(diag->current_dir.data, navbar_content, ui_Dialog_Navbar_Size - 1);
    }
    ImGui::EndDisabled();
    
    ImGui::SameLine();

    // NAVIGATE UP
    if (ImGui::ArrowButton("##go_up", ImGuiDir_Up) || (alt && ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)))
    {
        auto parent = fs::parent_path_segment(&diag->current_dir);
        
        if (parent.size > 0)
        {
            _history_push(&diag->back_stack, &diag->current_dir);
            _history_clear(&diag->forward_stack);
            diag->current_dir.size = parent.size;
            diag->current_dir.data[parent.size] = '\0';
            _ui_fs_dialog_load_path(diag);
            string_copy(diag->current_dir.data, navbar_content, ui_Dialog_Navbar_Size - 1);
        }
    }
    ImGui::PopStyleVar();

    // INPUT BAR
    const float total_space = ImGui::GetContentRegionMax().x;
    const float min_size_left = 300.f;

    if (!_ini_settings.edit_bar)
    {
        // The editing bar
        bool navigate_to_written_dir = false;

        if (ImGui::IsKeyPressed(ImGuiKey_F4))
            ImGui::SetKeyboardFocusHere();

        ImGui::SameLine();

        navigate_to_written_dir = ImGui::InputTextEx("##navbar",
                                                     nullptr,
                                                     navbar_content,
                                                     ui_Dialog_Navbar_Size - 1,
                                                     ImVec2(Max(10.f, total_space - min_size_left), 0),
                                                     ImGuiInputTextFlags_EnterReturnsTrue,
                                                     nullptr,
                                                     nullptr
                                                     );

        ImGui::SameLine();

        if (ImGui::Button("[-/-]"))
            _ini_settings.edit_bar = !_ini_settings.edit_bar;

        ImGui::SameLine();
        navigate_to_written_dir |= ImGui::Button("Go");

        if (navigate_to_written_dir)
        {
            _history_push(&diag->back_stack, &diag->current_dir);
            _history_clear(&diag->forward_stack);

            if (!::string_is_blank(navbar_content))
                fs::path_set(&diag->current_dir, navbar_content);

            _ui_fs_dialog_load_path(diag);
        }
    }
    else
    {
        // The segment click bar
        ImGui::SameLine();

        if (ImGui::BeginChild("##segment_click_bar", ImVec2(Max(10.f, total_space - min_size_left), ImGui::GetFrameHeight()), 0, 0))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style->ItemSpacing.x / 2, 0));
            ImGui::PushStyleColor(ImGuiCol_Button, style->Colors[ImGuiCol_WindowBg]);

            for_array(i, seg, &diag->current_dir_segments)
            {
                // (we skip the last entry, the button for the last entry does nothing)
                if (ButtonSlice(seg->c_str, seg->c_str + seg->size, ImVec2(0, 0), 0) && i < diag->current_dir_segments.size - 1)
                {
                    s64 end = (seg->c_str + seg->size) - diag->current_dir.data;
                    _history_push(&diag->back_stack, &diag->current_dir);
                    _history_clear(&diag->forward_stack);

                    diag->current_dir.size = end;
                    diag->current_dir.data[end] = '\0';

                    _ui_fs_dialog_load_path(diag);
                    string_copy(diag->current_dir.data, navbar_content, ui_Dialog_Navbar_Size - 1);
                }

                ImGui::SameLine();
            }

            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }

        ImGui::EndChild();

        ImGui::SameLine();

        if (ImGui::Button("Edit"))
            _ini_settings.edit_bar = !_ini_settings.edit_bar;
    }
    
    // NEXT LINE, mostly just options
    ImGui::Checkbox("show hidden", &_ini_settings.show_hidden);

    ImGui::SameLine();

    // Quicksearch
    bool quicksearch_performed = false;
    s64  quicksearch_result = -1;
    bool quicksearch_submit = false;
    ImGui::SetNextItemWidth(200.f);

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_F))
    {
        quicksearch_content[0] = '\0';
        ImGui::SetKeyboardFocusHere();
    }

    if (ImGui::InputText("quicksearch", quicksearch_content, 255, ImGuiInputTextFlags_EnterReturnsTrue))
        quicksearch_submit = quicksearch_content[0] != '\0';

    if (ImGui::IsItemFocused()
     && ImGui::IsItemEdited())
    {
        if (quicksearch_content[0] == '\0')
            quicksearch_result = -1;
        else
        {
            quicksearch_performed = true;
            for_array(i, item, &diag->items)
                if (string_begins_with(item->path.data, quicksearch_content))
                {
                    quicksearch_result = i;
                    break;
                }
        }
    }

    if (quicksearch_performed && quicksearch_result == -1)
        diag->single_selection_index = -1;

    // ITEMS
    const float font_size = ImGui::GetFontSize();
    const float bottom_padding = -1 * (ImGui::GetFrameHeight() + style->ItemSpacing.y);
    ImU32 font_color = ImGui::ColorConvertFloat4ToU32(style->Colors[ImGuiCol_Text]);

    bool submit_selection = false;

    // pins
    if (ImGui::BeginChild("Pins", ImVec2(100.f, bottom_padding), ImGuiChildFlags_ResizeX))
    {
        ImGui::Text("Pins");
        ImGui::Separator();

        for_array(i, pin, &_ini_settings.pins)
        {
            ImGui::PushID((int)i);
            if (ImGui::Selectable(pin->name.c_str))
            {
                diag->selection_buffer[0] = '\0';
                selection_changed = true;

                _history_push(&diag->back_stack, &diag->current_dir);
                _history_clear(&diag->forward_stack);
                fs::path_set(&diag->current_dir, pin->path);
                _ui_fs_dialog_load_path(diag);
                string_copy(diag->current_dir.data, navbar_content, ui_Dialog_Navbar_Size - 1);
            }

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                // Set payload to carry the index of our item (could be anything)
                ImGui::SetDragDropPayload("pin_reorder", &i, sizeof(i));
                ImGui::Text("%s", pin->name.c_str);
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("pin_reorder"))
                {
                    assert(payload->DataSize == sizeof(i));
                    s64 i_next = *(const s64*)payload->Data;

                    if (i != i_next)
                    {
                        ui_fs_pin tmp = *pin;
                        _ini_settings.pins[i] = _ini_settings.pins[i_next];
                        _ini_settings.pins[i_next] = tmp;
                    }
                }

                ImGui::EndDragDropTarget();
            }

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Unpin"))
                {
                    free(pin);
                    remove_elements(&_ini_settings.pins, i, 1);
                    i -= 1;
                }

                ImGui::EndPopup();
            }

            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, style->ItemSpacing.y));
    ImGui::SameLine();
    ImGui::PopStyleVar();

    if (!diag->current_dir_ok)
    {
        if (ImGui::BeginChild("##empty_or_not_found", ImVec2(-0, bottom_padding)))
            ImGui::Text("%s\n", diag->navigation_error_message.data);

        ImGui::EndChild();
    }
    else
    {
        const int table_flags = ImGuiTableFlags_ScrollY
                              | ImGuiTableFlags_ScrollX
                              | ImGuiTableFlags_NoBordersInBody
                              | ImGuiTableFlags_BordersOuterV
                              | ImGuiTableFlags_BordersOuterH
                              | ImGuiTableFlags_RowBg
                              | ImGuiTableFlags_Resizable
                              | ImGuiTableFlags_Sortable
                              // | ImGuiTableFlags_SortMulti
                              ;

        s64 navigate_into_index = -1;

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(style->CellPadding.x, 3));
        if (ImGui::BeginTable("fs_dialog_content_table", 5, table_flags, ImVec2(-0, bottom_padding)))
        {
            // Display headers so we can inspect their interaction with borders
            // (Headers are not the main purpose of this section of the demo, so we are not elaborating on them now. See other sections for details)
            ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
            ImGui::TableSetupColumn(""/*type*/, ImGuiTableColumnFlags_DefaultSort);
            ImGui::TableSetupColumn("Name",     ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Size",     ImGuiTableColumnFlags_DefaultSort);
            ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_DefaultSort);
            ImGui::TableSetupColumn("Created",  ImGuiTableColumnFlags_DefaultSort);
            ImGui::TableHeadersRow();

            if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs())
            if (sort_specs->SpecsDirty)
            {
                _ui_fs_sort_by_imgui_spec(diag, sort_specs);
                sort_specs->SpecsDirty = false;
            }

            ImDrawList *draw_list = ImGui::GetWindowDrawList();

            for_array(i, item, &diag->items)
            {
                if (!_ini_settings.show_hidden && item->path.data[0] == '.')
                    continue;

                bool is_dir = _is_directory(item);

                if ((!is_dir) && (flags & ui_FilepickerFlags_NoFiles))
                    continue;

                if ((!is_dir) && !ui_fs_matches_filter(to_const_string(item->path), diag->filters.data + diag->selected_filter_index))
                    continue;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                _ui_fs_render_filesystem_type(draw_list, item->type, font_size, font_color);

                ImGui::TableNextColumn();

                if (quicksearch_result == i)
                {
                    diag->single_selection_index = i;
                    string_copy(item->path.data, diag->selection_buffer, 255);
                    selection_changed = true;
                    ImGui::SetScrollHereY(0.5f);
                }

                bool navigate_into = false;

                // TODO: if (single/multi select ...)
                if (ImGui::Selectable(item->path.data, diag->single_selection_index == i, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
                {
                    diag->single_selection_index = i;
                    string_copy(item->path.data, diag->selection_buffer, 255);
                    selection_changed = true;

                    navigate_into |= ImGui::IsMouseDoubleClicked(0);
                    navigate_into |= ImGui::IsKeyPressed(ImGuiKey_Enter);
                }
                ImGui::SetItemKeyOwner(ImGuiMod_Alt);

                if (_is_directory(item))
                {
                    if (ImGui::BeginPopupContextItem())
                    {
                        // pins
                        if (ImGui::MenuItem("Pin / Unpin"))
                        {
                            fs::path_set(&diag->_it_path, diag->current_dir);
                            fs::path_append(&diag->_it_path, item->path);

                            s64 pin_index = -1;

                            pin_index = ui_fs_path_pin_index(to_const_string(diag->_it_path), &_ini_settings.pins);

                            if (pin_index < 0)
                            {
                                ui_fs_pin *pin = add_at_end(&_ini_settings.pins);
                                init(pin);

                                string_set(&pin->path, to_const_string(diag->_it_path));
                                // pin->path = string_copy(diag->_it_path.data);
                                pin->name.c_str = pin->path.data + (pin->path.size - item->path.size);
                                pin->name.size = item->path.size;
                            }
                            else
                            {
                                ui_fs_pin *pin = _ini_settings.pins.data + pin_index;
                                free(pin);
                                remove_elements(&_ini_settings.pins, pin_index, 1);
                            }
                        }

                        ImGui::EndPopup();
                    }
                }

                navigate_into |= (diag->single_selection_index == i && quicksearch_submit);

                if (navigate_into)
                    navigate_into_index = i;

                // Size
                ImGui::TableNextColumn();
                ImGui::Text("%s", item->size_label);
                if (item->size_accurate_label[0] && ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", item->size_accurate_label);

                ImGui::TableNextColumn();
                ImGui::Text("%s", item->modified_label);

                ImGui::TableNextColumn();
                ImGui::Text("%s", item->created_label);
            }

            // TODO: maybe Keyboard input for quicksearch?

            ImGui::EndTable();
        }
        ImGui::PopStyleVar();

        if (navigate_into_index >= 0)
        {
            assert(navigate_into_index < diag->items.size);
            ui_fs_dialog_item *item = diag->items.data + navigate_into_index;

            // Directories shall always be navigated into with double click / enter,
            // never opened.
            if (_is_directory(item))
            {
                diag->selection_buffer[0] = '\0';
                selection_changed = true;

                _history_push(&diag->back_stack, &diag->current_dir);
                _history_clear(&diag->forward_stack);
                fs::path_append(&diag->current_dir, item->path);
                _ui_fs_dialog_load_path(diag);
                string_copy(diag->current_dir.data, navbar_content, ui_Dialog_Navbar_Size - 1);
            }
            else
                submit_selection = true;
        }
    }

    // TODO: if multiple, display number of items instead
    // TODO: completion / suggestions
    const float filter_width = 7.f * font_size;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - filter_width - min_size_left);
    submit_selection |= ImGui::InputText("##selection", diag->selection_buffer, 255, ImGuiInputTextFlags_EnterReturnsTrue);

    if (ImGui::IsItemFocused()
     && ImGui::IsItemEdited())
        selection_changed = true;

    ImGui::SameLine();

    // filter
    ImGui::SetNextItemWidth(filter_width);
    if (ImGui::BeginCombo("##filter", diag->selected_filter_label.data, flags))
    {
        for_array(i, dfilter, &diag->filters)
        {
            ImGui::PushID((int)i);
            if (ImGui::Selectable("##x", i == diag->selected_filter_index, ImGuiSelectableFlags_AllowOverlap))
            {
                diag->selected_filter_index = i;
                string_set(&diag->selected_filter_label, diag->filters[i].label);
                selection_changed = true;
            }

            if (i == diag->selected_filter_index)
                ImGui::SetItemDefaultFocus();
            ImGui::PopID();
            ImGui::SameLine();

            TextSlice(dfilter->label);

            ImGui::SameLine();
            ImGui::Text("(");

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, style->ItemSpacing.y));
            ui_fs_dialog_filter_item *item = &dfilter->items[0];

            ImGui::SameLine();
            TextSlice(item->filename);

            if (item->extension.size > 0)
            {
                ImGui::SameLine();
                TextSlice(item->extension);
            }

            for (s64 ii = 1; ii < dfilter->items.size; ++ii)
            {
                item = dfilter->items.data + ii;

                ImGui::SameLine();
                ImGui::Text(", ");
                ImGui::SameLine();
                TextSlice(item->filename);

                if (item->extension.size > 0)
                {
                    ImGui::SameLine();
                    TextSlice(item->extension);
                }
            }

            ImGui::SameLine();
            ImGui::Text(")");
            ImGui::PopStyleVar();
        }

        ImGui::EndCombo();
    }

    if (selection_changed)
    {
        // if selection changed, either through clicking an item inside the table
        // or manually editing the selection content, check if selection is correct.

        if (diag->selection_buffer[0] == '\0')
        {
            diag->selection_exists = false;
            diag->selection_type = fs::filesystem_type::Unknown;
            diag->selection_symlink_target_type = fs::filesystem_type::Unknown;
            diag->selection_matches_filter = false;
        }
        else
        {
            fs::path_set(&diag->_it_path, diag->current_dir);
            fs::path_append(&diag->_it_path, diag->selection_buffer);

            diag->selection_exists = fs::exists(diag->_it_path) == 1;

            if (diag->selection_exists)
            {
                fs::get_filesystem_type(&diag->_it_path, &diag->selection_type, false);

                // Gather filesystem information
                if (diag->selection_type == fs::filesystem_type::Symlink)
                    fs::get_filesystem_type(&diag->_it_path, &diag->selection_symlink_target_type);
            }
            else
            {
                // does not exist
                diag->selection_type = fs::filesystem_type::Unknown;
                diag->selection_symlink_target_type = fs::filesystem_type::Unknown;
            }

            if (_is_directory_type(diag->selection_type, diag->selection_symlink_target_type))
                diag->selection_matches_filter = true; // directories always match
            else
                diag->selection_matches_filter = ui_fs_matches_filter(to_const_string(diag->selection_buffer), diag->filters.data + diag->selected_filter_index);
        }
    }


    ImGui::SameLine();

    bool cancelled = ImGui::Button("Cancel");

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        cancelled = true;

    ImGui::SameLine();

    // TODO: if multiple
    bool selection_allowed = diag->selection_buffer[0] != '\0';

    if (selection_allowed && (flags & ui_FilepickerFlags_SelectionMustExist))
        selection_allowed &= diag->selection_exists;

    if (selection_allowed && (flags & ui_FilepickerFlags_NoDirectories))
        selection_allowed &= !_is_directory_type(diag->selection_type, diag->selection_symlink_target_type);
    else if (selection_allowed && (flags & ui_FilepickerFlags_NoFiles))
        selection_allowed &=  _is_directory_type(diag->selection_type, diag->selection_symlink_target_type);

    if (selection_allowed)
        selection_allowed &= diag->selection_matches_filter;

    submit_selection &= selection_allowed;
    ImGui::BeginDisabled(!selection_allowed);
    submit_selection |= ImGui::Button("Select");
    ImGui::EndDisabled();

    if (submit_selection || cancelled)
    {
        // tprint("Free'd\n");

        if (submit_selection)
        {
            // assemble the full path
            fs::path_set(&diag->_it_path, diag->current_dir);
            fs::path_append(&diag->_it_path, diag->selection_buffer);

            // copy to output
            string_copy(diag->_it_path.data, out_filebuf, filebuf_size);

            // save settings
            ui_fs_dialog_settings *settings = search(&_ini_settings.dialog_settings, &id);

            assert(settings != nullptr);
            string_set(&settings->last_directory, to_const_string(diag->current_dir));
        }

        free(diag);
        dealloc(diag);
        storage->SetVoidPtr(id, nullptr);
    }

    return submit_selection || cancelled;
}
}

bool ui::Filepicker(const char *label, char *buf, size_t buf_size, const char *filter, int flags)
{
#define ui_IllegalFlag (ui_FilepickerFlags_NoDirectories | ui_FilepickerFlags_NoFiles)
    assert(((flags & ui_IllegalFlag) != ui_IllegalFlag) && "NoDirectories and NoFiles may not be set at the same time");

    ImGuiContext &g = *GImGui; (void)g;
    ImGuiWindow *window = g.CurrentWindow; (void)window;
    ImGuiStyle &st = ImGui::GetStyle();

    ImGui::PushID(label);
    bool text_edited = ImGui::InputTextEx("", NULL, buf, (int)buf_size, ImVec2(0, 0), 0);
    ImGui::SameLine();
    ImGui::Dummy({-st.FramePadding.x * 2, 0});
    ImGui::SameLine();
    bool picker_clicked = ImGui::Button("...");
    ImGui::SameLine();
    ImGui::Text(label);

    constexpr const char *popup_label = "Select File...";

    if (picker_clicked)
        ImGui::OpenPopup(popup_label);

    if (ImGui::BeginPopupModal(popup_label))
    {
        if (ui::FileDialog(popup_label, buf, buf_size, filter, flags))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    ImGui::PopID();

    return text_edited;
}
