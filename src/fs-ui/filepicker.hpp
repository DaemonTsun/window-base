
#pragma once

namespace FsUi
{
void Init();
void Exit();

bool Filepicker(const char *label, char *buf, size_t buf_size, int flags = 0);
}

