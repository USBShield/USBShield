#pragma once
#include <string>
#include <vector>

struct UsbDevice
{
    std::string instanceId;
    std::string name;
    bool isInput = false;
};

std::vector<UsbDevice> listUsbDevices();
