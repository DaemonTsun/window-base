
#pragma once

#include "shl/string.hpp"

namespace FsUi
{
// Filter docs:
// https://learn.microsoft.com/en-us/dotnet/api/microsoft.win32.filedialog.filter?view=windowsdesktop-8.0
#define DefaultDialogFilter "Any files|*.*"
void Init();
void Exit();

bool Filepicker(const char *label, char *buf, size_t buf_size, const char *filter = DefaultDialogFilter,  int flags = 0);
}

