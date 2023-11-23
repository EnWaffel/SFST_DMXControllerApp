//#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#include "Application.h"
#include "Script.h"
#include <iostream>

int main()
{
	Application app;
	Application::INSTANCE = &app;
	app.Init();
	return 0;
}