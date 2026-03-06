#include "stdafx.h"

#ifdef _DEBUG

#include "UGFBoss.h"
#include "Interface.h"
#include "IATHook.h"
#include "ModuleDir.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

// Original function
_OnPacket oOnPacket;

// Callback
_OnPacketCallback OnPacketCallback;

// Hook
IATHook* packetHook;

namespace
{
	struct PacketVector3
	{
		float x;
		float y;
		float z;
	};

	struct ProjectileLaunchState
	{
		int serverTimeMs;
		int attackerID;
		int armIndex;
		PacketVector3 origin;
	};

	map<int, PacketVector3> g_lastUnitPositions;
	map<int, ProjectileLaunchState> g_projectileLaunches;

	enum PacketCategoryMask
	{
		PacketCategoryAuth = 1u << 0,
		PacketCategoryLobby = 1u << 1,
		PacketCategoryRoom = 1u << 2,
		PacketCategoryBattle = 1u << 3,
		PacketCategoryMovement = 1u << 4,
		PacketCategoryCombat = 1u << 5,
		PacketCategorySkill = 1u << 6,
		PacketCategoryNpc = 1u << 7,
		PacketCategoryShop = 1u << 8,
		PacketCategoryFactory = 1u << 9,
		PacketCategoryChat = 1u << 10,
		PacketCategorySystem = 1u << 11,
		PacketCategoryInventory = 1u << 12
	};

	struct PacketMeta
	{
		BYTE id;
		const char* name;
		unsigned int categories;
		int fixedBytes; // -1 = unknown/variable length
	};

	struct PacketLogConfig
	{
		bool enabled;
		bool decodeKnown;
		bool logHex;
		bool logUnknown;
		bool allPacketsProfile;
		unsigned int categoryMask;
		string profile;
		set<BYTE> include;
		set<BYTE> exclude;
		string sourcePath;
	};

	static PacketLogConfig g_packetLogConfig = {
		true,   // enabled
		true,   // decodeKnown
		true,   // logHex
		false,  // logUnknown
		false,  // allPacketsProfile
		PacketCategoryBattle | PacketCategoryMovement | PacketCategoryCombat | PacketCategorySkill | PacketCategoryNpc | PacketCategorySystem,
		"combat",
		set<BYTE>(),
		set<BYTE>(),
		"<defaults>"
	};

	static const PacketMeta kPacketMeta[] = {
		{0x00, "S_HANDSHAKE_0", PacketCategoryAuth | PacketCategorySystem, -1},
		{0x01, "S_HANDSHAKE_1", PacketCategoryAuth | PacketCategorySystem, -1},
		{0x02, "S_CONNECTED", PacketCategoryAuth | PacketCategorySystem, -1},
		{0x03, "S_SERVER_MESSAGE", PacketCategoryAuth | PacketCategorySystem, -1},
		{0x04, "S_SERVERTIME", PacketCategorySystem, -1},
		{0x09, "S_TEXT_MESSAGE", PacketCategorySystem, -1},
		{0x0A, "S_USER_ENTER", PacketCategoryRoom | PacketCategoryBattle, -1},
		{0x0B, "S_USER_LEAVE", PacketCategoryRoom | PacketCategoryBattle, -1},
		{0x0C, "S_VALID_NAME", PacketCategoryFactory | PacketCategorySystem, -1},
		{0x1A, "S_PILOT_LEVEL_UP", PacketCategorySystem, -1},
		{0x1B, "S_PILOT_WEAPON_LEVEL_UP", PacketCategorySystem, -1},
		{0x1D, "S_INV_INVENTORY", PacketCategoryInventory, -1},
		{0x1E, "S_INV_DEFAULT", PacketCategoryInventory, -1},
		{0x1F, "S_INV_CODE_LIST", PacketCategoryInventory, -1},
		{0x20, "S_INV_PARTS_LIST", PacketCategoryInventory, -1},
		{0x21, "S_INV_UNIT_LIST", PacketCategoryInventory, -1},
		{0x22, "S_INV_UNIT_INFO", PacketCategoryInventory, -1},
		{0x23, "S_INV_PALETTE_LIST", PacketCategoryInventory, -1},
		{0x24, "S_INV_OP_ITEM_LIST", PacketCategoryInventory, -1},
		{0x25, "S_INV_SPECIAL_ITEM_LIST", PacketCategoryInventory, -1},
		{0x26, "S_INV_END", PacketCategoryInventory | PacketCategoryLobby | PacketCategoryRoom, -1},
		{0x2F, "S_LOBBY_GAME_SEARCHED", PacketCategoryLobby, -1},
		{0x30, "S_LOBBY_SEARCH_CANCELED", PacketCategoryLobby, -1},
		{0x31, "S_LOBBY_USER_FOUND", PacketCategoryLobby, -1},
		{0x32, "S_LOBBY_GAME_CREATED", PacketCategoryLobby, -1},
		{0x33, "S_LOBBY_GAME_ENTERED", PacketCategoryLobby, -1},
		{0x35, "S_ROOM_USER_INFO", PacketCategoryRoom, -1},
		{0x3D, "S_ROOM_GAME_START", PacketCategoryRoom, -1},
		{0x3E, "S_ROOM_UNIT_INFO", PacketCategoryRoom, -1},
		{0x3F, "S_ROOM_CREATE_CONFIG", PacketCategoryRoom, -1},
		{0x43, "S_ROOM_CHATTING", PacketCategoryRoom | PacketCategoryChat, -1},
		{0x48, "S_USER_READY", PacketCategoryRoom, -1},
		{0x49, "S_GAME_USER_INFO", PacketCategoryBattle, -1},
		{0x4A, "S_GAME_CODE_LIST", PacketCategoryBattle | PacketCategorySkill, -1},
		{0x4B, "S_PALETTE_LIST", PacketCategoryBattle | PacketCategorySkill, -1},
		{0x4C, "S_GAME_UNIT_INFO", PacketCategoryBattle, -1},
		{0x4D, "S_GAME_SPAWN_UNIT", PacketCategoryBattle, -1},
		{0x4E, "S_GAME_STARTED", PacketCategoryBattle, -1},
		{0x50, "S_GAME_END", PacketCategoryBattle, -1},
		{0x54, "S_GAME_BASE_INFO", PacketCategoryBattle, -1},
		{0x55, "S_GAME_BASE_SELECTED", PacketCategoryBattle, -1},
		{0x57, "S_GAME_REGAIN_RESULT", PacketCategoryBattle, -1},
		{0x58, "S_QUIT_BATTLE", PacketCategoryBattle, -1},
		{0x59, "S_GAME_REGAIN_INFO", PacketCategoryBattle, 24},
		{0x5A, "S_GAME_REGAIN_UNIT_REPAIRED", PacketCategoryBattle, 4},
		{0x5D, "S_BASE_TRYROB", PacketCategoryBattle, -1},
		{0x5E, "S_BASE_ROBRESULT", PacketCategoryBattle, -1},
		{0x5F, "S_HP_CHARGED_UNIT", PacketCategoryBattle, -1},
		{0x60, "S_HP_CHARGER_STATE", PacketCategoryBattle, -1},
		{0x61, "S_BASE_STATE_UPDATE", PacketCategoryBattle, -1},
		{0x62, "S_UNIT_DESTROYED", PacketCategoryBattle | PacketCategoryCombat, 20},
		{0x63, "S_UNIT_MOVE", PacketCategoryBattle | PacketCategoryMovement, 39},
		{0x64, "S_STOP_UNIT", PacketCategoryBattle | PacketCategoryMovement, 42},
		{0x65, "S_AIM_UNIT", PacketCategoryBattle | PacketCategoryMovement | PacketCategoryCombat, 36},
		{0x66, "S_UNAIM_UNIT", PacketCategoryBattle | PacketCategoryMovement | PacketCategoryCombat, 16},
		{0x67, "S_ATTACK_RANGE", PacketCategoryBattle | PacketCategoryCombat, 52},
		{0x68, "S_ATTACK_BLADE", PacketCategoryBattle | PacketCategoryCombat, -1},
		{0x69, "S_ATTACK_GEOOBJECT", PacketCategoryBattle | PacketCategoryCombat, -1},
		{0x6A, "S_GEOOBJECTS_HP", PacketCategoryBattle, -1},
		{0x6B, "S_ATTACK_SHIELD", PacketCategoryBattle | PacketCategoryCombat, 52},
		{0x6D, "S_ATTACK_BLADE_ALT", PacketCategoryBattle | PacketCategoryCombat, -1},
		{0x6E, "S_ATTACK_RANGE_ALT", PacketCategoryBattle | PacketCategoryCombat, -1},
		{0x6F, "S_ATTACK_IFO", PacketCategoryBattle | PacketCategoryCombat, 44},
		{0x70, "S_GAME_CODE_PALETTE", PacketCategoryBattle | PacketCategorySkill, -1},
		{0x71, "S_STOP_ATTACK", PacketCategoryBattle | PacketCategoryCombat, 8},
		{0x72, "S_ATTACK_IFO_RESULT", PacketCategoryBattle | PacketCategoryCombat, -1},
		{0x73, "S_UNIT_DESTROYED_NOTIFY", PacketCategoryBattle, -1},
		{0x74, "S_MOVE_SYNC", PacketCategoryBattle | PacketCategoryMovement, 20},
		{0x75, "S_NPC_UNIT_INFO", PacketCategoryBattle | PacketCategoryNpc, -1},
		{0x76, "S_NPC_WEAPON_INFO", PacketCategoryBattle | PacketCategoryNpc, -1},
		{0x77, "S_NPC_UP_DOWN", PacketCategoryBattle | PacketCategoryNpc, -1},
		{0x78, "S_NPC_MOVE_SMOOTH", PacketCategoryBattle | PacketCategoryMovement | PacketCategoryNpc, 51},
		{0x79, "S_NPC_MOVE", PacketCategoryBattle | PacketCategoryMovement | PacketCategoryNpc, -1},
		{0x7A, "S_STATUS_CHANGED", PacketCategoryBattle | PacketCategoryCombat | PacketCategorySystem, 16},
		{0x7B, "S_NPC_KILL_DISSOLVE", PacketCategoryBattle | PacketCategoryNpc, 4},
		{0x7C, "S_GEOOBJECTS_UPDATE", PacketCategoryBattle, -1},
		{0x7D, "S_MODE_SNIPER_RESULT", PacketCategoryBattle | PacketCategoryCombat, -1},
		{0x7E, "S_SCORE_UPDATE", PacketCategoryBattle, -1},
		{0x7F, "S_DEFENSIVE_BATTLE_SCORE", PacketCategoryBattle | PacketCategoryNpc, -1},
		{0x80, "S_GAME_HEAL", PacketCategoryBattle | PacketCategoryCombat | PacketCategorySkill, 8},
		{0x83, "S_OVERHEAT_STATUS", PacketCategoryBattle | PacketCategoryCombat, 14},
		{0x84, "S_WEAPONSET_CHANGED", PacketCategoryBattle | PacketCategoryCombat, 18},
		{0x86, "S_CANT_USE_SKILL", PacketCategoryBattle | PacketCategorySkill, 8},
		{0x87, "S_ON_SKILL", PacketCategoryBattle | PacketCategorySkill | PacketCategoryCombat, -1},
		{0x88, "S_CAST_SKILL", PacketCategoryBattle | PacketCategorySkill, 12},
		{0x89, "S_CODE_EFFECT", PacketCategoryBattle | PacketCategorySkill, 16},
		{0x8A, "S_CODE_REMOVE", PacketCategoryBattle | PacketCategorySkill, 8},
		{0x8B, "S_CODE_ACTIVATION", PacketCategoryBattle | PacketCategorySkill, 8},
		{0x8D, "S_SHOP_BUY_RESULT", PacketCategoryShop, -1},
		{0x8E, "S_BUY_LIST_RESULT", PacketCategoryShop, -1},
		{0x8F, "S_COUNT_ITEM_USED", PacketCategoryBattle | PacketCategorySkill, 16},
		{0x90, "S_GOODS_DATA_BEGIN", PacketCategoryShop, -1},
		{0x91, "S_GOODS_DATA", PacketCategoryShop, -1},
		{0x92, "S_GOODS_DATA_END", PacketCategoryShop, -1},
		{0x97, "S_BRIDGE_AVATAR_INFO", PacketCategoryLobby, -1},
		{0x9F, "S_BRIDGE_OVERALL_INFO", PacketCategoryLobby, -1},
		{0xA0, "S_SHOP_NCCOIN_UPDATE", PacketCategoryShop, -1},
		{0xA1, "S_ADM_CHANGE_EXP", PacketCategorySystem, -1},
		{0xB1, "S_CLAN_MARK_INFO", PacketCategoryLobby, -1},
		{0xB2, "S_CLAN_MARK_DATA", PacketCategoryLobby, -1},
		{0xB5, "S_PING_LIST", PacketCategoryRoom | PacketCategoryBattle, -1},
		{0xBE, "S_CLAN_OTHERTEAM_VIEW_ACK", PacketCategoryRoom, -1},
		{0xBF, "S_ROOM_CREATE_ACK", PacketCategoryRoom, -1},
		{0xC0, "S_CLAN_INVITE_INFO_FWD_REQ", PacketCategoryRoom | PacketCategoryChat, -1},
		{0xC1, "S_CHAT_MSG", PacketCategoryRoom | PacketCategoryChat, -1},
		{0xC2, "S_CLAN_INVITE_FWD_REQ", PacketCategoryRoom | PacketCategoryChat, -1},
		{0xC3, "S_CLAN_INVITE_FWD_ACK", PacketCategoryRoom | PacketCategoryChat, -1},
		{0xC4, "S_CLAN_INVITE_FWD_CONFIRM", PacketCategoryRoom | PacketCategoryChat, -1},
		{0xC5, "S_CLAN_INVITE_FWD_CANCEL", PacketCategoryRoom | PacketCategoryChat, -1},
		{0xC6, "S_CLAN_INVITE_FWD_INFO", PacketCategoryRoom | PacketCategoryChat, -1},
		{0xC8, "S_ROOM_LEAVE", PacketCategoryRoom, -1},
		{0xC9, "S_ROOM_RESERVED_EVENT", PacketCategoryRoom, -1},
		{0xCB, "S_NPC_HP_UPDATE", PacketCategoryBattle | PacketCategoryNpc, 8},
		{0xE6, "S_SELECT_OPERATOR_INFO", PacketCategoryInventory | PacketCategoryLobby, -1},
		{0xED, "S_ATTACK_SKILL_RESULT", PacketCategoryBattle | PacketCategoryCombat | PacketCategorySkill, -1},
		{0xEE, "S_ATTACK_BLADE_LEGACY", PacketCategoryBattle | PacketCategoryCombat, -1},
		{0xF8, "S_PC_HP_UPDATE", PacketCategoryBattle | PacketCategoryCombat, 8}
	};

	string TrimCopy(const string& value)
	{
		const string::size_type begin = value.find_first_not_of(" \t\r\n");
		if (begin == string::npos) {
			return string();
		}
		const string::size_type end = value.find_last_not_of(" \t\r\n");
		return value.substr(begin, end - begin + 1);
	}

	string LowerCopy(string value)
	{
		transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
			return static_cast<char>(tolower(c));
		});
		return value;
	}

	vector<string> SplitTokens(const string& value)
	{
		vector<string> out;
		string current;
		for (string::size_type i = 0; i < value.size(); ++i) {
			const char ch = value[i];
			if (ch == ',' || ch == ';' || ch == ' ' || ch == '\t') {
				const string token = TrimCopy(current);
				if (!token.empty()) {
					out.push_back(token);
				}
				current.clear();
				continue;
			}
			current.push_back(ch);
		}
		const string token = TrimCopy(current);
		if (!token.empty()) {
			out.push_back(token);
		}
		return out;
	}

	string JoinTokensCSV(const vector<string>& tokens)
	{
		string joined;
		for (size_t i = 0; i < tokens.size(); ++i) {
			if (i != 0) {
				joined += ",";
			}
			joined += tokens[i];
		}
		return joined;
	}

	bool ParseBoolValue(const string& value, bool defaultValue)
	{
		const string normalized = LowerCopy(TrimCopy(value));
		if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") {
			return true;
		}
		if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") {
			return false;
		}
		return defaultValue;
	}

	bool ParsePacketIDToken(const string& token, BYTE* outId)
	{
		if (outId == NULL) {
			return false;
		}

		const string trimmed = TrimCopy(token);
		if (trimmed.empty()) {
			return false;
		}

		const string normalized = LowerCopy(trimmed);
		for (size_t i = 0; i < (sizeof(kPacketMeta) / sizeof(kPacketMeta[0])); ++i) {
			if (LowerCopy(kPacketMeta[i].name) == normalized) {
				*outId = kPacketMeta[i].id;
				return true;
			}
		}

		char* end = NULL;
		const unsigned long parsed = strtoul(trimmed.c_str(), &end, 0);
		if (end == trimmed.c_str() || *end != '\0' || parsed > 0xFFul) {
			return false;
		}

		*outId = static_cast<BYTE>(parsed);
		return true;
	}

	bool TryProfileMaskFromName(const string& profileName, unsigned int* mask, bool* allPackets)
	{
		if (mask == NULL) {
			return false;
		}
		*mask = 0;
		if (allPackets != NULL) {
			*allPackets = false;
		}

		const string normalized = LowerCopy(TrimCopy(profileName));
		if (normalized.empty()) {
			return false;
		}
		if (normalized == "all") {
			if (allPackets != NULL) {
				*allPackets = true;
			}
			*mask = 0xFFFFFFFFu;
			return true;
		}
		if (normalized == "lobby") {
			*mask = PacketCategoryAuth | PacketCategoryLobby | PacketCategoryRoom | PacketCategoryInventory |
				PacketCategoryFactory | PacketCategoryShop | PacketCategoryChat | PacketCategorySystem;
			return true;
		}
		if (normalized == "matchflow") {
			*mask = PacketCategoryBattle | PacketCategoryNpc | PacketCategorySystem;
			return true;
		}
		if (normalized == "movement") {
			*mask = PacketCategoryMovement | PacketCategoryNpc;
			return true;
		}
		if (normalized == "gunplay") {
			*mask = PacketCategoryCombat;
			return true;
		}
		if (normalized == "skills") {
			*mask = PacketCategorySkill;
			return true;
		}
		if (normalized == "combat") {
			*mask = PacketCategoryBattle | PacketCategoryCombat | PacketCategorySkill |
				PacketCategoryMovement | PacketCategoryNpc;
			return true;
		}
		if (normalized == "minimal") {
			*mask = PacketCategorySystem;
			return true;
		}
		return false;
	}

	const PacketMeta* FindPacketMeta(BYTE msgId)
	{
		for (size_t i = 0; i < (sizeof(kPacketMeta) / sizeof(kPacketMeta[0])); ++i) {
			if (kPacketMeta[i].id == msgId) {
				return &kPacketMeta[i];
			}
		}
		return NULL;
	}

	void LoadPacketLogConfig()
	{
		PacketLogConfig cfg = g_packetLogConfig;
		cfg.include.clear();
		cfg.exclude.clear();
		cfg.sourcePath = "<defaults>";
		bool seenProfileConfig = false;

		char moduleDir[MAX_PATH];
		char configPath[MAX_PATH];
		if (!GetDllDirectory(moduleDir, MAX_PATH)) {
			g_packetLogConfig = cfg;
			LOG_F(INFO, "[PACKET-LOG] config path unresolved; profile=%s enabled=%u decode=%u hex=%u unknown=%u",
				cfg.profile.c_str(),
				(unsigned int)cfg.enabled,
				(unsigned int)cfg.decodeKnown,
				(unsigned int)cfg.logHex,
				(unsigned int)cfg.logUnknown);
			return;
		}

		_snprintf_s(configPath, MAX_PATH, _TRUNCATE, "%s\\edn_gf.ini", moduleDir);

		ifstream in(configPath);
		if (in.good()) {
			bool sawAnySection = false;
			bool sawPacketsSection = false;
			bool inPacketsSection = false;
			string line;
			while (getline(in, line)) {
				const string trimmed = TrimCopy(line);
				if (trimmed.empty()) {
					continue;
				}
				if (trimmed[0] == '#' || trimmed[0] == ';') {
					continue;
				}

				if (trimmed[0] == '[' && trimmed[trimmed.size() - 1] == ']') {
					sawAnySection = true;
					const string section = LowerCopy(TrimCopy(trimmed.substr(1, trimmed.size() - 2)));
					inPacketsSection = (section == "packets");
					if (inPacketsSection) {
						sawPacketsSection = true;
					}
					continue;
				}

				if (sawAnySection && !inPacketsSection) {
					continue;
				}

				const string::size_type equals = trimmed.find('=');
				if (equals == string::npos) {
					continue;
				}

				const string key = LowerCopy(TrimCopy(trimmed.substr(0, equals)));
				const string value = TrimCopy(trimmed.substr(equals + 1));

				if (key == "enabled") {
					cfg.enabled = ParseBoolValue(value, cfg.enabled);
				} else if (key == "decode_known" || key == "decode") {
					cfg.decodeKnown = ParseBoolValue(value, cfg.decodeKnown);
				} else if (key == "log_hex" || key == "hex") {
					cfg.logHex = ParseBoolValue(value, cfg.logHex);
				} else if (key == "log_unknown") {
					cfg.logUnknown = ParseBoolValue(value, cfg.logUnknown);
				} else if (key == "profile" || key == "profiles" || key == "template" || key == "templates") {
					if (!value.empty()) {
						const string normalizedValue = LowerCopy(value);
						if (!seenProfileConfig) {
							cfg.profile = normalizedValue;
							seenProfileConfig = true;
						} else if (!cfg.profile.empty()) {
							cfg.profile += ",";
							cfg.profile += normalizedValue;
						} else {
							cfg.profile = normalizedValue;
						}
					}
				} else if (key == "include") {
					const vector<string> tokens = SplitTokens(value);
					for (size_t i = 0; i < tokens.size(); ++i) {
						BYTE parsedID = 0;
						if (ParsePacketIDToken(tokens[i], &parsedID)) {
							cfg.include.insert(parsedID);
						}
					}
				} else if (key == "exclude") {
					const vector<string> tokens = SplitTokens(value);
					for (size_t i = 0; i < tokens.size(); ++i) {
						BYTE parsedID = 0;
						if (ParsePacketIDToken(tokens[i], &parsedID)) {
							cfg.exclude.insert(parsedID);
						}
					}
				}
			}

			cfg.sourcePath = configPath;
			if (sawAnySection && !sawPacketsSection) {
				LOG_F(INFO, "[PACKET-LOG] %s has no [packets] section; using defaults", configPath);
			}
		}

		unsigned int mergedMask = 0;
		bool anyValidProfile = false;
		bool allPackets = false;
		vector<string> resolvedProfiles;
		const vector<string> profileTokens = SplitTokens(cfg.profile);
		for (size_t i = 0; i < profileTokens.size(); ++i) {
			unsigned int tokenMask = 0;
			bool tokenAll = false;
			if (!TryProfileMaskFromName(profileTokens[i], &tokenMask, &tokenAll)) {
				continue;
			}
			anyValidProfile = true;
			resolvedProfiles.push_back(LowerCopy(TrimCopy(profileTokens[i])));
			mergedMask |= tokenMask;
			if (tokenAll) {
				allPackets = true;
			}
		}
		if (!resolvedProfiles.empty()) {
			cfg.profile = JoinTokensCSV(resolvedProfiles);
		}

		if (!anyValidProfile) {
			cfg.profile = "combat";
			unsigned int tokenMask = 0;
			bool tokenAll = false;
			TryProfileMaskFromName(cfg.profile, &tokenMask, &tokenAll);
			mergedMask = tokenMask;
			allPackets = tokenAll;
		}

		cfg.categoryMask = allPackets ? 0xFFFFFFFFu : mergedMask;
		cfg.allPacketsProfile = allPackets;
		if (cfg.allPacketsProfile) {
			cfg.logUnknown = true;
		}

		g_packetLogConfig = cfg;
			LOG_F(INFO,
			"[PACKET-LOG] source=%s profile=%s enabled=%u decode=%u hex=%u unknown=%u include=%u exclude=%u templates={all,lobby,matchflow,movement,gunplay,skills,combat,minimal}",
			g_packetLogConfig.sourcePath.c_str(),
			g_packetLogConfig.profile.c_str(),
			(unsigned int)g_packetLogConfig.enabled,
			(unsigned int)g_packetLogConfig.decodeKnown,
			(unsigned int)g_packetLogConfig.logHex,
			(unsigned int)g_packetLogConfig.logUnknown,
			(unsigned int)g_packetLogConfig.include.size(),
			(unsigned int)g_packetLogConfig.exclude.size());
	}

	bool ShouldLogPacket(BYTE msgId)
	{
		if (!g_packetLogConfig.enabled) {
			return false;
		}
		if (g_packetLogConfig.exclude.find(msgId) != g_packetLogConfig.exclude.end()) {
			return false;
		}
		if (g_packetLogConfig.include.find(msgId) != g_packetLogConfig.include.end()) {
			return true;
		}
		if (g_packetLogConfig.allPacketsProfile) {
			return true;
		}

		const PacketMeta* meta = FindPacketMeta(msgId);
		if (meta == NULL) {
			return g_packetLogConfig.logUnknown;
		}
		return (meta->categories & g_packetLogConfig.categoryMask) != 0;
	}

	const BYTE* PacketData(int* data)
	{
		return reinterpret_cast<const BYTE*>(data);
	}

	int ReadInt32(const BYTE* data, int offset)
	{
		return *reinterpret_cast<const int*>(data + offset);
	}

	unsigned int ReadUint32(const BYTE* data, int offset)
	{
		return *reinterpret_cast<const unsigned int*>(data + offset);
	}

	short ReadInt16(const BYTE* data, int offset)
	{
		return *reinterpret_cast<const short*>(data + offset);
	}

	float ReadFloat32(const BYTE* data, int offset)
	{
		return *reinterpret_cast<const float*>(data + offset);
	}

	PacketVector3 ReadVector3(const BYTE* data, int offset)
	{
		PacketVector3 v;
		v.x = ReadFloat32(data, offset + 0);
		v.y = ReadFloat32(data, offset + 4);
		v.z = ReadFloat32(data, offset + 8);
		return v;
	}

	double SignedRotatorToDegrees(int rawValue)
	{
		return static_cast<double>(rawValue) * 360.0 / 65536.0;
	}

	double UnsignedRotatorToDegrees(unsigned int rawValue)
	{
		return static_cast<double>(rawValue) * 360.0 / 65536.0;
	}

	const char* HitResultName(unsigned int resultCode)
	{
		switch (resultCode) {
		case 1:
			return "Hit";
		case 2:
			return "Miss";
		case 3:
			return "Block";
		case 4:
			return "Terrain";
		default:
			return "Unknown";
		}
	}

	void RememberUnitPosition(int unitId, const PacketVector3& pos)
	{
		g_lastUnitPositions[unitId] = pos;
	}

	bool TryGetUnitPosition(int unitId, PacketVector3* pos)
	{
		map<int, PacketVector3>::const_iterator it = g_lastUnitPositions.find(unitId);
		if (it == g_lastUnitPositions.end()) {
			return false;
		}

		if (pos != NULL) {
			*pos = it->second;
		}
		return true;
	}

	double Distance3D(const PacketVector3& a, const PacketVector3& b)
	{
		const double dx = static_cast<double>(a.x) - static_cast<double>(b.x);
		const double dy = static_cast<double>(a.y) - static_cast<double>(b.y);
		const double dz = static_cast<double>(a.z) - static_cast<double>(b.z);
		return sqrt(dx * dx + dy * dy + dz * dz);
	}

	string EstimatedRangeSuffix(int unitId, const PacketVector3& origin)
	{
		PacketVector3 targetPos;
		if (!TryGetUnitPosition(unitId, &targetPos)) {
			return " estRange=unknown";
		}

		ostringstream line;
		line << fixed << setprecision(1) << " estRange=" << Distance3D(origin, targetPos);
		return line.str();
	}

	void LogPacketHex(BYTE msgId, const BYTE* data, int byteCount)
	{
		ostringstream line;
		line << "BattleManager::OnPacket - ID: ["
			<< setfill('0') << setw(2) << right << hex << (int)msgId
			<< "] bytes(" << dec << byteCount << "): ";

		for (int i = 0; i < byteCount; ++i) {
			line << setfill('0') << setw(2) << right << hex << (int)data[i] << " ";
		}

		LOG_F(INFO, "%s\n", line.str().c_str());
	}

	void LogUnitMoved63(const BYTE* data)
	{
		const int unitId = ReadInt32(data, 4);
		const unsigned int aimY = static_cast<unsigned int>(*reinterpret_cast<const unsigned short*>(data + 35));
		const unsigned int aimX = static_cast<unsigned int>(*reinterpret_cast<const unsigned short*>(data + 37));
		const PacketVector3 pos = ReadVector3(data, 11);

		RememberUnitPosition(unitId, pos);
		LogPacketHex(0x63, data, 39);
		LOG_F(INFO,
			"  > 0x63 UnitMoved: time=%d unit=%d move=%u flag=%u boost=%u pos=(%.3f, %.3f, %.3f) vel=(%.3f, %.3f, %.3f) aim=(%u,%u raw / %.1f,%.1f deg)\n",
			ReadInt32(data, 0),
			unitId,
			(unsigned int)data[8],
			(unsigned int)data[9],
			(unsigned int)data[10],
			pos.x,
			pos.y,
			pos.z,
			ReadFloat32(data, 23),
			ReadFloat32(data, 27),
			ReadFloat32(data, 31),
			aimY,
			aimX,
			UnsignedRotatorToDegrees(aimY),
			UnsignedRotatorToDegrees(aimX));
	}

	void LogNpcMoveSmoothParse(const BYTE* data)
	{
		const int unitId = ReadInt32(data, 4);
		const int destRotPitch = (int)ReadInt16(data, 35);
		const int destRotYaw = (int)ReadInt16(data, 37);
		const int destRotRoll = ReadInt32(data, 39);
		const int rotVelPitch = (int)ReadInt16(data, 43);
		const int rotVelYaw = (int)ReadInt16(data, 45);
		const int rotVelRoll = ReadInt32(data, 47);
		const PacketVector3 dest = ReadVector3(data, 11);

		RememberUnitPosition(unitId, dest);
		LOG_F(INFO,
			"  > 0x78 NpcMoveSmooth: arrival=%d unit=%d move=%u flag1=%u flag2=%u dest=(%.3f, %.3f, %.3f) vel=(%.3f, %.3f, %.3f) destRot=(%d,%d,%d raw / %.1f,%.1f,%.1f deg) rotVel=(%d,%d,%d raw / %.1f,%.1f,%.1f deg)\n",
			ReadInt32(data, 0),
			unitId,
			(unsigned int)data[8],
			(unsigned int)data[9],
			(unsigned int)data[10],
			dest.x,
			dest.y,
			dest.z,
			ReadFloat32(data, 23),
			ReadFloat32(data, 27),
			ReadFloat32(data, 31),
			destRotPitch,
			destRotYaw,
			destRotRoll,
			SignedRotatorToDegrees(destRotPitch),
			SignedRotatorToDegrees(destRotYaw),
			SignedRotatorToDegrees(destRotRoll),
			rotVelPitch,
			rotVelYaw,
			rotVelRoll,
			SignedRotatorToDegrees(rotVelPitch),
			SignedRotatorToDegrees(rotVelYaw),
			SignedRotatorToDegrees(rotVelRoll));
	}

	void LogNpcMoveSmooth78(const BYTE* data)
	{
		LogPacketHex(0x78, data, 51);
		LogNpcMoveSmoothParse(data);
	}

	void LogNpcMoveNode79(const BYTE* data, int base, int index)
	{
		const int nodeBase = base + index * 35;
		const int rotPitch = (int)ReadInt16(data, nodeBase + 31);
		const int rotYaw = (int)ReadInt16(data, nodeBase + 33);
		LOG_F(INFO,
			"    node[%d]: time=%d move=%u flag1=%u flag2=%u start=(%.3f, %.3f, %.3f) dest=(%.3f, %.3f, %.3f) rot=(%d,%d raw / %.1f,%.1f deg)\n",
			index,
			ReadInt32(data, nodeBase + 0),
			(unsigned int)data[nodeBase + 4],
			(unsigned int)data[nodeBase + 5],
			(unsigned int)data[nodeBase + 6],
			ReadFloat32(data, nodeBase + 7),
			ReadFloat32(data, nodeBase + 11),
			ReadFloat32(data, nodeBase + 15),
			ReadFloat32(data, nodeBase + 19),
			ReadFloat32(data, nodeBase + 23),
			ReadFloat32(data, nodeBase + 27),
			rotPitch,
			rotYaw,
			SignedRotatorToDegrees(rotPitch),
			SignedRotatorToDegrees(rotYaw));
	}

	void LogNpcMove79(const BYTE* data)
	{
		const int unitId = ReadInt32(data, 0);
		const int pathStamp = ReadInt32(data, 4);
		const int nodeCount = ReadInt32(data, 8);
		int boundedNodeCount = nodeCount;
		if (boundedNodeCount < 0) {
			boundedNodeCount = 0;
		}
		if (boundedNodeCount > 4) {
			boundedNodeCount = 4;
		}

		int hexBytes = 12 + boundedNodeCount * 35;
		if (hexBytes < 12) {
			hexBytes = 12;
		}
		LogPacketHex(0x79, data, hexBytes);
		LOG_F(INFO, "  > 0x79 NpcMove: unit=%d pathStamp=%d nodes=%d\n", unitId, pathStamp, nodeCount);

		for (int i = 0; i < boundedNodeCount; ++i) {
			LogNpcMoveNode79(data, 12, i);
		}
	}

	void LogUnitStopped64(const BYTE* data)
	{
		const int unitId = ReadInt32(data, 4);
		const unsigned int aimY = static_cast<unsigned int>(*reinterpret_cast<const unsigned short*>(data + 34));
		const unsigned int aimX = static_cast<unsigned int>(*reinterpret_cast<const unsigned short*>(data + 36));
		const PacketVector3 pos = ReadVector3(data, 10);

		RememberUnitPosition(unitId, pos);
		LogPacketHex(0x64, data, 42);
		LOG_F(INFO,
			"  > 0x64 UnitStopped: time=%d unit=%d move=%u flag=%u pos=(%.3f, %.3f, %.3f) aim=(%u,%u raw / %.1f,%.1f deg) tail=%d\n",
			ReadInt32(data, 0),
			unitId,
			(unsigned int)data[8],
			(unsigned int)data[9],
			pos.x,
			pos.y,
			pos.z,
			aimY,
			aimX,
			UnsignedRotatorToDegrees(aimY),
			UnsignedRotatorToDegrees(aimX),
			ReadInt32(data, 38));
	}

	void LogMoveSync74(const BYTE* data)
	{
		const int unitId = ReadInt32(data, 4);
		const PacketVector3 pos = ReadVector3(data, 8);
		RememberUnitPosition(unitId, pos);

		LogPacketHex(0x74, data, 20);
		LOG_F(INFO,
			"  > 0x74 MoveSync: time=%d unit=%d pos=(%.3f, %.3f, %.3f)\n",
			ReadInt32(data, 0),
			unitId,
			pos.x,
			pos.y,
			pos.z);
	}

	void LogAimUnit65(const BYTE* data)
	{
		const int attackerID = ReadInt32(data, 4);
		const int victimID = ReadInt32(data, 8);
		const unsigned int aimY = static_cast<unsigned int>(*reinterpret_cast<const unsigned short*>(data + 20));
		const unsigned int aimX = static_cast<unsigned int>(*reinterpret_cast<const unsigned short*>(data + 22));
		const PacketVector3 pos = ReadVector3(data, 24);

		RememberUnitPosition(attackerID, pos);
		ostringstream line;
		line << fixed << setprecision(3)
			<< "  > 0x65 AimUnit: time=" << ReadInt32(data, 0)
			<< " attacker=" << attackerID
			<< " victim=" << victimID
			<< " arm=" << ReadInt32(data, 12)
			<< " aim=(" << aimY << "," << aimX << " raw / "
			<< setprecision(1) << UnsignedRotatorToDegrees(aimY) << "," << UnsignedRotatorToDegrees(aimX) << " deg)"
			<< setprecision(3)
			<< " pos=(" << pos.x << ", " << pos.y << ", " << pos.z << ")"
			<< EstimatedRangeSuffix(victimID, pos);
		LOG_F(INFO, "%s\n", line.str().c_str());
	}

	void LogUnAimUnit66(const BYTE* data)
	{
		LOG_F(INFO,
			"  > 0x66 UnAimUnit: time=%d attacker=%d victim=%d arm=%d\n",
			ReadInt32(data, 0),
			ReadInt32(data, 4),
			ReadInt32(data, 8),
			ReadInt32(data, 12));
	}

	void LogAttack67(const BYTE* data)
	{
		const unsigned int resultCode = (unsigned int)data[4];
		const int attackerID = ReadInt32(data, 8);
		const int victimID = ReadInt32(data, 16);
		const unsigned int aimY = static_cast<unsigned int>(*reinterpret_cast<const unsigned short*>(data + 32));
		const unsigned int aimX = static_cast<unsigned int>(*reinterpret_cast<const unsigned short*>(data + 34));
		const PacketVector3 pos = ReadVector3(data, 36);

		RememberUnitPosition(attackerID, pos);
		ostringstream line;
		line << fixed << setprecision(3)
			<< "  > 0x67 Attack: time=" << ReadInt32(data, 0)
			<< " attacker=" << attackerID
			<< " arm=" << ReadInt32(data, 12)
			<< " result=" << HitResultName(resultCode) << "(" << resultCode << ")"
			<< " victim=" << victimID
			<< " damage=" << ReadInt32(data, 20)
			<< " aim=(" << aimY << "," << aimX << " raw / "
			<< setprecision(1) << UnsignedRotatorToDegrees(aimY) << "," << UnsignedRotatorToDegrees(aimX) << " deg)"
			<< setprecision(3)
			<< " pos=(" << pos.x << ", " << pos.y << ", " << pos.z << ")"
			<< " overheat=" << ReadFloat32(data, 48)
			<< EstimatedRangeSuffix(victimID, pos);
		LOG_F(INFO, "%s\n", line.str().c_str());
	}

	void LogAttackShield6B(const BYTE* data)
	{
		const unsigned int resultCode = (unsigned int)data[4];
		const int blockerID = ReadInt32(data, 8);
		const int attackerID = ReadInt32(data, 16);
		const unsigned int aimY = static_cast<unsigned int>(*reinterpret_cast<const unsigned short*>(data + 32));
		const unsigned int aimX = static_cast<unsigned int>(*reinterpret_cast<const unsigned short*>(data + 34));
		const PacketVector3 pos = ReadVector3(data, 36);

		RememberUnitPosition(blockerID, pos);
		ostringstream line;
		line << fixed << setprecision(3)
			<< "  > 0x6B AttackShield: time=" << ReadInt32(data, 0)
			<< " blocker=" << blockerID
			<< " arm=" << ReadInt32(data, 12)
			<< " result=" << HitResultName(resultCode) << "(" << resultCode << ")"
			<< " attacker=" << attackerID
			<< " blockedDamage=" << ReadInt32(data, 20)
			<< " aim=(" << aimY << "," << aimX << " raw / "
			<< setprecision(1) << UnsignedRotatorToDegrees(aimY) << "," << UnsignedRotatorToDegrees(aimX) << " deg)"
			<< setprecision(3)
			<< " pos=(" << pos.x << ", " << pos.y << ", " << pos.z << ")"
			<< " shieldOH=" << ReadFloat32(data, 48)
			<< EstimatedRangeSuffix(attackerID, pos);
		LOG_F(INFO, "%s\n", line.str().c_str());
	}

	void LogAttackIfo6F(const BYTE* data)
	{
		const int attackerID = ReadInt32(data, 8);
		const int ifoID = ReadInt32(data, 16);
		const unsigned int aimY = static_cast<unsigned int>(*reinterpret_cast<const unsigned short*>(data + 24));
		const unsigned int aimX = static_cast<unsigned int>(*reinterpret_cast<const unsigned short*>(data + 26));
		const PacketVector3 pos = ReadVector3(data, 28);
		ProjectileLaunchState launch;
		launch.serverTimeMs = ReadInt32(data, 0);
		launch.attackerID = attackerID;
		launch.armIndex = ReadInt32(data, 12);
		launch.origin = pos;

		g_projectileLaunches[ifoID] = launch;
		RememberUnitPosition(attackerID, pos);

		ostringstream line;
		line << fixed << setprecision(3)
			<< "  > 0x6F AttackIfo: time=" << launch.serverTimeMs
			<< " attacker=" << attackerID
			<< " arm=" << launch.armIndex
			<< " ifo=" << ifoID
			<< " aim=(" << aimY << "," << aimX << " raw / "
			<< setprecision(1) << UnsignedRotatorToDegrees(aimY) << "," << UnsignedRotatorToDegrees(aimX) << " deg)"
			<< setprecision(3)
			<< " pos=(" << pos.x << ", " << pos.y << ", " << pos.z << ")"
			<< " overheat=" << ReadFloat32(data, 40);
		LOG_F(INFO, "%s\n", line.str().c_str());
	}

	void LogAttackIfoResult72(const BYTE* data)
	{
		const int serverTimeMs = ReadInt32(data, 0);
		const int ifoID = ReadInt32(data, 4);
		const int hitCount = ReadInt32(data, 8);
		int boundedHitCount = hitCount;
		if (boundedHitCount < 0) {
			boundedHitCount = 0;
		}
		if (boundedHitCount > 8) {
			boundedHitCount = 8;
		}

		map<int, ProjectileLaunchState>::const_iterator it = g_projectileLaunches.find(ifoID);
		const bool hasLaunch = it != g_projectileLaunches.end();

		ostringstream header;
		header << "  > 0x72 AttackIfoResult: time=" << serverTimeMs
			<< " ifo=" << ifoID
			<< " hits=" << hitCount;
		if (hasLaunch) {
			header << " attacker=" << it->second.attackerID
				<< " arm=" << it->second.armIndex
				<< " flightMs=" << (serverTimeMs - it->second.serverTimeMs);
		}
		LOG_F(INFO, "%s\n", header.str().c_str());

		for (int i = 0; i < boundedHitCount; ++i) {
			const int hitBase = 12 + i * 12;
			const unsigned int resultCode = (unsigned int)data[hitBase + 3];
			const int victimID = ReadInt32(data, hitBase + 4);
			const int damage = ReadInt32(data, hitBase + 8);

			ostringstream line;
			line << fixed << setprecision(1)
				<< "    hit[" << i << "]: result=" << HitResultName(resultCode) << "(" << resultCode << ")"
				<< " victim=" << victimID
				<< " damage=" << damage;
			if (hasLaunch) {
				line << EstimatedRangeSuffix(victimID, it->second.origin);
			}
			LOG_F(INFO, "%s\n", line.str().c_str());
		}

		g_projectileLaunches.erase(ifoID);
	}

	void LogStopAttack71(const BYTE* data)
	{
		LOG_F(INFO, "  > 0x71 StopAttack: unit=%d arm=%d\n", ReadInt32(data, 0), ReadInt32(data, 4));
	}

	void LogOverheatStatus83(const BYTE* data)
	{
		LOG_F(INFO,
			"  > 0x83 OverheatStatus: unit=%d value=%.3f set=%d arm=%u overheated=%u\n",
			ReadInt32(data, 0),
			ReadFloat32(data, 4),
			ReadInt32(data, 8),
			(unsigned int)data[12],
			(unsigned int)data[13]);
	}

	void LogWeaponsetChanged84(const BYTE* data)
	{
		LOG_F(INFO,
			"  > 0x84 WeaponsetChanged: unit=%d set=%d leftOH=%.3f rightOH=%.3f leftHot=%u rightHot=%u\n",
			ReadInt32(data, 0),
			ReadInt32(data, 4),
			ReadFloat32(data, 8),
			ReadFloat32(data, 12),
			(unsigned int)data[16],
			(unsigned int)data[17]);
	}

	void LogCantUseSkill86(const BYTE* data)
	{
		LogPacketHex(0x86, data, 8);
		LOG_F(INFO,
			"  > 0x86 CantUseSkill: code=%d result=%d\n",
			ReadInt32(data, 0),
			ReadInt32(data, 4));
	}

	void LogOnSkill87(const BYTE* data)
	{
		const int hitCount = ReadInt32(data, 20);
		int boundedHitCount = hitCount;
		if (boundedHitCount < 0) {
			boundedHitCount = 0;
		}
		if (boundedHitCount > 16) {
			boundedHitCount = 16;
		}

		const int hexBytes = 24 + (boundedHitCount * 8) + (6 * 12);
		LogPacketHex(0x87, data, hexBytes);

		LOG_F(INFO,
			"  > 0x87 OnSkill: time=%d unit=%d code=%d template=%u hits=%d flags=(%u,%u,%u,%u)\n",
			ReadInt32(data, 0),
			ReadInt32(data, 8),
			ReadInt32(data, 12),
			ReadUint32(data, 16),
			hitCount,
			(unsigned int)data[4],
			(unsigned int)data[5],
			(unsigned int)data[6],
			(unsigned int)data[7]);

		for (int i = 0; i < boundedHitCount; ++i) {
			const int hitBase = 24 + i * 8;
			LOG_F(INFO,
				"    hit[%d]: victim=%d damage=%d\n",
				i,
				ReadInt32(data, hitBase + 0),
				ReadInt32(data, hitBase + 4));
		}
	}

	void LogCastSkill88(const BYTE* data)
	{
		LogPacketHex(0x88, data, 12);
		LOG_F(INFO,
			"  > 0x88 CastSkill: unit=%d code=%d template=%u\n",
			ReadInt32(data, 0),
			ReadInt32(data, 4),
			ReadUint32(data, 8));
	}

	void LogCodeEffect89(const BYTE* data)
	{
		LogPacketHex(0x89, data, 16);
		LOG_F(INFO,
			"  > 0x89 CodeEffect: unit=%d effect=%d magnitude=%.3f duration=%.3f\n",
			ReadInt32(data, 0),
			ReadInt32(data, 4),
			ReadFloat32(data, 8),
			ReadFloat32(data, 12));
	}

	void LogCodeRemove8A(const BYTE* data)
	{
		LogPacketHex(0x8A, data, 8);
		LOG_F(INFO,
			"  > 0x8A CodeRemove: unit=%d effect=%d\n",
			ReadInt32(data, 0),
			ReadInt32(data, 4));
	}

	void LogCodeActivation8B(const BYTE* data)
	{
		LogPacketHex(0x8B, data, 8);
		LOG_F(INFO,
			"  > 0x8B CodeActivation: unit=%d code=%d\n",
			ReadInt32(data, 0),
			ReadInt32(data, 4));
	}

	void LogStatusChanged7A(const BYTE* data)
	{
		LOG_F(INFO,
			"  > 0x7A StatusChanged: unit=%d allowAttacks=%d allowDamage=%d allowMovement=%d\n",
			ReadInt32(data, 0),
			ReadInt32(data, 4),
			ReadInt32(data, 8),
			ReadInt32(data, 12));
	}

	void LogUnitDestroyed62(const BYTE* data)
	{
		const int victimID = ReadInt32(data, 8);
		g_lastUnitPositions.erase(victimID);

		LOG_F(INFO,
			"  > 0x62 UnitDestroyed: time=%d killer=%d victim=%d weapon=%u skill=%u\n",
			ReadInt32(data, 0),
			ReadInt32(data, 4),
			victimID,
			(unsigned int)ReadInt32(data, 12),
			(unsigned int)ReadInt32(data, 16));
	}

	void LogUnitRepaired5A(const BYTE* data)
	{
		LOG_F(INFO, "  > 0x5A UnitRepaired: unit=%d\n", ReadInt32(data, 0));
	}

	void LogRegainInfo59(const BYTE* data)
	{
		LOG_F(INFO,
			"  > 0x59 RegainInfo: count=%d unit=%d launchOrder=%d unk=%d launchHeight=%.1f cooldownMs=%d\n",
			ReadInt32(data, 0),
			ReadInt32(data, 4),
			ReadInt32(data, 8),
			ReadInt32(data, 12),
			ReadFloat32(data, 16),
			ReadInt32(data, 20));
	}

	void LogNpcHpUpdateCB(const BYTE* data)
	{
		LOG_F(INFO, "  > 0xCB NpcHpUpdate: unit=%d hp=%d\n", ReadInt32(data, 0), ReadInt32(data, 4));
	}

	void LogNpcKillDissolve7B(const BYTE* data)
	{
		const int unitId = ReadInt32(data, 0);
		g_lastUnitPositions.erase(unitId);
		LOG_F(INFO, "  > 0x7B NpcKillDissolve: unit=%d\n", unitId);
	}

	bool IsDecodedPacket(BYTE msgId)
	{
		switch (msgId) {
		case 0x59:
		case 0x5A:
		case 0x62:
		case 0x63:
		case 0x64:
		case 0x65:
		case 0x66:
		case 0x67:
		case 0x6B:
		case 0x6F:
		case 0x71:
		case 0x72:
		case 0x74:
		case 0x78:
		case 0x79:
		case 0x7A:
		case 0x7B:
		case 0x83:
		case 0x84:
		case 0x86:
		case 0x87:
		case 0x88:
		case 0x89:
		case 0x8A:
		case 0x8B:
		case 0xCB:
			return true;
		default:
			return false;
		}
	}

	void LogGenericPacket(BYTE msgId, const BYTE* data)
	{
		const PacketMeta* meta = FindPacketMeta(msgId);
		const char* packetName = (meta != NULL && meta->name != NULL) ? meta->name : "UNKNOWN_PACKET";
		const int fixedBytes = (meta != NULL) ? meta->fixedBytes : -1;

		if (g_packetLogConfig.logHex && data != NULL && fixedBytes > 0) {
			LogPacketHex(msgId, data, fixedBytes);
		}

		if (meta != NULL) {
			LOG_F(INFO, "  > 0x%02X %s\n", (unsigned int)msgId, packetName);
		} else {
			LOG_F(INFO, "  > 0x%02X %s (no metadata)\n", (unsigned int)msgId, packetName);
		}
	}

	void LogDecodedPacket(BYTE msgId, int* data)
	{
		const BYTE* bytes = PacketData(data);
		switch (msgId) {
		case 0x59:
			LogRegainInfo59(bytes);
			break;
		case 0x5A:
			LogUnitRepaired5A(bytes);
			break;
		case 0x62:
			LogUnitDestroyed62(bytes);
			break;
		case 0x63:
			LogUnitMoved63(bytes);
			break;
		case 0x64:
			LogUnitStopped64(bytes);
			break;
		case 0x65:
			LogAimUnit65(bytes);
			break;
		case 0x66:
			LogUnAimUnit66(bytes);
			break;
		case 0x67:
			LogAttack67(bytes);
			break;
		case 0x6B:
			LogAttackShield6B(bytes);
			break;
		case 0x6F:
			LogAttackIfo6F(bytes);
			break;
		case 0x71:
			LogStopAttack71(bytes);
			break;
		case 0x72:
			LogAttackIfoResult72(bytes);
			break;
		case 0x74:
			LogMoveSync74(bytes);
			break;
		case 0x78:
			LogNpcMoveSmooth78(bytes);
			break;
		case 0x79:
			LogNpcMove79(bytes);
			break;
		case 0x7A:
			LogStatusChanged7A(bytes);
			break;
		case 0x7B:
			LogNpcKillDissolve7B(bytes);
			break;
		case 0x83:
			LogOverheatStatus83(bytes);
			break;
		case 0x84:
			LogWeaponsetChanged84(bytes);
			break;
		case 0x86:
			LogCantUseSkill86(bytes);
			break;
		case 0x87:
			LogOnSkill87(bytes);
			break;
		case 0x88:
			LogCastSkill88(bytes);
			break;
		case 0x89:
			LogCodeEffect89(bytes);
			break;
		case 0x8A:
			LogCodeRemove8A(bytes);
			break;
		case 0x8B:
			LogCodeActivation8B(bytes);
			break;
		case 0xCB:
			LogNpcHpUpdateCB(bytes);
			break;
		default:
			LogGenericPacket(msgId, bytes);
			break;
		}
	}
}

// Injected function
void __fastcall hOnPacket(void* that, void* Unknown, char param1, int* param2)
{
	if (g_pFace) {
		const BYTE msgId = static_cast<BYTE>(param1);
		if (ShouldLogPacket(msgId)) {
			if (g_packetLogConfig.decodeKnown && IsDecodedPacket(msgId)) {
				LogDecodedPacket(msgId, param2);
			} else {
				LogGenericPacket(msgId, PacketData(param2));
			}
		}

		if (OnPacketCallback) {
			OnPacketCallback(param1, param2);
		}
	}

	return oOnPacket(that, param1, param2);
}

// Hooks GF.dll BattleManager::OnPacket for packet logging.
void HookOnPacket(uintptr_t moduleBase)
{
	LoadPacketLogConfig();

	LOG_F(INFO, "hooking BattleManager::OnPacket");

	// Pointer to vtable entry
	uintptr_t pOnPacket = moduleBase + 0x16b9ac;

	// Initialize the hook and memory addresses
	packetHook = new IATHook(&g_pFace->con);
	packetHook->Init((HMODULE)moduleBase, &hOnPacket, pOnPacket);

	// Save the original function
	oOnPacket = (_OnPacket)packetHook->GetOriginalFunction();

	// Activate the hook
	packetHook->Hook();

	LOG_F(INFO, "BattleManager::OnPacket hook ready");
}

// Cleanup
void UnHookOnPacket()
{
	if (packetHook) {
		packetHook->Unload();
		delete packetHook;
		packetHook = NULL;
	}
}

#endif // _DEBUG
