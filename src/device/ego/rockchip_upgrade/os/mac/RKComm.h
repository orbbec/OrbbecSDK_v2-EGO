#ifndef RKCOMM_HEADER
#define RKCOMM_HEADER
#include "../../DefineHeader.h"
typedef enum{
		USB_BULK_READ=0,
		USB_BULK_WRITE,
		USB_CONTROL,
}USB_ACCESS_TYPE;
typedef enum
{
	TU_NONE_SUBCODE=0,
	TU_ERASESYSTEM_SUBCODE=0xFE,
	TU_LOWERFORMAT_SUBCODE=0xFD,
	TU_ERASEUSERDATA_SUBCODE=0xFB,
	TU_GETUSERSECTOR_SUBCODE=0xF9,
	TU_USBTRANSFER4M_SUBCODE=0xF7
}TESTUNIT_SUBCODE;
typedef enum
{
	RST_NONE_SUBCODE=0,
	RST_RESETMSC_SUBCODE,
	RST_POWEROFF_SUBCODE,
	RST_RESETMASKROM_SUBCODE,
	RST_DISCONNECTRESET_SUBCODE
}RESET_SUBCODE;
typedef enum
{
	RWMETHOD_IMAGE=0,
	RWMETHOD_LBA
}RW_SUBCODE;
typedef enum
{
	EFUSE_KEYHASH=0,
	EFUSE_EXTEND_RW,
	EFUSE_SECURE_FLAG
}EFUSE_SUBCODE;

typedef enum
{
		TEST_UNIT_READY=0,
		READ_FLASH_ID=0x01,
		TEST_BAD_BLOCK=0x03,
		READ_SECTOR=0x04,
		WRITE_SECTOR=0x05,
		ERASE_NORMAL=0x06,
		ERASE_FORCE=0x0B,
		READ_LBA=0x14,
		WRITE_LBA=0x15,
		ERASE_SYSTEMDISK=0x16,
		READ_SDRAM=0x17,
		WRITE_SDRAM=0x18,
		EXECUTE_SDRAM=0x19,
		READ_FLASH_INFO=0x1A,
		READ_CHIP_INFO=0x1B,
		SET_RESET_FLAG=0x1E,
		WRITE_EFUSE=0x1F,
		READ_EFUSE = 0x20,
		READ_SPI_FLASH=0x21,
		WRITE_SPI_FLASH=0x22,
		WRITE_NEW_EFUSE=0x23,
		READ_NEW_EFUSE=0x24,
		ERASE_LBA=0x25,
		READ_COM_DATA=0x28,
		SWITCH_STORAGE=0x2A,
		LIST_STORAGE=0x2B,
		READ_OTP_CHIP=0x2C,
		READ_CAPABILITY=0xAA,
		SWITCH_USB3=0xBB,
		DEVICE_RESET=0xFF
}USB_OPERATION_CODE;
typedef enum
{
	STORAGE_FLASH_CODE=1<<0,
	STORAGE_EMMC_CODE=1<<1,
	STORAGE_SD0_CODE=1<<2,
	STORAGE_SD1_CODE=1<<3,
	STORAGE_SPINOR_CODE=1<<9,
	STORAGE_SPINAND_CODE=1<<8,
	STORAGE_RAM_CODE=1<<6,
	STORAGE_USB_CODE=1<<7,
	STORAGE_SATA_CODE=1<<10,
	STORAGE_PCIE_CODE=1<<11,
	STORAGE_UFS_CODE=1<<12,
	STORAGE_VDK_CODE=1<<13
}STORAGE_CODE;
#pragma pack(1)

typedef struct
{
	uint_8	ucOperCode;		
	uint_8	ucReserved;
	uint_32	dwAddress;		
	uint_8	ucReserved2;
	uint_16	usLength;
	uint_32 dwParity;	
	uint_8	ucReserved3[3];
}CBWCB, *PCBWCB;

typedef struct
{
	uint_32	dwCBWSignature;			
	uint_32	dwCBWTag;				
	uint_32	dwCBWTransferLength;		
	uint_8	ucCBWFlags;				
	uint_8	ucCBWLUN;			
	uint_8	ucCBWCBLength;			
	CBWCB	cbwcb;				
}CBW, *PCBW;

typedef struct 
{
	uint_32	dwCSWSignature;
	uint_32	dwCSWTag;
	uint_32	dwCBWDataResidue;
	uint_8	ucCSWStatus;
}CSW, *PCSW;

#pragma pack()
#define CMD_TIMEOUT 0
#define CMD_TIMEOUT_5S 5000

#define CBW_SIGN			0x43425355	/* "USBC" */
#define CSW_SIGN			0x53425355	/* "USBS" */

#define UVC_PID 0x0016
#define UVC_INTF_CLASS 0x0E
#define UVC_INTF_SUB_CLASS 0x01
#define UVC_INTF_PROTOCOL 0x00
#define DIRECTION_OUT		0x00	
#define DIRECTION_IN		0x80	
#define MAX_TEST_BLOCKS		512		
#define MAX_ERASE_BLOCKS	16
#define MAX_ERASE_LBA	32*1024
#define  MAX_CLEAR_LEN	16*1024

#ifndef ERR_SUCCESS
#define ERR_SUCCESS		0
#endif
#define ERR_DEVICE_READY		0
#define ERR_DEVICE_OPEN_FAILED	-1
#define ERR_CSW_OPEN_FAILED		-2
#define ERR_DEVICE_WRITE_FAILED	-3
#define ERR_DEVICE_READ_FAILED	-4
#define ERR_CMD_NOTMATCH		-5
#define ERR_DEVICE_UNREADY		-6
#define ERR_FOUND_BAD_BLOCK		-7
#define ERR_FAILED				-8
#define ERR_CROSS_BORDER		-9
#define ERR_DEVICE_NOT_SUPPORT  -10
#define ERR_REQUEST_NOT_SUPPORT  -11
#define ERR_REQUEST_FAIL		-12
#define ERR_BUFFER_NOT_ENOUGH   -13
#define ERR_PARITY              -14

#define UFI_CHECK_SIGN(cbw, csw) ((CSW_SIGN == (csw).dwCSWSignature) && ((csw).dwCSWTag == (cbw).dwCBWTag))

class CRKLog;
class CRKComm
{
public:
	virtual int RKU_EraseBlock(uint_8 ucFlashCS,uint_32 dwPos,uint_32 dwCount,uint_8 ucEraseType)=0;
	virtual int RKU_ReadChipInfo(uint_8* lpBuffer)=0;	
	virtual int RKU_ReadFlashID(uint_8* lpBuffer)=0;
	virtual int RKU_ReadOtpCpuID(uint_8* lpBuffer)=0;
	virtual int RKU_ReadFlashInfo(uint_8* lpBuffer,uint_32 *puiRead=NULL)=0;
	virtual int RKU_ReadCapability(uint_8* lpBuffer)=0;
	virtual int RKU_ReadLBA(uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer,uint_8 bySubCode=RWMETHOD_IMAGE)=0;				
	virtual int RKU_ReadSector(uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer)=0;
	virtual int RKU_ResetDevice(uint_8 bySubCode=RST_NONE_SUBCODE)=0;
	virtual int RKU_ResetPipe(uint_8 byPipe=2)=0;
	virtual int RKU_TestBadBlock(uint_8 ucFlashCS,uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer)=0;
	virtual int RKU_TestDeviceReady(uint_32 *dwTotal=NULL,uint_32 *dwCurrent=NULL,uint_8 bySubCode=TU_NONE_SUBCODE)=0;
	virtual int RKU_WriteLBA(uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer,uint_8 bySubCode=RWMETHOD_IMAGE)=0;
	virtual int RKU_WriteSector(uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer)=0;
	virtual int RKU_WriteSDRam(uint_32 dwCount,uint_8* lpBuffer)=0;
	virtual int RKU_RunSDRam()=0;
	virtual int RKU_ReadStorageList(uint_8* lpBuffer)=0;
	virtual int RKU_SwitchStorage(uint_8 emStorageCode)=0;

	virtual int RKU_ReadNewEfuse(uint_16 usPos,uint_16 usSize,uint_8 *lpBuffer,uint_8 bySubCode=EFUSE_KEYHASH)=0;
	virtual int RKU_WriteNewEfuse(uint_16 usPos,uint_16 usSize,uint_8 *lpBuffer,uint_8 bySubCode=EFUSE_KEYHASH)=0;
    virtual int RKU_DeviceRequest(uint_32 dwRequest,uint_8* lpBuffer,uint_32 dwDataSize,bool crcoff)=0;
    virtual int RKU_MscToRockusb()=0;
	virtual int RKU_UvcToRockusb()=0;
	virtual bool Reset_Usb_Config(STRUCT_RKDEVICE_DESC devDesc)=0;
	virtual bool Reset_Usb_Device()=0;
	virtual int RKU_EraseLBA(uint_32 dwPos,uint_32 dwCount)=0;
	virtual int RKU_ReadComData(uint_8 *lpBuffer,uint_32 dwCount=128)=0;
	virtual int RKU_SwitchUsb3(char *pszSn) = 0;
	CRKComm(CRKLog *pLog);
	virtual ~CRKComm(){};
protected:
	STRUCT_RKDEVICE_DESC m_deviceDesc;
	CRKLog *m_log;
private:
	virtual bool RKU_Write(uint_8* lpBuffer,uint_32 dwSize)=0;
	virtual bool RKU_Read(uint_8* lpBuffer, uint_32 dwSize)=0;
};
class CRKUsbComm: public CRKComm
{
public:
	int RKU_EraseBlock(uint_8 ucFlashCS,uint_32 dwPos,uint_32 dwCount,uint_8 ucEraseType) override;
	int RKU_ReadChipInfo(uint_8* lpBuffer) override;
	int RKU_ReadFlashID(uint_8* lpBuffer) override;
	int RKU_ReadOtpCpuID(uint_8* lpBuffer) override;
	int RKU_ReadFlashInfo(uint_8* lpBuffer,uint_32 *puiRead=NULL) override;
	int RKU_ReadCapability(uint_8* lpBuffer) override;
	int RKU_ReadLBA(uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer,uint_8 bySubCode=RWMETHOD_IMAGE) override;
	int RKU_ReadSector(uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer) override;
	int RKU_ResetDevice(uint_8 bySubCode=RST_NONE_SUBCODE) override;
	int RKU_ResetPipe(uint_8 byPipe=2) override;
	int RKU_TestBadBlock(uint_8 ucFlashCS,uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer) override;
	int RKU_TestDeviceReady(uint_32 *dwTotal=NULL,uint_32 *dwCurrent=NULL,uint_8 bySubCode=TU_NONE_SUBCODE) override;
	int RKU_WriteLBA(uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer,uint_8 bySubCode=RWMETHOD_IMAGE) override;
	int RKU_WriteSector(uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer) override;
	int RKU_WriteSDRam(uint_32 dwCount,uint_8* lpBuffer) override;
	int RKU_RunSDRam() override;
	int RKU_ReadStorageList(uint_8* lpBuffer) override;
	int RKU_SwitchStorage(uint_8 emStorageCode) override;
	int RKU_ReadNewEfuse(uint_16 usPos,uint_16 usSize,uint_8 *lpBuffer,uint_8 bySubCode=EFUSE_KEYHASH) override;
	int RKU_WriteNewEfuse(uint_16 usPos,uint_16 usSize,uint_8 *lpBuffer,uint_8 bySubCode=EFUSE_KEYHASH) override;
    int RKU_DeviceRequest(uint_32 dwRequest,uint_8* lpBuffer,uint_32 dwDataSize,bool crcoff=false) override;
    int RKU_MscToRockusb() override;
	int RKU_UvcToRockusb() override;
	int RKU_EraseLBA(uint_32 dwPos,uint_32 dwCount) override;
	int RKU_ReadComData(uint_8 *lpBuffer,uint_32 dwCount=128) override;
	int RKU_SwitchUsb3(char *pszSn) override;
	CRKUsbComm(STRUCT_RKDEVICE_DESC devDesc,CRKLog *pLog,bool &bRet);
	~CRKUsbComm() override;
    bool Reset_Usb_Config(STRUCT_RKDEVICE_DESC devDesc) override;
	bool Reset_Usb_Device() override;
protected:
	
private:
	void * m_pUsbHandle;
	unsigned char m_pipeBulkIn;
	unsigned char m_pipeBulkOut;
	int m_interfaceNum;
	bool RKU_Write(uint_8* lpBuffer,uint_32 dwSize) override;
	bool RKU_Read( uint_8* lpBuffer, uint_32 dwSize) override;
	bool InitializeUsb(STRUCT_RKDEVICE_DESC &devDesc);
	void UninitializeUsb();
	bool RKU_ClearBuffer(CBW& cbw,CSW& csw);
	uint_32 RKU_Read_EX( uint_8* lpBuffer, uint_32 dwSize);
	void InitializeCBW(PCBW pCBW, USB_OPERATION_CODE code);
	int RandomInteger(int low, int high);
	uint_32 MakeCBWTag();
 
};

#endif
