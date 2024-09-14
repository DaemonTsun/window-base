
#pragma once

// naming convention same as ImGui
enum ui_FilepickerFlags_
{
    ui_FilepickerFlags_NoDirectories      = 1 << 0, // must not be used together with the next option
    ui_FilepickerFlags_NoFiles            = 1 << 1, // use if only directories should be selectable, also implies selection must exist
    ui_FilepickerFlags_SelectionMustExist = 1 << 2  // may only select existing files / directories
};

typedef int ui_FilepickerFlags;

namespace ui
{
// Filter docs:
// https://learn.microsoft.com/en-us/dotnet/api/microsoft.win32.filedialog.filter?view=windowsdesktop-8.0
// 
// Example:
// "Any files|*.*|Office files|*.doc;*.docx;*.xlsx|Specific file|EBOOT.BIN"
#define ui_DefaultDialogFilter "Any files|*.*"

void filepicker_init();
void filepicker_exit();

// names for controls are capitalized, same as ImGui
bool Filepicker(const char *label, char *buf, size_t buf_size, const char *filter = ui_DefaultDialogFilter, ui_FilepickerFlags flags = 0);

bool FileDialog(const char *label, char *out_filebuf, size_t filebuf_size, const char *filter = ui_DefaultDialogFilter, ui_FilepickerFlags flags = 0);
}

