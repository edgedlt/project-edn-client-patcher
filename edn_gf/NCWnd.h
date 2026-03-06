#pragma once

// Hook for NWindow.dll AddEventInternal
typedef void(__cdecl * _AddEventInternal)(float param_1, int param_2, int param_3, void * param_4, char * param_5, int param_6);

// Functions for hooking and unhooking
void HookAddEventInternal(uintptr_t moduleBase);
void UnHookAddEventInternal();
