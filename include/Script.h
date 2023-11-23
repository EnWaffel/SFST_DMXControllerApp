#pragma once
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <string>


class Script
{
private:
	lua_State* L;
	int error;
public:
	Script(const std::string& path);
	~Script();
};