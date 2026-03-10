#include "EventLogger.h"
#include <windows.h>

void logBlockedDevice(const std::string& deviceId, const std::string& deviceName)
{
    HANDLE hEventLog = RegisterEventSourceA(NULL, "usbshield");

    if (!hEventLog)
        return;

    std::string msg = "USBShield blocked device\n" + deviceId + "\n" + deviceName;

    LPCSTR strings[1];
    strings[0] = msg.c_str();

    ReportEventA(
        hEventLog,
        EVENTLOG_WARNING_TYPE,
        0,
        1001,
        NULL,
        1,
        0,
        strings,
        NULL
    );

    DeregisterEventSource(hEventLog);
}
