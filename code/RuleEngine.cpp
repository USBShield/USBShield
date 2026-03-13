#include "RuleEngine.h"

#include <fstream>
#include <algorithm>

static std::string trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");

    if(start == std::string::npos)
        return "";

    return s.substr(start, end - start + 1);
}

std::vector<std::string> loadRules(const std::string& path)
{
    std::vector<std::string> rules;

    std::ifstream f(path);

    std::string line;

    while(std::getline(f,line))
    {
        size_t pos=line.find('#');

        if(pos!=std::string::npos)
            line=line.substr(0,pos);

        line=trim(line);

        if(line.empty())
            continue;

        rules.push_back(line);
    }

    return rules;
}

bool evaluateDevice(
    const UsbDevice& device,
    const std::vector<std::string>& rules)
{
    for(const auto& rule:rules)
    {
        if(rule=="allow input" && device.isInput)
            return true;

        if(rule.rfind("allow ",0)==0)
        {
            std::string id=rule.substr(6);

            if(device.instanceId.rfind(id,0)==0)
                return true;
        }
    }

    return false;
}

bool hasAllowConnected(const std::vector<std::string>& rules)
{
    for(const auto& r : rules)
        if(r == "allow connected")
            return true;

    return false;
}
