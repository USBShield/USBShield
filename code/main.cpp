#include "UsbDevice.h"
#include "RuleEngine.h"
#include "EventLogger.h"
#include "ServiceManager.h"

#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <map>
#include <set>

#include <windows.h>

#include <setupapi.h>
#include <cfgmgr32.h>
#include <initguid.h>

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

void disableDevice(const std::string& instanceId)
{
    HDEVINFO devInfo = SetupDiCreateDeviceInfoList(NULL, NULL);

    if (devInfo == INVALID_HANDLE_VALUE)
    {
        std::cout << "Failed to create device list\n";
        return;
    }

    SP_DEVINFO_DATA devData{};
    devData.cbSize = sizeof(devData);

    if (!SetupDiOpenDeviceInfoA(
            devInfo,
            instanceId.c_str(),
            NULL,
            0,
            &devData))
    {
        std::cout << "Failed to open device\n";
        SetupDiDestroyDeviceInfoList(devInfo);
        return;
    }

    // -------- SAFETY CHECKS --------

    // 1. Prevent disabling ROOT HUB
    if(instanceId.find("ROOT_HUB") != std::string::npos)
    {
        std::cout << "Not Disable: Root hub\n";
        SetupDiDestroyDeviceInfoList(devInfo);
        return;
    }

    // 2. Prevent disabling USB infrastructure classes
    char cls[256] = {0};
    if(SetupDiGetDeviceRegistryPropertyA(
            devInfo,
            &devData,
            SPDRP_CLASS,
            NULL,
            (PBYTE)cls,
            sizeof(cls),
            NULL))
    {
        std::string c = cls;

        if(c == "USBHub" || c == "HDC")
        {
            SetupDiDestroyDeviceInfoList(devInfo);
            std::cout << "Not Disable: USB infrastructure\n";
            return;
        }
    }

    // Disable Device

    SP_PROPCHANGE_PARAMS params{};
    params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
    params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;

    params.StateChange = DICS_DISABLE;
    params.Scope = DICS_FLAG_GLOBAL;
    params.HwProfile = 0;

    if (!SetupDiSetClassInstallParams(
            devInfo,
            &devData,
            (SP_CLASSINSTALL_HEADER*)&params,
            sizeof(params)))
    {
        std::cout << "Failed to set install parameters\n";
        SetupDiDestroyDeviceInfoList(devInfo);
        return;
    }

    if (!SetupDiCallClassInstaller(
            DIF_PROPERTYCHANGE,
            devInfo,
            &devData))
    {
        DWORD err = GetLastError();
        std::cout << "Disable failed. Error: " << err << "\n";
        SetupDiDestroyDeviceInfoList(devInfo);
        return;
    }

    CM_Reenumerate_DevNode(devData.DevInst, CM_REENUMERATE_SYNCHRONOUS);

    SetupDiDestroyDeviceInfoList(devInfo);
}

void showHelp() {
    std::cout << "USBShield\n";
    std::cout << "Usage:\n";
    std::cout << "  usbshield <command>\n\n";
    std::cout << "Commands:\n";
    std::cout << "  generate-rules      Generate a rule set based on currently connected USB devices\n";
    std::cout << "  list-devices        List currently connected USB devices\n";
    std::cout << "  watch               Monitor USB device events in real time\n";
    std::cout << "  service install     Install the USBShield service\n";
    std::cout << "  service uninstall   Uninstall the USBShield service\n";
    std::cout << "  service start       Start the USBShield service\n";
    std::cout << "  service stop        Stop the USBShield service\n";
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
    f << "allow input\n";
    f << "# allow currently connected devices\n";
    f << "allow connected\n\n";

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

    if(hasAllowConnected(rules))
    {
        for(auto& d : devices)
        {
            bool allowed = evaluateDevice(d, rules);

            if(!allowed)
            {
                std::cout
                << "Warning: Device is not in rules.conf but allowed anyway. "
                << d.instanceId << "\n";

                logAllowedConnected(d.instanceId, d.name);

                rules.push_back("allow " + d.instanceId);
            }
        }
    }

    for(auto& d : devices)
    {
        bool allow = evaluateDevice(d, rules);

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
        std::cout << "Administrator is needed\n";
        return;
    }

    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);

    auto rules = loadRules(getRulesPath().string());

    if(hasAllowConnected(rules))
    {
        auto devices = listUsbDevices();

        for(auto& d : devices)
        {
            bool allowed = evaluateDevice(d, rules);

            if(!allowed)
            {
                std::cout
                << "Warning: Device is not in rules.conf but allowed anyway. "
                << d.instanceId << "\n";

                logAllowedConnected(d.instanceId, d.name);

                rules.push_back("allow " + d.instanceId);
            }
        }
    }

    std::map<std::string,std::string> knownDevices;

    std::cout << "Watching...\n";

    extern bool serviceRunning;
    extern bool runningAsService;

    while(!runningAsService || serviceRunning)
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
    if (argc >= 2 && std::string(argv[1]) == "service-run")
    {
        SERVICE_TABLE_ENTRYA table[] =
        {
            { (LPSTR)"USBShield", ServiceMain },
            { NULL, NULL }
        };

        StartServiceCtrlDispatcherA(table);
        return 0;
    }

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
    else if(cmd == "service")
    {
        if(!isAdmin())
        {
            std::cout << "Administrator rights required.\n";
            return 0;
        }

        if(argc < 3)
        {
            std::cout << "usbshield service install|uninstall|start|stop\n";
            return 0;
        }

        std::string sub = argv[2];

        if(sub == "install")
            installService();
        else if(sub == "uninstall")
            uninstallService();
        else if(sub == "start")
            startService();
        else if(sub == "stop")
            stopService();
        else
            std::cout << "Unknown service command\n";
    }
    else
        showHelp();

    return 0;
}
