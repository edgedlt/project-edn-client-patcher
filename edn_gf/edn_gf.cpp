// edn_gf.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "edn_gf.h"
#include "FPacketSender.h"
#include "Interface.h"
#include "NCWnd.h"
#ifdef _DEBUG
#include "UGFBoss.h"
#endif

// Main thread for our patches
DWORD WINAPI EdnGfThread(HMODULE hModule) 
{

	// Activate UI
	Interface ui = Interface();

	// Wait for GF.dll to be loaded.
	uintptr_t moduleBase = (uintptr_t)GetModuleHandle(L"GF.dll");
	while (moduleBase == NULL) {
		Sleep(5);
		moduleBase = (uintptr_t)GetModuleHandle(L"GF.dll");
	}

	// Give GF.dll time to finish initialization before touching internals.
	Sleep(500);

	// Get the packet sender
	LoadPacketSender(moduleBase);

	// Get module for nwindow
	uintptr_t nwin = (uintptr_t)GetModuleHandle(L"NWindow.dll");

	// Wait until its loaded
	while (nwin == NULL) {
		Sleep(5);
		nwin = (uintptr_t)GetModuleHandle(L"NWindow.dll");
	}

	// Hook NWindow.dll External AddEventInternal
	HookAddEventInternal(nwin);

	// Hook packet logging only in debug builds.
#ifdef _DEBUG
	HookOnPacket(moduleBase);
#endif

	// Hooks stay active for the process lifetime.
	// Block this thread indefinitely; the game process exiting is what
	// cleans everything up. There is no mid-session unload.
	Sleep(INFINITE);

	return 0;
}
