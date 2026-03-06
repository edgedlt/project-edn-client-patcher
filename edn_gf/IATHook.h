#pragma once
#include "Console.h"

class IATHook
{
public:
	IATHook(Console * con);
	IATHook(char* sModule, void* pHook, char* sSymbol);
	IATHook(HMODULE hModule, void* pHook, char* sSymbol);
	~IATHook() noexcept;
	void SetConsole(Console * con);
	void Init(char* sModule, void* pHook, char* sSymbol);
	void Init(HMODULE hModule, void* pHook, char* sSymbol);
	void Init(HMODULE hModule, void * pHook, uintptr_t pFunc);
	static void** FindInIAT(const char* sFunction, HMODULE hModule, Console * con);
	bool Hook();
	bool Unload();
	uintptr_t GetOriginalFunction() { return pOldFunc; }

private:
	Console * con = nullptr;
	DWORD dwOldPrt = 0;
	HMODULE hModule = NULL;
	char* sModule = nullptr;
	char* sSymbol = nullptr;
	void* pHook = nullptr;
	uintptr_t pFunctionPtr = 0;
	uintptr_t pOldFunc = 0;
};
