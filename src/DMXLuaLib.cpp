#include "DMXLuaLib.h"
#include <iostream>
#include "Application.h"
#include <chrono>
#include <thread>
#include <sstream>

uint64_t timeSinceEpochMillisec() {
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}


template <typename T>
std::string ToString(const T a_value, const int n = 6)
{
	std::ostringstream out;
	out.precision(n);
	out << std::fixed << a_value;
	return std::move(out).str();
}

double lerp(double a, double b, double f)
{
	return a * (1.0 - f) + (b * f);
}

float colors[4] = {};

static int L_DMX_setColor(lua_State* L)
{
	colors[0] = luaL_checknumber(L, 1);
	colors[1] = luaL_checknumber(L, 2);
	colors[2] = luaL_checknumber(L, 3);
	Application::INSTANCE->scriptActions.push_back("Set Colors to: Red: " + ToString(colors[0], 0) + " | Green: " + ToString(colors[1], 0) + " | Blue: " + ToString(colors[2], 0));
	Application::INSTANCE->UpdateDMXColors(colors);
	return 0;
}

static int L_DMX_setBrightness(lua_State* L)
{
	colors[3] = luaL_checknumber(L, 1);
	Application::INSTANCE->scriptActions.push_back("Set Brightness to: " + ToString(colors[3], 0));
	Application::INSTANCE->UpdateDMXColors(colors);
	return 0;
}

static int L_DMX_getChannels(lua_State* L)
{
	Application::INSTANCE->scriptActions.push_back("Get Channels");
	lua_pushnumber(L, Application::INSTANCE->dmxChannels);
	return 1;
}

static int L_appRunning(lua_State* L)
{
	lua_pushboolean(L, Application::INSTANCE->running);
	return 1;
}

static int L_lerp(lua_State* L)
{
	double a = luaL_checknumber(L, 1);
	double b = luaL_checknumber(L, 2);
	double f = luaL_checknumber(L, 3);
	lua_pushnumber(L, lerp(a, b, f));
	return 1;
}

static int L_wait(lua_State* L)
{
	double s = luaL_checknumber(L, 1);
	Application::INSTANCE->scriptActions.push_back("Wait: " + ToString(s, 2) + "s");
	uint64_t start = timeSinceEpochMillisec();

	while (Application::INSTANCE->running)
	{
		uint64_t now = timeSinceEpochMillisec();
		if (now - start - (s * 1000) > 0)
		{
			break;
		}
	}
	return 0;
}

void DMXLuaLib::LoadLib(lua_State* L)
{
	lua_pushcfunction(L, L_appRunning);
	lua_setglobal(L, "appRunning");
	lua_pushcfunction(L, L_wait);
	lua_setglobal(L, "wait");
	lua_pushcfunction(L, L_lerp);
	lua_setglobal(L, "lerp");

	lua_pushnumber(L, DMX_RGB);
	lua_setglobal(L, "DMX_RGB");
	lua_pushnumber(L, DMX_DRGB);
	lua_setglobal(L, "DMX_DRGB");

	lua_pushcfunction(L, L_DMX_setColor);
	lua_setglobal(L, "DMX_setColor");
	lua_pushcfunction(L, L_DMX_setBrightness);
	lua_setglobal(L, "DMX_setBrightness");
	lua_pushcfunction(L, L_DMX_getChannels);
	lua_setglobal(L, "DMX_getChannels");
}