// edn_gf.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "edn_gf.h"
#include "FPacketSender.h"
#include "Interface.h"
#include "NCWnd.h"
#include "UGFBoss.h"

// Function to exit the game
_RequestExit RequestExit;

// Main thread for our patches
DWORD WINAPI EdnGfThread(HMODULE hModule) 
{

	// Activate UI
	Interface ui = Interface();

	// Wait for GF.dll to be loaded
	uintptr_t moduleBase = (uintptr_t)GetModuleHandle(L"GF.dll");
	while (moduleBase == NULL) {
		Sleep(5);
		moduleBase = (uintptr_t)GetModuleHandle(L"GF.dll");
	}

	// Give GF.dll time to finish initialization before we touch its internals
	Sleep(500);

	// Get the packet sender
	LoadPacketSender(moduleBase);

	// Wait for NWindow.dll to be loaded
	uintptr_t nwin = (uintptr_t)GetModuleHandle(L"NWindow.dll");
	while (nwin == NULL) {
		Sleep(5);
		nwin = (uintptr_t)GetModuleHandle(L"NWindow.dll");
	}

	// Hook NWindow.dll External AddEventInternal
	HookAddEventInternal(nwin);

	// cleanup & eject
#ifdef _DEBUG

	// Hook packets
	HookOnPacket(moduleBase);

	// Main loop
	while (ui.bRunning) {
		Sleep(5);
	}

#endif

	ui.~Interface();

	UnHookAddEventInternal();
	
	FreeLibraryAndExitThread(hModule, 0);
	
	return 0;
}
