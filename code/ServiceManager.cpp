#include "ServiceManager.h"
#include <windows.h>
#include <iostream>

extern void watch();

SERVICE_STATUS status{};
SERVICE_STATUS_HANDLE statusHandle;

bool serviceRunning = true;
bool runningAsService = false;

std::string getExePath()
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return std::string(path);
}

bool serviceExists()
{
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm)
        return false;

    SC_HANDLE service = OpenServiceA(scm, "USBShield", SERVICE_QUERY_STATUS);

    if(service)
    {
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return true;
    }

    CloseServiceHandle(scm);
    return false;
}

bool installService()
{
    if(serviceExists())
    {
        std::cout << "The USBShield service is already installed.\n";
        return false;
    }

    std::cout << "Installing USBShield service\n";

    SC_HANDLE scm = OpenSCManager(NULL,NULL,SC_MANAGER_CREATE_SERVICE);

    if(!scm)
    {
        std::cout << "Failed to open service manager\n";
        return false;
    }

    std::string exe = getExePath();
    std::string cmd = "\"" + exe + "\" service-run";

    SC_HANDLE service = CreateServiceA(
        scm,
        "USBShield",
        "USBShield",
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        cmd.c_str(),
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    );

    if(!service)
    {
        std::cout << "Failed to create service\n";
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_DELAYED_AUTO_START_INFO delayed{};
    delayed.fDelayedAutostart = TRUE;

    ChangeServiceConfig2(service,
                         SERVICE_CONFIG_DELAYED_AUTO_START_INFO,
                         &delayed);

    std::cout << "Starting USBShield service\n";
    StartService(service,0,nullptr);

    std::cout << "Done!\n";

    CloseServiceHandle(service);
    CloseServiceHandle(scm);

    return true;
}

bool uninstallService()
{
    if(!serviceExists())
    {
        std::cout << "USBShield service is not installed.\n";
        return false;
    }

    SC_HANDLE scm = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
    SC_HANDLE service = OpenServiceA(scm,"USBShield",SERVICE_ALL_ACCESS);

    std::cout << "Stopping USBShield service\n";

    SERVICE_STATUS_PROCESS ssp;
    DWORD bytesNeeded;

    ControlService(service,SERVICE_CONTROL_STOP,(LPSERVICE_STATUS)&ssp);

    while(QueryServiceStatusEx(
            service,
            SC_STATUS_PROCESS_INFO,
            (LPBYTE)&ssp,
            sizeof(SERVICE_STATUS_PROCESS),
            &bytesNeeded))
    {
        if(ssp.dwCurrentState == SERVICE_STOPPED)
            break;

        Sleep(500);
    }

    std::cout << "Removing USBShield service\n";

    DeleteService(service);

    std::cout << "Done!\n";

    CloseServiceHandle(service);
    CloseServiceHandle(scm);

    return true;
}

bool startService()
{
    if(!serviceExists())
    {
        std::cout << "USBShield service is not installed.\n";
        return false;
    }

    SC_HANDLE scm = OpenSCManager(NULL,NULL,SC_MANAGER_CONNECT);
    SC_HANDLE service = OpenServiceA(scm,"USBShield",SERVICE_START);

    if(!service)
    {
        std::cout << "Failed to open service.\n";
        CloseServiceHandle(scm);
        return false;
    }

    std::cout << "Starting USBShield service\n";

    if(!StartService(service,0,nullptr))
    {
        std::cout << "Failed to start service.\n";
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return false;
    }

    std::cout << "Done!\n";

    CloseServiceHandle(service);
    CloseServiceHandle(scm);

    return true;
}

bool stopService()
{
    if(!serviceExists())
    {
        std::cout << "USBShield service is not installed.\n";
        return false;
    }

    SC_HANDLE scm = OpenSCManager(NULL,NULL,SC_MANAGER_CONNECT);
    SC_HANDLE service = OpenServiceA(scm,"USBShield",SERVICE_STOP | SERVICE_QUERY_STATUS);

    if(!service)
    {
        std::cout << "Failed to open service.\n";
        CloseServiceHandle(scm);
        return false;
    }

    std::cout << "Stopping USBShield service\n";

    SERVICE_STATUS_PROCESS ssp;
    DWORD bytesNeeded;

    ControlService(service,SERVICE_CONTROL_STOP,(LPSERVICE_STATUS)&ssp);

    while(QueryServiceStatusEx(
        service,
        SC_STATUS_PROCESS_INFO,
        (LPBYTE)&ssp,
        sizeof(SERVICE_STATUS_PROCESS),
        &bytesNeeded))
    {
        if(ssp.dwCurrentState == SERVICE_STOPPED)
            break;

        Sleep(500);
    }

    std::cout << "Done!\n";

    CloseServiceHandle(service);
    CloseServiceHandle(scm);

    return true;
}

void WINAPI ServiceCtrlHandler(DWORD ctrl)
{
    if(ctrl == SERVICE_CONTROL_STOP)
    {
        status.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(statusHandle, &status);

        serviceRunning = false;
    }
}

void WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
    runningAsService = true;
    statusHandle = RegisterServiceCtrlHandlerA("USBShield", ServiceCtrlHandler);

    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = SERVICE_START_PENDING;
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    status.dwWin32ExitCode = 0;

    SetServiceStatus(statusHandle, &status);

    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);

    status.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(statusHandle, &status);

    watch();

    status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(statusHandle, &status);
}
