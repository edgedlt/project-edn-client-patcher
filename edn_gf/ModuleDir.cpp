#include "stdafx.h"

#ifdef _DEBUG

#include "ModuleDir.h"

#include <cstring>

bool GetDllDirectory(char* buffer, DWORD bufferCount) noexcept
{
	HMODULE module = NULL;
	// Use a static variable's address to locate this DLL module.
	// Avoids overload-resolution issues with function pointers.
	static const char anchor = '\0';
	if (!GetModuleHandleExA(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		&anchor,
		&module))
	{
		return false;
	}

	DWORD length = GetModuleFileNameA(module, buffer, bufferCount);
	if (length == 0 || length >= bufferCount) {
		return false;
	}

	char* slash = strrchr(buffer, '\\');
	if (slash == NULL) {
		slash = strrchr(buffer, '/');
	}
	if (slash == NULL) {
		return false;
	}

	*slash = '\0';
	return true;
}

#endif // _DEBUG
