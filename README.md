# project-edn-client-patcher

Runtime client hook DLL (`edn_gf.dll`) for Exteel.

## Attribution

Original project: **project-edn-client-patcher** by **jakefahrbach**

## What It Does

- Loads inside the game process via `GF.dll` import patching.
- Hooks login flow (`NWindow.dll` `AddEventInternal`) and sends direct login packet.
- In debug builds, hooks packet receive path (`BattleManager::OnPacket`) with structured packet logging and filters.

## Build And Install

1. Build `edn_gf` as **x86** (`Debug` or `Release`).
2. Copy `edn_gf.dll` to `Exteel (US)/System/`.
3. Patch `GF.dll` import table so it imports `edn_gf.dll` (for example with IIDKing or the integrated `rexteel patch` flow).

## Debug vs Release

- `Debug`:
  - Enables packet hook/logging.
  - Writes log file to `System/edn_gf.log`.
  - Falls back to console output only if file logging cannot be opened.
- `Release`:
  - Patches silently (no packet logger output by default).
  - Keeps runtime hooks required for patch behavior.

## Log Config (Debug)

Config file path: `Exteel (US)/System/edn_gf.ini`

Use the `[log]` section to control log file behavior:
- `log_mode` — `append` (default) keeps previous session logs; `replace` starts fresh each session.

## Packet Logging Config (Debug)

Use the `[packets]` section. Keys:
- `enabled`
- `profile` (comma-separated)
- `decode_known`
- `log_hex`
- `log_unknown`
- `include`
- `exclude`

Supported profiles:
- `all`
- `lobby`
- `matchflow`
- `movement`
- `gunplay`
- `skills`
- `combat`
- `minimal`

Example config is included at `edn_gf/edn_gf.ini.example`.

## Legal

This project is distributed as-is and requires user-provided game files.
No game assets are included.
