#include "RKComm.h"
#include "../../RKLog.h"

extern unsigned short CRC_CCITT(unsigned char* p, uint_32 CalculateNumber);
extern bool g_bCBW_parity;
CRKComm::CRKComm(CRKLog *pLog)
{
	memset((void*)&m_deviceDesc,0,18);
	m_deviceDesc.pUsbHandle = NULL;
	m_deviceDesc.strDevPath = "";
	m_deviceDesc.strSerial = "";
	m_log = pLog;
}
CRKUsbComm::CRKUsbComm(STRUCT_RKDEVICE_DESC devDesc,CRKLog *pLog,bool &bRet):CRKComm(pLog)
{	
	bRet = InitializeUsb(devDesc);
}
CRKUsbComm::~CRKUsbComm()
{
	UninitializeUsb();
}

bool CRKUsbComm::InitializeUsb(STRUCT_RKDEVICE_DESC &devDesc)
{
	m_pUsbHandle = NULL;
	m_pipeBulkIn = m_pipeBulkOut = 0;
	m_interfaceNum = -1;
	if (!devDesc.pUsbHandle) {
		if (m_log) {
			m_log->Record("Error:InitializeUsb-->device is null");
		}
		return false;
	}
	m_deviceDesc.usVid = devDesc.usVid;
	m_deviceDesc.usPid = devDesc.usPid;
	m_deviceDesc.usbcdUsb = devDesc.usbcdUsb;
	m_deviceDesc.uiLocationID = devDesc.uiLocationID;
	m_deviceDesc.emUsbType = devDesc.emUsbType;
	m_deviceDesc.emDeviceType = devDesc.emDeviceType;
	m_deviceDesc.pUsbHandle = devDesc.pUsbHandle;
	m_deviceDesc.strSerial = devDesc.strSerial;
	m_deviceDesc.strDevPath = devDesc.strDevPath;
	int iRet;
	iRet = libusb_open((libusb_device *)devDesc.pUsbHandle,(libusb_device_handle **)&m_pUsbHandle);
	if (iRet!=0)
	{
		if (m_log) {
			m_log->Record("Error:InitializeUsb-->open device failed,err=%d,device=%p",iRet,devDesc.pUsbHandle);
		}
		return false;
	}
	struct libusb_config_descriptor *pConfigDesc=NULL;
	iRet = libusb_get_active_config_descriptor((libusb_device *)devDesc.pUsbHandle,&pConfigDesc);
	if (iRet!=0)
	{
		if (m_log) {
			m_log->Record("Error:InitializeUsb-->get device config descriptor failed,err=%d",iRet);
		}
		return false;
	}
	if (m_log) {
		m_log->Record("Info:InitializeUsb-->device has %d interface",pConfigDesc->bNumInterfaces);
	}
	int i,j,k;
	const struct libusb_interface *pInterface;
	struct libusb_endpoint_descriptor *pEndpointDesc;
	const struct libusb_interface_descriptor *pInterfaceDesc;
	unsigned char buf[256];
	for(i=0;i<pConfigDesc->bNumInterfaces;i++)
	{
		pInterface = pConfigDesc->interface + i;
		//printf("Info:intf=%d,num_alt=%d\r\n",i,pInterface->num_altsetting);
		// if (m_log) {
		// 	m_log->Record("Info:intf=%d,num_alt=%d",i,pInterface->num_altsetting);
		// }
		for(j=0;j<pInterface->num_altsetting;j++)
		{
			pInterfaceDesc = pInterface->altsetting+j;
			if (m_log) {
				m_log->Record("Info:class=%02x,sub=%02x,prot=%02x",pInterfaceDesc->bInterfaceClass,pInterfaceDesc->bInterfaceSubClass,pInterfaceDesc->bInterfaceProtocol);
			}
			//printf("Info:class=%d,sub=%d,prot=%d\r\n",pInterfaceDesc->bInterfaceClass,pInterfaceDesc->bInterfaceSubClass,pInterfaceDesc->bInterfaceProtocol);	
			if (m_deviceDesc.usPid==UVC_PID)
			{
				if( (pInterfaceDesc->bInterfaceClass!=UVC_INTF_CLASS)||(pInterfaceDesc->bInterfaceSubClass!=UVC_INTF_SUB_CLASS)||(pInterfaceDesc->bInterfaceProtocol!=UVC_INTF_PROTOCOL))
					continue;
			}
			else if(m_deviceDesc.emUsbType==RKUSB_MSC){
				if( (pInterfaceDesc->bInterfaceClass!=8)||(pInterfaceDesc->bInterfaceSubClass!=6)||(pInterfaceDesc->bInterfaceProtocol!=0x50))
					continue;
			}
			else
			{
				if( (pInterfaceDesc->bInterfaceClass!=0xff)||(pInterfaceDesc->bInterfaceSubClass!=6)||(pInterfaceDesc->bInterfaceProtocol!=5))
					continue;
			}
			//printf("Info:alt=%d,num_ep=%d\r\n",j,pInterfaceDesc->bNumEndpoints);
			for(k=0;k<pInterfaceDesc->bNumEndpoints;k++)
			{
				if (pInterfaceDesc->endpoint == NULL) {
					iRet = libusb_get_descriptor((libusb_device_handle *)m_pUsbHandle, LIBUSB_DT_ENDPOINT, k, buf, 256);
					if (iRet<0) {
						if (m_log) {
							m_log->Record("Error:InitializeUsb-->get device endpoint descriptor failed,err=%d",iRet);
						}
						return false;
					}
					pEndpointDesc = (struct libusb_endpoint_descriptor *)buf;
				}
				else	
					pEndpointDesc = (struct libusb_endpoint_descriptor *)(pInterfaceDesc->endpoint+k);
				//printf("Info:ep_addr=%x\r\n",pEndpointDesc->bEndpointAddress);
				if ((pEndpointDesc->bEndpointAddress&0x80)==0)
				{
					if (m_pipeBulkOut==0)
						m_pipeBulkOut = pEndpointDesc->bEndpointAddress;
				}
				else
				{
					if (m_pipeBulkIn==0)
						m_pipeBulkIn= pEndpointDesc->bEndpointAddress;
				}
				if ((m_deviceDesc.usPid==UVC_PID)||(m_deviceDesc.emUsbType==RKUSB_MSC)){
					if ((m_pipeBulkIn!=0)||(m_pipeBulkOut!=0))
					{//found it
						m_interfaceNum = i;
						libusb_free_config_descriptor(pConfigDesc);
//							iRet = libusb_claim_interface((libusb_device_handle *)m_pUsbHandle,m_interfaceNum);
//							if (iRet!=0)
//							{
//								if (m_log) 
//								{
//							    	m_log->Record("Error:libusb_claim_interface failed,err=%d",iRet);
//							    }
//							    return false;
//							}
						return true;
					}
				} else {
					if ((m_pipeBulkIn!=0)&&(m_pipeBulkOut!=0))
					{//found it
						m_interfaceNum = i;
						libusb_free_config_descriptor(pConfigDesc);
						iRet = libusb_claim_interface((libusb_device_handle *)m_pUsbHandle,m_interfaceNum);
						if (iRet!=0)
						{
							if (m_log) 
							{
						    	m_log->Record("Error:libusb_claim_interface failed,err=%d",iRet);
						    }
						    return false;
						}
						return true;
					}
				}
				
			}
		}
	}
	libusb_free_config_descriptor(pConfigDesc);
	if (m_log) {
		m_log->Record("Info:InitializeUsb-->out,interface=%d,bulkin=%02x,bulkout=%02x",m_interfaceNum,m_pipeBulkIn,m_pipeBulkOut);
	}
	return false;
}
void CRKUsbComm::UninitializeUsb()
{
    int iRet;
	if (m_pUsbHandle) {
		if (m_interfaceNum!=-1) {
			iRet = libusb_release_interface((libusb_device_handle *)m_pUsbHandle,m_interfaceNum);
			if (iRet!=0)
			{
				if (m_log) 
				{
			    	m_log->Record("Error:libusb_release_interface failed,err=%d",iRet);
			    }
			}
		}
			
		libusb_close((libusb_device_handle *)m_pUsbHandle);
		m_pUsbHandle = NULL;
	}
	memset((void*)&m_deviceDesc,0,18);
	m_deviceDesc.pUsbHandle = NULL;
	m_deviceDesc.strDevPath = "";
	m_deviceDesc.strSerial = "";
	m_pipeBulkIn = m_pipeBulkOut = 0;
	return ;
}
bool CRKUsbComm::Reset_Usb_Config(STRUCT_RKDEVICE_DESC devDesc)
{
    bool bRet;
    UninitializeUsb();
    bRet = InitializeUsb(devDesc);
    return bRet;
}
bool CRKUsbComm::Reset_Usb_Device()
{
	int iRet=-1;
    if (m_pUsbHandle) {
		iRet=libusb_reset_device((libusb_device_handle *)m_pUsbHandle);
	}
    return (iRet==0)?true:false;
}

bool CRKUsbComm::RKU_Read(uint_8* lpBuffer, uint_32 dwSize)
{
	int  iRet;
	int  nRead;
	iRet = libusb_bulk_transfer((libusb_device_handle *)m_pUsbHandle,m_pipeBulkIn,lpBuffer,dwSize,&nRead,CMD_TIMEOUT_5S);	
	if (iRet!=0)
	{
	    if (m_log) {
	        m_log->Record("Error:RKU_Read failed,err=%d",iRet);
	    }
	    return false;
	}
	if (static_cast<uint_32>(nRead)!=dwSize)
	{
       	if (m_log) 
		{
            m_log->Record("Error:RKU_Read failed,size=%d,read=%d",dwSize,nRead);
        }
        return false;
    }
	return true;
}

bool CRKUsbComm::RKU_Write(uint_8* lpBuffer,uint_32 dwSize)
{
	int  iRet;
	int nWrite;
	iRet = libusb_bulk_transfer((libusb_device_handle *)m_pUsbHandle,m_pipeBulkOut,lpBuffer,dwSize,&nWrite,CMD_TIMEOUT_5S);	
	if (iRet!=0)
	{
	    if (m_log) {
	        m_log->Record("Error:RKU_Write failed,err=%d",iRet);
	    }
	    return false;
	}
    if (static_cast<uint_32>(nWrite)!=dwSize)
	{
       	if (m_log) 
		{
            m_log->Record("Error:RKU_Write failed,size=%d,read=%d",dwSize,nWrite);
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
	int iTryCount;
	iTryCount = 3;
	do
	{
		dwReadBytes = RKU_Read_EX((uint_8*)&csw, sizeof(CSW) );

		if (UFI_CHECK_SIGN(cbw,csw))
		{
			return true;
		}
		if (dwReadBytes!=sizeof(CSW))
		{
			iTryCount--;
			sleep_s(3);
		}
		dwTotalRead += dwReadBytes;
		if (dwTotalRead>=MAX_CLEAR_LEN)
		{
			break;
		}
	}while ( iTryCount>0 );
	return false;
}

uint_32 CRKUsbComm::RKU_Read_EX(uint_8* lpBuffer, uint_32 dwSize)
{
	int  iRet;
	int  nRead;
	iRet = libusb_bulk_transfer((libusb_device_handle *)m_pUsbHandle,m_pipeBulkIn,lpBuffer,dwSize,&nRead,5000);	
	if (iRet!=0)
	{
	    if (m_log) {
	        m_log->Record("Error:RKU_Read_EX failed,err=%d",iRet);
	    }
	    return 0;
	}

	return nRead;
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
	
	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;		
	}

	
	if(!RKU_Read((uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}

	
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
	
	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;	
	}

	
	if(!RKU_Read(lpBuffer, 16))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}

	
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
	
	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;	
	}

	
	if(!RKU_Read(lpBuffer, 5))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}

	
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

	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;	
	}

	if(!RKU_Read(lpBuffer, 32))
	{
		return ERR_DEVICE_READ_FAILED;		
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{
		return ERR_DEVICE_READ_FAILED;		
	}
	
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
	
	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;		
	}

	uint_32 dwRead;
	dwRead = RKU_Read_EX(lpBuffer,512);
	if ((dwRead<11)||(dwRead>512))
	{
		return ERR_DEVICE_READ_FAILED;
	}
	if (puiRead)
	{
		*puiRead = dwRead;
	}
/*	
	if(!RKU_Read(hDev, lpBuffer, 11))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}
*/	
	if(!RKU_Read((uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}
	
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
	
	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;	
	}

	dwRead = RKU_Read_EX((uint_8*)&csw, sizeof(CSW));
		
	if(dwRead!=8)
	{	
		return ERR_DEVICE_READ_FAILED;		
	}
	memcpy(lpBuffer,(uint_8*)&csw,8);
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}

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
	
	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;	
	}
	uint_32 dwTotal;
	dwTotal = usSectorLen * wSectorSize;

	if(!RKU_Read(lpBuffer, dwTotal))
	{	
		return ERR_DEVICE_READ_FAILED;	
	}
	
	
	if(!RKU_Read((uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}

	
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
	
	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;		
	}
	
	if(!RKU_Read(lpBuffer, usSectorLen * (uint32_t)wSectorSize))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}
	
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
	
	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;		
	}
		
	if(!RKU_Read((uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;	
	}
	
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
	
	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;	
	}

	
	if(!RKU_Read(lpBuffer, MAX_TEST_BLOCKS/8))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;	
	}
	
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
	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;	
	}

	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;	
	}

	
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
	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;		
	}
	
	if(!RKU_Write( lpBuffer, dwTotal))
	{
		return ERR_DEVICE_WRITE_FAILED;	
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;	
	}
	
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
	if (wSectorSize==0)
	{
		wSectorSize = 528;
	}
	
	InitializeCBW(&cbw, WRITE_SECTOR);
	cbw.dwCBWTransferLength = (uint32_t)usCount * (uint32_t)wSectorSize;
	cbw.cbwcb.dwAddress = EndianU32_LtoB(dwPos);
	cbw.cbwcb.usLength = EndianU16_LtoB(usCount);
	
	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;	
	}
	
	if(!RKU_Write( lpBuffer, usCount * (uint32_t)wSectorSize))
	{
		return ERR_DEVICE_WRITE_FAILED;	
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;	
	}
	
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
	
	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;	
	}

	if (bySubCode!=EFUSE_SECURE_FLAG) {
		if(!RKU_Read( lpBuffer, usSize))
		{	
			return ERR_DEVICE_READ_FAILED;	
		}
	}
	
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}
	
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

	
	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;		
	}
	
	if(!RKU_Write( lpBuffer, usSize))
	{
		return ERR_DEVICE_WRITE_FAILED;		
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;	
	}
	
	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	if(csw.ucCSWStatus == 1)
		return ERR_FAILED;				
	
	return ERR_SUCCESS;	
}

int CRKUsbComm::RKU_ResetPipe(uint_8 byPipe)
{
	int iRet;
	if ((m_deviceDesc.emUsbType!=RKUSB_LOADER)&&(m_deviceDesc.emUsbType!=RKUSB_MASKROM)) {
        if (m_log) {
            m_log->Record("Error:RKU_ResetPipe failed,device not support");
        }
        return ERR_DEVICE_NOT_SUPPORT;
    }
	if (byPipe==2)
	{
		iRet = libusb_control_transfer((libusb_device_handle *)m_pUsbHandle,0x2,0x1,0,m_pipeBulkIn,NULL,0,CMD_TIMEOUT_5S);
		if (iRet<0)
		{
			if (m_log) {
				m_log->Record("Error:RKU_ResetPipe-->reset read pipe failed,err=%d",iRet);
			}
			return ERR_REQUEST_FAIL;
		}
		iRet = libusb_control_transfer((libusb_device_handle *)m_pUsbHandle,0x2,0x1,0,m_pipeBulkOut,NULL,0,CMD_TIMEOUT_5S);
		if (iRet<0)
		{
			if (m_log) {
				m_log->Record("Error:RKU_ResetPipe-->reset write pipe failed,err=%d",iRet);
			}
			return ERR_REQUEST_FAIL;
		}
	}
	else if(byPipe==1)
	{
		iRet = libusb_control_transfer((libusb_device_handle *)m_pUsbHandle,0x2,0x1,0,m_pipeBulkOut,NULL,0,CMD_TIMEOUT_5S);
		if (iRet<0)
		{
			if (m_log) {
				m_log->Record("Error:RKU_ResetPipe-->reset write pipe failed,err=%d",iRet);
			}
			return ERR_REQUEST_FAIL;
		}
	}
	else
	{
		iRet = libusb_control_transfer((libusb_device_handle *)m_pUsbHandle,0x2,0x1,0,m_pipeBulkIn,NULL,0,CMD_TIMEOUT_5S);
		if (iRet<0)
		{
			if (m_log) {
				m_log->Record("Error:RKU_ResetPipe-->reset read pipe failed,err=%d",iRet);
			}
			return ERR_REQUEST_FAIL;
		}
	}
	return ERR_SUCCESS;
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
	int iRet;
	
	while(dwTotalSended < dwDataSize)
	{

		nSendBytes = ( (dwDataSize-dwTotalSended) > 4096) ? 4096 : (dwDataSize-dwTotalSended);
		iRet = libusb_control_transfer((libusb_device_handle *)m_pUsbHandle,0x40,0xC,0,dwRequest,pData+dwTotalSended,nSendBytes,CMD_TIMEOUT_5S);
		if (iRet!=static_cast<int>(nSendBytes))
		{
			if (m_log) {
				m_log->Record("Error:RKU_DeviceRequest-->DeviceRequest vendor=0x%x failed,err=%d",dwRequest,iRet);
			}
			delete []pData;
			return ERR_REQUEST_FAIL;
			
		}
		dwTotalSended += nSendBytes;
	}
	
	if(bSendPendPacket)
	{
		uint_8 ucFillByte = 0;
		iRet = libusb_control_transfer((libusb_device_handle *)m_pUsbHandle,0x40,0xC,0,dwRequest,&ucFillByte,1,CMD_TIMEOUT_5S);
		if (iRet!=0)
		{
			if (m_log) {
				m_log->Record("Error:RKU_DeviceRequest-->DeviceRequest vendor=0x%x failed,err=%d",dwRequest,iRet);
			}
			delete []pData;
			return ERR_REQUEST_FAIL;
		}		
	}
	
	delete []pData;	
	
    return ERR_SUCCESS;
}
int CRKUsbComm::RKU_MscToRockusb()
{
	if (m_deviceDesc.emUsbType!=RKUSB_MSC) {
	    if (m_log) {
	        m_log->Record("Error:RKU_MscToRockusb failed,device not support");
	    }
	    return ERR_DEVICE_NOT_SUPPORT;
	}
	int iRet;
	iRet = libusb_kernel_driver_active((libusb_device_handle *)m_pUsbHandle,m_interfaceNum);
	if( (iRet!=0)&&(iRet!=1))
	{
		if (m_log) 
		{
	      		m_log->Record("Error:libusb_kernel_driver_active failed,err=%d",iRet);
	    	}
	    	return ERR_DEVICE_NOT_SUPPORT;
	}
	else
	{
		if (iRet==1)
		{
			iRet = libusb_detach_kernel_driver((libusb_device_handle *)m_pUsbHandle,m_interfaceNum);
			if (iRet!=0)
			{
				if (m_log) 
				{
			      		m_log->Record("Error:libusb_detach_kernel_driver failed,err=%d",iRet);
			    	}
			    	return ERR_DEVICE_NOT_SUPPORT;
			}
			iRet = libusb_claim_interface((libusb_device_handle *)m_pUsbHandle,m_interfaceNum);
			if (iRet!=0)
			{
				if (m_log) 
				{
			      		m_log->Record("Error:libusb_claim_interface failed,err=%d",iRet);
			    	}
			    	return ERR_DEVICE_NOT_SUPPORT;
			}
			
		}
	}
	CBW cbw;
	CSW csw;

	InitializeCBW(&cbw, DEVICE_RESET);
	cbw.ucCBWCBLength = 0xa;
	cbw.cbwcb.ucReserved = 0xe0;
	cbw.cbwcb.dwAddress = EndianU32_LtoB(0xfffffffe);
	
	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;		
	}
		
	if(!RKU_Read((uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;	
	}
	
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
int CRKUsbComm::RKU_UvcToRockusb()
{
	if (m_deviceDesc.usPid!=UVC_PID) {
	    if (m_log) {
	        m_log->Record("Error:RKU_UvcToRockusb failed,device not support");
	    }
	    return ERR_DEVICE_NOT_SUPPORT;
	}
	int iRet;
	iRet = libusb_kernel_driver_active((libusb_device_handle *)m_pUsbHandle,m_interfaceNum);
	if( (iRet!=0)&&(iRet!=1))
	{
		if (m_log) 
		{
	      		m_log->Record("Error:RKU_UvcToRockusb failed,libusb_kernel_driver_active failed,err=%d",iRet);
	    }
	    return ERR_DEVICE_NOT_SUPPORT;
	}
	else
	{
		if (iRet==1)
		{
			iRet = libusb_detach_kernel_driver((libusb_device_handle *)m_pUsbHandle,m_interfaceNum);
			if (iRet!=0)
			{
				if (m_log) 
				{
			      		m_log->Record("Error:RKU_UvcToRockusb failed,libusb_detach_kernel_driver failed,err=%d",iRet);
			    	}
			    	return ERR_DEVICE_NOT_SUPPORT;
			}
			iRet = libusb_claim_interface((libusb_device_handle *)m_pUsbHandle,m_interfaceNum);
			if (iRet!=0)
			{
				if (m_log) 
				{
			      		m_log->Record("Error:RKU_UvcToRockusb failed,libusb_claim_interface failed,err=%d",iRet);
			    	}
			    	return ERR_DEVICE_NOT_SUPPORT;
			}
			
		}
	}
//		unsigned int buf;
//		iRet = libusb_control_transfer((libusb_device_handle *)m_pUsbHandle,0xa1,0x85,0x0100,0,(unsigned char*)&buf,2,CMD_TIMEOUT_5S);
//		if (iRet<0)
//		{
//			if (m_log) {
//				m_log->Record("Error:RKU_UvcToRockusb-->libusb_control_transfer failed,cmd=a1,err=%d",iRet);
//			}
//			return ERR_REQUEST_FAIL;
//		}

	unsigned int data = 0xFFFFFFFF;
	iRet = libusb_control_transfer((libusb_device_handle *)m_pUsbHandle,0x21,0x01,0x0100,0x0600,(unsigned char*)&data,4,CMD_TIMEOUT_5S);
	if (iRet<0)
	{
		if (m_log) {
			m_log->Record("Error:RKU_UvcToRockusb-->libusb_control_transfer failed,cmd=21,err=%d",iRet);
		}
		return ERR_REQUEST_FAIL;
	}
	int nWrite;
	libusb_bulk_transfer((libusb_device_handle *)m_pUsbHandle,m_pipeBulkOut,(unsigned char*)&data,4,&nWrite,CMD_TIMEOUT_5S);
//		iRet = libusb_bulk_transfer((libusb_device_handle *)m_pUsbHandle,m_pipeBulkOut,(unsigned char*)&data,4,&nWrite,CMD_TIMEOUT_5S);	
//		if (iRet<0)
//		{
//		    if (m_log) {
//		        m_log->Record("Error:RKU_UvcToRockusb-->libusb_bulk_transfer failed,err=%d",iRet);
//		    }
//		    return ERR_REQUEST_FAIL;
//		}
//	    if (nWrite!=4) 
//		{
//	       	if (m_log) 
//			{
//	           m_log->Record("Error:RKU_UvcToRockusb-->libusb_bulk_transfer failed,size=4,write=%d",nWrite);
//	        }
//	        return ERR_REQUEST_FAIL;
//	    }

	return ERR_SUCCESS;
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
	uint_16 usCount;
	usCount = dwCount;
	
	InitializeCBW(&cbw, ERASE_LBA);
	cbw.cbwcb.dwAddress = EndianU32_LtoB(dwPos);
	cbw.cbwcb.usLength = EndianU16_LtoB(usCount);

	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;		
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;	
	}
	
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

	
	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;	
	}
	uint_32 dwTotal;
	dwTotal = usSectorLen * wSectorSize;

	if(!RKU_Read(lpBuffer, dwTotal))
	{	
		return ERR_DEVICE_READ_FAILED;	
	}
	
	
	if(!RKU_Read((uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}

	
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
	
	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;		
	}
	
	if(!RKU_Write( lpBuffer, usCount))
	{
		return ERR_DEVICE_WRITE_FAILED;		
	}
	
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;	
	}
	
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
	
	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;		
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}
	
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
	
	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;	
	}

	
	if(!RKU_Read(lpBuffer, 4))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;		
	}

	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;

	if(csw.ucCSWStatus != 0)
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
	
	if(!RKU_Write((uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;		
	}
		
	if(!RKU_Read((uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;	
	}
	
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
	
	if(!RKU_Write( (uint_8*)&cbw, sizeof(CBW)))
	{
		return ERR_DEVICE_WRITE_FAILED;		
	}
	
	if(!RKU_Read( (uint_8*)&csw, sizeof(CSW)))
	{	
		return ERR_DEVICE_READ_FAILED;	
	}
	
	if( !UFI_CHECK_SIGN(cbw, csw) )
		return ERR_CMD_NOTMATCH;			
	
	if(csw.ucCSWStatus == 1)
		return ERR_FAILED;				
	
	return ERR_SUCCESS;						
}