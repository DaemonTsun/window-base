
// TODO: filter (by extension / filetype / file or dir)


// future features:
// TODO: multiple selection
// TODO: pins
// TODO: jump when typing in result table

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include <time.h> // how sad, used for modified/created timestamp formatting
#include "imgui_internal.h"

#include "shl/print.hpp"
#include "shl/sort.hpp"
#include "shl/hash_table.hpp"
#include "shl/memory.hpp"
#include "shl/string.hpp"
#include "shl/time.hpp"
#include "fs/path.hpp"
#include "fs-ui/filepicker.hpp"

// utils
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

// why does ImGui not support this again...?
bool ButtonSlice(const char* label, const char *label_end, const ImVec2& size_arg, ImGuiButtonFlags flags)
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

// fsui

struct fs_ui_dialog_settings
{
    ImGuiID id;
    string last_directory;
};

static void free(fs_ui_dialog_settings *settings)
{
    settings->id = 0;
    free(&settings->last_directory);
}

struct fs_ui_ini_settings
{
    ImGuiID id;
    hash_table<ImGuiID, fs_ui_dialog_settings> dialog_settings;
    bool show_hidden;
    bool edit_bar;

    // TODO: pins
};

static fs_ui_ini_settings _ini_settings;

static void init(fs_ui_ini_settings *settings)
{
    fill_memory(settings, 0);
    settings->id = (ImGuiID)-1;
    init_for_n_items(&settings->dialog_settings, 32);
}

static void free(fs_ui_ini_settings *settings)
{
    free<false, true>(&settings->dialog_settings);
}

static void _fs_ui_ClearAllFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler)
{
    // tprint("ClearAll\n");
    free(&_ini_settings);
    init(&_ini_settings);
}

static void *_fs_ui_ReadOpenFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name)
{
    // tprint("ReadOpen %\n", name);
    ImGuiID id = 0;

    if (compare_strings(name, "global") == 0)
        return &_ini_settings;

    if (sscanf(name, "OpenFileDialog,%08x", &id) < 1)
        return nullptr;

    fs_ui_dialog_settings *settings = search_or_insert(&_ini_settings.dialog_settings, &id);

    if (settings == nullptr)
        return nullptr;

    fill_memory(settings, 0);
    settings->id = id;

    return (void*)settings;
}

static void _fs_ui_ReadLineFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* _entry, const char* _line)
{
    // tprint("ReadLine\n");

    ImGuiID *id = (ImGuiID*)_entry;
    const_string line = to_const_string(_line);

    if (*id == (ImGuiID)-1)
    {
        // global settings have id -1
        if      (line == "EditBar=1"_cs)    _ini_settings.edit_bar = true;
        else if (line == "ShowHidden=1"_cs) _ini_settings.show_hidden = true;
    }
    else
    {
        fs_ui_dialog_settings *settings = (fs_ui_dialog_settings*)_entry;

        if (begins_with(line, "LastDirectory="))
        {
            const_string dir = substring(line, 14);

            if (!is_blank(dir))
                settings->last_directory = copy_string(dir);
        }
    }
}

static void _fs_ui_WriteAllFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    // tprint("WriteAll\n");

    buf->appendf("[%s][global]\n", handler->TypeName);

    buf->appendf("EditBar=%s\n",    _ini_settings.edit_bar    ? "1" : "0");
    buf->appendf("ShowHidden=%s\n", _ini_settings.show_hidden ? "1" : "0");
    buf->append("\n");

    for_hash_table(v, &_ini_settings.dialog_settings)
    {
        buf->appendf("[%s][OpenFileDialog,%08x]\n", handler->TypeName, v->id);

        if (!is_blank(v->last_directory))
            buf->appendf("LastDirectory=%s\n", v->last_directory.data);

        buf->append("\n");
    }

    buf->append("\n");
}

void FsUi::Init()
{
    init(&_ini_settings);
    ImGuiSettingsHandler ini_handler{};
    ini_handler.TypeName = "FsUi";
    ini_handler.TypeHash = ImHashStr("FsUi");
    ini_handler.ClearAllFn = _fs_ui_ClearAllFn;
    ini_handler.ReadOpenFn = _fs_ui_ReadOpenFn;
    ini_handler.ReadLineFn = _fs_ui_ReadLineFn;
    ini_handler.WriteAllFn = _fs_ui_WriteAllFn;
    ImGui::AddSettingsHandler(&ini_handler);
}

void FsUi::Exit()
{
    free(&_ini_settings);
}

#define fs_ui_dialog_label_size 32
struct fs_ui_dialog_item
{
    // properties
    fs::path path;
    fs::filesystem_type type;
    fs::filesystem_type symlink_target_type; // only used by symlinks, obviously

    s64  size; // item count for directories, bytes for files / symlinks
    timespan modified;
    timespan created;

    char size_label[fs_ui_dialog_label_size];
    char size_accurate_label[fs_ui_dialog_label_size];
    char modified_label[fs_ui_dialog_label_size];
    char created_label[fs_ui_dialog_label_size];

    // settings / interaction data
    // bool multi_selected; // when multiple items can be selected, this is set per item
};

static void free(fs_ui_dialog_item *item)
{
    fs::free(&item->path);
}

struct fs_ui_dialog
{
    // ImGuiID id;
    // navigation
    fs::path current_dir;
    array<fs::const_fs_string> current_dir_segments;
    bool current_dir_ok;
    string navigation_error_message;
    array<fs::path> back_stack;
    array<fs::path> forward_stack;

    // items, display and sorting
    array<fs_ui_dialog_item> items;
    s64 single_selection_index;
    int  last_sort_criteria;
    bool last_sort_ascending;

    // etc
    fs::path _it_path; // used during iteration, constantly overwritten
    char selection_buffer[255];
};

static void init(fs_ui_dialog *diag)
{
    fill_memory(diag, 0);
    diag->single_selection_index = -1;
    diag->last_sort_ascending = true;

    fs::init(&diag->current_dir);
    init(&diag->navigation_error_message);
    init(&diag->back_stack);
    init(&diag->forward_stack);
    init(&diag->items);

    fs::init(&diag->_it_path);
}

static void free(fs_ui_dialog *diag)
{
    fs::free(&diag->current_dir);
    free(&diag->navigation_error_message);
    free<true>(&diag->back_stack);
    free<true>(&diag->forward_stack);

    free<true>(&diag->items);
    fs::free(&diag->_it_path);
}

// must be same order as the table
enum fs_ui_dialog_sort_criteria
{
    FsUi_Sort_Type,
    FsUi_Sort_Name,
    FsUi_Sort_Size,
    FsUi_Sort_Modified,
    FsUi_Sort_Created,
};

static int _compare_item_type_ascending(const fs_ui_dialog_item *lhs, const fs_ui_dialog_item *rhs)
{
    return compare_ascending((int)lhs->type, (int)rhs->type);
}

static int _compare_item_type_descending(const fs_ui_dialog_item *lhs, const fs_ui_dialog_item *rhs)
{
    return -_compare_item_type_ascending(lhs, rhs);
}

#define _is_directory(X) ((X)->type == fs::filesystem_type::Directory || ((X)->type == fs::filesystem_type::Symlink && (X)->symlink_target_type == fs::filesystem_type::Directory))

#define sort_directories_first(lhs, rhs)\
    if      (_is_directory(lhs) && !_is_directory(rhs)) return -1;\
    else if (_is_directory(rhs) && !_is_directory(lhs)) return 1;

static int _compare_item_name_ascending(const fs_ui_dialog_item *lhs, const fs_ui_dialog_item *rhs)
{
    sort_directories_first(lhs, rhs);
    return lexicoraphical_compare(lhs->path.data, rhs->path.data);
}

static int _compare_item_name_descending(const fs_ui_dialog_item *lhs, const fs_ui_dialog_item *rhs)
{
    sort_directories_first(lhs, rhs);
    return -lexicoraphical_compare(lhs->path.data, rhs->path.data);
}

static int _compare_item_size_ascending(const fs_ui_dialog_item *lhs, const fs_ui_dialog_item *rhs)
{
    sort_directories_first(lhs, rhs);
    return compare_ascending(lhs->size, rhs->size);
}

static int _compare_item_size_descending(const fs_ui_dialog_item *lhs, const fs_ui_dialog_item *rhs)
{
    sort_directories_first(lhs, rhs);
    return -compare_ascending(lhs->size, rhs->size);
}

static int _compare_item_modified_ascending(const fs_ui_dialog_item *lhs, const fs_ui_dialog_item *rhs)
{
    sort_directories_first(lhs, rhs);
    return timespan_compare(&lhs->modified, &rhs->modified);
}

static int _compare_item_modified_descending(const fs_ui_dialog_item *lhs, const fs_ui_dialog_item *rhs)
{
    sort_directories_first(lhs, rhs);
    return -timespan_compare(&lhs->modified, &rhs->modified);
}

static int _compare_item_created_ascending(const fs_ui_dialog_item *lhs, const fs_ui_dialog_item *rhs)
{
    sort_directories_first(lhs, rhs);
    return timespan_compare(&lhs->created, &rhs->created);
}

static int _compare_item_created_descending(const fs_ui_dialog_item *lhs, const fs_ui_dialog_item *rhs)
{
    sort_directories_first(lhs, rhs);
    return -timespan_compare(&lhs->created, &rhs->created);
}

static void _fs_ui_dialog_sort_items(fs_ui_dialog *diag, int criteria, bool ascending = true)
{
    fs_ui_dialog_item *items = diag->items.data;
    s64 count = diag->items.size;

    if (count <= 0)
        return;

    compare_function_p<fs_ui_dialog_item> comp = nullptr;

    switch (criteria)
    {
    case FsUi_Sort_Type:
        if (ascending)  comp = _compare_item_type_ascending;
        else            comp = _compare_item_type_descending;
        break;
    case FsUi_Sort_Name:
        if (ascending)  comp = _compare_item_name_ascending;
        else            comp = _compare_item_name_descending;
        break;
    case FsUi_Sort_Size:
        if (ascending)  comp = _compare_item_size_ascending;
        else            comp = _compare_item_size_descending;
        break;
    case FsUi_Sort_Modified:
        if (ascending)  comp = _compare_item_modified_ascending;
        else            comp = _compare_item_modified_descending;
        break;
    case FsUi_Sort_Created:
        if (ascending)  comp = _compare_item_created_ascending;
        else            comp = _compare_item_created_descending;
        break;
    }

    if (comp != nullptr)
        sort(items, count, comp);

    diag->last_sort_criteria  = criteria;
    diag->last_sort_ascending = ascending;
}

static void _fs_ui_sort_by_imgui_spec(fs_ui_dialog *diag, ImGuiTableSortSpecs *specs)
{
    for (int i = 0; i < specs->SpecsCount; ++i)
    {
        const ImGuiTableColumnSortSpecs *s = specs->Specs + i;

        _fs_ui_dialog_sort_items(diag, s->ColumnIndex, s->SortDirection == ImGuiSortDirection_Ascending);
    }
}

static inline ImVec2 floor(float x, float y)
{
    return ImVec2((int)x, (int)y);
}

static inline ImVec2 floor(ImVec2 v)
{
    return ImVec2((int)v.x, (int)v.y);
}

static void _fs_ui_render_filesystem_type(ImDrawList *lst, fs::filesystem_type type, float size, ImU32 color)
{
    ImVec2 p0 = ImGui::GetCursorScreenPos() + ImVec2(0.5f, 0.5f);
    const float factor = 0.2f;
    float thickness = (int)(size / 20.f);

    if (thickness < 1.f)
        thickness = 1.f;

    switch (type)
    {
    case fs::filesystem_type::Directory:
    {
        p0 += ImVec2((int)(-size * factor * 0.25f), (int)(size * factor * 0.3f));
        // Draw a folder
        const auto tl  = p0 + floor(size * (factor),            size * (factor * 2.f));
        const auto m1  = p0 + floor(size * (factor * 2.f),     size * (factor * 2.f));
        const auto m2  = p0 + floor(size * (factor * 3.f),     size * (factor * 1.0f));
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
        copy_string("?", buf);
        return;
    }

    struct tm t;

    tzset();

    if (localtime_r((time_t*)sp, &t) == nullptr)
    {
        copy_string("?", buf);
        return;
    }

    if (strftime(buf, buf_size, "%F %T", &t) <= 0)
    {
        copy_string("?", buf);
        return;
    }
}

static bool _fs_ui_dialog_load_path(fs_ui_dialog *diag)
{
    fs::path *it = &diag->_it_path;
    fs::set_path(it, &diag->current_dir);

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
        fs::append_path(it, item->path);

        fs_ui_dialog_item *ditem = add_at_end(&diag->items);
        fill_memory(ditem, 0);

        fs::set_path(&ditem->path, item->path);
        ditem->type = item->type;

        // Gather filesystem information
        if (item->type == fs::filesystem_type::Symlink)
            fs::get_filesystem_type(it, &ditem->symlink_target_type);

        // _is_directory also checks for symlinks targeting directories
        if (_is_directory(ditem))
            ditem->size = fs::get_children_count(it);
#if Windows
        // TODO: implement
#else // Linux
        fs::filesystem_info info{};

        if (!fs::get_filesystem_info(it, &info, true, FS_QUERY_DEFAULT_FLAGS))
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
            copy_string("?", ditem->size_label);
        else
        {
            if (_is_directory(ditem))
                format(ditem->size_label, fs_ui_dialog_label_size, "% items", ditem->size);
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
        format(ditem->size_label, fs_ui_dialog_label_size, "%.1f " #Unit, (double)ditem->size / (double)Unit);

                format(ditem->size_accurate_label, fs_ui_dialog_label_size, "% bytes", ditem->size);

                if (false) {}
                _FSUI_Format_Unit(EiB)
                _FSUI_Format_Unit(PiB)
                _FSUI_Format_Unit(TiB)
                _FSUI_Format_Unit(GiB)
                _FSUI_Format_Unit(MiB)
                _FSUI_Format_Unit(KiB)
                else
                    format(ditem->size_label, fs_ui_dialog_label_size, "% bytes", ditem->size);

#undef _FSUI_Format_Unit
            }
        }

        // Modified & Created date
        _format_date(ditem->modified_label, fs_ui_dialog_label_size, &ditem->modified);
        _format_date(ditem->created_label,  fs_ui_dialog_label_size, &ditem->created);
    }

    diag->current_dir_ok = _err.error_code == 0;

    if (_err.error_code != 0)
    {
        set_string(&diag->navigation_error_message, _err.what);
        return false;
    }

    _fs_ui_dialog_sort_items(diag, diag->last_sort_criteria, diag->last_sort_ascending);

    return true;
}

static void _history_push(array<fs::path> *stack, fs::path *path)
{
    if (path == nullptr || path->data == nullptr || is_blank(path->data))
        return;

    if (stack->size > 0 && compare_strings(path->data, (end(stack)-1)->data) == 0)
        // don't add if its already at the end of the stack
        return;

    fs::path *e = add_at_end(stack);
    init(e);
    fs::set_path(e, path);
}

static void _history_pop(array<fs::path> *stack, fs::path *out)
{
    if (stack->size <= 0)
        return;

    fs::path *ret = end(stack) - 1;
    fs::set_path(out, ret);
    fs::free(ret);
    remove_from_end(stack);
}

static void _history_clear(array<fs::path> *stack)
{
    for_array(p, stack)
        fs::free(p);

    clear(stack);
}

namespace FsUi
{
bool OpenFileDialog(const char *label, char *out_filebuf, size_t filebuf_size, const char *filter = nullptr, int flags = 0)
{
    ImGuiContext &g = *GImGui; (void)g;
    ImGuiWindow *window = g.CurrentWindow; (void)window;
    ImGuiStorage *storage = ImGui::GetStateStorage(); (void)storage;
    ImGuiStyle *style = &g.Style;

    bool alt  = ImGui::GetIO().KeyMods & ImGuiModFlags_Alt;
    bool ctrl = ImGui::GetIO().KeyMods & ImGuiModFlags_Ctrl;

    (void)label;
    (void)out_filebuf;
    (void)filebuf_size;
    (void)filter;
    (void)flags;
    // ImGuiSettingsHandler

    const ImGuiID id = window->GetID(label);
    static char input_bar_content[4096] = {0};
    static char quicksearch_content[256] = {0};

    // SETUP
    fs_ui_dialog *diag = (fs_ui_dialog*)storage->GetVoidPtr(id);

    if (diag == nullptr)
    {
        // tprint("Alloc'd\n");
        diag = alloc<fs_ui_dialog>();
        storage->SetVoidPtr(id, (void*)diag);

        init(diag);

        fs_ui_dialog_settings *settings = search(&_ini_settings.dialog_settings, &id);

        if (settings == nullptr)
        {
            settings = add_element_by_key(&_ini_settings.dialog_settings, &id);
            settings->id = id;
            init(&settings->last_directory);
        }

        _history_push(&diag->back_stack, &diag->current_dir);
        _history_clear(&diag->forward_stack);

        if (is_blank(settings->last_directory))
            fs::get_current_path(&diag->current_dir);
        else
            fs::set_path(&diag->current_dir, to_const_string(settings->last_directory));

        _fs_ui_dialog_load_path(diag);
        copy_string(diag->current_dir.data, input_bar_content, 4095);
        quicksearch_content[0] = '\0';
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F5))
    {
        // refresh
        _fs_ui_dialog_load_path(diag);
        copy_string(diag->current_dir.data, input_bar_content, 4095);
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
        _fs_ui_dialog_load_path(diag);
        copy_string(diag->current_dir.data, input_bar_content, 4095);
    }

    ImGui::SameLine();

    // HISTORY
    bool can_back = diag->back_stack.size > 0;
    ImGui::BeginDisabled(!can_back);
    if (ImGui::ArrowButton("##history_back", ImGuiDir_Left) || (can_back && alt && ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)))
    {
        _history_push(&diag->forward_stack, &diag->current_dir);
        _history_pop(&diag->back_stack, &diag->current_dir);

        _fs_ui_dialog_load_path(diag);
        copy_string(diag->current_dir.data, input_bar_content, 4095);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    bool can_forward = diag->forward_stack.size > 0;
    ImGui::BeginDisabled(!can_forward);
    if (ImGui::ArrowButton("##history_forward", ImGuiDir_Right) || (can_forward && alt && ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)))
    {
        _history_push(&diag->back_stack, &diag->current_dir);
        _history_pop(&diag->forward_stack, &diag->current_dir);

        _fs_ui_dialog_load_path(diag);
        copy_string(diag->current_dir.data, input_bar_content, 4095);
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
            _fs_ui_dialog_load_path(diag);
            copy_string(diag->current_dir.data, input_bar_content, 4095);
        }
    }
    ImGui::PopStyleVar();

    // INPUT BAR
    const float total_space = ImGui::GetContentRegionMaxAbs().x;
    const float min_size_left = 200.f;

    if (!_ini_settings.edit_bar)
    {
        // The editing bar
        bool navigate_to_written_dir = false;

        if (ImGui::IsKeyPressed(ImGuiKey_F4))
            ImGui::SetKeyboardFocusHere();

        ImGui::SameLine();

        navigate_to_written_dir = ImGui::InputTextEx("##input_bar",
                                                     nullptr,
                                                     input_bar_content,
                                                     4095,
                                                     ImVec2(Max(10.f, total_space - min_size_left - window->DC.CursorPos.x), 0),
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

            if (!is_blank(input_bar_content))
                fs::set_path(&diag->current_dir, input_bar_content);

            _fs_ui_dialog_load_path(diag);
        }
    }
    else
    {
        // The segment click bar
        ImGui::SameLine();

        if (ImGui::BeginChild("##segment_click_bar", ImVec2(Max(10.f, total_space - min_size_left - window->DC.CursorPos.x), ImGui::GetFrameHeight()), 0, 0))
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

                    _fs_ui_dialog_load_path(diag);
                    copy_string(diag->current_dir.data, input_bar_content, 4095);
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

    bool quicksearch_performed = false;
    s64  quicksearch_result = -1;
    bool quicksearch_submit = false;
    ImGui::SetNextItemWidth(200.f);

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_F))
    {
        quicksearch_content[0] = '\0';
        ImGui::SetKeyboardFocusHere();
    }

    if (ImGui::InputText("quicksearch", quicksearch_content, 255))
    {

        if (quicksearch_content[0] == '\0')
            quicksearch_result = -1;
        else
        {
            quicksearch_performed = true;
            for_array(i, item, &diag->items)
                if (begins_with(item->path.data, quicksearch_content))
                {
                    quicksearch_result = i;
                    break;
                }
        }
    }

    if (quicksearch_content[0] != '\0'
     && ImGui::IsItemFocused()
     && ImGui::IsKeyPressed(ImGuiKey_Enter))
    {
        quicksearch_submit = true;
    }

    if (quicksearch_performed && quicksearch_result == -1)
        diag->single_selection_index = -1;

    // ITEMS
    const float font_size = ImGui::GetFontSize();
    const float bottom_padding = -1 * (ImGui::GetFrameHeight() + style->ItemSpacing.y); // TODO: calculate with style, height of button + separator, etc
    ImU32 font_color = ImGui::ColorConvertFloat4ToU32(style->Colors[ImGuiCol_Text]);

    bool selected = false;

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
                _fs_ui_sort_by_imgui_spec(diag, sort_specs);
                sort_specs->SpecsDirty = false;
            }

            ImDrawList *draw_list = ImGui::GetWindowDrawList();

            for_array(i, item, &diag->items)
            {
                if (!_ini_settings.show_hidden && item->path.data[0] == '.')
                    continue;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                _fs_ui_render_filesystem_type(draw_list, item->type, font_size, font_color);

                ImGui::TableNextColumn();

                if (quicksearch_result == i)
                {
                    diag->single_selection_index = i;
                    copy_string(item->path.data, diag->selection_buffer, 255);
                    ImGui::SetScrollHereY(0.5f);
                }

                bool navigate_into = false;

                // TODO: if (single/multi select ...)
                if (ImGui::Selectable(item->path.data, diag->single_selection_index == i, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
                {
                    diag->single_selection_index = i;
                    copy_string(item->path.data, diag->selection_buffer, 255);

                    navigate_into |= ImGui::IsMouseDoubleClicked(0);
                    navigate_into |= ImGui::IsKeyPressed(ImGuiKey_Enter);
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
            fs_ui_dialog_item *item = diag->items.data + navigate_into_index;

            // Directories shall always be navigated into with double click / enter,
            // never opened.
            if (_is_directory(item))
            {
                diag->selection_buffer[0] = '\0';

                _history_push(&diag->back_stack, &diag->current_dir);
                _history_clear(&diag->forward_stack);
                fs::append_path(&diag->current_dir, item->path);
                _fs_ui_dialog_load_path(diag);
                copy_string(diag->current_dir.data, input_bar_content, 4095);
            }
            else
            {
                // TODO: check filter
                selected = true;
            }
        }
    }

    // TODO: if multiple, display number of items instead
    // TODO: completion / suggestions
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - min_size_left);
    ImGui::InputText("##selection", diag->selection_buffer, 255);
    ImGui::SameLine();

    bool cancelled = ImGui::Button("Cancel");

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        cancelled = true;

    ImGui::SameLine();

    // TODO: if multiple
    // TODO: must exist
    bool has_selection = diag->selection_buffer[0] != '\0';

    ImGui::BeginDisabled(!has_selection);
    selected |= ImGui::Button("Select");
    ImGui::EndDisabled();

    if (selected || cancelled)
    {
        // tprint("Free'd\n");

        if (selected)
        {
            // assemble the full path
            fs::set_path(&diag->_it_path, diag->current_dir);
            fs::append_path(&diag->_it_path, diag->selection_buffer);

            // copy to output
            copy_string(diag->_it_path.data, out_filebuf, filebuf_size);

            // save settings
            fs_ui_dialog_settings *settings = search(&_ini_settings.dialog_settings, &id);

            assert(settings != nullptr);
            set_string(&settings->last_directory, to_const_string(diag->current_dir));
        }

        free(diag);
        dealloc(diag);
        storage->SetVoidPtr(id, nullptr);
    }

    return selected || cancelled;
}
}

bool FsUi::Filepicker(const char *label, char *buf, size_t buf_size, int flags)
{
    ImGuiContext &g = *GImGui; (void)g;
    ImGuiWindow *window = g.CurrentWindow; (void)window;
    ImGuiStyle &st = ImGui::GetStyle();

    ImGui::PushID(label);
    bool text_edited = ImGui::InputTextEx("", NULL, buf, (int)buf_size, ImVec2(0, 0), flags);
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
        if (FsUi::OpenFileDialog(popup_label, buf, buf_size, nullptr, 0))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();

    }

    ImGui::PopID();

    return text_edited;
}
