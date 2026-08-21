#include "RKScan.h"

int CRKScan::GetDEVICE_COUNTS()
{
	return m_list.size();
}


uint_32 CRKScan::GetMSC_TIMEOUT()
{
	return m_waitMscSecond;
}
uint_32 CRKScan::GetRKUSB_TIMEOUT()
{
	return m_waitRKusbSecond;
}


void CRKScan::SetMSC_TIMEOUT(uint_32 value)
{
	m_waitMscSecond = value;
}

void CRKScan::SetRKUSB_TIMEOUT(uint_32 value)
{
	m_waitRKusbSecond = value;
}


CRKScan::CRKScan(uint_32 uiMscTimeout,uint_32 uiRKusbTimeout)
{
	DEVICE_COUNTS.setContainer(this);
    	DEVICE_COUNTS.getter(&CRKScan::GetDEVICE_COUNTS);
 	
 	MSC_TIMEOUT.setContainer(this);
    	MSC_TIMEOUT.getter(&CRKScan::GetMSC_TIMEOUT);
 	MSC_TIMEOUT.setter(&CRKScan::SetMSC_TIMEOUT);
	
	RKUSB_TIMEOUT.setContainer(this);
   	 RKUSB_TIMEOUT.getter(&CRKScan::GetRKUSB_TIMEOUT);
 	RKUSB_TIMEOUT.setter(&CRKScan::SetRKUSB_TIMEOUT);
	
	m_waitMscSecond = uiMscTimeout;
	m_waitRKusbSecond = uiRKusbTimeout;
	m_log = NULL;
	m_list.clear();
	m_deviceConfigSet.clear();
	m_deviceMscConfigSet.clear();
	m_vecLocalHub.clear();
}
bool CRKScan::FindRockusbVidPid(ENUM_RKDEVICE_TYPE type,uint_16 &usVid,uint_16 &usPid)
{
	uint_32 i;
	bool bRet=false;
	for (i=0;i<m_deviceConfigSet.size();i++)
	{
		if (m_deviceConfigSet[i].emDeviceType==type)
		{
			usVid = m_deviceConfigSet[i].usVid;
			usPid = m_deviceConfigSet[i].usPid;
			bRet = true;
			break;
		}
	}
	return bRet;
}
void CRKScan::AddRockusbVidPid(uint_16 newVid,uint_16 newPid,uint_16 oldVid,uint_16 oldPid)
{
	if ((newVid==0)||(newPid==0)||(oldVid==0)||(oldPid==0))
	{
		return;
	}
	STRUCT_DEVICE_CONFIG config;
	int i;
	for (i=0;i<m_deviceConfigSet.size();i++)
	{
		if ((m_deviceConfigSet[i].usVid==oldVid)&&(m_deviceConfigSet[i].usPid==oldPid))
		{
			config.usVid = newVid;
			config.usPid = newPid;
			config.emDeviceType = m_deviceConfigSet[i].emDeviceType;
			break;
		}
	}
	if (i<m_deviceConfigSet.size())
		m_deviceConfigSet.push_back(config);
}

void CRKScan::SetVidPid(uint_16 mscVid,uint_16 mscPid)
{
	STRUCT_DEVICE_CONFIG config;
	m_deviceConfigSet.clear();
	
	config.emDeviceType = RK27_DEVICE;
	config.usPid = 0x3201;
	config.usVid = 0x071B;
	m_deviceConfigSet.push_back(config);
	
	config.emDeviceType = RK28_DEVICE;
	config.usPid = 0x3228;
	config.usVid = 0x071B;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RKNANO_DEVICE;
	config.usPid = 0x3226;
	config.usVid = 0x071B;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RKCROWN_DEVICE;
	config.usPid = 0x261A;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RK281X_DEVICE;
	config.usPid = 0x281A;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RKCAYMAN_DEVICE;
	config.usPid = 0x273A;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RK29_DEVICE;
	config.usPid = 0x290A;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RKPANDA_DEVICE;
	config.usPid = 0x282B;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RKSMART_DEVICE;
	config.usPid = 0x262C;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RK292X_DEVICE;
	config.usPid = 0x292A;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RK30_DEVICE;
	config.usPid = 0x300A;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RK30B_DEVICE;
	config.usPid = 0x300B;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RK31_DEVICE;
	config.usPid = 0x310B;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RK32_DEVICE;
	config.usPid = 0x320A;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);


	m_deviceMscConfigSet.clear();

	config.emDeviceType = RKNONE_DEVICE;
	config.usPid = 0x3203;
	config.usVid = 0x071B;
	m_deviceMscConfigSet.push_back(config);

	config.emDeviceType = RKNONE_DEVICE;
	config.usPid = 0x3205;
	config.usVid = 0x071B;
	m_deviceMscConfigSet.push_back(config);

	config.emDeviceType = RKNONE_DEVICE;
	config.usPid = 0x2910;
	config.usVid = 0x0BB4;
	m_deviceMscConfigSet.push_back(config);

	config.emDeviceType = RKNONE_DEVICE;
	config.usPid = 0x0000;
	config.usVid = 0x2207;
	m_deviceMscConfigSet.push_back(config);

	config.emDeviceType = RKNONE_DEVICE;
	config.usPid = 0x0010;
	config.usVid = 0x2207;
	m_deviceMscConfigSet.push_back(config);

	config.emDeviceType = RKNONE_DEVICE;
	config.usPid = 0x0016;
	config.usVid = 0x2207;
	m_deviceMscConfigSet.push_back(config);

	if ((mscVid!=0)||(mscPid!=0))
	{
		if (FindConfigSetPos(m_deviceMscConfigSet,mscVid,mscPid)==-1)
		{
			config.emDeviceType = RKNONE_DEVICE;
			config.usPid = mscPid;
			config.usVid = mscVid;
			m_deviceMscConfigSet.push_back(config);
		}
	}
}
int CRKScan::FindWaitSetPos(const RKDEVICE_CONFIG_SET &waitDeviceSet,uint_16 vid,uint_16 pid)
{
	int pos=-1;
	uint_32 i;
	for ( i=0;i<waitDeviceSet.size();i++ )
	{
		if ( (vid==waitDeviceSet[i].usVid)&&(pid==waitDeviceSet[i].usPid) )
		{
			pos = i;
			break;
		}
	}
	return pos;
}
int CRKScan::FindConfigSetPos(RKDEVICE_CONFIG_SET &devConfigSet,uint_16 vid,uint_16 pid)
{
	int pos=-1;
	uint_32 i;
	for ( i=0;i<devConfigSet.size();i++ )
	{
		if ( (vid==devConfigSet[i].usVid)&&(pid==devConfigSet[i].usPid) )
		{
			pos = i;
			break;
		}
	}
	return pos;
}
bool CRKScan::InitLocalHub() 
{
	BOOL bRet;
	HDEVINFO                         deviceInfo;
	SP_DEVICE_INTERFACE_DATA         deviceInfoData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA_A deviceDetailData;
	DWORD                            index;
	DWORD                            requiredLength;
	string strHostPath,strHubName;
	STRUCT_HUB_INFO	hubInfo;
	uint_8 hubPort;
	m_vecLocalHub.clear();
	deviceInfo = SetupDiGetClassDevsA((LPGUID)&GUID_DEVINTERFACE_USB_HOST_CONTROLLER,
		NULL,
		NULL,
		(DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
	if (deviceInfo==INVALID_HANDLE_VALUE)
	{
		if (m_log)
		{
			m_log->Record("Error:InitLocalHub-->SetupDiGetClassDevs failed,err=%d",GetLastError());
		}
		return false;
	}
	
    deviceInfoData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	
    for (index=0;
		SetupDiEnumDeviceInterfaces(deviceInfo,
		0,
		(LPGUID)&GUID_DEVINTERFACE_USB_HOST_CONTROLLER,
		index,
		&deviceInfoData);
	index++)
    {
		requiredLength = 0;
        SetupDiGetDeviceInterfaceDetailA(deviceInfo,
			&deviceInfoData,
			NULL,
			0,
			&requiredLength,
			NULL);
		if (requiredLength<=0)
		{
			if (m_log)
			{
				m_log->Record("Error:InitLocalHub-->SetupDiGetDeviceInterfaceDetail failed,err=%d",GetLastError());
			}
			continue;
		}
        deviceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)GlobalAlloc(GPTR, requiredLength);
		
        deviceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
		
        bRet = SetupDiGetDeviceInterfaceDetailA(deviceInfo,
			&deviceInfoData,
			deviceDetailData,
			requiredLength,
			&requiredLength,
			NULL);
		if (!bRet)
		{
			GlobalFree(deviceDetailData);
			if (m_log)
			{
				m_log->Record("Error:InitLocalHub-->SetupDiGetDeviceInterfaceDetail failed,err=%d",GetLastError());
			}
			continue;
		}
		strHostPath = deviceDetailData->DevicePath;
		GlobalFree(deviceDetailData);
		bRet = GetHostHubName(strHostPath,strHubName);
		if (!bRet)
		{
			if (m_log)
			{
				m_log->Record("Error:InitLocalHub-->GetHostHubName failed");
			}
			continue;
		}
		hubInfo.strHubPath = strHubName;
		hubInfo.strHubLayer = std::to_string(index + 1);
		bRet = GetHubPortNum(strHubName,hubPort);
		if (!bRet)
		{
			if (m_log)
			{
				m_log->Record("Error:InitLocalHub-->GetHubPortNum failed");
			}
			continue;
		}
		hubInfo.nPort = hubPort;
		m_vecLocalHub.push_back(hubInfo);
	}
    SetupDiDestroyDeviceInfoList(deviceInfo);
	return true;
}
BOOL CRKScan::GetHostHubName(string strHost,string &strHub)
{
	BOOL bRet,bSuccess=FALSE;
	HANDLE hHCDev = INVALID_HANDLE_VALUE;
	USB_ROOT_HUB_NAME  rootHubName;
	PUSB_ROOT_HUB_NAME rootHubNameW = NULL;
	char *szHubName;
	DWORD              nBytes;
	ZeroMemory(&rootHubName,sizeof(rootHubName));
	hHCDev = CreateFileA(strHost.c_str(),
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	if (hHCDev==INVALID_HANDLE_VALUE)
	{
		if (m_log)
		{
			m_log->Record("Error:GetHostHubName-->CreateFile failed,err=%d",GetLastError());
		}
		return FALSE;
	}
	bRet = DeviceIoControl(hHCDev,
		IOCTL_USB_GET_ROOT_HUB_NAME,
		0,
		0,
		&rootHubName,
		sizeof(rootHubName),
		&nBytes,
		NULL);
	if (!bRet)
	{
		if (m_log)
		{
			m_log->Record("Error:GetHostHubName-->Get root hub name failed,err=%d",GetLastError());
		}
		goto Exit_GetHubName;
	}
	nBytes = rootHubName.ActualLength;
	
    rootHubNameW = (PUSB_ROOT_HUB_NAME)GlobalAlloc(GPTR, nBytes);
	bRet = DeviceIoControl(hHCDev,
		IOCTL_USB_GET_ROOT_HUB_NAME,
		0,
		0,
		rootHubNameW,
		nBytes,
		&nBytes,
		NULL);
	
    if (!bRet)
    {
        goto Exit_GetHubName;
    }
	szHubName = WideStrToMultiStr(rootHubNameW->RootHubName);
	if (!szHubName)
		goto Exit_GetHubName;
	strHub = szHubName;
	GlobalFree(szHubName);
	bSuccess = TRUE;
Exit_GetHubName:
	if (hHCDev!=INVALID_HANDLE_VALUE)
		CloseHandle(hHCDev);
	if (rootHubNameW)
		GlobalFree(rootHubNameW);
	return bSuccess;
}
BOOL CRKScan::GetHubPortNum(string strHub, uint_8 &nPort)
{
	string strDeviceName;
	int nTryCount=3;
	BOOL bRet;
	HANDLE hHub=INVALID_HANDLE_VALUE;
	DWORD nBytes;
	PUSB_NODE_INFORMATION HubInfo = NULL;
	// Create the full hub device name
	strDeviceName = "\\\\.\\" + strHub;
	
	// Try to hub the open device
	while(nTryCount>0)
	{
		hHub = CreateFileA(strDeviceName.c_str(),
			GENERIC_WRITE,
			FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);
		
		// Done with temp buffer for full hub device name
		if (hHub == INVALID_HANDLE_VALUE)
		{
			if (m_log)
			{
				m_log->Record("Error:GetHubPortNum-->CreateFile failed,err=%d",GetLastError());
			}
			nTryCount--;
			sleep_ms(500);
		}
		else
			break;
	}
	if (nTryCount<=0)
	{
		return FALSE;
	}
	HubInfo = (PUSB_NODE_INFORMATION)GlobalAlloc(GPTR,sizeof(USB_NODE_INFORMATION));
	if ( HubInfo==NULL )
	{
		if (m_log)
		{
			m_log->Record("Error:EnumerateHub-->alloc USB_NODE_INFORMATION failed,err=%d",GetLastError());
		}
		CloseHandle(hHub);
		return FALSE;
	}
	
	bRet = DeviceIoControl(hHub,
		IOCTL_USB_GET_NODE_INFORMATION,
		HubInfo,
		sizeof(USB_NODE_INFORMATION),
		HubInfo,
		sizeof(USB_NODE_INFORMATION),
		&nBytes,
		NULL);
	
	if (!bRet)
	{
		if (m_log)
		{
			m_log->Record("Error:EnumerateHub-->DeviceIoControl failed,err=%d",GetLastError());
		}
		GlobalFree(HubInfo);
		CloseHandle(hHub);
		return FALSE;
	}
	nPort = HubInfo->u.HubInformation.HubDescriptor.bNumberOfPorts;
	GlobalFree(HubInfo);
	CloseHandle(hHub);
	return TRUE;
}
char* CRKScan::WideStrToMultiStr(wchar_t *WideStr)
{
	DWORD nBytes;
    char* MultiStr;
	
    // Get the length of the converted string
    nBytes = WideCharToMultiByte(
		CP_ACP,
		0,
		WideStr,
		-1,
		NULL,
		0,
		NULL,
		NULL);
	
    if (nBytes == 0)
    {
        return NULL;
    }
	
    // Allocate space to hold the converted string
	
    MultiStr = (char *)GlobalAlloc(GPTR,nBytes);
	
    if (MultiStr == NULL)
    {
        return NULL;
    }
	
    // Convert the string
	
    nBytes = WideCharToMultiByte(
		CP_ACP,
		0,
		WideStr,
		-1,
		MultiStr,
		nBytes,
		NULL,
		NULL);
	
    if (nBytes == 0)
    {
        GlobalFree(MultiStr);
        return NULL;
    }
	
    return MultiStr;
}
BOOL CRKScan::GetExHubName (HANDLE hHub,DWORD nConnIndex, string &strName)
{
	BOOL                        bSuccess=FALSE,bRet;
    DWORD                       nBytes;
    USB_NODE_CONNECTION_NAME    extHubName;
    PUSB_NODE_CONNECTION_NAME   extHubNameW=NULL;
    char                       *extHubNameA=NULL;
	
    // Get the length of the name of the external hub attached to the
    // specified port.
    //
	ZeroMemory(&extHubName,sizeof(extHubName));
    extHubName.ConnectionIndex = nConnIndex;
	
    bRet = DeviceIoControl(hHub,
		IOCTL_USB_GET_NODE_CONNECTION_NAME,
		&extHubName,
		sizeof(extHubName),
		&extHubName,
		sizeof(extHubName),
		&nBytes,
		NULL);
	
    if (!bRet)
    {
        goto GetExternalHubNameError;
    }
	
    // Allocate space to hold the external hub name
	
    nBytes = extHubName.ActualLength;
	
    if (nBytes <= sizeof(extHubName))
    {
        goto GetExternalHubNameError;
    }
	
    extHubNameW = (PUSB_NODE_CONNECTION_NAME)GlobalAlloc(GPTR,nBytes);
	
    if (extHubNameW == NULL)
    {
        goto GetExternalHubNameError;
    }
	
    // Get the name of the external hub attached to the specified port
	
    extHubNameW->ConnectionIndex = nConnIndex;
	
    bRet = DeviceIoControl(hHub,
		IOCTL_USB_GET_NODE_CONNECTION_NAME,
		extHubNameW,
		nBytes,
		extHubNameW,
		nBytes,
		&nBytes,
		NULL);
	
    if (!bRet)
    {
        goto GetExternalHubNameError;
    }
	extHubNameA = WideStrToMultiStr(extHubNameW->NodeName);
	if (extHubNameA) {
		strName = extHubNameA;
		GlobalFree(extHubNameA);
		bSuccess = TRUE;
	}

GetExternalHubNameError:
    // There was an error, free anything that was allocated
    
    if (extHubNameW != NULL)
    {
        GlobalFree(extHubNameW);
        extHubNameW = NULL;
    }
    return bSuccess;
}
BOOL CRKScan::GetDriverName (HANDLE Hub,DWORD nConnIndex,string &strName)
{
	BOOL                                bSuccess=FALSE,bRet;
    DWORD                               nBytes;
    USB_NODE_CONNECTION_DRIVERKEY_NAME  driverKeyName;
    PUSB_NODE_CONNECTION_DRIVERKEY_NAME driverKeyNameW=NULL;
    char                               *driverKeyNameA=NULL;
	
	
    // Get the length of the name of the driver key of the device attached to
    // the specified port.
    //
	ZeroMemory(&driverKeyName,sizeof(driverKeyName));
    driverKeyName.ConnectionIndex = nConnIndex;
	
    bRet = DeviceIoControl(Hub,
		IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
		&driverKeyName,
		sizeof(driverKeyName),
		&driverKeyName,
		sizeof(driverKeyName),
		&nBytes,
		NULL);
	
    if (!bRet)
    {
        goto GetDriverKeyNameError;
    }
	
    // Allocate space to hold the driver key name
	
    nBytes = driverKeyName.ActualLength;
	
    if (nBytes <= sizeof(driverKeyName))
    {
        goto GetDriverKeyNameError;
    }
	
    driverKeyNameW = (PUSB_NODE_CONNECTION_DRIVERKEY_NAME)GlobalAlloc(GPTR,nBytes);
	
    if (driverKeyNameW == NULL)
    {
        goto GetDriverKeyNameError;
    }
	
    // Get the name of the driver key of the device attached to
    // the specified port.
	
    driverKeyNameW->ConnectionIndex = nConnIndex;
	
    bRet = DeviceIoControl(Hub,
		IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
		driverKeyNameW,
		nBytes,
		driverKeyNameW,
		nBytes,
		&nBytes,
		NULL);
	
    if (!bRet)
    {
        goto GetDriverKeyNameError;
    }
	driverKeyNameA = WideStrToMultiStr(driverKeyNameW->DriverKeyName);
	if (driverKeyNameA) {
		strName = driverKeyNameA;
		GlobalFree(driverKeyNameA);
		driverKeyNameA = NULL;
		bSuccess = TRUE;
	}
	
GetDriverKeyNameError:
    // There was an error, free anything that was allocated
	
    if (driverKeyNameW != NULL)
    {
        GlobalFree(driverKeyNameW);
        driverKeyNameW = NULL;
    }
    return bSuccess;
}
void CRKScan::GetDeviceInstance(string strDriverName, DEVINST &inst)
{
	DEVINST     devInst;
    DEVINST     devInstNext;
    CONFIGRET   cr;
    ULONG       walkDone = 0;
    ULONG       len;
	char buf[512];
    // Get Root DevNode
    //
    cr = CM_Locate_DevNode(&devInst,NULL,CM_LOCATE_DEVNODE_NORMAL); 
    if (cr != CR_SUCCESS)
    {
        return ;
    }
	
    // Do a depth first search for the DevNode with a matching
    // DriverName value
    //
    while (!walkDone)
    {
        // Get the DriverName value
        len = sizeof(buf);
        cr = CM_Get_DevNode_Registry_PropertyA(devInst,CM_DRP_DRIVER,NULL,buf,&len,0);
		
        // If the DriverName value matches, return the DeviceDescription
        if ( (cr==CR_SUCCESS)&&(stricmp(strDriverName.c_str(), buf)==0) )
        {
			inst = devInst;
			return ;
        }
		
        // This DevNode didn't match, go down a level to the first child.
        cr = CM_Get_Child(&devInstNext,devInst,0);
        if (cr == CR_SUCCESS)
        {
            devInst = devInstNext;
            continue;
        }
		
        // Can't go down any further, go across to the next sibling.  If
        // there are no more siblings, go back up until there is a sibling.
        // If we can't go up any further, we're back at the root and we're
        // done.
        //
        for (;;)
        {
            cr = CM_Get_Sibling(&devInstNext,devInst,0);
            if (cr == CR_SUCCESS)
            {
                devInst = devInstNext;
                break;
            }
            cr = CM_Get_Parent(&devInstNext,devInst,0);
            if ( cr==CR_SUCCESS )
            {
                devInst = devInstNext;
            }
            else
            {
                walkDone = 1;
                break;
            }
        }
    }
}
void CRKScan::EnumerateHubPorts(STRUCT_HUB_INFO &hubInfo)
{
	int  index;
    BOOL bSuccess=FALSE;
	uint_8 nPort;
	DWORD nBytes;
	string strHubPath,strLayer,strDriverName,strLinkName,strSerial;
	HANDLE hHubDevice = INVALID_HANDLE_VALUE;
	PUSB_DESCRIPTOR_REQUEST usb_desc_req = NULL;
	PUSB_STRING_DESCRIPTOR usb_serial_desc = NULL;
    PUSB_NODE_CONNECTION_INFORMATION_EX    connectionInfo=NULL;
	PUSB_NODE_CONNECTION_INFORMATION_EX_V2 connectionInfoExV2=NULL;
	STRUCT_HUB_INFO exHubInfo;
	DEVINST devInstance;
	char szBuf[512];

	strHubPath = "\\\\.\\" + hubInfo.strHubPath;
	
	// Try to hub the open device
	hHubDevice = CreateFileA(strHubPath.c_str(),
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	
	// Done with temp buffer for full hub device name
	if (hHubDevice == INVALID_HANDLE_VALUE)
	{
		if (m_log)
		{
			m_log->Record("Error:EnumerateHubPorts-->CreateFile failed,err=%d",GetLastError());
		}
		goto Exit_EnumPorts;
	}
	connectionInfoExV2 = (PUSB_NODE_CONNECTION_INFORMATION_EX_V2)GlobalAlloc(GPTR,sizeof(USB_NODE_CONNECTION_INFORMATION_EX_V2));
	if (connectionInfoExV2 == NULL)
	{
		if (m_log)
		{
			m_log->Record("Error:EnumerateHubPorts-->malloc memory for connection info failed,err=%d",GetLastError());
		}
		goto Exit_EnumPorts;
	}
	nBytes = sizeof(USB_NODE_CONNECTION_INFORMATION_EX) + sizeof(USB_PIPE_INFO) * 30;

	connectionInfo = (PUSB_NODE_CONNECTION_INFORMATION_EX)GlobalAlloc(GPTR,nBytes);

	if (connectionInfo == NULL)
	{
		if (m_log)
		{
			m_log->Record("Error:EnumerateHubPorts-->malloc memory for connection info failed,err=%d",GetLastError());
		}
		goto Exit_EnumPorts;
	}
	for (index=1; index <= hubInfo.nPort; index++)
    {
        
		sprintf(szBuf,"%s-%d",hubInfo.strHubLayer.c_str(),index);
		strLayer = szBuf;
		devInstance = 0;

        nBytes = sizeof(USB_NODE_CONNECTION_INFORMATION_EX) + sizeof(USB_PIPE_INFO) * 30;
		ZeroMemory(connectionInfo, nBytes);

        connectionInfo->ConnectionIndex = index;

        bSuccess = DeviceIoControl(hHubDevice,
                                  IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
                                  connectionInfo,
                                  nBytes,
                                  connectionInfo,
                                  nBytes,
                                  &nBytes,
                                  NULL);

        if (!bSuccess)
        {
			if (m_log)
			{
				m_log->Record("Error:EnumerateHubPorts-->DeviceIoControl failed,err=%d",GetLastError());
			}
            goto Exit_EnumPorts;
        }

        if (connectionInfo->ConnectionStatus != DeviceConnected)
        {//disconnected state
			continue;
        }

        if (connectionInfo->DeviceIsHub)
        {
			exHubInfo.strHubLayer = strLayer;
			bSuccess = GetExHubName(hHubDevice,index,exHubInfo.strHubPath);
			if (!bSuccess)
			{
				if (m_log)
				{
					m_log->Record("Error:EnumerateHubPorts-->GetExHubName failed,layer=%s",strLayer.c_str());
				}
				continue;
			}
			bSuccess = GetHubPortNum(exHubInfo.strHubPath,nPort);
			if (!bSuccess)
			{
				if (m_log)
				{
					m_log->Record("Error:EnumerateHubPorts-->GetHubPortNum failed,layer=%s",strLayer.c_str());
				}
				continue;
			}
			exHubInfo.nPort = nPort;
			EnumerateHubPorts(exHubInfo);
        }
		bSuccess = GetDriverName(hHubDevice,index,strDriverName);
		if (!bSuccess)
		{
			if (m_log)
			{
				m_log->Record("Error:EnumerateHubPorts-->GetDriverName failed,layer=%s",strLayer.c_str());
			}
			continue;
		}
		GetDeviceInstance(strDriverName,devInstance);
		if (!devInstance)
		{
			if (m_log)
			{
				m_log->Record("Error:EnumerateHubPorts-->GetDeviceInstance failed,layer=%s",strLayer.c_str());
			}
			continue;
		}
		nBytes = sizeof(USB_NODE_CONNECTION_INFORMATION_EX_V2);
		ZeroMemory(connectionInfoExV2, nBytes);
		connectionInfoExV2->ConnectionIndex = index;
		connectionInfoExV2->Length = nBytes;
		connectionInfoExV2->SupportedUsbProtocols.Usb300 = 1;
		connectionInfoExV2->SupportedUsbProtocols.Usb200 = 1;

		bSuccess = DeviceIoControl(hHubDevice,
			IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX_V2,
			connectionInfoExV2,
			sizeof(USB_NODE_CONNECTION_INFORMATION_EX_V2),
			connectionInfoExV2,
			sizeof(USB_NODE_CONNECTION_INFORMATION_EX_V2),
			&nBytes,
			NULL);

		if (!bSuccess || nBytes < sizeof(USB_NODE_CONNECTION_INFORMATION_EX_V2))
		{
			if (m_log)
			{
				m_log->Record("Error:EnumerateHubPorts-->DeviceIoControl EX_V2 failed,index=%d,err=0x%x", index, GetLastError());
			}
		}
		else {
			if (connectionInfo->Speed == UsbHighSpeed
				&& (connectionInfoExV2->Flags.DeviceIsOperatingAtSuperSpeedOrHigher ||
					connectionInfoExV2->Flags.DeviceIsOperatingAtSuperSpeedPlusOrHigher))
			{
				connectionInfo->Speed = UsbSuperSpeed;
			}
		}
		strSerial = "";
		if (connectionInfo->DeviceDescriptor.iSerialNumber>0) {
			nBytes = sizeof(USB_DESCRIPTOR_REQUEST) + sizeof(USB_STRING_DESCRIPTOR) + 128;

			usb_desc_req = (PUSB_DESCRIPTOR_REQUEST)GlobalAlloc(GPTR,nBytes);
			if (usb_desc_req)
			{
				usb_desc_req->ConnectionIndex = connectionInfo->ConnectionIndex;
				usb_desc_req->SetupPacket.wValue = (USB_STRING_DESCRIPTOR_TYPE << 8) + connectionInfo->DeviceDescriptor.iSerialNumber;
				usb_desc_req->SetupPacket.wLength = 128;
				usb_desc_req->SetupPacket.wIndex = 0x409; // Language Code,united state
				bSuccess = DeviceIoControl(hHubDevice,
									IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
									usb_desc_req,
									nBytes,
									usb_desc_req,
									nBytes,
									&nBytes,
									NULL);

				if (!bSuccess)
				{
					if (m_log)
					{
						m_log->Record("Error:EnumerateHubPorts-->DeviceIoControl get serial failed,err=%d",GetLastError());
					}
				} else {
					usb_serial_desc = (PUSB_STRING_DESCRIPTOR)usb_desc_req->Data;
					strSerial = WideStrToMultiStr(usb_serial_desc->bString);
				}
				GlobalFree(usb_desc_req);
			}
		}

		memset(szBuf,0,512);
		CM_Get_Device_IDA(devInstance, szBuf, 512, 0);
		strLinkName = szBuf;
		replace(strLinkName.begin(),strLinkName.end(),'\\','#');
		strLinkName = "\\\\?\\" + strLinkName;

		STRUCT_RKDEVICE_DESC device;
		device.usVid = connectionInfo->DeviceDescriptor.idVendor;
		device.usPid = connectionInfo->DeviceDescriptor.idProduct;
		device.usbcdUsb = connectionInfo->DeviceDescriptor.bcdUSB;
		device.uiLocationID = GetLayerID(strLayer);
		device.pUsbHandle = (void *)devInstance;
		device.strSerial = strSerial;
		device.strDevPath = strLinkName;
		device.emUsbSpeed = (ENUM_USB_SPEED)connectionInfo->Speed;

		m_list.push_back(device);
	}
Exit_EnumPorts:
	if (hHubDevice != INVALID_HANDLE_VALUE)
		CloseHandle(hHubDevice);
	if (connectionInfo)
		GlobalFree(connectionInfo);
	if (connectionInfoExV2)
		GlobalFree(connectionInfoExV2);
}
uint_32 CRKScan::GetLayerID(string strLayer)
{
	uint_32 dwRet=0;
	int i=0;

	strLayer.erase(remove(strLayer.begin(),strLayer.end(),'-'),strLayer.end());
	if (strLayer.size()>8)
	{
		strLayer = strLayer.substr(0,8);
	}
	for ( i=0;i<strLayer.size()-1;i++ )
	{
		dwRet += (strLayer[i]-'0');
		dwRet = dwRet<<4;
	}
	dwRet += (strLayer[i]-'0');

	return dwRet;
}
void CRKScan::EnumerateUsbDevice(uint_32 &uiTotalMatchDevices)
{
	m_list.clear();
	if (m_vecLocalHub.size()==0) {
		InitLocalHub();
	} 
	int i;
	for (i = 0; i < m_vecLocalHub.size(); i++)
	{
		EnumerateHubPorts(m_vecLocalHub[i]);
	}
    
}

void CRKScan::FreeDeviceList(RKDEVICE_DESC_SET &list)
{
   	list.clear();
}
bool CRKScan::IsRockusbDevice(ENUM_RKDEVICE_TYPE &type,uint_16 vid,uint_16 pid)
{
	int iPos;
	iPos = FindConfigSetPos(m_deviceConfigSet,vid,pid);
	if (iPos!=-1)
	{
		type = m_deviceConfigSet[iPos].emDeviceType;
		return true;
	}
	if ( vid==0x2207)
	{
		if ((pid>>8)>0)
		{
			type = RKNONE_DEVICE;
			return true;
		}
	}
	return false;
}
int CRKScan::Search(uint_32 type)
{
	device_list_iter iter,new_iter;
	ENUM_RKDEVICE_TYPE devType;
	uint_32 uiTotalDevice;
	int iPos;

	// FreeDeviceList(m_list);
    EnumerateUsbDevice( uiTotalDevice );



	for ( iter=m_list.begin();iter!=m_list.end(); )
	{
		if( (iPos=FindConfigSetPos(m_deviceMscConfigSet,(*iter).usVid,(*iter).usPid))!=-1 ) 
		{
			(*iter).strDevPath += "#{53f56307-b6bf-11d0-94f2-00a0c91efb8b}";
			(*iter).emDeviceType = RKNONE_DEVICE;
			iter++;
			continue;
		}
		else if (IsRockusbDevice(devType,(*iter).usVid,(*iter).usPid) )
		{
			(*iter).strDevPath += "#{a5dcbf10-6530-11d2-901f-00c04fb951ed}";
			(*iter).emDeviceType = devType;
			iter++;
			continue;
		}
		else
		{
			iter = m_list.erase(iter);
			uiTotalDevice--;
		}
	}

	if (m_list.size()<=0)
	{
		return 0;
	}

	if ( (type&RKUSB_MASKROM)==0 )
	{
		for ( iter=m_list.begin();iter!=m_list.end(); )
		{
			if( (IsRockusbDevice(devType,(*iter).usVid,(*iter).usPid))&&(((*iter).usbcdUsb&0x1)==0) )
			{
				iter = m_list.erase(iter);
	            uiTotalDevice--;
			}
			else
			{
				iter++;
				continue;
			}
		}
	}
	if (m_list.size()<=0)
	{
		return 0;
	}

	if ( (type&RKUSB_LOADER)==0 )
	{
		for ( iter=m_list.begin();iter!=m_list.end(); )
		{
			if( (IsRockusbDevice(devType,(*iter).usVid,(*iter).usPid))&&(((*iter).usbcdUsb&0x1)==1) )
			{
				iter = m_list.erase(iter);
	            uiTotalDevice--;
			}
			else
			{
				iter++;
				continue;
			}
		}
	}
	if (m_list.size()<=0)
	{
		return 0;
	}

	if ( (type&RKUSB_MSC)==0 )
	{
		for ( iter=m_list.begin();iter!=m_list.end(); )
		{
			if(FindConfigSetPos(m_deviceMscConfigSet,(*iter).usVid,(*iter).usPid)!=-1)
			{
				iter = m_list.erase(iter);
                uiTotalDevice--;
			}
			else
			{
				iter++;
				continue;
			}
		}
	}
	if (m_list.size()<=0)
	{
		return 0;
	}

	for ( iter=m_list.begin();iter!=m_list.end();iter++ )
	{
		if(FindConfigSetPos(m_deviceMscConfigSet,(*iter).usVid,(*iter).usPid)!=-1)
		{
			(*iter).emUsbType = RKUSB_MSC;
		}
		else
		{
			uint_16 usTemp;
			usTemp = (*iter).usbcdUsb;
			usTemp= usTemp &0x1;
			if ( usTemp==0 )
				(*iter).emUsbType = RKUSB_MASKROM;
			else
				(*iter).emUsbType = RKUSB_LOADER;
		}
	}
	return m_list.size();
}


bool CRKScan::Wait(STRUCT_RKDEVICE_DESC &device,ENUM_RKUSB_TYPE usbType,uint_16 usVid,uint_16 usPid)
{
	ENUM_RKUSB_TYPE curDeviceType;
	ENUM_RKDEVICE_TYPE devType;
	device_list_iter iter;
	uint_32 totalDevice;
	uint_32 vid,pid;
	int uiWaitSecond;
	int iFoundCount=0;
	bool bRet=false;
	if (usbType==RKUSB_MSC)
		uiWaitSecond = m_waitMscSecond;
	else
		uiWaitSecond = m_waitRKusbSecond;
	time_t tmInit, tmNow;
	time(&tmInit);
	while( difftime(time(&tmNow),tmInit)<=uiWaitSecond )
	{
		EnumerateUsbDevice(totalDevice);
		
		for ( iter=m_list.begin();iter!=m_list.end();iter++ )
		{
			vid = (*iter).usVid;
			pid = (*iter).usPid;
			if ( (*iter).uiLocationID==device.uiLocationID)
			{
				if ((usVid!=0)||(usPid!=0))
				{
					if ( ((*iter).usVid!=usVid)||((*iter).usPid!=usPid) )
						continue;
				}
				if (IsRockusbDevice(devType,(*iter).usVid,(*iter).usPid))
				{
					if ( ((*iter).usbcdUsb&0x0001)==0 )
						curDeviceType = RKUSB_MASKROM;
					else
						curDeviceType = RKUSB_LOADER;
				}
				else if ( FindConfigSetPos(m_deviceMscConfigSet,(*iter).usVid,(*iter).usPid)!=-1 )
				{
					curDeviceType = RKUSB_MSC;
				}
				else
					curDeviceType = RKUSB_NONE;
								
				if ( curDeviceType==usbType )
				{
					if ((device.emUsbType != RKUSB_MASKROM) && (usbType == RKUSB_MASKROM)) {
						if (((*iter).strSerial).size() == 0)
							continue;
					}
					iFoundCount++;
					break;
				}
			}
		}
		if ( iter==m_list.end() )
		{
			iFoundCount = 0;
		}
		if ( iFoundCount>=8 )
		{
			device.usVid = (*iter).usVid;
			device.usPid = (*iter).usPid;
			device.uiLocationID = (*iter).uiLocationID;
			device.pUsbHandle= (*iter).pUsbHandle;
			device.emUsbType = usbType;
			device.emUsbSpeed = (*iter).emUsbSpeed;
			device.usbcdUsb = (*iter).usbcdUsb;
			device.strDevPath = (*iter).strDevPath;
			device.strSerial = (*iter).strSerial;
            
			if (usbType==RKUSB_MSC)
			{
				device.strDevPath += "#{53f56307-b6bf-11d0-94f2-00a0c91efb8b}";
				device.emDeviceType = RKNONE_DEVICE;
			}
			else
			{
				device.strDevPath += "#{a5dcbf10-6530-11d2-901f-00c04fb951ed}";
				if (IsRockusbDevice(devType,device.usVid,device.usPid))
					device.emDeviceType = devType;
			}
			bRet = true;
			break;
		}
		sleep_ms(50);
	}

	return bRet;
}
bool CRKScan::Wait2(STRUCT_RKDEVICE_DESC &device,ENUM_RKUSB_TYPE usbType,uint_16 usVid,uint_16 usPid)
{
	ENUM_RKUSB_TYPE curDeviceType;
	ENUM_RKDEVICE_TYPE devType;
	device_list_iter iter;
	uint_32 totalDevice;
	uint_32 vid,pid;
	int uiWaitSecond;
	int iFoundCount=0;
	bool bRet=false;

	uiWaitSecond = m_waitRKusbSecond;
	time_t tmInit, tmNow;
	time(&tmInit);
	while( difftime(time(&tmNow),tmInit)<=uiWaitSecond )
	{
		EnumerateUsbDevice(totalDevice);
		
		for ( iter=m_list.begin();iter!=m_list.end();iter++ )
		{
			vid = (*iter).usVid;
			pid = (*iter).usPid;
			if ( (*iter).strSerial==device.strSerial)
			{
				if ((usVid!=0)||(usPid!=0))
				{
					if ( ((*iter).usVid!=usVid)||((*iter).usPid!=usPid) )
						continue;
				}
				if (IsRockusbDevice(devType,(*iter).usVid,(*iter).usPid))
				{
					if ( ((*iter).usbcdUsb&0x0001)==0 )
						curDeviceType = RKUSB_MASKROM;
					else
						curDeviceType = RKUSB_LOADER;
				}
				else
					curDeviceType = RKUSB_NONE;
								
				if ( curDeviceType==usbType )
				{
					iFoundCount++;
					break;
				}
			}
		}
		if ( iter==m_list.end() )
		{
			iFoundCount = 0;
		}
		if ( iFoundCount>=8 )
		{
			device.usVid = (*iter).usVid;
			device.usPid = (*iter).usPid;
			device.uiLocationID = (*iter).uiLocationID;
			device.pUsbHandle= (*iter).pUsbHandle;
			device.emUsbType = usbType;
			device.emUsbSpeed = (*iter).emUsbSpeed;
			device.usbcdUsb = (*iter).usbcdUsb;
			device.strDevPath = (*iter).strDevPath;
			device.strSerial = (*iter).strSerial;

			device.strDevPath += "#{a5dcbf10-6530-11d2-901f-00c04fb951ed}";
			if (IsRockusbDevice(devType,device.usVid,device.usPid))
				device.emDeviceType = devType;

			bRet = true;
			break;
		}
		sleep_ms(50);
	}

	return bRet;
}
int CRKScan::GetPos(uint_32 locationID)
{
	device_list_iter iter;
	int pos=0;
	bool bFound=false;
	for (iter=m_list.begin();iter!=m_list.end();iter++)
	{
		if (locationID==(*iter).uiLocationID)
		{
			bFound=true;
			break;
		}
		pos++;
	}
	return (bFound?pos:-1);
}
bool CRKScan::GetDevice(STRUCT_RKDEVICE_DESC &device,int pos)
{
	if ( (pos<0)||(pos>=m_list.size()) )
	{
		return false;
	}
	device_list_iter iter;
	for (iter=m_list.begin();iter!=m_list.end();iter++)
	{
		if (pos==0)
		{
			break;
		}
		pos--;
	}
	device.usVid = (*iter).usVid;
	device.usPid = (*iter).usPid;
	device.emDeviceType = (*iter).emDeviceType;
	device.emUsbType = (*iter).emUsbType;
	device.emUsbSpeed = (*iter).emUsbSpeed;
	device.uiLocationID = (*iter).uiLocationID;
	device.pUsbHandle= (*iter).pUsbHandle;
	device.usbcdUsb = (*iter).usbcdUsb;
	device.strDevPath = (*iter).strDevPath;
	device.strSerial = (*iter).strSerial;
	return true;
}


bool CRKScan::SetLogObject(CRKLog *pLog)
{
	if (pLog)
	{
		if (m_log)
		{
			delete m_log;
		}
		m_log = pLog;
	}
	else
		return false;
	return true;
}
CRKScan::~CRKScan()
{
	FreeDeviceList(m_list);
	if (m_log)
	{
		delete m_log;
		m_log = NULL;
	}
}
bool CRKScan::GetRockusbDriverVer(uint_32 &major, uint_32 &minor)
{
	bool success=false;
	HDEVINFO deviceInfoSet = SetupDiGetClassDevsA(NULL, NULL, NULL, DIGCF_PROFILE | DIGCF_ALLCLASSES);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return success;
    }

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); ++i) {
        DWORD dataSize;
        char driverProviderName[256];

        // Fetch the provider name
        if (SetupDiGetDeviceRegistryPropertyA(deviceInfoSet, &deviceInfoData, SPDRP_MFG, NULL, (PBYTE)driverProviderName, sizeof(driverProviderName), &dataSize)) {
            if (stricmp(driverProviderName, "Fuzhou Rockchip") == 0) {

                SP_DRVINFO_DATA_A driverInfoData;
                driverInfoData.cbSize = sizeof(SP_DRVINFO_DATA_A);
                if (!SetupDiBuildDriverInfoList(deviceInfoSet, &deviceInfoData, SPDIT_CLASSDRIVER)) {
                    break;
                }

                // Enumerate drivers in the list
                for (DWORD i = 0; SetupDiEnumDriverInfoA(deviceInfoSet, &deviceInfoData, SPDIT_CLASSDRIVER, i, &driverInfoData); i++) {
                    // Optionally, check the driver provider name here, if you need specific providers
                    DWORDLONG version = driverInfoData.DriverVersion;
                    DWORD majorVersion = HIWORD(version >> 32);
                    DWORD minorVersion = LOWORD(version >> 32);
                    DWORD buildNumber = HIWORD(version);
                    DWORD revisionNumber = LOWORD(version);
                    
                    major = majorVersion;
					minor = minorVersion;
					success = true;
                    break;
                }

                // Clean up the driver list
                SetupDiDestroyDriverInfoList(deviceInfoSet, &deviceInfoData, SPDIT_CLASSDRIVER);
                break;
            }
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return success;
}
