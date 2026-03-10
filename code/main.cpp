#include "UsbDevice.h"
#include "RuleEngine.h"
#include "EventLogger.h"

#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <map>
#include <set>

#include <windows.h>
#include <shellapi.h>

std::filesystem::path getExeDirectory()
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);

    std::filesystem::path exePath(path);
    return exePath.parent_path();
}

std::filesystem::path getRulesPath()
{
    return getExeDirectory() / "rules.conf";
}

std::filesystem::path getUSBDeviewPath()
{
    return getExeDirectory() / "USBDeview.exe";
}

bool usbDeviewExists()
{
    return std::filesystem::exists(getUSBDeviewPath());
}

void disableDevice(const std::string& instanceId)
{
    std::string exe = getUSBDeviewPath().string();

    std::string cmd =
        "\"" + exe + "\" /disable \"" +
        instanceId + "\"";

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    CreateProcessA(
        NULL,
        cmd.data(),
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void showHelp()
{
    std::cout << "usbshield generate-rules\n";
    std::cout << "usbshield list-devices\n";
    std::cout << "usbshield watch\n";
}

void generateRule()
{
    auto rulesPath = getRulesPath();

    if(std::filesystem::exists(rulesPath))
    {
        std::cout << "rules.conf already exists.\n";
        std::cout << "Do you want to override it? [y/n] ";

        char answer;
        std::cin >> answer;

        if(answer != 'y' && answer != 'Y')
        {
            std::cout << "Operation cancelled.\n";
            return;
        }
    }

    std::ofstream f(rulesPath);

    if(!f)
    {
        std::cout << "Failed to create rules.conf\n";
        return;
    }

    f << "# permit input devices\n";
    f << "allow input\n\n";

    auto devices = listUsbDevices();

    for(auto& d : devices)
    {
        f << "allow " << d.instanceId
          << " # " << d.name << "\n";
    }

    std::cout << "rules.conf generated\n";
}

void listDevices()
{
    auto rules = loadRules(getRulesPath().string());
    auto devices = listUsbDevices();

    for(auto& d : devices)
    {
        bool allow = evaluateDevice(d,rules);

        std::cout
        << (allow ? "[Allow] " : "[Deny] ")
        << d.instanceId << "  "
        << d.name << "\n";
    }
}

bool isAdmin()
{
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;

    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(
            &NtAuthority,
            2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0,0,0,0,0,0,
            &adminGroup))
    {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin;
}

void watch()
{
    if(!isAdmin())
    {
        std::cout << "admin is needed\n";
        return;
    }

    if(!usbDeviewExists())
    {
        std::cout << "USBDeview.exe is not found!\n";
        return;
    }

    auto rules = loadRules(getRulesPath().string());

    std::map<std::string,std::string> knownDevices;
    std::cout << "Watching...\n";

    while(true)
    {
        auto devices = listUsbDevices();

        std::set<std::string> currentIds;

        for(auto& d : devices)
            currentIds.insert(d.instanceId);

        // detect removed devices
        for(auto it = knownDevices.begin(); it != knownDevices.end(); )
        {
            if(currentIds.find(it->first) == currentIds.end())
            {
                std::cout
                << "removed device: "
                << it->first << " "
                << it->second << "\n";

                it = knownDevices.erase(it);
            }
            else
                ++it;
        }

        // detect new devices
        for(auto& d : devices)
        {
            if(knownDevices.find(d.instanceId) == knownDevices.end())
            {
                knownDevices[d.instanceId] = d.name;

                bool allow = evaluateDevice(d,rules);

                if(!allow)
                {
                    std::cout
                    << "disable device: "
                    << d.instanceId << " "
                    << d.name << "\n";

                    disableDevice(d.instanceId);
                    logBlockedDevice(d.instanceId, d.name);
                }
            }
        }

        std::this_thread::sleep_for(
            std::chrono::seconds(2));
    }
}

int main(int argc,char* argv[])
{
    if(argc < 2)
    {
        showHelp();
        return 0;
    }

    std::string cmd = argv[1];

    if(cmd == "generate-rules")
        generateRule();
    else if(cmd == "list-devices")
        listDevices();
    else if(cmd == "watch")
        watch();
    else
        showHelp();

    return 0;
}
