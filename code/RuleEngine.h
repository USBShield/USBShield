#pragma once

#include "UsbDevice.h"
#include <vector>
#include <string>

std::vector<std::string> loadRules(const std::string& path);

bool evaluateDevice(
    const UsbDevice& device,
    const std::vector<std::string>& rules
);

bool hasAllowConnected(const std::vector<std::string>& rules);
