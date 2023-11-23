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
#include "Script.h"
#include <filesystem>

Application* Application::INSTANCE = nullptr;

static GLFWwindow* window;
static HWND nativeWindow;
static int windowWidth;
static int windowHeight;
static float dmxColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static bool autoUpdate = false;
static std::vector<std::string> usableUSBPorts;
int selectedUSBPortIndex = 0;
int connectedStatus = 0;
static bool syncMode = true;
static int dmxChannelsSelected = 0;
static bool dmxEnabled = false;
static bool smoothing = false;
static float smoothingSpeed = 0.001f;
static Script* curScript = nullptr;
static int scriptIndex = 0;
static std::vector<std::string> scriptPaths;
static std::thread* scriptThread;

namespace fs = std::filesystem;

#define WIDTH 500
#define HEIGHT 600
#define WIDTHf 500.0f
#define HEIGHTf 600.0f

std::string GetFileExtension(const std::string& filePath) {
	fs::path file_path(filePath);

	std::string file_extension = file_path.extension().string();

	if (!file_extension.empty()) {
		if (file_extension[0] == '.') {
			file_extension = file_extension.substr(1);
		}
		return file_extension;
	}
	else {
		return "";
	}
}

std::string ToLower(const std::string& input) {
	std::string result = input;
	for (char& c : result) {
		c = std::tolower(c);
	}
	return result;
}

void ScanScripts()
{
	try {
		for (const auto& entry : fs::directory_iterator("scripts")) {
			if (fs::is_regular_file(entry)) {
				std::string ext = ToLower(GetFileExtension(entry.path().string()));
				if (entry.path().string() == "scripts\\dmx.lua" || entry.path().string() == "scripts\\example.lua") continue;
				if (ext == "lua")
				{
					scriptPaths.push_back(entry.path().string());
				}
			}
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
}

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

	ScanScripts();

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

		if (connectedStatus == CONN_STATUS_NOT_CONNECTED)
		{
			ImGui::BeginDisabled();
		}

		//ImGui::Checkbox(u8"Automatisch Senden", &autoUpdate);

		if (ImGui::Checkbox("Sync-Modus", &syncMode) && connectedStatus == CONN_STATUS_CONNECTED)
		{
			comm.WriteByte(1);

			std::string str;
			str.append(std::to_string(CMD_SYNC_MODE));
			str.push_back(':');
			str.append(std::to_string(syncMode));

			str.push_back(';');
			comm.WriteString(str);
		}

		if (ImGui::Checkbox("Smoothing", &smoothing))
		{
			comm.WriteByte(1);

			std::string str;
			str.append(std::to_string(CMD_SMOOTHING));
			str.push_back(':');
			str.append(std::to_string(smoothing));

			str.push_back(';');
			comm.WriteString(str);
		}

		if (ImGui::SliderFloat("Smoothing Speed", &smoothingSpeed, 0.001f, 0.35f))
		{
			comm.WriteByte(1);

			std::string str;
			str.append(std::to_string(CMD_SMOOTHING_SPEED));
			str.push_back(':');
			str.append(std::to_string(smoothingSpeed));

			str.push_back(';');
			comm.WriteString(str);
		}

		if (ImGui::Checkbox("DMX", &dmxEnabled))
		{
			comm.WriteByte(1);

			std::string str;
			str.append(std::to_string(CMD_DMX_MODE));
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
			str.append(std::to_string(CMD_DMX_CHANNELS));
			str.push_back(':');
			str.append(std::to_string(dmxChannels));

			str.push_back(';');
			comm.WriteString(str);
		}

		if (ImGui::Button("Farben Senden"))
		{
			UpdateDMXColors(nullptr);
		}

		if (dmxChannels == DMX_DRGB)
		{
			ImGui::SetColorEditOptions(ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview);
		}
		else
		{
			ImGui::SetColorEditOptions(ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_PickerHueBar);
		}
		ImGui::SetNextItemWidth(200);
		if (ImGui::ColorPicker4("##colorpicker", (float*)&dmxColor, ImGuiColorEditFlags_NoInputs))
		{
		}

		ImGui::Separator();

		std::string s;
		for (std::string path : scriptPaths)
		{
			s.append(path);
			s.push_back('\0');
		}
		ImGui::Combo("Skript", &scriptIndex, s.c_str());
		ImGui::SameLine();
		if (ImGui::Button("Skript Starten"))
		{
			if (curScript != nullptr)
			{
				running = false;
				scriptThread->join();
				delete curScript;
				delete scriptThread;
			}
			running = true;
			scriptThread = new std::thread([]() {
				curScript = new Script(scriptPaths.at(scriptIndex));
			});
		}

		ImGui::BeginChild("##script_actions", ImVec2(WIDTH - 15, 100), true);

		for (std::string action : scriptActions)
		{
			ImGui::TextWrapped(action.c_str());
		}

		ImGui::EndChild();

		if (connectedStatus == CONN_STATUS_NOT_CONNECTED)
		{
			ImGui::EndDisabled();
		}

		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwTerminate();
	running = false;
	if (connectedStatus == CONN_STATUS_CONNECTED)
	{
		comm.Close();
	}
	if (curScript != nullptr)
	{
		delete curScript;
		scriptThread->join();
		delete scriptThread;
	}
}

void Application::UpdateDMXColors(float* colors)
{
	if (connectedStatus == CONN_STATUS_CONNECTED)
	{
		std::string str;
		str.push_back(1);

		if (colors != nullptr)
		{
			str.append(std::to_string((int)(colors[0] * 255.0f)));
		}
		else
		{
			str.append(std::to_string((int)(dmxColor[0] * 255.0f)));
		}
		str.push_back(':');
		if (colors != nullptr)
		{
			str.append(std::to_string((int)(colors[1] * 255.0f)));
		}
		else
		{
			str.append(std::to_string((int)(dmxColor[1] * 255.0f)));
		}
		str.push_back(':');
		if (colors != nullptr)
		{
			str.append(std::to_string((int)(colors[2] * 255.0f)));
		}
		else
		{
			str.append(std::to_string((int)(dmxColor[2] * 255.0f)));
		}
		str.push_back(':');
		if (colors != nullptr)
		{
			str.append(std::to_string((int)(colors[3] * 255.0f)));
		}
		else
		{
			str.append(std::to_string((int)(dmxColor[3] * 255.0f)));
		}


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