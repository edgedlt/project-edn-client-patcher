#pragma once
#include <stdio.h>
#include <string>
#include "Console.h"
#include "loguru.hpp"

class Interface
{
public:
	Interface();
	~Interface() noexcept;

	Console con;
	bool fileLoggingEnabled = false;

private:
	void Init() noexcept;
	void PrintIntro() noexcept;
};

extern Interface * g_pFace;
