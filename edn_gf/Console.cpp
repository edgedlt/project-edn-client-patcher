#include "stdafx.h"
#include "Interface.h"

#include <vector>


Console::Console()
	: hOut(INVALID_HANDLE_VALUE), enabled(false)
{
}


Console::~Console() noexcept
{
	Disable();
}

bool Console::Enable() noexcept
{
	if (enabled) {
		return true;
	}

	if (!AllocConsole()) {
		MessageBox(NULL, L"Failed to allocate a console...", L"Error!", MB_OK);
		return false;
	}

	hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	enabled = (hOut != INVALID_HANDLE_VALUE && hOut != NULL);
	return enabled;
}

void Console::Disable() noexcept
{
	if (!enabled) {
		return;
	}

	FreeConsole();
	hOut = INVALID_HANDLE_VALUE;
	enabled = false;
}

void Console::Clear() noexcept
{
	if (!enabled) {
		return;
	}

	DWORD dwW = 0;
	FillConsoleOutputCharacterA(hOut, ' ', dwSize.X * dwSize.Y, COORD{ 0 }, &dwW);
}

void Console::SetCursorPos(COORD dwPos) noexcept
{
	if (!enabled) {
		return;
	}

	SetConsoleCursorPosition(hOut, dwPos);
	GetConsoleInfo();
}

void Console::GetConsoleInfo() noexcept
{
	if (!enabled) {
		return;
	}

	GetConsoleScreenBufferInfo(hOut, &csbi);
	dwSize = csbi.dwSize;
	dwCursorPos = csbi.dwCursorPosition;
	srWindow = csbi.srWindow;
}

void Console::Write(const char * input) noexcept
{
	if (!enabled) {
		return;
	}

	WriteConsoleA(hOut, input, strlen(input), nullptr, NULL);
	//GetConsoleInfo();
}

void Console::WritePointer(uintptr_t input) noexcept
{
	static const size_t size = 20;
	char buffer[size]{ 0 };
	sprintf_s(buffer, size, "%p", reinterpret_cast<void*>(input));
	Write(buffer);
}

void Console::WriteF(const char * fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	va_list measureArgs;
	va_copy(measureArgs, args);
	int length = _vscprintf(fmt, measureArgs);
	va_end(measureArgs);

	if (length < 0)
	{
		va_end(args);
		return;
	}

	const size_t bufferSize = static_cast<size_t>(length) + 1u;
	std::vector<char> buffer(bufferSize);
	vsprintf_s(buffer.data(), bufferSize, fmt, args);
	va_end(args);
	Write(buffer.data());
}
