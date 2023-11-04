#include "Application.h"
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <iostream>
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>
#include <string>
#include <vector>
#include "SerialComm.h"
#include <thread>
#include <chrono>

#define CONN_STATUS_NOT_CONNECTED 0
#define CONN_STATUS_CONNECTING 1
#define CONN_STATUS_CONNECTED 2

#define DMX_RGB 3
#define DMX_DRGB 4

static GLFWwindow* window;
static HWND nativeWindow;
static int windowWidth;
static int windowHeight;
static float dmxColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static bool autoUpdate = false;
static std::vector<std::string> usableUSBPorts;
int selectedUSBPortIndex = 0;
int connectedStatus = 0;
static SerialComm comm;
static bool syncMode = true;
static int dmxChannels = DMX_RGB;
static int dmxChannelsSelected = 0;
static bool dmxEnabled = false;

#define WIDTH 500
#define HEIGHT 400
#define WIDTHf 500.0f
#define HEIGHTf 400.0f

void ScanUSBPorts()
{
	HDEVINFO hDevInfo;
	SP_DEVINFO_DATA DeviceInfoData;
	char portName[256];  // Buffer to store port names

	// Initialize the structure before using it.
	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	// Get a list of all connected devices with the COM class GUID
	hDevInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS, 0, 0, DIGCF_PRESENT);

	if (hDevInfo == INVALID_HANDLE_VALUE) {
		std::cerr << "SetupDiGetClassDevs failed." << std::endl;
		return;
	}

	// Enumerate through the devices to find the Arduino
	DWORD index = 0;
	while (SetupDiEnumDeviceInfo(hDevInfo, index, &DeviceInfoData)) {
		// Get the friendly name of the device
		if (SetupDiGetDeviceRegistryPropertyA(hDevInfo, &DeviceInfoData, SPDRP_FRIENDLYNAME, NULL, (PBYTE)portName, sizeof(portName), NULL)) {
			// Check if the friendly name contains "Arduino"
			std::string comPortName;
			if (strstr(portName, "Arduino")) {
				std::string friendlyNameStr(portName);
				size_t startPos = friendlyNameStr.find("(COM");
				size_t endPos = friendlyNameStr.find(")");
				if (startPos != std::string::npos && endPos != std::string::npos && endPos > startPos) {
					comPortName = friendlyNameStr.substr(startPos + 1, endPos - startPos - 1);
					usableUSBPorts.push_back(comPortName);
					// Additional filtering or processing can be done here
				}
			}
		}
		index++;
	}

	// Clean up
	SetupDiDestroyDeviceInfoList(hDevInfo);
}

void Application::Init()
{
	ScanUSBPorts();

	if (!glfwInit())
	{
		std::cout << "Failed to initialize GLFW!" << std::endl;
		return;
	}

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(WIDTH, HEIGHT, "DMX Contoller App", nullptr, nullptr);

	nativeWindow = glfwGetWin32Window(window);
	if (CoInitialize(NULL) != 0)
	{
		std::cout << "Failed to initialize Common Item Dialog" << std::endl;
		glfwTerminate();
		return;
	}

	glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD!" << std::endl;
		glfwTerminate();
		return;
	}

	int width, height;
	glfwGetWindowSize(window, &width, &height);
	windowWidth = width;
	windowHeight = height;

	const GLFWvidmode* vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());

	glfwSetWindowPos(
		window,
		(vidmode->width - width) / 2,
		(vidmode->height - height) / 2
	);

	glViewport(0, 0, WIDTH, HEIGHT);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	ImGui::StyleColorsLight();
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3);
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	glfwShowWindow(window);
	glfwSetWindowSize(window, WIDTH + 1, HEIGHT + 1);
	glfwSetWindowSize(window, WIDTH, HEIGHT);

	while (!glfwWindowShouldClose(window))
	{
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glfwPollEvents();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImVec2(500, HEIGHTf));
		ImGui::Begin("dmx_controller", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDecoration);

		std::string selection;
		for (std::string port : usableUSBPorts)
		{
			selection.append(port);
			selection.push_back('\0');
		}

		ImGui::SetNextItemWidth(75);
		ImGui::Combo("Arduino Port", &selectedUSBPortIndex, selection.c_str());
		if (ImGui::Button("Verbinden") && connectedStatus == CONN_STATUS_NOT_CONNECTED)
		{
			connectedStatus = CONN_STATUS_CONNECTING;
			ConnectToArduino();
		}

		ImGui::SameLine();
		if (ImGui::Button("Trennen") && connectedStatus == CONN_STATUS_CONNECTED)
		{
			connectedStatus = CONN_STATUS_NOT_CONNECTED;
			comm.Close();
		}

		ImGui::SameLine();
		switch (connectedStatus)
		{
		case CONN_STATUS_NOT_CONNECTED:
		{
			ImGui::TextColored(ImVec4(0.75f, 0.0f, 0.0f, 1.0f), "Nicht verbunden");
			break;
		}
		case CONN_STATUS_CONNECTING:
		{
			ImGui::TextColored(ImVec4(1.0f, 0.5019607843137255f, 0.0f, 1.0f), "Verbinde...");
			break;
		}
		case CONN_STATUS_CONNECTED:
		{
			ImGui::TextColored(ImVec4(0.0f, 0.75f, 0.0f, 1.0f), "Verbunden");
			break;
		}
		}

		ImGui::NewLine();
		ImGui::Separator();
		ImGui::NewLine();

		//ImGui::Checkbox(u8"Automatisch Senden", &autoUpdate);

		if (ImGui::Checkbox("Sync-Modus", &syncMode) && connectedStatus == CONN_STATUS_CONNECTED)
		{
			comm.WriteByte(1);

			std::string str;
			str.append(std::to_string(-1));
			str.push_back(':');
			str.append(std::to_string(syncMode));

			str.push_back(';');
			comm.WriteString(str);
		}

		if (autoUpdate) ImGui::BeginDisabled();

		if (ImGui::Checkbox("DMX", &dmxEnabled))
		{
			comm.WriteByte(1);

			std::string str;
			str.append(std::to_string(-3));
			str.push_back(':');
			str.append(std::to_string(dmxEnabled));

			str.push_back(';');
			comm.WriteString(str);
		}

		ImGui::SetNextItemWidth(75);
		if (ImGui::Combo("DMX Channel-Modus", &dmxChannelsSelected, "RGB\0DRGB\0"))
		{
			switch (dmxChannelsSelected)
			{
			case 0:
			{
				dmxChannels = DMX_RGB;
				break;
			}
			case 1:
			{
				dmxChannels = DMX_DRGB;
				break;
			}
			}

			comm.WriteByte(1);

			std::string str;
			str.append(std::to_string(-2));
			str.push_back(':');
			str.append(std::to_string(dmxChannels));

			str.push_back(';');
			comm.WriteString(str);
		}

		if (ImGui::Button("Farben Senden"))
		{
			UpdateDMXColors();
		}

		if (autoUpdate) ImGui::EndDisabled();

		if (dmxChannels == DMX_DRGB)
		{
			ImGui::SetColorEditOptions(ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);
		}
		else
		{
			ImGui::SetColorEditOptions(ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_PickerHueBar);
		}
		ImGui::SetNextItemWidth(200);
		ImGui::ColorPicker4("##colorpicker", (float*)&dmxColor, ImGuiColorEditFlags_NoInputs);

		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwTerminate();
	if (connectedStatus == CONN_STATUS_CONNECTED)
	{
		comm.Close();
	}
}

void Application::UpdateDMXColors()
{
	if (connectedStatus == CONN_STATUS_CONNECTED)
	{
		comm.WriteByte(1);

		std::string str;
		str.append(std::to_string((int)(dmxColor[0] * 255.0f)));
		str.push_back(':');
		str.append(std::to_string((int)(dmxColor[1] * 255.0f)));
		str.push_back(':');
		str.append(std::to_string((int)(dmxColor[2] * 255.0f)));
		str.push_back(':');
		str.append(std::to_string((int)(dmxColor[3] * 255.0f)));

		str.push_back(';');
		comm.WriteString(str);
	}
}

void Application::ConnectToArduino()
{
	if (usableUSBPorts.size() < 1)
	{
		connectedStatus = CONN_STATUS_NOT_CONNECTED;
		return;
	}
	Result result = comm.Open(SerialComm::GetDevice(usableUSBPorts.at(selectedUSBPortIndex)), 115200);
	if (result == RESULT_ERROR)
	{
		connectedStatus = CONN_STATUS_NOT_CONNECTED;
	}
	else
	{
		connectedStatus = CONN_STATUS_CONNECTED;
	}
}