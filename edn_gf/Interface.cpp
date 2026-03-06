#include "stdafx.h"
#include "Interface.h"
#include "ModuleDir.h"

// Externals
Interface * g_pFace;

#ifdef _DEBUG
namespace
{
	// Read log_mode from edn_gf.ini.  Returns loguru::Truncate for "replace",
	// loguru::Append for "append" (or when the file/key is absent).
	loguru::FileMode ReadLogModeFromIni(const char* moduleDir) noexcept
	{
		char configPath[MAX_PATH];
		_snprintf_s(configPath, MAX_PATH, _TRUNCATE, "%s\\edn_gf.ini", moduleDir);

		FILE* f = nullptr;
		if (fopen_s(&f, configPath, "r") != 0 || f == nullptr) {
			return loguru::Append;
		}

		loguru::FileMode mode = loguru::Append;
		char line[256];
		bool inLogSection = false;
		while (fgets(line, sizeof(line), f)) {
			// Skip comments and blank lines.
			const char* p = line;
			while (*p == ' ' || *p == '\t') ++p;
			if (*p == '\0' || *p == '\n' || *p == '\r' || *p == ';' || *p == '#') {
				continue;
			}

			// Section header?
			if (*p == '[') {
				inLogSection = (_strnicmp(p, "[log]", 5) == 0);
				continue;
			}

			if (!inLogSection) {
				continue;
			}

			if (_strnicmp(p, "log_mode", 8) == 0) {
				const char* eq = strchr(p, '=');
				if (eq) {
					++eq;
					while (*eq == ' ' || *eq == '\t') ++eq;
					if (_strnicmp(eq, "replace", 7) == 0) {
						mode = loguru::Truncate;
					}
				}
			}
		}
		fclose(f);
		return mode;
	}

	bool InitFileLogging() noexcept
	{
		char moduleDir[MAX_PATH];
		char logPath[MAX_PATH];

		if (GetDllDirectory(moduleDir, MAX_PATH)) {
			loguru::FileMode mode = ReadLogModeFromIni(moduleDir);
			_snprintf_s(logPath, MAX_PATH, _TRUNCATE, "%s\\edn_gf.log", moduleDir);
			if (loguru::add_file(logPath, mode, loguru::Verbosity_INFO)) {
				LOG_F(INFO, "client file logging enabled: %s (mode=%s)",
					logPath, mode == loguru::Truncate ? "replace" : "append");
				return true;
			}
		}

		if (loguru::add_file("edn_gf.log", loguru::Append, loguru::Verbosity_INFO)) {
			LOG_F(WARNING, "client file logging fallback enabled: edn_gf.log");
			return true;
		}

		return false;
	}
}

void DuplicateToConsole(void* user_data, const loguru::Message& message) noexcept
{
	Console* console = reinterpret_cast<Console*>(user_data);
	console->WriteF("%s%s", message.prefix, message.message);
}
#endif

// Constructor
Interface::Interface()
{
#ifdef _DEBUG
	// Keep debug logs compact and readable.
	loguru::g_internal_verbosity = loguru::Verbosity_OFF;
	loguru::g_preamble_header = false;
	loguru::g_preamble_uptime = false;
	loguru::g_preamble_thread = false;
	loguru::g_preamble_file = false;
	loguru::g_preamble_verbose = false;
	loguru::g_preamble_pipe = false;

	fileLoggingEnabled = InitFileLogging();
	if (!fileLoggingEnabled && con.Enable()) {
		loguru::add_callback("console_logger",
			DuplicateToConsole, &con, loguru::Verbosity_MAX);
	}
#else
	// Release builds should patch silently without verbose runtime logging.
	loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
	fileLoggingEnabled = false;
#endif

	Init();
}

Interface::~Interface() noexcept
{
}

void Interface::Init() noexcept
{
	if (con.IsEnabled()) {
		con.Clear();
		con.SetCursorPos(COORD{ 0,0 });
	}
	PrintIntro();
	g_pFace = this;
}

void Interface::PrintIntro() noexcept
{
#ifdef _DEBUG
	LOG_F(INFO, "edn_gf patch active (debug)");
#endif
}
