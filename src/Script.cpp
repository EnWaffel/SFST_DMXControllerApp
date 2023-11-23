#include "Script.h"
#include <iostream>
#include <fstream>
#include "DMXLuaLib.h"
#include <Windows.h>
#include <locale>
#include <iostream>
#include <sstream>
#include "Application.h"

std::wstring widen(const std::string& str)
{
	std::wostringstream wstm;
	const std::ctype<wchar_t>& ctfacet = std::use_facet<std::ctype<wchar_t>>(wstm.getloc());
	for (size_t i = 0; i < str.size(); ++i)
		wstm << ctfacet.widen(str[i]);
	return wstm.str();
}

Script::Script(const std::string& path) : error(0)
{
	Application::INSTANCE->scriptActions.clear();
	L = luaL_newstate();
	luaL_openlibs(L);
	DMXLuaLib::LoadLib(L);

	if (luaL_dofile(L, path.c_str()) != 0) {
		const char* errorMessage = lua_tostring(L, -1);
		printf("Lua error: %s\n", errorMessage);
		MessageBox(NULL, widen(errorMessage).c_str(), L"Lua Error", MB_OK | MB_ICONERROR);
		lua_close(L);
		L = nullptr;
	}

	Application::INSTANCE->scriptActions.push_back("Done");
}

Script::~Script()
{
	if (L != nullptr)
	{
		lua_close(L);
	}
}