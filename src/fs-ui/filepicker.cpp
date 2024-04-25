
#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui_internal.h"

#include "shl/print.hpp"
#include "shl/hash_table.hpp"
#include "shl/memory.hpp"
#include "shl/string.hpp"
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

struct fs_ui_dialog_item
{
    fs::path path;
    fs::filesystem_type type;
    // fs::filesystem_info info;
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
};

static void init(fs_ui_dialog *diag)
{
    fs::init(&diag->current_dir);
    init(&diag->items);
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

    // TODO: reuse item memory
    free<true>(&diag->items);
    init(&diag->items);

    if (err) err->error_code = 0;

    for_path(item, &diag->current_dir, fs::iterate_option::QueryType, err)
    {
        fs_ui_dialog_item *ditem = add_at_end(&diag->items);
        fill_memory(ditem, 0);

        fs::set_path(&ditem->path, item->path);
        ditem->type = item->type;
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
        tprint("Alloc'd\n");
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

    ImGui::InputText("##input_bar", input_bar_content, 4095);
    
    ImGui::Separator();

    for_array(item, &diag->items)
    {
        ImGui::Text("%s", item->path.data);
    }

    ImGui::Separator();

    bool selected = ImGui::Button("Select");

    ImGui::SameLine();

    bool cancelled = ImGui::Button("Cancel");

    if (selected || cancelled)
    {
        tprint("Free'd\n");

        if (selected)
        {
            fs_ui_dialog_settings *settings = search(&_ini_dialog_settings, &id);

            assert(settings != nullptr);
            set_string(&settings->last_directory, to_const_string(diag->current_dir));
        }

        free(diag);
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

    if (picker_clicked)
        ImGui::OpenPopup("Select File...");

    if (ImGui::BeginPopupModal("Select File..."))
    {
        if (FsUi::OpenFileDialog("Select File...", buf, buf_size, nullptr, 0))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();

    }

    ImGui::PopID();

    return text_edited;
}
