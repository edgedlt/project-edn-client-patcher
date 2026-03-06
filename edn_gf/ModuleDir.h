#pragma once

#ifdef _DEBUG

#include <windows.h>

// Resolves the directory containing the DLL that this function lives in.
// Writes the null-terminated path into buffer. Returns false on failure.
bool GetDllDirectory(char* buffer, DWORD bufferCount) noexcept;

#endif // _DEBUG
