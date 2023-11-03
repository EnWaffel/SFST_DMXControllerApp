#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#include "Application.h"

int main()
{
	Application app;
	app.Init();
	return 0;
}