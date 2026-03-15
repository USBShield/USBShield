#include <windows.h>
#include <shellapi.h>
#include <string>
#include <ctime>
#include <filesystem>
#include <vector>
#include <winevt.h>

#define ID_START 101
#define ID_STOP 102
#define ID_INSTALL 103
#define ID_UNINSTALL 104
#define ID_LIST 105
#define ID_EDITRULES 106
#define ID_TIMER 200
#define IDI_ICON1 1

HWND hwndMain;
HWND txtStatus;
HWND txtEvents;
HFONT hFont;
HWND btnStart;
HWND btnStop;
HWND btnInstall;
HWND btnUninstall;

DWORD lastEventRecordID = 0;
DWORD lastRecordID = 0;

bool IsAdmin()
{
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;

    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

    AllocateAndInitializeSid(
        &NtAuthority,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &adminGroup
    );

    CheckTokenMembership(NULL, adminGroup, &isAdmin);
    FreeSid(adminGroup);

    return isAdmin;
}

void RelaunchAsAdmin()
{
    char path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);

    ShellExecute(NULL, "runas", path, NULL, NULL, SW_SHOWNORMAL);
}

void AppendText(HWND box, const std::string& text)
{
    int len = GetWindowTextLength(box);

    SendMessage(box, EM_SETSEL, len, len);
    SendMessage(box, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());

    SendMessage(box, EM_SCROLLCARET, 0, 0);

    int lines = SendMessage(box, EM_GETLINECOUNT, 0, 0);

    if (lines > 100)
    {
        int index = SendMessage(box, EM_LINEINDEX, lines - 100, 0);

        SendMessage(box, EM_SETSEL, 0, index);
        SendMessage(box, EM_REPLACESEL, FALSE, (LPARAM)"");
    }
}

void LogStatus(const std::string& msg)
{
    time_t t = time(NULL);
    tm* now = localtime(&t);

    char buf[32];

    sprintf(buf, "[%02d:%02d:%02d] ",
        now->tm_hour,
        now->tm_min,
        now->tm_sec);

    std::string line = std::string(buf) + msg + "\r\n";

    AppendText(txtStatus, line);
}

bool ServiceExists()
{
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return false;

    SC_HANDLE svc = OpenService(scm, "USBShield", SERVICE_QUERY_STATUS);

    if (svc)
    {
        CloseServiceHandle(svc);
        CloseServiceHandle(scm);
        return true;
    }

    CloseServiceHandle(scm);
    return false;
}

bool ServiceRunning()
{
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return false;

    SC_HANDLE svc = OpenService(scm, "USBShield", SERVICE_QUERY_STATUS);

    if (!svc)
    {
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_STATUS_PROCESS ssp;
    DWORD needed;

    QueryServiceStatusEx(
        svc,
        SC_STATUS_PROCESS_INFO,
        (LPBYTE)&ssp,
        sizeof(ssp),
        &needed
    );

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);

    return ssp.dwCurrentState == SERVICE_RUNNING;
}

void UpdateButtons()
{
    if (!ServiceExists())
    {
        EnableWindow(btnStart, FALSE);
        EnableWindow(btnStop, FALSE);
        EnableWindow(btnInstall, TRUE);
        EnableWindow(btnUninstall, FALSE);

        SetWindowText(hwndMain, "USBShield: Not Installed");
        return;
    }

    EnableWindow(btnInstall, FALSE);
    EnableWindow(btnUninstall, TRUE);

    if (ServiceRunning())
    {
        EnableWindow(btnStart, FALSE);
        EnableWindow(btnStop, TRUE);

        SetWindowText(hwndMain, "USBShield: Running");
    }
    else
    {
        EnableWindow(btnStart, TRUE);
        EnableWindow(btnStop, FALSE);

        SetWindowText(hwndMain, "USBShield: Stopped");
    }
}

std::string ExecRead(const std::string& cmd)
{
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE readPipe, writePipe;

    CreatePipe(&readPipe, &writePipe, &sa, 0);

    STARTUPINFO si {};
    PROCESS_INFORMATION pi {};

    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;

    char buffer[512];
    strcpy(buffer, cmd.c_str());

    CreateProcess(
        NULL,
        buffer,
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );

    CloseHandle(writePipe);

    char out[4096];
    DWORD read;

    std::string result;

    while (ReadFile(readPipe, out, sizeof(out) - 1, &read, NULL))
    {
        if (!read) break;

        out[read] = 0;
        result += out;
    }

    CloseHandle(readPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return result;
}

void RunCommand(const std::string& cmd)
{
    char buffer[256];
    strcpy(buffer, cmd.c_str());

    STARTUPINFO si {};
    PROCESS_INFORMATION pi {};

    si.cb = sizeof(si);

    CreateProcess(
        NULL,
        buffer,
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

std::string ExtractEventMessage(const std::string& xml)
{
    std::string message;

    size_t pos = 0;

    while (true)
    {
        size_t start = xml.find("<Data>", pos);
        if (start == std::string::npos) break;

        size_t end = xml.find("</Data>", start);
        if (end == std::string::npos) break;

        start += 6;

        std::string part = xml.substr(start, end - start);

        size_t amp;
        while ((amp = part.find("&amp;")) != std::string::npos)
            part.replace(amp, 5, "&");

        message += part + " ";

        pos = end + 7;
    }

    return message;
}

std::string ExtractEventTime(const std::string& xml)
{
    size_t start = xml.find("SystemTime=");
    if (start == std::string::npos)
        return "[--:--:--] ";

    start += 11; // move past SystemTime=

    if (start >= xml.size())
        return "[--:--:--] ";

    char quote = xml[start];
    start++;

    size_t end = xml.find(quote, start);
    if (end == std::string::npos)
        return "[--:--:--] ";

    std::string time = xml.substr(start, end - start);

    size_t t = time.find("T");
    if (t == std::string::npos)
        return "[--:--:--] ";

    std::string hms = time.substr(t + 1, 8);

    return "[" + hms + "] ";
}

void RefreshEvents()
{
    EVT_HANDLE query = EvtQuery(
        NULL,
        L"Application",
        L"*[System[Provider[@Name='usbshield']]]",
        EvtQueryReverseDirection
    );

    if (!query)
        return;

    EVT_HANDLE events[10];
    DWORD returned = 0;

    if (!EvtNext(query, 10, events, 0, 0, &returned))
    {
        EvtClose(query);
        return;
    }

    for (DWORD i = 0; i < returned; i++)
    {
        DWORD bufferUsed = 0;
        DWORD propertyCount = 0;

        EvtRender(
            NULL,
            events[i],
            EvtRenderEventXml,
            0,
            NULL,
            &bufferUsed,
            &propertyCount
        );

        std::vector<wchar_t> buffer(bufferUsed / sizeof(wchar_t) + 1);

        if (EvtRender(
            NULL,
            events[i],
            EvtRenderEventXml,
            bufferUsed,
            &buffer[0],
            &bufferUsed,
            &propertyCount))
        {
            std::wstring xml(&buffer[0]);
            std::string msg(xml.begin(), xml.end());

            size_t idStart = msg.find("<EventRecordID>");
            size_t idEnd = msg.find("</EventRecordID>");

            if (idStart == std::string::npos || idEnd == std::string::npos)
            {
                EvtClose(events[i]);
                continue;
            }

            idStart += 15;

            DWORD recordID =
                atoi(msg.substr(idStart, idEnd - idStart).c_str());

            if (recordID <= lastEventRecordID)
            {
                EvtClose(events[i]);
                continue;
            }

            lastEventRecordID = recordID;

            std::string message = ExtractEventMessage(msg);

            if (!message.empty())
            {
                size_t pos = 0;
                while ((pos = message.find("USB\\", pos)) != std::string::npos)
                {
                    if (pos > 0 && message[pos - 1] != ' ')
                    {
                        message.insert(pos, " ");
                        pos++;
                    }
                    pos += 4;
                }

                std::string line =
                    ExtractEventTime(msg) + message + "\r\n";

                AppendText(txtEvents, line);
            }
        }

        EvtClose(events[i]);
    }

    EvtClose(query);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    {
        HWND control = (HWND)lParam;

        if (control == txtStatus || control == txtEvents)
        {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(255, 255, 255));
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        }

        break;
    }
    case WM_CREATE:
    {
        hwndMain = hwnd;
        if (!IsAdmin())
        {
            RelaunchAsAdmin();
            ExitProcess(0);
        }
        hFont = CreateFont(
            -16, 0, 0, 0,
            FW_NORMAL,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            "Segoe UI"
        );
        if (!std::filesystem::exists("usbshield.exe"))
        {
            MessageBox(hwnd,
                "usbshield.exe not found in this folder",
                "USBShield GUI",
                MB_ICONERROR);
            PostQuitMessage(0);
        }
        if (!std::filesystem::exists("rules.conf"))
        {
            LogStatus("rules.conf not found, generating rules...");
            RunCommand("usbshield.exe generate-rules");
        }

        HWND lblService = CreateWindow("STATIC", "Service Control",
            WS_VISIBLE | WS_CHILD,
            20, 20, 200, 20,
            hwnd, NULL, NULL, NULL);
        SendMessage(lblService, WM_SETFONT, (WPARAM)hFont, TRUE);

        btnStart = CreateWindow("BUTTON", "Start",
            WS_VISIBLE | WS_CHILD,
            20, 50, 80, 30,
            hwnd, (HMENU)ID_START, NULL, NULL);
        SendMessage(btnStart, WM_SETFONT, (WPARAM)hFont, TRUE);

        btnStop = CreateWindow("BUTTON", "Stop",
            WS_VISIBLE | WS_CHILD,
            110, 50, 80, 30,
            hwnd, (HMENU)ID_STOP, NULL, NULL);
        SendMessage(btnStop, WM_SETFONT, (WPARAM)hFont, TRUE);

        btnInstall = CreateWindow("BUTTON", "Install",
            WS_VISIBLE | WS_CHILD,
            200, 50, 80, 30,
            hwnd, (HMENU)ID_INSTALL, NULL, NULL);
        SendMessage(btnInstall, WM_SETFONT, (WPARAM)hFont, TRUE);

        btnUninstall = CreateWindow("BUTTON", "Uninstall",
            WS_VISIBLE | WS_CHILD,
            290, 50, 80, 30,
            hwnd, (HMENU)ID_UNINSTALL, NULL, NULL);
        SendMessage(btnUninstall, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND btnListDev = CreateWindow("BUTTON", "List devices",
            WS_VISIBLE | WS_CHILD,
            380, 50, 110, 30,
            hwnd, (HMENU)ID_LIST, NULL, NULL);
        SendMessage(btnListDev, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND btnEditRules = CreateWindow("BUTTON", "Edit rules",
            WS_VISIBLE | WS_CHILD,
            500, 50, 120, 30,
            hwnd, (HMENU)ID_EDITRULES, NULL, NULL);
        SendMessage(btnEditRules, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND lblStatus = CreateWindow("STATIC", "Status",
            WS_VISIBLE | WS_CHILD,
            20, 100, 200, 20,
            hwnd, NULL, NULL, NULL);
        SendMessage(lblStatus, WM_SETFONT, (WPARAM)hFont, TRUE);

        txtStatus = CreateWindowEx(
            WS_EX_CLIENTEDGE, "EDIT", "",
            WS_VISIBLE | WS_CHILD |
            ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
            20, 120, 620, 120,
            hwnd, NULL, NULL, NULL);
        SendMessage(txtStatus, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND lblEvent = CreateWindow("STATIC", "Event Viewer",
            WS_VISIBLE | WS_CHILD,
            20, 260, 200, 20,
            hwnd, NULL, NULL, NULL);
        SendMessage(lblEvent, WM_SETFONT, (WPARAM)hFont, TRUE);

        txtEvents = CreateWindowEx(
            WS_EX_CLIENTEDGE, "EDIT", "",
            WS_VISIBLE | WS_CHILD |
            ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
            20, 280, 620, 160,
            hwnd, NULL, NULL, NULL);
        SendMessage(txtEvents, WM_SETFONT, (WPARAM)hFont, TRUE);

        UpdateButtons();
        RefreshEvents();
        SetTimer(hwnd, ID_TIMER, 5000, NULL);
    }
    break;

    case WM_TIMER:
    {
        RefreshEvents();
        UpdateButtons();
    }
    break;

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case ID_START:
        {
            if (!ServiceExists())
            {
                LogStatus("Service is not installed!");
                break;
            }

            if (ServiceRunning())
            {
                MessageBox(hwnd, "Service already running", "USBShield", MB_OK);
            }
            else
            {
                EnableWindow(btnStart, FALSE);
                EnableWindow(btnStop, FALSE);

                RunCommand("usbshield.exe service start");
                LogStatus("Service started");
            }
        }
        break;
        case ID_STOP:
        {
            if (!ServiceExists())
            {
                LogStatus("Service is not installed!");
                break;
            }

            if (!ServiceRunning())
            {
                MessageBox(hwnd, "Service already stopped", "USBShield", MB_OK);
            }
            else
            {
                EnableWindow(btnStart, FALSE);
                EnableWindow(btnStop, FALSE);

                RunCommand("usbshield.exe service stop");
                LogStatus("Service stopped");
            }
        }
        break;
        case ID_INSTALL:
        {
            if (ServiceExists())
            {
                LogStatus("Service already installed");
                break;
            }

            RunCommand("usbshield.exe service install");
            LogStatus("Service installed");

            UpdateButtons();
        }
        break;
        case ID_UNINSTALL:
        {
            if (!ServiceExists())
            {
                LogStatus("Service not installed");
                break;
            }

            RunCommand("usbshield.exe service uninstall");
            LogStatus("Service uninstalled");

            UpdateButtons();
        }
        break;
        case ID_LIST:
        {
            SetWindowText(txtStatus, "");
            if (!std::filesystem::exists("rules.conf"))
            {
               LogStatus("rules.conf not found, generating rules...");
               ExecRead("usbshield.exe generate-rules");
            }
            std::string output =
                ExecRead("usbshield.exe list-devices");

            AppendText(txtStatus, output);
        }
        break;
        case ID_EDITRULES:
        {
            if (!std::filesystem::exists("rules.conf"))
            {
                MessageBox(hwnd,
                    "rules.conf not found",
                    "USBShield",
                    MB_ICONERROR);
                break;
            }
            ShellExecute(
                NULL,
                "open",
                "notepad.exe",
                "rules.conf",
                NULL,
                SW_SHOWNORMAL
            );
        }
        break;
        }
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE,
                   LPSTR,
                   int nCmdShow)
{
    const char CLASS_NAME[] = "USBShieldGUI";

    WNDCLASSEX wc {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_TOOLWINDOW,
        CLASS_NAME,
        "USBShield GUI",
        WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        680,
        500,
        NULL,
        NULL,
        hInstance,
        NULL
    );
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    HICON icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));

    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);

    ShowWindow(hwnd, nCmdShow);

    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
