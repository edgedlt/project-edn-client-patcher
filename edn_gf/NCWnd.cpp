#include "stdafx.h"
#include "NCWnd.h"
#include "Interface.h"
#include "IATHook.h"
#include "FPacketSender.h"

// Original function
_AddEventInternal oAddEventInternal;

// Hook
IATHook * eventHook;

// Module base
uintptr_t nwindowModule;

// Hooked AddEventInternal -- intercepts UI events from NWindow.dll.
void __cdecl hAddEventInternal(float curTime, int eventId, int param_3, void * param_4, char * param_5, int param_6)
{
	// Log if available
	if (g_pFace)
	{
		DLOG_F(1, "Add event internal: [%f] [%d] [%d] [%d] [%d] [%d]\n", curTime, eventId, param_3, param_4, param_5, param_6);
	}

	// Login event
	if (eventId == 0x1773)
	{
		// Find login data
		uintptr_t* dataAddr = (uintptr_t*)(nwindowModule + 0x1fe3b0);

		// Find the user name and pass
		wchar_t* user = *(wchar_t**)(*dataAddr + 0x38);
		DLOG_F(1, "Login request: %s\n", user);

		wchar_t* pass = *(wchar_t**)(*dataAddr + 0x50);

		// Create packet definition
		CConnectClient_packet packet;

		// packet data
		packet.userName = user;
		packet.passWord = pass;

		packet.userNameSize = wcslen(user) * 2;
		packet.passWordSize = wcslen(pass) * 2;

		packet.unknownA = 0;
		packet.unknownB = 0;
		packet.unknownC = 0;

		// Send packet
		GPacketSender.SendCConnectClient(GPacketSender.pThis, &packet);

		return;
	}

	return oAddEventInternal(curTime, eventId, param_3, param_4, param_5, param_6);
}

// Hooks NWindow.dll AddEventInternal to intercept login events.
void HookAddEventInternal(uintptr_t moduleBase)
{
	// Pointer to vtable entry
	uintptr_t pAddEventInternal = moduleBase + 0x136324;

	// Initialize the hook and memory addresses
	if (eventHook) {
		eventHook->Unload();
		delete eventHook;
		eventHook = NULL;
	}
	eventHook = new IATHook(&g_pFace->con);
	eventHook->Init((HMODULE)moduleBase, &hAddEventInternal, pAddEventInternal);

	// Save the original function
	oAddEventInternal = (_AddEventInternal)eventHook->GetOriginalFunction();

	// Save module base
	nwindowModule = moduleBase;

	// Activate the hook
	eventHook->Hook();
}

// Cleanup
void UnHookAddEventInternal()
{
	if (eventHook) {
		eventHook->Unload();
		delete eventHook;
		eventHook = NULL;
	}
}
