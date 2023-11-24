#pragma once
#include <SerialComm.h>
#include <vector>

#define CONN_STATUS_NOT_CONNECTED 0
#define CONN_STATUS_CONNECTING 1
#define CONN_STATUS_CONNECTED 2

#define DMX_RGB 3
#define DMX_DRGB 4

#define CMD_SYNC_MODE -1
#define CMD_DMX_CHANNELS -2
#define CMD_DMX_MODE -3
#define CMD_SMOOTHING -4
#define CMD_SMOOTHING_SPEED -5
#define CMD_TARGET_ID -6

class Application
{
public:
	static Application* INSTANCE;

	SerialComm comm;
	int dmxChannels = DMX_RGB;
	bool running = true;
	std::vector<std::string> scriptActions;

	void Init();
	void UpdateDMXColors(float* colors);
	void ConnectToArduino();
};