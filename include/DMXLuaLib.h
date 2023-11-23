#pragma once
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

class DMXLuaLib
{
public:
	static void LoadLib(lua_State* L);
};