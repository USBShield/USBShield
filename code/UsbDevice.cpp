#include "UsbDevice.h"

#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>

#include <vector>
#include <string>

static const GUID GUID_DEVINTERFACE_USB_DEVICE =
{0xA5DCBF10,0x6530,0x11D2,{0x90,0x1F,0x00,0xC0,0x4F,0xB9,0x51,0xED}};

static bool subtreeHasInput(DEVINST node)
{
    DEVINST child;

    if (CM_Get_Child(&child,node,0)!=CR_SUCCESS)
        return false;

    while(true)
    {
        ULONG size=0;

        CM_Get_DevNode_Registry_PropertyA(
            child,
            CM_DRP_CLASS,
            NULL,
            NULL,
            &size,
            0
        );

        if(size)
        {
            std::vector<char> buffer(size);

            if(CM_Get_DevNode_Registry_PropertyA(
                child,
                CM_DRP_CLASS,
                NULL,
                buffer.data(),
                &size,
                0)==CR_SUCCESS)
            {
                std::string cls=buffer.data();

                if(cls=="Keyboard" || cls=="Mouse")
                    return true;
            }
        }

        if(subtreeHasInput(child))
            return true;

        DEVINST sibling;

        if(CM_Get_Sibling(&sibling,child,0)!=CR_SUCCESS)
            break;

        child=sibling;
    }

    return false;
}

std::vector<UsbDevice> listUsbDevices()
{
    std::vector<UsbDevice> devices;

    HDEVINFO devInfo=SetupDiGetClassDevs(
        &GUID_DEVINTERFACE_USB_DEVICE,
        NULL,
        NULL,
        DIGCF_PRESENT|DIGCF_DEVICEINTERFACE
    );

    if(devInfo==INVALID_HANDLE_VALUE)
        return devices;

    SP_DEVICE_INTERFACE_DATA iface;
    iface.cbSize=sizeof(iface);

    for(DWORD i=0;
        SetupDiEnumDeviceInterfaces(devInfo,NULL,&GUID_DEVINTERFACE_USB_DEVICE,i,&iface);
        i++)
    {
        DWORD size=0;

        SetupDiGetDeviceInterfaceDetailA(devInfo,&iface,NULL,0,&size,NULL);

        std::vector<char> buffer(size);

        SP_DEVICE_INTERFACE_DETAIL_DATA_A* detail=
        (SP_DEVICE_INTERFACE_DETAIL_DATA_A*)buffer.data();

        detail->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        SP_DEVINFO_DATA devData;
        devData.cbSize=sizeof(devData);

        if(!SetupDiGetDeviceInterfaceDetailA(devInfo,&iface,detail,size,NULL,&devData))
            continue;

        char instanceId[512];

        if(!SetupDiGetDeviceInstanceIdA(
            devInfo,
            &devData,
            instanceId,
            sizeof(instanceId),
            NULL))
            continue;

        std::string id=instanceId;

        if(id.rfind("USB\\VID_",0)!=0)
            continue;

        char name[512]={0};

        if(!SetupDiGetDeviceRegistryPropertyA(
            devInfo,
            &devData,
            SPDRP_FRIENDLYNAME,
            NULL,
            (PBYTE)name,
            sizeof(name),
            NULL))
        {
            SetupDiGetDeviceRegistryPropertyA(
                devInfo,
                &devData,
                SPDRP_DEVICEDESC,
                NULL,
                (PBYTE)name,
                sizeof(name),
                NULL);
        }

        bool isInput=subtreeHasInput(devData.DevInst);

        UsbDevice d;
        d.instanceId=id;
        d.name=name;
        d.isInput=isInput;

        if(isInput)
            d.name+=" [Input]";

        devices.push_back(d);
    }

    SetupDiDestroyDeviceInfoList(devInfo);

    return devices;
}
