#include "RKComm.h"
#include "..\\..\\RKLog.h"
extern unsigned short CRC_CCITT(unsigned char* p, uint_32 CalculateNumber);
extern bool g_bCBW_parity;
CRKComm::CRKComm(CRKLog *pLog)
{
	m_log = pLog;
}
CRKUsbComm::CRKUsbComm(STRUCT_RKDEVICE_DESC &devDesc,CRKLog *pLog,bool &bRet):CRKComm(pLog)
{	
	memset(&m_deviceDesc,0,18);
	m_deviceDesc.pUsbHandle = NULL;
	m_deviceDesc.strDevPath = "";
	m_deviceDesc.strSerial = "";
	m_pUsbHandle = INVALID_HANDLE_VALUE;
	bRet = Reset_Usb_Config(devDesc);
}
CRKUsbComm::~CRKUsbComm()
{
}

bool CRKUsbComm::Reset_Usb_Config(STRUCT_RKDEVICE_DESC &devDesc)
{
	m_deviceDesc.usVid = devDesc.usVid;
	m_deviceDesc.usPid = devDesc.usPid;
	m_deviceDesc.usbcdUsb = devDesc.usbcdUsb;
	m_deviceDesc.uiLocationID = devDesc.uiLocationID;
	m_deviceDesc.emUsbType = devDesc.emUsbType;
	m_deviceDesc.emDeviceType = devDesc.emDeviceType;
	m_deviceDesc.pUsbHandle = devDesc.pUsbHandle;
	m_deviceDesc.strSerial = devDesc.strSerial;
	m_deviceDesc.strDevPath = devDesc.strDevPath;
	return true;
}
bool CRKUsbComm::Reset_Usb_Device()
{
    return true;
}
bool CRKUsbComm::RKU_Open(USB_ACCESS_TYPE accessMode)
{
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	string strDevPipe = m_deviceDesc.strDevPath;
	
 	switch(accessMode)
 	{
 	case USB_BULK_READ:
	 	strDevPipe += "\\PIPE00";
 		hDevice = CreateFileA(strDevPipe.c_str(),
 			GENERIC_READ,
 			FILE_SHARE_READ,
 			NULL,
 			OPEN_EXISTING,
 			0,// FILE_FLAG_OVERLAPPED,
 			NULL);
 		break;
 	case USB_BULK_WRITE:
		strDevPipe += "\\PIPE01";
 		hDevice = CreateFileA(strDevPipe.c_str(),
 			GENERIC_WRITE,
 			FILE_SHARE_WRITE,
 			NULL,
 			OPEN_EXISTING,
 			0,//FILE_FLAG_OVERLAPPED,
 			NULL);
 		break;
 	case USB_CONTROL:
		hDevice = CreateFileA(strDevPipe.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);
 		break;
 	default:
 		return false;
 	}
	if (hDevice==INVALID_HANDLE_VALUE)
	{
		if (m_log)
		{
			m_log->Record("Error:RKU_Open-->CreateFile failed,err=%d,path=%s",GetLastError(),strDevPipe.c_str());
		}
		return false;
	}
	m_pUsbHandle = hDevice;
	return true;
}
void CRKUsbComm::RKU_Close()
{
	if (m_pUsbHandle!=INVALID_HANDLE_VALUE) {
		CloseHandle(m_pUsbHandle);
		m_pUsbHandle = INVALID_HANDLE_VALUE;
	}
	
}
bool CRKUsbComm::RKU_Read(uint_8* lpBuffer, uint_32 dwSize)
{
	uint_32 totalRead = 0;
	while(totalRead < dwSize) {
		DWORD bytesRead = 0;
		BOOL bRet = ReadFile(m_pUsbHandle, lpBuffer + totalRead, dwSize - totalRead, &bytesRead, NULL);
		if(!bRet || bytesRead == 0) {
			if (m_log) {
				m_log->Record("Error:RKU_Read-->ReadFile failed,bRet=%d,err=%d,size=%d,read=%d", bRet, GetLastError(), dwSize, totalRead + bytesRead);
			}
			return false;
		}
		totalRead += bytesRead;
	}
	return true;
}

bool CRKUsbComm::RKU_Write(uint_8* lpBuffer,uint_32 dwSize)
{
	BOOL bRet;
	uint_32 dwTransfered = 0;
	bRet = WriteFile(m_pUsbHandle,(LPVOID)lpBuffer,dwSize,(LPDWORD)&dwTransfered,NULL);
	if (dwSize!=dwTransfered)
	{
		if (m_log)
		{
			m_log->Record("Error:RKU_Write-->WriteFile failed,bRet=%d,err=%d,size=%d,write=%d",bRet,GetLastError(),dwSize,dwTransfered);
		}
		return false;
	}
	return true;
}
int CRKUsbComm::RandomInteger(int low, int high)
{
	int k;
	double d;
	
	d = (double)rand() / ((double)RAND_MAX + 1);
	k = (int)(d * (high - low + 1));
	return (low+k);
}
uint_32 CRKUsbComm::MakeCBWTag()
{
	uint_32 tag = 0;
	int i = 0;
	
	for(i=0; i<4; i++){
		tag <<= 8;
		tag += RandomInteger(0, 0xFF);
	}
	return tag;
}
void CRKUsbComm::InitializeCBW(PCBW pCBW, USB_OPERATION_CODE code)
{
	memset(pCBW,0, sizeof(CBW));
	
	pCBW->dwCBWSignature = CBW_SIGN;
	pCBW->dwCBWTag = MakeCBWTag();
	pCBW->cbwcb.ucOperCode = code;
	
	switch(code)
	{
		case TEST_UNIT_READY:	/* Test Unit Ready	: 0 */
		case READ_FLASH_ID:		/* Read Flash ID	: 1 */
		case READ_FLASH_INFO:
		case READ_CHIP_INFO:
		case READ_OTP_CHIP:
		case READ_EFUSE:
		case READ_CAPABILITY:
		case LIST_STORAGE:
			pCBW->ucCBWFlags= DIRECTION_IN;
			pCBW->ucCBWCBLength = 0x06;
			break;
		case DEVICE_RESET:		/* Reset Device		: 0xff */
		case ERASE_SYSTEMDISK:
		case SET_RESET_FLAG:
		case SWITCH_STORAGE:
			pCBW->ucCBWFlags = DIRECTION_OUT;
			pCBW->ucCBWCBLength = 0x06;
			break;
		case TEST_BAD_BLOCK:	/* Test Bad Block	: 3 */
		case READ_SECTOR:		/* Read Pages		: 4 */
		case READ_LBA:		/* Read Pages		: 4 */
		case READ_SDRAM:		/* Write Pages		: 15 */
		case READ_SPI_FLASH:
		case READ_NEW_EFUSE:
		case READ_COM_DATA:
		
			pCBW->ucCBWFlags = DIRECTION_IN;
			pCBW->ucCBWCBLength = 0x0a;
			break;
		case WRITE_SECTOR:		/* Write Pages		: 5 */
		case WRITE_LBA:		/* Write Pages		: 15 */
		case WRITE_SDRAM:		/* Write Pages		: 15 */
		case EXECUTE_SDRAM:
		case ERASE_NORMAL:		/* Erase Blocks		: 6 */
		case ERASE_FORCE:		/* Read Spare		: 11 */
		case WRITE_EFUSE:
		case WRITE_SPI_FLASH:
		case WRITE_NEW_EFUSE:
		case ERASE_LBA:
		case SWITCH_USB3:
			pCBW->ucCBWFlags = DIRECTION_OUT;
			pCBW->ucCBWCBLength = 0x0a;
			break;
		default:
			break;
	}
}


bool CRKUsbComm::RKU_ClearBuffer(CBW& cbw,CSW& csw)
{
	uint_32 dwReadBytes=0;
	uint_32 dwTotalRead=0;
	uint_8 buffer[512];
	BOOL bRet;

	if(!RKU_Open(USB_BULK_READ))
		return false;
	do
	{
		dwReadBytes = 0;
		bRet = ReadFile(m_pUsbHandle,(LPVOID)buffer,512,(LPDWORD)&dwReadBytes,NULL);
		if (!bRet)
		{
			if (m_log)
			{
				m_log->Record("Error:RKU_ClearBuffer-->ReadFile failed,bRet=%d,err=%d,size=%d,read=%d",bRet,GetLastError(),512,dwReadBytes);
			}
			RKU_Close();
			return false;
		}
		if (dwReadBytes==0)
		{
			break;
		}
		dwTotalRead += dwReadBytes;
	}while ( dwTotalRead<(1024*1024) );
	if (m_log)
	{
		m_log->Record("Error:RKU_ClearBuffer-->totalRead=%d", dwTotalRead);
	}
	RKU_Close();
	return true;
}

int CRKUsbComm::RKU_EraseBlock(uint_8 ucFlashCS,uint_32 dwPos,uint_32 dwCount,uint_8 ucEraseType)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_EraseBlock failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
	CBW cbw;
	CSW csw;
	uint_16 usCount;
	usCount = dwCount;
	if(dwCount > MAX_ERASE_BLOCKS)
		return ERR_CROSS_BORDER;
	
	
	InitializeCBW(&cbw, (USB_OPERATION_CODE)ucEraseType);
	cbw.ucCBWLUN = ucFlashCS;
	cbw.cbwcb.dwAddress = EndianU32_LtoB(dwPos);
	cbw.cbwcb.usLength = EndianU16_LtoB(usCount);
	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;
	
	if(!RKU_Write((uint_8 *)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;
	}
	RKU_Close();
	
	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;	
	
	if(!RKU_Read((uint_8 *)&csw, sizeof(CSW)))
	{	
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;
	}
	RKU_Close();
	
	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	if(csw.ucCSWStatus == 1)
		return ERR_FOUND_BAD_BLOCK;			
	
	return ERR_SUCCESS;						
}
int CRKUsbComm::RKU_ReadChipInfo(uint_8* lpBuffer)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_ReadChipInfo failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
    
	CBW cbw;
	CSW csw;
	
	InitializeCBW(&cbw, READ_CHIP_INFO);
	cbw.dwCBWTransferLength = 16;

	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;	
	}
	RKU_Close();
	
	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if(!RKU_Read(lpBuffer, 16))
	{	
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	RKU_Close();
	
	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	return ERR_SUCCESS;						
}
int CRKUsbComm::RKU_ReadFlashID(uint_8* lpBuffer)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_ReadChipInfo failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
    
	CBW cbw;
	CSW csw;
	
	InitializeCBW(&cbw, READ_FLASH_ID);
	cbw.dwCBWTransferLength = 5;
	
	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;	
	}
	RKU_Close();

	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if(!RKU_Read(lpBuffer, 5))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	RKU_Close();
	
	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	return ERR_SUCCESS;						
}
int CRKUsbComm::RKU_ReadOtpCpuID(uint_8* lpBuffer)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_ReadOtpCpuID failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
    
	CBW cbw;
	CSW csw;
	
	InitializeCBW(&cbw, READ_OTP_CHIP);
	cbw.dwCBWTransferLength = 32;
	cbw.cbwcb.ucReserved = 0;
	
	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;	
	}
	RKU_Close();

	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if(!RKU_Read(lpBuffer, 32))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	RKU_Close();
	
	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	return ERR_SUCCESS;						
}
int CRKUsbComm::RKU_ReadFlashInfo(uint_8* lpBuffer,uint_32 *puiRead)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_ReadFlashInfo failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
	CBW cbw;
	CSW csw;
	
	InitializeCBW(&cbw, READ_FLASH_INFO);
	cbw.dwCBWTransferLength = 11;
	
	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;		
	}
	RKU_Close();

	uint_32 dwRead;
	BOOL bRet;
	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;
	bRet = ReadFile(m_pUsbHandle,lpBuffer,512,(LPDWORD)&dwRead,NULL);
	if (!bRet)
	{
		if (m_log)
		{
			m_log->Record("Error:RKU_ReadFlashInfo-->ReadFile failed,bRet=%d,err=%d,size=%d,read=%d",bRet,GetLastError(),512,dwRead);
		}
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;
	}

	if ((dwRead<11)||(dwRead>512))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;
	}
	if (puiRead)
	{
		*puiRead = dwRead;
	}

	if(!RKU_Read((uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	
	RKU_Close();

	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	//handle available page size
	lpBuffer[11] = lpBuffer[6] >> 4;
	if (lpBuffer[11]) {
		lpBuffer[6] = lpBuffer[6] & 0x0F;
	}
	else
		lpBuffer[11] = lpBuffer[6];
	return ERR_SUCCESS;					
}
int CRKUsbComm::RKU_ReadCapability(uint_8* lpBuffer)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_ReadCapability failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
    
	CBW cbw;
	CSW csw;
	uint_32 dwRead;
	
	InitializeCBW(&cbw, READ_CAPABILITY);
	cbw.dwCBWTransferLength = 8;
	
	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;	
	}
	RKU_Close();

	BOOL bRet;
	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;
	bRet = ReadFile(m_pUsbHandle,(uint_8*)&csw, sizeof(CSW),(LPDWORD)&dwRead,NULL);
	if (!bRet)
	{
		if (m_log)
		{
			m_log->Record("Error:RKU_ReadCapability-->ReadFile failed,bRet=%d,err=%d,size=%d,read=%d",bRet,GetLastError(),sizeof(CSW),dwRead);
		}
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;
	}
	
	if(dwRead!=8)
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	memcpy(lpBuffer,(uint_8*)&csw,8);
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	RKU_Close();

	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	return ERR_SUCCESS;			
}

int CRKUsbComm::RKU_ReadLBA(uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer,uint_8 bySubCode)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_ReadLBA failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
	CBW cbw;
	CSW csw;
	uint_16 wSectorSize;
	uint_16 usSectorLen;
	wSectorSize = 512;
	usSectorLen=dwCount;
	
	InitializeCBW(&cbw, READ_LBA);
	cbw.dwCBWTransferLength = dwCount * wSectorSize;
	cbw.cbwcb.dwAddress = EndianU32_LtoB(dwPos);
	cbw.cbwcb.usLength = EndianU16_LtoB(usSectorLen);
	cbw.cbwcb.ucReserved = bySubCode;
	
	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;	
	}
	RKU_Close();

	uint_32 dwTotal;
	dwTotal = usSectorLen * wSectorSize;

	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if(!RKU_Read(lpBuffer, dwTotal))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;	
	}
	
	if(!RKU_Read((uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	RKU_Close();
	
	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	if(csw.ucCSWStatus == 1)
		return ERR_FAILED;				
	
	return ERR_SUCCESS;					
}
int CRKUsbComm::RKU_ReadSector(uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_ReadSector failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
	CBW cbw;
	CSW csw;
	uint_16 wSectorSize;
	uint_16 usSectorLen;
	usSectorLen = dwCount & 0xFFFF;
	if(usSectorLen > 32)
		return ERR_CROSS_BORDER;
	wSectorSize = dwCount >> 16;
	if (wSectorSize==0)
	{
		wSectorSize = 528;
	}
	
	InitializeCBW(&cbw, READ_SECTOR);
	cbw.dwCBWTransferLength = (uint32_t)usSectorLen * (uint32_t)wSectorSize;
	cbw.cbwcb.dwAddress = EndianU32_LtoB(dwPos);
	cbw.cbwcb.usLength = EndianU16_LtoB(usSectorLen);
	
	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;		
	}
	RKU_Close();
	
	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if(!RKU_Read(lpBuffer, usSectorLen * (uint32_t)wSectorSize))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	RKU_Close();

	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	if(csw.ucCSWStatus == 1)
		return ERR_FAILED;					
	
	return ERR_SUCCESS;					
}
int CRKUsbComm::RKU_ResetDevice(uint_8 bySubCode)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_ResetDevice failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
    
	CBW cbw;
	CSW csw;

	InitializeCBW(&cbw, DEVICE_RESET);
	cbw.cbwcb.ucReserved = bySubCode;
	
	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;		
	}
	RKU_Close();

	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if(!RKU_Read((uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;	
	}
	RKU_Close();

	if( !UFI_CHECK_SIGN(cbw, csw) )
	{
		bool bRet;
		bRet = RKU_ClearBuffer(cbw,csw);
		if (!bRet)
		{
			return ERR_CMD_NOTMATCH;			
		}
	}
	
	if(csw.ucCSWStatus == 1)
		return ERR_FAILED;					
	
	return ERR_SUCCESS;						
}


int CRKUsbComm::RKU_TestBadBlock(uint_8 ucFlashCS,uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_TestBadBlock failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
	CBW cbw;
	CSW csw;
	uint_16 usCount;
	usCount = dwCount;

	if((dwCount > MAX_TEST_BLOCKS) || (ucFlashCS >= 4))
		return ERR_CROSS_BORDER;
	
	InitializeCBW(&cbw, TEST_BAD_BLOCK);
	cbw.dwCBWTransferLength = 64;
	cbw.ucCBWLUN = ucFlashCS;
	cbw.cbwcb.dwAddress = EndianU32_LtoB(dwPos);
	cbw.cbwcb.usLength = EndianU16_LtoB(usCount);

	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;
	
	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;	
	}
	RKU_Close();

	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if(!RKU_Read(lpBuffer, MAX_TEST_BLOCKS/8))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;	
	}
	RKU_Close();

	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	if(csw.ucCSWStatus == 1)
		return ERR_FOUND_BAD_BLOCK;		
	
	return ERR_SUCCESS;						
}
int CRKUsbComm::RKU_TestDeviceReady(uint_32 *dwTotal,uint_32 *dwCurrent,uint_8 bySubCode)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_TestDeviceReady failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
	CBW cbw;
	CSW csw;
	
	InitializeCBW(&cbw, TEST_UNIT_READY);
	
	cbw.cbwcb.ucReserved = bySubCode;
	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;	
	}
	RKU_Close();
	
	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;	
	}
	RKU_Close();
	
	if( !UFI_CHECK_SIGN(cbw, csw) )
	{
		bool bRet;
		bRet = RKU_ClearBuffer(cbw,csw);
		if (!bRet)
		{
			return ERR_CMD_NOTMATCH;			
		}
		
	}
	
	if ((dwTotal!=NULL)&&(dwCurrent!=NULL))
	{
		
		*dwCurrent = (csw.dwCBWDataResidue >>16);
		*dwTotal = (csw.dwCBWDataResidue & 0x0000FFFF);
	
		*dwTotal = EndianU16_BtoL(*dwTotal);
		*dwCurrent = EndianU16_BtoL(*dwCurrent);
	}
	if(csw.ucCSWStatus == 1)
	{
		return ERR_DEVICE_UNREADY;			
	}
	
	return ERR_DEVICE_READY;			
}
int CRKUsbComm::RKU_WriteLBA(uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer,uint_8 bySubCode)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_WriteLBA failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
	CBW cbw;
	CSW csw;
	uint_16 wSectorSize=512;
	uint_16 usCount;
	usCount = dwCount;
	uint_32 dwTotal = dwCount*wSectorSize;
	uint_32 i,j;
	
	InitializeCBW(&cbw, WRITE_LBA);
	if (g_bCBW_parity)
	{
		cbw.ucCBWCBLength = 13;
		cbw.cbwcb.dwParity = 0x55aa;
		for (i = 0; i < dwCount; i++)
		{
			for (j = 0; j < wSectorSize/4; j++)
			{
				cbw.cbwcb.dwParity += (*(uint32_t *)(lpBuffer+i*wSectorSize+j*4));
			}
		}
	}
	
	cbw.dwCBWTransferLength = dwTotal;
	cbw.cbwcb.dwAddress = EndianU32_LtoB(dwPos);
	cbw.cbwcb.usLength = EndianU16_LtoB(usCount);
	cbw.cbwcb.ucReserved = bySubCode;

	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;		
	}
	
	if(!RKU_Write( lpBuffer, dwTotal))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;	
	}
	RKU_Close();

	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;	
	}
	RKU_Close();

	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	if(csw.ucCSWStatus == 1)
		return ERR_FAILED;
	
	if(csw.ucCSWStatus == 2)
		return ERR_PARITY;			
	
	return ERR_SUCCESS;						
}
int CRKUsbComm::RKU_WriteSector(uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_WriteSector failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
	CBW cbw;
	CSW csw;
	uint_16 wSectorSize;
	uint_16 usCount;
	usCount=dwCount & 0xFFFF;
	if(usCount > 32)
		return ERR_CROSS_BORDER;
	wSectorSize = dwCount >> 16;
	if(wSectorSize==0)
		wSectorSize = 528;
	
	InitializeCBW(&cbw, WRITE_SECTOR);
	cbw.dwCBWTransferLength = (uint32_t)usCount * (uint32_t)wSectorSize;
	cbw.cbwcb.dwAddress = EndianU32_LtoB(dwPos);
	cbw.cbwcb.usLength = EndianU16_LtoB(usCount);
	
	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;	
	}
	
	if(!RKU_Write( lpBuffer, usCount * (uint32_t)wSectorSize))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;	
	}
	RKU_Close();
	
	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;	
	}
	RKU_Close();
	
	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	if(csw.ucCSWStatus == 1)
		return ERR_FAILED;					
	
	return ERR_SUCCESS;						
}

int CRKUsbComm::RKU_ReadNewEfuse(uint_16 usPos,uint_16 usSize,uint_8 *lpBuffer,uint_8 bySubCode)
{
	if (m_deviceDesc.emUsbType!=RKUSB_MASKROM) {
        if (m_log) {
            m_log->Record("Error:RKU_ReadNewEfuse failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
	CBW cbw;
	CSW csw;
	uint_32 dwPos;
	
	InitializeCBW(&cbw, READ_NEW_EFUSE);
	if (bySubCode==EFUSE_KEYHASH)
	{
		dwPos = 0;
	}
	else if (bySubCode==EFUSE_SECURE_FLAG) {
		dwPos = 0;
		usSize = 0;
	} else
		dwPos = usPos;
	cbw.dwCBWTransferLength = usSize;
	cbw.cbwcb.dwAddress = EndianU32_LtoB(dwPos);
	cbw.cbwcb.usLength = EndianU16_LtoB(usSize);
	cbw.cbwcb.ucReserved = bySubCode;
	
	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;	
	}
	RKU_Close();

	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if (bySubCode!=EFUSE_SECURE_FLAG) {
		if(!RKU_Read( lpBuffer, usSize))
		{
			RKU_Close();
			return ERR_DEVICE_READ_FAILED;	
		}
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	RKU_Close();
	
	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;		
	
	if (bySubCode==EFUSE_SECURE_FLAG)
	{
		bool *pFlag = (bool *)lpBuffer;
		if (csw.ucCSWStatus == 1)
		{
			*pFlag = true;
			return ERR_SUCCESS;
		}
		else if (csw.ucCSWStatus == 0)
		{
			*pFlag = false;
			return ERR_SUCCESS;
		}
		else
			return ERR_FAILED;
	
	}else{
		if(csw.ucCSWStatus == 0)
			return ERR_SUCCESS;
		else
			return ERR_FAILED;
	}					
}

int CRKUsbComm::RKU_WriteNewEfuse(uint_16 usPos,uint_16 usSize,uint_8 *lpBuffer,uint_8 bySubCode)
{
	if (m_deviceDesc.emUsbType!=RKUSB_MASKROM) {
        if (m_log) {
            m_log->Record("Error:RKU_WriteNewEfuse failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
	CBW cbw;
	CSW csw;
	uint_32 dwPos;

	InitializeCBW(&cbw, WRITE_NEW_EFUSE);
	if (bySubCode==EFUSE_KEYHASH)
	{
		dwPos = 0;
		*((uint_16 *)cbw.cbwcb.ucReserved3) = EndianU16_LtoB(usPos);
	}
	else
		dwPos = usPos;
	cbw.dwCBWTransferLength = usSize;
	cbw.cbwcb.dwAddress = EndianU32_LtoB(dwPos);
	cbw.cbwcb.usLength = EndianU16_LtoB(usSize);
	cbw.cbwcb.ucReserved = bySubCode;

	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;		
	}
	
	if(!RKU_Write( lpBuffer, usSize))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;		
	}
	RKU_Close();
	
	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;	
	}
	RKU_Close();

	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	if(csw.ucCSWStatus == 1)
		return ERR_FAILED;				
	
	return ERR_SUCCESS;	
}

int CRKUsbComm::RKU_ResetPipe(uint_8 byPipe)
{
	BOOL bRet;
	int ret=ERR_FAILED;
	uint_32 dwRet;
	if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_ResetPipe failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }

	if(!RKU_Open(USB_CONTROL))
	{
		if (m_log)
		{
			m_log->Record("Error:RKU_ResetPipe-->RKU_Open failed");
		}
		goto Exit_ResetPipe;
	}
	
	bRet = DeviceIoControl(m_pUsbHandle,RESET_PIPE,&byPipe,1,NULL,0,(LPDWORD)&dwRet,NULL);
	if (!bRet)
	{
		if (m_log)
		{
			m_log->Record("Error:RKU_ResetPipe-->DeviceIoControl failed,err=%d",GetLastError());
		}
		goto Exit_ResetPipe;
	}
	else
	{
		if (m_log)
		{
			m_log->Record("Info:RKU_ResetPipe-->DeviceIoControl OK");
		}
		ret = ERR_SUCCESS;
	}

Exit_ResetPipe:
	RKU_Close();
	return ret;
}

int CRKUsbComm::RKU_DeviceRequest(uint_32 dwRequest,uint_8* lpBuffer,uint_32 dwDataSize,bool crcoff)
{
	
	if (m_deviceDesc.emUsbType!=RKUSB_MASKROM) {
	    if (m_log) {
	        m_log->Record("Error:RKU_DeviceRequest failed,device not support");
	    }
	    return ERR_DEVICE_NOT_SUPPORT;
	}
	if ((dwRequest!=0x0471)&&(dwRequest!=0x0472)) {
		if (m_log) {
	        m_log->Record("Error:RKU_DeviceRequest failed,request not support");
	    }
	    return ERR_REQUEST_NOT_SUPPORT;
	}
	if (dwRequest==0x0471)
		dwRequest = DOWNLOAD_471;
	else
		dwRequest = DOWNLOAD_472;

	if (!RKU_Open(USB_CONTROL)) {
		if (m_log) {
	        m_log->Record("Error:RKU_DeviceRequest RKU_Open failed");
	    }
	    return ERR_DEVICE_OPEN_FAILED;
	}

	bool bSendPendPacket=false;
	uint_16 crcValue = 0;
	uint_8* pData=NULL;
	pData = new uint_8[dwDataSize+5]; 
	memset(pData, 0,dwDataSize+5);

	memcpy(pData,lpBuffer,dwDataSize);
	
	switch(dwDataSize%4096)
	{
		case 4095: 
			++dwDataSize;
			break;
		case 4094:
			bSendPendPacket = true;
			break;
		case 0:
		default:
			break;
	}
	if (!crcoff)
		crcValue = CRC_CCITT(pData, dwDataSize);
	pData[dwDataSize] = (crcValue & 0xff00)>>8;
	pData[dwDataSize+1] = crcValue & 0x00ff;	
	dwDataSize += 2;
	
	uint_32 nSendBytes = 0;
	uint_32 dwTotalSended = 0; 
	uint_32 dwReturnByte = 0;
	BOOL bRet;
	
	while(dwTotalSended < dwDataSize)
	{

		nSendBytes = ( (dwDataSize-dwTotalSended) > 4096) ? 4096 : (dwDataSize-dwTotalSended);
		bRet = DeviceIoControl(m_pUsbHandle,dwRequest,pData+dwTotalSended,nSendBytes,NULL,0,(LPDWORD)&dwReturnByte,NULL);
		if (!bRet||(dwReturnByte!=nSendBytes))
		{
			if (m_log) {
				m_log->Record("Error:RKU_DeviceRequest-->DeviceRequest vendor=0x%x failed,err=%d",dwRequest,GetLastError());
			}
			RKU_Close();
			delete []pData;
			return ERR_REQUEST_FAIL;
			
		}
		dwTotalSended += nSendBytes;
	}
	
	if(bSendPendPacket)
	{
		uint_8 ucFillByte = 0;
		bRet = DeviceIoControl(m_pUsbHandle,dwRequest,&ucFillByte,1,NULL,0,(LPDWORD)&dwReturnByte,NULL);
		if (!bRet||(dwReturnByte!=1))
		{
			if (m_log) {
				m_log->Record("Error:RKU_DeviceRequest-->DeviceRequest vendor=0x%x fillbyte failed,err=%d",dwRequest,GetLastError());
			}
			RKU_Close();
			delete []pData;
			return ERR_REQUEST_FAIL;
		}		
	}
	
	RKU_Close();
	delete []pData;	
	
    return ERR_SUCCESS;
}
int CRKUsbComm::RKU_MscToRockusb()
{
	return ERR_REQUEST_NOT_SUPPORT;
}
int CRKUsbComm::RKU_UvcToRockusb()
{
	return ERR_REQUEST_NOT_SUPPORT;
}
int CRKUsbComm::RKU_EraseLBA(uint_32 dwPos,uint_32 dwCount)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_EraseLBA failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
	CBW cbw;
	CSW csw;
	uint_16 wSectorSize;
	uint_16 usCount;
	wSectorSize = 512;
	usCount = dwCount;
	uint_32 dwTotal = usCount*wSectorSize;
	
	InitializeCBW(&cbw, ERASE_LBA);
	cbw.cbwcb.dwAddress = EndianU32_LtoB(dwPos);
	cbw.cbwcb.usLength = EndianU16_LtoB(usCount);

	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;		
	}
	RKU_Close();
	
	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;	
	}
	RKU_Close();

	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	if(csw.ucCSWStatus == 1)
		return ERR_FAILED;				
	
	return ERR_SUCCESS;						
}

int CRKUsbComm::RKU_ReadComData(uint_8* lpBuffer,uint_32 dwCount)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_ReadComData failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
	CBW cbw;
	CSW csw;
	uint_16 wSectorSize;
	uint_16 usSectorLen;
	wSectorSize = 512;
	usSectorLen=dwCount;
	
	InitializeCBW(&cbw, READ_COM_DATA);

	cbw.cbwcb.usLength = EndianU16_LtoB(usSectorLen);

	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;	
	}
	RKU_Close();

	uint_32 dwTotal;
	dwTotal = usSectorLen * wSectorSize;

	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if(!RKU_Read(lpBuffer, dwTotal))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;	
	}
	
	
	if(!RKU_Read((uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	RKU_Close();
	
	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	if(csw.ucCSWStatus == 1)
		return ERR_FAILED;				
	
	return ERR_SUCCESS;					
}
int CRKUsbComm::RKU_WriteSDRam(uint_32 dwCount,uint_8* lpBuffer)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:%s failed,device not support", __func__);
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
	CBW cbw;
	CSW csw;
	uint_16 usCount=dwCount;

	
	InitializeCBW(&cbw, WRITE_SDRAM);
	cbw.dwCBWTransferLength = dwCount;
	cbw.cbwcb.ucReserved = 0xAA;
	cbw.cbwcb.usLength = EndianU16_LtoB(usCount);
	
	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;		
	}
	
	if(!RKU_Write( lpBuffer, usCount))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;		
	}
	RKU_Close();
	
	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;	
	}
	RKU_Close();
	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	if(csw.ucCSWStatus == 1)
		return ERR_FAILED;				
	
	return ERR_SUCCESS;						
}
int CRKUsbComm::RKU_RunSDRam()
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:%s failed,device not support", __func__);
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
    
	CBW cbw;
	CSW csw;
	
	InitializeCBW(&cbw, EXECUTE_SDRAM);
	cbw.cbwcb.ucReserved = 0xAA;
	
	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;		
	}
	RKU_Close();
	
	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	RKU_Close();

	if( !UFI_CHECK_SIGN(cbw, csw) )
	{
		return ERR_CMD_NOTMATCH;
	}
	
	if(csw.ucCSWStatus == 1)
		return ERR_FAILED;					
	
	return ERR_SUCCESS;						
}

int CRKUsbComm::RKU_ReadStorageList(uint_8* lpBuffer)
{
	if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_ReadStorageList failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
    
	CBW cbw;
	CSW csw;
	
	InitializeCBW(&cbw, LIST_STORAGE);
	cbw.dwCBWTransferLength = 4;

	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;	
	}
	RKU_Close();
	
	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if(!RKU_Read(lpBuffer, 4))
	{	
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;		
	}
	RKU_Close();
	
	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;

	if(csw.ucCSWStatus != 0 )
		return ERR_DEVICE_UNREADY;	
	
	return ERR_SUCCESS;
}
int CRKUsbComm::RKU_SwitchStorage(uint_8 emStorageCode)
{
	if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_SwitchStorage failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
    
	CBW cbw;
	CSW csw;

	InitializeCBW(&cbw, SWITCH_STORAGE);
	cbw.cbwcb.ucReserved = emStorageCode;
	
	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;		
	}
	RKU_Close();

	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if(!RKU_Read((uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;	
	}
	RKU_Close();

	if( !UFI_CHECK_SIGN(cbw, csw) )
	{
		return ERR_CMD_NOTMATCH;
	}
	
	if(csw.ucCSWStatus == 1)
		return ERR_FAILED;					
	
	return ERR_SUCCESS;	
}

int CRKUsbComm::RKU_SwitchUsb3(char *pszSn)
{
    if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:%s failed,device not support", __func__);
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
	CBW cbw;
	CSW csw;
	size_t len;

	len = strlen(pszSn);
	
	InitializeCBW(&cbw, SWITCH_USB3);
	if (len > 8)
		memcpy(&cbw.cbwcb.ucReserved, pszSn, 8);
	else
		memcpy(&cbw.cbwcb.ucReserved, pszSn, len);
	
	if(!RKU_Open(USB_BULK_WRITE))
		return ERR_DEVICE_OPEN_FAILED;

	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		RKU_Close();
		return ERR_DEVICE_WRITE_FAILED;		
	}
	
	RKU_Close();
	
	if(!RKU_Open(USB_BULK_READ))
		return ERR_CSW_OPEN_FAILED;

	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{
		RKU_Close();
		return ERR_DEVICE_READ_FAILED;	
	}
	RKU_Close();
	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	if(csw.ucCSWStatus == 1)
		return ERR_FAILED;				
	
	return ERR_SUCCESS;						
}
