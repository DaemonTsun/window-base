
#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui_internal.h"

#include "shl/print.hpp"
#include "shl/hash_table.hpp"
#include "shl/memory.hpp"
#include "shl/string.hpp"
#include "shl/time.hpp"
#include "fs/path.hpp"
#include "fs-ui/filepicker.hpp"

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

static hash_table<ImGuiID, fs_ui_dialog_settings> _ini_dialog_settings;

static void _fs_ui_ClearAllFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler)
{
    // tprint("ClearAll\n");
    free<false, true>(&_ini_dialog_settings);
    init(&_ini_dialog_settings);
}

static void *_fs_ui_ReadOpenFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name)
{
    // tprint("ReadOpen %\n", name);
    ImGuiID id = 0;

    if (sscanf(name, "OpenFileDialog,%08x", &id) < 1)
        return nullptr;

    fs_ui_dialog_settings *settings = search_or_insert(&_ini_dialog_settings, &id);

    if (settings == nullptr)
        return nullptr;

    fill_memory(settings, 0);
    settings->id = id;

    return (void*)settings;
}

static void _fs_ui_ReadLineFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* _entry, const char* _line)
{
    // tprint("ReadLine\n");
    fs_ui_dialog_settings *settings = (fs_ui_dialog_settings*)_entry;
    
    const_string line = to_const_string(_line);

    if (begins_with(line, "LastDirectory="))
    {
        const_string dir = substring(line, 14);

        if (!is_blank(dir))
            settings->last_directory = copy_string(dir);
    }
}

/*
static void _fs_ui_ApplyAllFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler)
{
    tprint("ApplyAll\n");
}
*/

static void _fs_ui_WriteAllFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    // tprint("WriteAll\n");

    for_hash_table(v, &_ini_dialog_settings)
    {
        buf->appendf("[%s][OpenFileDialog,%08x]\n", handler->TypeName, v->id);

        if (!is_blank(v->last_directory))
            buf->appendf("LastDirectory=%s\n", v->last_directory.data);
    }

    buf->append("\n");
}

void FsUi::Init()
{
    init_for_n_items(&_ini_dialog_settings, 64);
    ImGuiSettingsHandler ini_handler{};
    ini_handler.TypeName = "FsUi";
    ini_handler.TypeHash = ImHashStr("FsUi");
    ini_handler.ClearAllFn = _fs_ui_ClearAllFn;
    ini_handler.ReadOpenFn = _fs_ui_ReadOpenFn;
    ini_handler.ReadLineFn = _fs_ui_ReadLineFn;
    // ini_handler.ApplyAllFn = _fs_ui_ApplyAllFn;
    ini_handler.WriteAllFn = _fs_ui_WriteAllFn;
    ImGui::AddSettingsHandler(&ini_handler);
}

void FsUi::Exit()
{
    free<false, true>(&_ini_dialog_settings);
}

#define fs_ui_dialog_label_size 32
struct fs_ui_dialog_item
{
    fs::path path;
    fs::filesystem_type type;

    s64  size; // item count for directories, bytes for files / symlinks
    timespan modified;
    timespan created;

    char size_label[fs_ui_dialog_label_size];
    char size_accurate_label[fs_ui_dialog_label_size];
    char modified_label[fs_ui_dialog_label_size];
    char created_label[fs_ui_dialog_label_size];
};

static void free(fs_ui_dialog_item *item)
{
    fs::free(&item->path);
}

struct fs_ui_dialog
{
    // ImGuiID id;
    fs::path current_dir;

    array<fs_ui_dialog_item> items;
    int  last_sort_criteria;
    bool last_sort_ascending;

    fs::path _it_path; // used during iteration, constantly overwritten
};

static void init(fs_ui_dialog *diag)
{
    fs::init(&diag->current_dir);
    init(&diag->items);

    fs::init(&diag->_it_path);
}

static void free(fs_ui_dialog *diag)
{
    fs::free(&diag->current_dir);
    free<true>(&diag->items);
}

enum fs_ui_dialog_sort_criteria
{
    FsUi_Sort_Type,
    FsUi_Sort_Name,
    FsUi_Sort_Size,
    FsUi_Sort_Modified,
    FsUi_Sort_Created,
};

[[maybe_unused]]
static void _fs_ui_dialog_sort_items(fs_ui_dialog *diag, int criteria, bool ascending = true)
{
    // TODO: implement

    diag->last_sort_criteria  = criteria;
    diag->last_sort_ascending = ascending;
}

static bool _fs_ui_dialog_load_path(fs_ui_dialog *diag, fs::path *path = nullptr, error *err = nullptr)
{
    if (path != nullptr)
        fs::weakly_canonical_path(path, &diag->current_dir);

    fs::path *it = &diag->_it_path;
    fs::set_path(it, &diag->current_dir);

    // TODO: reuse item memory
    free<true>(&diag->items);
    init(&diag->items);

    if (err) err->error_code = 0;

    const s64 base_size = it->size;
    for_path(item, &diag->current_dir, fs::iterate_option::QueryType, err)
    {
        it->size = base_size;
        it->data[base_size] = '\0';
        fs::append_path(it, item->path);

        fs_ui_dialog_item *ditem = add_at_end(&diag->items);
        fill_memory(ditem, 0);

        fs::set_path(&ditem->path, item->path);
        ditem->type = item->type;

        // Gather filesystem information

        if (item->type == fs::filesystem_type::Directory)
            ditem->size = fs::get_children_count(it);
#if Windows
        // TODO: implement
#else // Linux
        fs::filesystem_info info{};

        if (!fs::get_filesystem_info(it, &info, true, FS_QUERY_DEFAULT_FLAGS, err))
        {
            free(ditem);
            remove_from_end(&diag->items);
            continue;
        }

        if (item->type != fs::filesystem_type::Directory)
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
            if (item->type == fs::filesystem_type::Directory)
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
        // TODO: implement
    }

    if (err && err->error_code != 0)
        return false;

    _fs_ui_dialog_sort_items(diag, diag->last_sort_criteria, diag->last_sort_ascending);

    return true;
}

namespace FsUi
{
bool OpenFileDialog(const char *label, char *out_filebuf, size_t filebuf_size, const char *filter = nullptr, int flags = 0)
{
    ImGuiContext &g = *GImGui; (void)g;
    ImGuiWindow *window = g.CurrentWindow; (void)window;
    ImGuiStorage *storage = ImGui::GetStateStorage(); (void)storage;

    (void)label;
    (void)out_filebuf;
    (void)filebuf_size;
    (void)filter;
    (void)flags;
    // ImGuiSettingsHandler

    const ImGuiID id = window->GetID(label);
    static char input_bar_content[4096] = {0};

    fs_ui_dialog *diag = (fs_ui_dialog*)storage->GetVoidPtr(id);

    if (diag == nullptr)
    {
        // tprint("Alloc'd\n");
        diag = alloc<fs_ui_dialog>();
        storage->SetVoidPtr(id, (void*)diag);

        init(diag);

        fs_ui_dialog_settings *settings = search(&_ini_dialog_settings, &id);

        if (settings == nullptr)
        {
            settings = add_element_by_key(&_ini_dialog_settings, &id);
            settings->id = id;
            init(&settings->last_directory);
        }

        if (is_blank(settings->last_directory))
            fs::get_current_path(&diag->current_dir);
        else
            fs::set_path(&diag->current_dir, to_const_string(settings->last_directory));

        copy_string(diag->current_dir.data, input_bar_content, 4095);

        _fs_ui_dialog_load_path(diag);
    }

    ImGui::PushItemWidth(-1);
    ImGui::InputText("##input_bar", input_bar_content, 4095);
    
    const int table_flags = ImGuiTableFlags_ScrollY
                          | ImGuiTableFlags_ScrollX
                          | ImGuiTableFlags_NoBordersInBody
                          | ImGuiTableFlags_BordersOuterV
                          | ImGuiTableFlags_BordersOuterH
                          | ImGuiTableFlags_RowBg
                          | ImGuiTableFlags_Resizable;
    const float bottom_padding = -100; // TODO: calculate with style, height of button + separator, etc
    if (ImGui::BeginTable("fs_dialog_content_table", 5, table_flags, ImVec2(-0, bottom_padding)))
    {
        // Display headers so we can inspect their interaction with borders
        // (Headers are not the main purpose of this section of the demo, so we are not elaborating on them now. See other sections for details)
        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Size");
        ImGui::TableSetupColumn("Modified");
        ImGui::TableSetupColumn("Created");
        ImGui::TableHeadersRow();

        for_array(item, &diag->items)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            switch (item->type)
            {
                case fs::filesystem_type::Directory: ImGui::Text("D"); break;
                case fs::filesystem_type::File:      ImGui::Text("F"); break;
                case fs::filesystem_type::Symlink:   ImGui::Text("L"); break;
                default:                             ImGui::Text("?"); break;
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", item->path.data);

            // Size
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", item->size_label);
            if (item->size_accurate_label[0] && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", item->size_accurate_label);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", 0);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%d", 0);
        }

        ImGui::EndTable();
    }

    bool selected = ImGui::Button("Select");

    ImGui::SameLine();

    bool cancelled = ImGui::Button("Cancel");

    if (selected || cancelled)
    {
        // tprint("Free'd\n");

        if (selected)
        {
            fs_ui_dialog_settings *settings = search(&_ini_dialog_settings, &id);

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

bool FsUi::Filepicker(const char *label, char *buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* callback_user_data)
{
    ImGuiContext &g = *GImGui; (void)g;
    ImGuiWindow *window = g.CurrentWindow; (void)window;
    ImGuiStyle &st = ImGui::GetStyle();

    ImGui::PushID(label);
    bool text_edited = ImGui::InputTextEx("", NULL, buf, (int)buf_size, ImVec2(0, 0), flags, callback, callback_user_data);
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
