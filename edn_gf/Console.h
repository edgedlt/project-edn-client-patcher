#pragma once

class Console
{
public:
	Console();
	~Console() noexcept;
	bool Enable() noexcept;
	void Disable() noexcept;
	void Clear() noexcept;
	void SetCursorPos(COORD dwPos) noexcept;
	void GetConsoleInfo() noexcept;
	void Write(const char * input) noexcept;
	void WritePointer(uintptr_t input) noexcept;
	void WriteF(const char * fmt, ...);
	bool IsEnabled() const noexcept { return enabled; }

private:
	HANDLE hOut;
	COORD dwSize;
	COORD dwCursorPos;
	SMALL_RECT srWindow;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	bool enabled;
};

