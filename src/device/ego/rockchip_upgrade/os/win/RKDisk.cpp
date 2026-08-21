#include "RKDisk.h"

int g_lastDiskErr=0;
GUID GUID_CLASS_DISK_RK={0x53f56307L,0xb6bf,0x11d0,{0x94,0xf2,0x00,0xa0,0xc9,0x1e,0xfb,0x8b}};
#ifndef IOCTL_DISK_UPDATE_PROPERTIES
#define IOCTL_DISK_UPDATE_PROPERTIES    CTL_CODE(IOCTL_DISK_BASE, 0x0050, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

BOOL g_bFormatOK;
typedef enum {
		PROGRESS,
		DONEWITHSTRUCTURE,
		UNKNOWN2,
		UNKNOWN3,
		UNKNOWN4,
		UNKNOWN5,
		INSUFFICIENTRIGHTS,
		UNKNOWN7,
		UNKNOWN8,
		UNKNOWN9,
		UNKNOWNA,
		DONE,
		UNKNOWNC,
		UNKNOWND,
		OUTPUT,
		STRUCTUREPROGRESS
}CALLBACKCOMMAND;
typedef BOOLEAN (__stdcall *PFMIFSCALLBACK)(CALLBACKCOMMAND Command, DWORD SubAction, PVOID ActionInfo);
#define FMIFS_HARDDISK 0xC
#define FMIFS_FLOPPY   0x8
#define FMIFS_REMOVABLE 0xB
typedef VOID (__stdcall *PFORMATEX)(LPWSTR DriveRoot,DWORD MediaFlag,LPWSTR Format,LPWSTR Label,BOOL QuickFormat,DWORD ClusterSize,PFMIFSCALLBACK Callback);

BOOLEAN __stdcall FormatExCallback( CALLBACKCOMMAND Command, DWORD Modifier, PVOID Argument )
{
	switch( Command ) {
	case DONE:
		g_bFormatOK = *(PBOOLEAN) Argument;
		break;
	}
	return TRUE;
}
BOOL AnsiToUnicode(LPWSTR &lpwzDst,int &nDstSize,LPCSTR lpszSrc)
{
	lpwzDst = NULL;
	if (!lpszSrc)
	{
		return FALSE;
	}
	int nSrcSize;
	nSrcSize = strlen(lpszSrc);
	if (nSrcSize<=0)
	{
		return FALSE;
	}
	nDstSize = MultiByteToWideChar(CP_ACP,0,lpszSrc,-1,NULL,0);
	if (nDstSize<=0)
	{
		return FALSE;
	}
	lpwzDst = new WCHAR[nDstSize];
	if (!lpwzDst)
	{
		return FALSE;
	}
	int nCopySize;
	nCopySize = MultiByteToWideChar(CP_ACP,0,lpszSrc,-1,lpwzDst,nDstSize);
	if (nCopySize<=0)
	{
		delete []lpwzDst;
		lpwzDst = NULL;
		return FALSE;
	}
	return TRUE;
}

string CRKDisk::VolumeRoot(string strVolume)
{
	string strRoot;
	char szVolumeRoot[MAX_PATH];
	
	sprintf(szVolumeRoot,"%s:\\",strVolume.c_str());
	strRoot = szVolumeRoot;

	return strRoot;
}
bool CRKDisk::FormatVolume(string strVolume)
{
	HMODULE hDll=NULL;
	PFORMATEX   FormatEx;
	DWORD	media;
	DWORD	ClusterSize = 0;
	LPWSTR  lpwzDriveRoot,lpwzFormat,lpwzLabel;
	int nSize;
	char szVolumeRoot[MAX_PATH];
	bool bSuccess=false;
	hDll = LoadLibraryA( "fmifs.dll");
	if (!hDll)
	{
		g_lastDiskErr = GetLastError();
		return false;
	}
	
	if( !(FormatEx = (PFORMATEX) GetProcAddress(hDll,"FormatEx")) ) 
	{
		g_lastDiskErr = GetLastError();
		goto Exit_FormatVolume;
	}
	sprintf(szVolumeRoot,"%s:\\",strVolume.c_str());
	lpwzDriveRoot=NULL;
	lpwzFormat=L"FAT32";
	lpwzLabel=L"";
	AnsiToUnicode(lpwzDriveRoot,nSize,szVolumeRoot);

	media = FMIFS_REMOVABLE;
	FormatEx( lpwzDriveRoot, media, lpwzFormat, lpwzLabel, TRUE, ClusterSize, FormatExCallback);
	
	if (!g_bFormatOK)
	{
		g_lastDiskErr = DISK_ERROR_FORMAT_FAIL;
	}
	else
		bSuccess = true;
Exit_FormatVolume:
	if (lpwzDriveRoot)
		delete []lpwzDriveRoot;
	FreeLibrary(hDll);
	return bSuccess;
}

int CRKDisk::GetLastDiskError()
{
	return g_lastDiskErr;
}

int CRKDisk::GetAllDisk(CmDiskBaseVec &vecAllDisk)
{
	HDEVINFO hDevInfoSet;
    SP_DEVICE_INTERFACE_DATA ifdata;
    SP_DEVICE_INTERFACE_DETAIL_DATA_A *pDevInterfaceData=NULL;
	SP_DEVINFO_DATA devInfoData;
	STORAGE_DEVICE_NUMBER number;
	STRUCT_DISK_BASE diskBase;
	HANDLE hDevice;
    DWORD nCount,dwReturn;
    BOOL bRet=TRUE;
	BYTE buffer[MAX_PATH];
	int i;
	vecAllDisk.clear();

    //get a handle to a device information set
    hDevInfoSet = SetupDiGetClassDevsA(
		&GUID_CLASS_DISK_RK,      // class GUID
		NULL,        // Enumerator
		NULL,        // hwndParent
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE    // present devices
		);
	

    if (hDevInfoSet == INVALID_HANDLE_VALUE)
    {
		g_lastDiskErr = GetLastError();
        return -1;
    }

    
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    nCount = 0;

    // device index = 0, 1, 2... test the device interface one by one
    while (bRet)
    {
        ifdata.cbSize = sizeof(ifdata);
		
        //enumerates the device interfaces that are contained in a device information set
        bRet = SetupDiEnumDeviceInterfaces(
			hDevInfoSet,     // DeviceInfoSet
			NULL,            // DeviceInfoData
			&GUID_CLASS_DISK_RK, // GUID
			nCount,   // MemberIndex
			&ifdata        // DeviceInterfaceData
			);
        if (bRet)
        {
			
			bRet = SetupDiGetDeviceInterfaceDetailA(
				hDevInfoSet,    // DeviceInfoSet
				&ifdata,        // DeviceInterfaceData
				NULL,        // DeviceInterfaceDetailData
				0,    // DeviceInterfaceDetailDataSize
				&dwReturn,           // RequiredSize
				NULL          // DeviceInfoData
				);
			
			if ( (bRet)||(GetLastError()==ERROR_INSUFFICIENT_BUFFER) )
			{
				pDevInterfaceData = (SP_DEVICE_INTERFACE_DETAIL_DATA_A *)GlobalAlloc(GPTR,dwReturn);
				if (!pDevInterfaceData)
				{
					g_lastDiskErr = DISK_ERROR_NEW_MEMORY;
					bRet = FALSE;
					break;
				}
				pDevInterfaceData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
				// get details about a device interface
				bRet = SetupDiGetDeviceInterfaceDetailA(
					hDevInfoSet,    // DeviceInfoSet
					&ifdata,        // DeviceInterfaceData
					pDevInterfaceData,        // DeviceInterfaceDetailData
					dwReturn,    // DeviceInterfaceDetailDataSize
					NULL,           // RequiredSize
					&devInfoData          // DeviceInfoData
					);
				if (bRet)
				{
					// copy the path to output buffer
					diskBase.diskPath = pDevInterfaceData->DevicePath;
					bRet = SetupDiGetDeviceRegistryPropertyA(hDevInfoSet,&devInfoData,SPDRP_FRIENDLYNAME,NULL,
						buffer,MAX_PATH,NULL);
					if (bRet)
					{
						diskBase.diskDesc = (char *)buffer;
						vecAllDisk.push_back(diskBase);
						nCount++;
					}
					else
						g_lastDiskErr = GetLastError();
				}
				else
					g_lastDiskErr = GetLastError();
				
				GlobalFree(pDevInterfaceData);
				pDevInterfaceData = NULL;
			}
			else
				g_lastDiskErr = GetLastError();
        }
		else
			g_lastDiskErr = GetLastError();
    }
	
    SetupDiDestroyDeviceInfoList(hDevInfoSet);

	/*if ((!bRet)&&(g_lastDiskErr!=ERROR_NO_MORE_ITEMS))
	{
		return -1;
	}*/
	for (i = 0; i < nCount; i++)
    {
        hDevice = CreateFileA(
			vecAllDisk[i].diskPath.c_str(), // drive to open
			GENERIC_READ | GENERIC_WRITE,     // access to the drive
			FILE_SHARE_READ | FILE_SHARE_WRITE, //share mode
			NULL,             // default security attributes
			OPEN_EXISTING,    // disposition
			0,                // file attributes
			NULL            // do not copy file attribute
			);
        if (hDevice == INVALID_HANDLE_VALUE) // cannot open the drive
        {
            g_lastDiskErr = GetLastError();
            return -1;
        }
        bRet = DeviceIoControl(
			hDevice,                // handle to device
			IOCTL_STORAGE_GET_DEVICE_NUMBER, // dwIoControlCode
			NULL,                            // lpInBuffer
			0,                               // nInBufferSize
			&number,           // output buffer
			sizeof(number),         // size of output buffer
			&dwReturn,       // number of bytes returned
			NULL      // OVERLAPPED structure
			);
        if (!bRet) // fail
        {
			g_lastDiskErr = GetLastError();
            CloseHandle(hDevice);
            return -1;
        }
		vecAllDisk[i].diskNum = number.DeviceNumber;
		
        CloseHandle(hDevice);
    }
	
    return nCount;
}
int CRKDisk::GetDiskFromVolume(string strVolume)
{
	HANDLE hDevice;               // handle to the drive to be examined
    BOOL bRet;                 // results flag
    DWORD dwReturn;                   // discard results
    STORAGE_DEVICE_NUMBER number;   //use this to get disk numbers
	char buffer[MAX_PATH];
    string strVolumePath;
	sprintf(buffer,"\\\\.\\%s:",strVolume.c_str());
	strVolumePath = buffer;
    hDevice = CreateFileA(strVolumePath.c_str(), // drive to open
		GENERIC_READ | GENERIC_WRITE,    // access to the drive
		FILE_SHARE_READ | FILE_SHARE_WRITE,    //share mode
		NULL,             // default security attributes
		OPEN_EXISTING,    // disposition
		0,                // file attributes
		NULL);            // do not copy file attribute
    if (hDevice == INVALID_HANDLE_VALUE) // cannot open the drive
    {
        g_lastDiskErr = GetLastError();
        return -1;
    }
	
    bRet = DeviceIoControl(
		hDevice,                // handle to device
		IOCTL_STORAGE_GET_DEVICE_NUMBER, // dwIoControlCode
		NULL,                            // lpInBuffer
		0,                               // nInBufferSize
		&number,           // output buffer
		sizeof(number),         // size of output buffer
		&dwReturn,       // number of bytes returned
		NULL      // OVERLAPPED structure
		);
    if (!bRet) // fail
    {
        g_lastDiskErr = GetLastError();
        CloseHandle(hDevice);
        return -1;
    }
	
    CloseHandle(hDevice);
    return number.DeviceNumber;
}
int CRKDisk::GetAllVolumeFromDisk(int nDiskNum,STRING_VECTOR &vecAllVolume)
{
	DWORD mask;
    DWORD driveType;
    DWORD dwDriveMask;
    DWORD diskNumber;
	string strVolumePath,strVolume;   
    char letter;
    int letterNum;
    int i;

	vecAllVolume.clear();
    dwDriveMask = GetLogicalDrives();
    if (0 == dwDriveMask)
    {
		g_lastDiskErr = GetLastError();
        return -1;
    }
	
    letterNum = 0;
    for (i = 0; i < sizeof(DWORD) * 8; i++)
    {
        mask = 1 << i;
        if ((mask & dwDriveMask) == 0)        //get one letter
        {
            continue;
        }
        letter = 'A'+i;    //ASCII change
		strVolume = letter;
		strVolumePath = strVolume + ":\\";
        driveType = GetDriveTypeA(strVolumePath.c_str());
        if ((driveType != DRIVE_FIXED)&&(driveType!=DRIVE_REMOVABLE))
        {
            dwDriveMask &= ~mask;     //clear this bit
            continue;
        }
        diskNumber = GetDiskFromVolume(strVolume);
        if (diskNumber != nDiskNum)
        {
            dwDriveMask &= ~mask;     //clear this bit
            continue;
        }
        letterNum++;
		vecAllVolume.push_back(strVolume);
    }
	
    return letterNum;
}
bool CRKDisk::CheckDiskRemovable(int nDiskNum,bool &bRemoved)
{
	STRING_VECTOR vecVolume;
	string strVolumePath;
	uint32_t driveType;
	int ret,i;
	bRemoved = false;
	ret = GetAllVolumeFromDisk(nDiskNum,vecVolume);
	if (ret <=0)
		return false;
	for (i = 0; i < ret; i++)
	{
		strVolumePath = vecVolume[i] + ":\\";
        driveType = GetDriveTypeA(strVolumePath.c_str());
		if (driveType==DRIVE_REMOVABLE)
        {
            bRemoved = true;
			return true;
        }
	}
	return true;
}
uint64_t CRKDisk::GetDriveSize(string strDrive)
{
	HANDLE hDrive = INVALID_HANDLE_VALUE;
	INT64 driveSize;
	string strDrivePath;
	char buffer[MAX_PATH];
	sprintf(buffer,"\\\\.\\%s:",strDrive.c_str());
	strDrivePath = buffer;
	hDrive = CreateFileA(strDrivePath.c_str(), // drive to open
		GENERIC_READ | GENERIC_WRITE,    // access to the drive
		FILE_SHARE_READ | FILE_SHARE_WRITE,    //share mode
		NULL,             // default security attributes
		OPEN_EXISTING,    // disposition
		0,                // file attributes
		NULL);            // do not copy file attribute
    if (hDrive == INVALID_HANDLE_VALUE) 
	{
		g_lastDiskErr = DISK_ERROR_OPEN_FAIL;
		return 0;
	}
	
	DISK_GEOMETRY_EX diskGeometry;
	DWORD dwReturn;
	BOOL bRet;
	bRet = DeviceIoControl(hDrive,  // device we are querying
		IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,  // operation to perform
		NULL, 0, // no input buffer, so pass zero
		&diskGeometry, sizeof(diskGeometry),  // output buffer
		&dwReturn, // discard count of bytes returned
		NULL);  // synchronous I/O
	if (!bRet)
	{
		g_lastDiskErr = DISK_ERROR_GETSIZE_FAIL;
		CloseHandle(hDrive);
		hDrive = INVALID_HANDLE_VALUE;
		return 0;
	} 
	
	driveSize = diskGeometry.DiskSize.QuadPart;
	return driveSize;
}
CRKDisk::CRKDisk(STRUCT_DISK_BASE disk)
{
	m_diskNum = disk.diskNum;
	m_diskHandle = INVALID_HANDLE_VALUE;
	m_vecAllVolume.clear();
	m_strDiskPath = disk.diskPath;
	m_strDiskDesc = disk.diskDesc;
	m_diskSize = 0;
	m_bytePerSector = 0;
}
CRKDisk::~CRKDisk()
{
	if (m_diskHandle!=INVALID_HANDLE_VALUE)
	{
		CloseDisk();
	}
}
bool CRKDisk::OpenDisk()
{
	char buffer[MAX_PATH];
	if (m_diskNum<0)
	{
		g_lastDiskErr = DISK_ERROR_NO_DISKNUM;
		return false;
	}
	if (m_diskHandle != INVALID_HANDLE_VALUE)
	{
		g_lastDiskErr = DISK_ERROR_HANDLE_EXIST;
		return false;
	}

	string strDiskPath;
	sprintf(buffer,"\\\\.\\PHYSICALDRIVE%d",m_diskNum);
	strDiskPath = buffer;
	m_diskHandle = CreateFileA(strDiskPath.c_str(), // drive to open
		GENERIC_READ | GENERIC_WRITE,    // access to the drive
		FILE_SHARE_READ | FILE_SHARE_WRITE,    //share mode
		NULL,             // default security attributes
		OPEN_EXISTING,    // disposition
		0,                // file attributes
		NULL);            // do not copy file attribute
    if (m_diskHandle == INVALID_HANDLE_VALUE) 
	{
		g_lastDiskErr = DISK_ERROR_OPEN_FAIL;
		return false;
	}

	DISK_GEOMETRY_EX diskGeometry;
	DWORD dwReturn;
	BOOL bRet;
	bRet = DeviceIoControl(m_diskHandle,  // device we are querying
		IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,  // operation to perform
		NULL, 0, // no input buffer, so pass zero
		&diskGeometry, sizeof(diskGeometry),  // output buffer
		&dwReturn, // discard count of bytes returned
		NULL);  // synchronous I/O
	if (!bRet)
	{
		g_lastDiskErr = DISK_ERROR_GETSIZE_FAIL;
		CloseHandle(m_diskHandle);
		m_diskHandle = INVALID_HANDLE_VALUE;
		return false;
	} 
	
	m_diskSize = diskGeometry.DiskSize.QuadPart;
	m_bytePerSector = diskGeometry.Geometry.BytesPerSector;

	int nDriveCount ;
	nDriveCount = GetAllVolumeFromDisk(m_diskNum,m_vecAllVolume);
	if (nDriveCount==-1)
	{
		g_lastDiskErr = DISK_ERROR_GETALLVOLUME_FAIL;
		CloseHandle(m_diskHandle);
		m_diskHandle = INVALID_HANDLE_VALUE;
		return false;
	}

	return true;
}

bool CRKDisk::UpdateDiskLayout()
{
	DWORD dwReturn;
	BOOL bRet;

	if (m_diskNum<0)
	{
		g_lastDiskErr = DISK_ERROR_NO_DISKNUM;
		return false;
	}
	if (m_diskHandle==INVALID_HANDLE_VALUE)
	{
		g_lastDiskErr = DISK_ERROR_HANDLE_NOTEXIST;
		return false;
	}

	bRet = DeviceIoControl(m_diskHandle,  // device we are querying
		IOCTL_DISK_UPDATE_PROPERTIES,  // operation to perform
		NULL, 0, // no input buffer, so pass zero
		NULL, 0,  // output buffer
		&dwReturn, // discard count of bytes returned
		NULL);  // synchronous I/O
	if (!bRet)
	{
		g_lastDiskErr = GetLastError();
		return false;
	} 
	return true;
}
bool CRKDisk::ReadDisk(size_t nStart,int nCount,uint8_t *pReceiveBuf)
{
	if (m_diskNum<0)
	{
		g_lastDiskErr = DISK_ERROR_NO_DISKNUM;
		return false;
	}
	if (m_diskHandle==INVALID_HANDLE_VALUE)
	{
		g_lastDiskErr = DISK_ERROR_HANDLE_NOTEXIST;
		return false;
	}
	BOOL bRet;
	bRet = OffsetDisk(nStart,FILE_BEGIN);
	if (!bRet)
	{
		return false;
	}
	DWORD dwRead;
	bRet = ReadFile(m_diskHandle,pReceiveBuf,nCount,&dwRead,NULL);
	if (!bRet)
	{
		g_lastDiskErr = GetLastError();
		return false;
	}
	if ((dwRead!=nCount)&&(dwRead!=0))
	{
		g_lastDiskErr = DISK_ERROR_READ_PARTIAL;
		return false;
	}

	return true;
}
bool CRKDisk::WriteDisk(size_t nStart,int nCount,uint8_t *pSendBuf)
{
	if (m_diskNum<0)
	{
		g_lastDiskErr = DISK_ERROR_NO_DISKNUM;
		return false;
	}
	if (m_diskHandle==INVALID_HANDLE_VALUE)
	{
		g_lastDiskErr = DISK_ERROR_HANDLE_NOTEXIST;
		return false;
	}
	BOOL bRet;
	bRet = OffsetDisk(nStart,FILE_BEGIN);
	if (!bRet)
	{
		return false;
	}
	DWORD dwWrite;
	bRet = WriteFile(m_diskHandle,pSendBuf,nCount,&dwWrite,NULL);
	if (!bRet)
	{
		g_lastDiskErr = GetLastError();
		return false;
	}
	if (dwWrite!=nCount)
	{
		g_lastDiskErr = DISK_ERROR_WRITE_PARTIAL;
		return false;
	}
	
	return true;
}
BOOL CRKDisk::OffsetDisk(uint64_t nMoveBytes,uint32_t dwMovestart)
{
	if (m_diskNum<0)
	{
		g_lastDiskErr = DISK_ERROR_NO_DISKNUM;
		return FALSE;
	}
	if (m_diskHandle==INVALID_HANDLE_VALUE)
	{
		g_lastDiskErr = DISK_ERROR_HANDLE_NOTEXIST;
		return FALSE;
	}
	DWORD dwRet;
	LONG lHighPart;
	lHighPart = nMoveBytes>>32;
	dwRet = SetFilePointer(m_diskHandle,nMoveBytes,&lHighPart,dwMovestart);
	if (dwRet==(DWORD)-1)
	{
		g_lastDiskErr = GetLastError();
		return FALSE;
	}
	return TRUE;
}
void CRKDisk::CloseDisk()
{
	if (m_diskHandle!=INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_diskHandle);
		m_diskHandle = INVALID_HANDLE_VALUE;
	}
	m_diskNum = -1;
	m_vecAllVolume.clear();
	m_strDiskPath = m_strDiskDesc = "";
	m_diskSize = 0;
	m_bytePerSector = 0;
}

bool CRKDisk::LockAndDismount()
{
	STRING_VECTOR vecVolumes;
	int i;
	DWORD dwRet;
	BOOL bRet;
	char buffer[MAX_PATH];
	string strVol;

	if (m_diskNum<0)
	{
		g_lastDiskErr = DISK_ERROR_NO_DISKNUM;
		return false;
	}
	if (m_diskHandle==INVALID_HANDLE_VALUE)
	{
		g_lastDiskErr = DISK_ERROR_HANDLE_NOTEXIST;
		return false;
	}

	GetAllDrive(vecVolumes,true);
	for (i=0;i<vecVolumes.size();i++)
	{
		sprintf(buffer,"\\\\.\\%s:",vecVolumes[i].c_str());
		strVol = buffer;
		HANDLE hVol = CreateFileA(strVol.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, 0, NULL);
		if (hVol!=INVALID_HANDLE_VALUE)
		{
			bRet = DeviceIoControl(hVol, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwRet, NULL);
			if (bRet)
				bRet = DeviceIoControl(hVol, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dwRet, NULL);
			CloseHandle(hVol);
		}
		else {
			g_lastDiskErr = GetLastError();
			return false;
		}
	}

	return true;
}

bool CRKDisk::GetAllDrive(STRING_VECTOR &vecAllDrive,bool bRefresh)
{
	if (m_diskNum<0)
	{
		g_lastDiskErr = DISK_ERROR_NO_DISKNUM;
		return false;
	}
	if (m_diskHandle==INVALID_HANDLE_VALUE)
	{
		g_lastDiskErr = DISK_ERROR_HANDLE_NOTEXIST;
		return false;
	}
	if (bRefresh)
	{
		int nDriveCount ;
		nDriveCount = GetAllVolumeFromDisk(m_diskNum,m_vecAllVolume);
		if (nDriveCount==-1)
		{
			g_lastDiskErr = DISK_ERROR_GETALLVOLUME_FAIL;
			return false;
		}
	}
	vecAllDrive.clear();
	int i;
	for (i=0;i<m_vecAllVolume.size();i++)
	{
		vecAllDrive.push_back(m_vecAllVolume[i]);
	}
	
	return true;
}

uint64_t CRKDisk::GetDiskSize()
{
	if (m_diskNum<0)
	{
		g_lastDiskErr = DISK_ERROR_NO_DISKNUM;
		return 0;
	}
	if (m_diskHandle==INVALID_HANDLE_VALUE)
	{
		g_lastDiskErr = DISK_ERROR_HANDLE_NOTEXIST;
		return 0;
	}
	return m_diskSize;
}

bool CRKDisk::IsRemovedDisk(bool &bRemoved)
{
	if (m_diskNum<0)
	{
		g_lastDiskErr = DISK_ERROR_NO_DISKNUM;
		return false;
	}
	if (m_diskHandle==INVALID_HANDLE_VALUE)
	{
		g_lastDiskErr = DISK_ERROR_HANDLE_NOTEXIST;
		return false;
	}

	// Set the input data structure
	DWORD dwRet;
	STORAGE_PROPERTY_QUERY storagePropertyQuery;
	ZeroMemory(&storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY));
	storagePropertyQuery.PropertyId = StorageDeviceProperty;
	storagePropertyQuery.QueryType = PropertyStandardQuery;

	// Get the necessary output buffer size
	STORAGE_DESCRIPTOR_HEADER storageDescriptorHeader = {0};
	DWORD dwBytesReturned = 0;
	if(! ::DeviceIoControl(m_diskHandle, IOCTL_STORAGE_QUERY_PROPERTY,
		&storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY),
		&storageDescriptorHeader, sizeof(STORAGE_DESCRIPTOR_HEADER),
		&dwBytesReturned, NULL))
	{
		dwRet = ::GetLastError();
		g_lastDiskErr = dwRet;
		return false;
	}

	// Alloc the output buffer
	const DWORD dwOutBufferSize = storageDescriptorHeader.Size;
	BYTE* pOutBuffer = new BYTE[dwOutBufferSize];
	ZeroMemory(pOutBuffer, dwOutBufferSize);

	// Get the storage device descriptor
	if(! ::DeviceIoControl(m_diskHandle, IOCTL_STORAGE_QUERY_PROPERTY,
		&storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY),
		pOutBuffer, dwOutBufferSize,
		&dwBytesReturned, NULL))
	{
		dwRet = ::GetLastError();
		delete []pOutBuffer;
		g_lastDiskErr = dwRet;
		return false;
	}

	// Now, the output buffer points to a STORAGE_DEVICE_DESCRIPTOR structure
	// followed by additional info like vendor ID, product ID, serial number, and so on.
	STORAGE_DEVICE_DESCRIPTOR* pDeviceDescriptor = (STORAGE_DEVICE_DESCRIPTOR*)pOutBuffer;
	bRemoved = false;
	if (pDeviceDescriptor->BusType == BusTypeUsb)
	{
		bRemoved = true;
	}
	delete []pOutBuffer;
	return true;
}