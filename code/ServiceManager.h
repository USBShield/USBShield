#pragma once
#include <windows.h>

bool installService();
bool uninstallService();
bool serviceExists();

bool startService();
bool stopService();

void WINAPI ServiceMain(DWORD argc, LPTSTR *argv);

extern bool serviceRunning;
extern bool runningAsService;
