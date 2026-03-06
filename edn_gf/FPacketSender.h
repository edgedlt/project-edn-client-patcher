#pragma once

// Struct for sending a connect client packet
struct CConnectClient_packet {

	char unknown[4]; //0x00
	wchar_t* userName; //0x04
	int userNameSize; //0x08 - always set to 0x10 in program
	wchar_t* passWord; //0xc 
	int passWordSize; //0x10 - always set to 0x10 in program
	int unknownA; //0x14
	int unknownB; //0x18
	int unknownC; //0x1c
};

// Representation of the packet sender for calling from our patches
struct FPacketSender
{
	// Sends a C_LOG to the server
	typedef void(__thiscall * _SendCLog)(void* pThis, wchar_t *message);
	_SendCLog SendCLog;

	// Sends a C_CONNECT_CLIENT (login) to the server
	typedef void(__thiscall * _SendCConnectClient)(void* pThis, CConnectClient_packet *message);
	_SendCConnectClient SendCConnectClient;

	// Global pointer to the FPacketSender instance in GF.dll.
	// Resolved as moduleBase + offset in LoadPacketSender().
	void* pThis = nullptr;
};

// Global packet sender reference
extern FPacketSender GPacketSender;

// Function to load the packet sender;
void LoadPacketSender(uintptr_t moduleBase);