#ifndef RKDEVICE_HEADER
#define RKDEVICE_HEADER
#include "RKImage.h"
#include "RKLog.h"
#include "DefineHeader.h"
#ifdef _WIN32
#include "os\win\RKScan.h"
#include "os\win\RKComm.h"
#elif __APPLE__
#include "os/mac/RKScan.h"
#include "os/mac/RKComm.h"
#else
#include "os/linux/RKScan.h"
#include "os/linux/RKComm.h"
#endif
#define  DEVICE_ERROR_BASE 100
/*device error list*/
#define ERR_NEW_BUFFER (DEVICE_ERROR_BASE+0)
#define ERR_COMM_RW (DEVICE_ERROR_BASE+1)
#define ERR_PAGE_ZERO (DEVICE_ERROR_BASE+2)
#define ERR_SIGN_UNLOCK (DEVICE_ERROR_BASE+3)
#define ERR_FW_UNSIGN (DEVICE_ERROR_BASE+4)
#define ERR_WRONG_CHIP (DEVICE_ERROR_BASE+5)
#define ERR_FLASHBOOT (DEVICE_ERROR_BASE+6)
#define ERR_FLASHDATA (DEVICE_ERROR_BASE+7)
#define ERR_FLASHHEAD (DEVICE_ERROR_BASE+8)
#define ERR_CHECK_IDB (DEVICE_ERROR_BASE+9)
#define ERR_DEVICE_CREATE (DEVICE_ERROR_BASE+10)
#define ERR_BOOT_471 (DEVICE_ERROR_BASE+11)
#define ERR_BOOT_472 (DEVICE_ERROR_BASE+12)
#define ERR_RUN_BOOT471 (DEVICE_ERROR_BASE+13)
#define ERR_RUN_BOOT472 (DEVICE_ERROR_BASE+14)
#define ERR_NO_FLASH_CS (DEVICE_ERROR_BASE+15)
#define ERR_FW_TOO_LARGER (DEVICE_ERROR_BASE+16)
#define ERR_FW_READ (DEVICE_ERROR_BASE+17)
#define ERR_FW_CHECK (DEVICE_ERROR_BASE+18)
#define ERR_NOT_EXIST_PARAMETER (DEVICE_ERROR_BASE+19)
#define ERR_CROSS_IDBLOCK (DEVICE_ERROR_BASE+20)
#define ERR_GPT_DIRECTLBA (DEVICE_ERROR_BASE+21)
#define ERR_PARTITION_NOT_ENOUGH (DEVICE_ERROR_BASE+22)
#define ERR_PARTITION_CROSS_FLASH (DEVICE_ERROR_BASE+23)
#define ERR_UBI_PAGE_CHECK (DEVICE_ERROR_BASE+24)

#define RKNANO_SEC3_RESERVED_LEN 435
#define RK27_SEC3_RESERVED_LEN 466
#define RK28_SEC3_RESERVED_LEN 419
#define RKCROWN_SEC3_RESERVED_LEN 435
#define RKANDROID_SEC3_RESERVED_LEN 419

#define RK28_SEC2_RESERVED_LEN 473
#define RKCROWN_SEC2_RESERVED_LEN 484
#define RKANDROID_SEC2_RESERVED_LEN 473
#define RKNANO_SEC2_RESERVED_LEN 473

#define RKDEVICE_CUSTOMDATA_LEN 512
#define RKDEVICE_SN_LEN 30
#define RKDEVICE_UID_LEN 30
#define RKDEVICE_MAC_LEN 6
#define RKDEVICE_BT_LEN 6
#define RKDEVICE_IMEI_LEN 15

#define SECTOR_SIZE 512
// Rockchip image page size. Do not use the generic PAGE_SIZE name: Android
// system headers define it as the OS memory-page size (typically 4096).
#define RK_IMAGE_PAGE_SIZE 2048
#define SPARE_SIZE 16
#define IDB_BLOCKS 5
#define CHIPINFO_LEN 16
#define IDBLOCK_TOP 50
#define WBBUFFER_BOTTOM 5
#define WBBUFFER_TOP 10
#define CALC_UNIT(a, b)		((a>0)?((a-1)/b+1):(a))
#define BYTE2SECTOR(x)		(CALC_UNIT(x, SECTOR_SIZE))
#define PAGEALIGN(x)		(CALC_UNIT(x, 4))
//#define MAX_TEST_BLOCKS		512
//#define MAX_ERASE_BLOCKS	128
#define MAX_WRITE_SECTOR	16
#define IOCTL_TRANSFER_SIZE		4096

#define CRC_TAG "CRC"
#define IDBLOCK_LBA 64
#define IDBLOCK_SPAN 1024
#define SECTOR3_BACKUP_LBA 60
#pragma pack(1)
typedef struct _STRUCT_FLASH_INFO
{
	char	szManufacturerName[8];
	uint_32 uiFlashSizeSector;
	uint_16 usBlockSizeSector;
	uint_8  bPageSizeSector;
	uint_8  bPageAvailSizeSector;
	uint_32	uiFlashSize;  //MB
	uint_16	usBlockSize;//KB
	uint_32	uiPageSize;   //KB
	uint_32	uiSectorPerBlock;
	uint_32 uiLogicalBlockSize;//sector
	uint_8	blockState[IDBLOCK_TOP];
	uint_32	uiBlockNum;
	uint_8	bECCBits;
	uint_8	bAccessTime; 
	uint_8	bFlashCS;
	uint_16  usValidSecPerBlock;
	uint_16  usPhyBlokcPerIDB;
	uint_32    uiSecNumPerIDB;
}STRUCT_FLASH_INFO, *PSTRUCT_FLASH_INFO;
typedef struct _STRUCT_FLASHINFO_CMD
{
	uint_32	uiFlashSize;
	uint_16	usBlockSize;
	uint_8	bPageSize;
	uint_8	bECCBits;
	uint_8	bAccessTime;
	uint_8	bManufCode;
	uint_8	bFlashCS;
	uint_8  bPageAvailSize;
	uint_8	reserved[500];
}STRUCT_FLASHINFO_CMD, *PSTRUCT_FLASHINFO_CMD;
typedef struct  
{
	uint_8 bFlashCS;
	uint_32 uiBlockNum;
	uint_16 usBlockStateSize;
	uint_8* pBlockStateData;
}STRUCT_BLOCK_STATE,*PSTRUCT_BLOCK_STATE;
typedef struct 
{
	uint_8  tag[2];
	uint_16 version;
	uint_16 size;
	uint_16 reserved;
	uint_8	chip_code[4];
	uint_8	lot_id[16];
	uint_32	hash;
}STRUCT_OTPCHIP, *PSTRUCT_OTPCHIP;
#pragma pack()
typedef struct
{
	uint_16 usVid;
	uint_16 usPid;
}STRUCT_DEVICE_PROP;
typedef vector<STRUCT_DEVICE_PROP> DEVICE_PROP_SET;

class CRKDevice
{
public:
	uint_16 GetVendorID();
	void SetVendorID(uint_16 value);
 	property<CRKDevice,uint_16,READ_WRITE> VendorID;
	
	uint_16 GetProductID();
	void SetProductID(uint_16 value);
 	property<CRKDevice,uint_16,READ_WRITE> ProductID;
	
	ENUM_RKDEVICE_TYPE GetDeviceType();
	void SetDeviceType(ENUM_RKDEVICE_TYPE value);
 	property<CRKDevice,ENUM_RKDEVICE_TYPE,READ_WRITE> DeviceType;
	
	ENUM_RKUSB_TYPE GetUsbType();
	void SetUsbType(ENUM_RKUSB_TYPE value);
 	property<CRKDevice,ENUM_RKUSB_TYPE,READ_WRITE> UsbType;

	ENUM_USB_SPEED GetUsbSpeed();
	void SetUsbSpeed(ENUM_USB_SPEED value);
 	property<CRKDevice,ENUM_USB_SPEED,READ_WRITE> UsbSpeed;
	
	string GetLayerName();
	void SetLayerName(string value);
 	property<CRKDevice,string,READ_WRITE> LayerName;

	string GetLinkName();
	void SetLinkName(string value);
 	property<CRKDevice,string,READ_WRITE> LinkName;

	string GetSerialName();
	void SetSerialName(string value);
 	property<CRKDevice,string,READ_WRITE> SerialName;
	
	uint_32 GetLocationID();
	void SetLocationID(uint_32 value);
 	property<CRKDevice,uint_32,READ_WRITE> LocationID;
	
	uint_16 GetBcdUsb();
	void SetBcdUsb(uint_16 value);
 	property<CRKDevice,uint_16,READ_WRITE> BcdUsb;

	ENUM_OS_TYPE GetOsType();
	void SetOsType(ENUM_OS_TYPE value);
 	property<CRKDevice,ENUM_OS_TYPE,READ_WRITE> OsType;

	void SetGetNewDiskSizeFlag(bool value);
 	property<CRKDevice,bool,WRITE_ONLY> GetNewDiskSizeFlag;
	
	PSTRUCT_BLOCK_STATE GetBlockStatePointer();
	void SetBlockStatePointer(PSTRUCT_BLOCK_STATE value);
 	property<CRKDevice,PSTRUCT_BLOCK_STATE,READ_WRITE> BlockStatePointer;
	
	bool GetRemallocDisk();
	void SetRemallocDisk(bool value);
 	property<CRKDevice,bool,READ_WRITE> RemallocDisk;

	void SetPrepareEraseFlag(bool value);
 	property<CRKDevice,bool,WRITE_ONLY> PrepareEraseFlag;

	void SetWorkFlow(uint_32 value);
 	property<CRKDevice,uint_32,WRITE_ONLY> WorkFlow;
	
	void SetMiscModifyFlag(ENUM_MISC_MODIFY_FLAG value);
 	property<CRKDevice,ENUM_MISC_MODIFY_FLAG,WRITE_ONLY> MiscModifyFlag;

	
	bool GetUidWriteOK();
 	property<CRKDevice,bool,READ_ONLY> UidWriteOK;

	bool GetEmmcStor();
 	property<CRKDevice,bool,READ_ONLY> EmmcStor;

	bool GetNorStor();
 	property<CRKDevice,bool,READ_ONLY> NorStor;
	
	uint_8* GetSn();
	void SetSn(uint_8* value);
 	property<CRKDevice,uint_8*,READ_WRITE> Sn;

	uint_8 GetSnSize();
	void SetSnSize(uint_8 value);
 	property<CRKDevice,uint_8,READ_WRITE> SnSize;

	uint_8* GetMac();
	void SetMac(uint_8* value);
 	property<CRKDevice,uint_8*,READ_WRITE> Mac;

	uint_8 GetMacSize();
 	property<CRKDevice,uint_8,READ_ONLY> MacSize;
	
	uint_8* GetImei();
	void SetImei(uint_8* value);
 	property<CRKDevice,uint_8*,READ_WRITE> Imei;
	
	uint_8 GetImeiSize();
 	property<CRKDevice,uint_8,READ_ONLY> ImeiSize;
	
	uint_8* GetBTAddr();
	void SetBTAddr(uint_8* value);
 	property<CRKDevice,uint_8*,READ_WRITE> BTAddr;
	
	uint_8 GetBTSize();
 	property<CRKDevice,uint_8,READ_ONLY> BTSize;

	uint_8* GetUid();
	void SetUid(uint_8* value);
 	property<CRKDevice,uint_8*,READ_WRITE> Uid;
	
	uint_8 GetUidSize();
 	property<CRKDevice,uint_8,READ_ONLY> UidSize;
	
	uint_8* GetCustomData();
	void SetCustomData(uint_8* value);
 	property<CRKDevice,uint_8*,READ_WRITE> CustomData;

	uint_16 GetCustomDataSize();
	void SetCustomDataSize(uint_16 value);
 	property<CRKDevice,uint_16,READ_WRITE> CustomDataSize;
	
	uint_16 GetCustomDataOffset();
	void SetCustomDataOffset(uint_16 value);
 	property<CRKDevice,uint_16,READ_WRITE> CustomDataOffset;

	CRKLog* GetLogObjectPointer();
 	property<CRKDevice,CRKLog*,READ_ONLY> LogObjectPointer;
	
	uint_32 GetIDBend();
 	property<CRKDevice,uint_32,READ_ONLY> IDBend;

	 CRKComm* GetCommObjectPointer();
 	property<CRKDevice,CRKComm*,READ_ONLY> CommObjectPointer;

	void SetCallBackPointer(ProgressPromptCB value);
 	property<CRKDevice,ProgressPromptCB,WRITE_ONLY> CallBackPointer;

	bool GetDirectLBA();
	void SetDirectLBA(bool value);
 	property<CRKDevice,bool,READ_WRITE> DirectLBA;

	bool GetFirst4Access();
	void SetFirst4Access(bool value);
 	property<CRKDevice,bool,READ_WRITE> First4Access;

	bool GetReadIDBConfigFlag();
	void SetReadIDBConfigFlag(bool value);
 	property<CRKDevice,bool,READ_WRITE> ReadIDBConfigFlag;

	bool GetSecureModeFlag();
	void SetSecureModeFlag(bool value);
 	property<CRKDevice,bool,READ_WRITE> SecureModeFlag;

	bool GetNewIDBFlag();
	void SetNewIDBFlag(bool value);
 	property<CRKDevice,bool,READ_WRITE> NewIDBFlag;

	bool GetSwitchStorageFlag();
	void SetSwitchStorageFlag(bool value);
 	property<CRKDevice,bool,READ_WRITE> SwitchStorageFlag;

	bool GetSwitchUsb3Flag();
	void SetSwitchUsb3Flag(bool value);
 	property<CRKDevice,bool,READ_WRITE> SwitchUsb3Flag;

	bool GetUsbDriver4M();
	void SetUsbDriver4M(bool value);
 	property<CRKDevice,bool,READ_WRITE> UsbDriver4M;

	bool GetGptFlag();
	void SetGptFlag(bool value);
 	property<CRKDevice,bool,READ_WRITE> GptFlag;

	bool GetNoCheckMode();
	void SetNoCheckMode(bool value);
 	property<CRKDevice,bool,READ_WRITE> NoCheckMode;

	bool GetIDBCheckMode();
	void SetIDBCheckMode(bool value);
 	property<CRKDevice,bool,READ_WRITE> IDBCheckMode;

	bool GetUsb3Transfer();
	void SetUsb3Transfer(bool value);
 	property<CRKDevice,bool,READ_WRITE> Usb3Transfer;

	bool GetNorSingleIDB();
	void SetNorSingleIDB(bool value);
 	property<CRKDevice,bool,READ_WRITE> NorSingleIDB;

	uint_32 GetForceDataBand();
	void SetForceDataBand(uint_32 value);
 	property<CRKDevice,uint_32,READ_WRITE> ForceDataBand;

	int GetError();
 	property<CRKDevice,int,READ_ONLY> Error;
	
	static bool FormatDisk(char drive,char *lpDiskVolume,CRKLog *pLog=NULL);
	static bool CopyData(char drive,char *lpDataFrom,CRKLog *pLog=NULL);
	static bool SendExtCommand(char drive,uint_32 dwCode,CRKLog *pLog=NULL,uint_8* pOutData=NULL,uint_32 uiOutDataLen=0);

	virtual int DownloadBoot()=0;
	virtual int DownloadIDBlock(bool &bNeedJump)=0;
	virtual int DownloadImage()=0;
	virtual int PrepareIDB(bool &bNeedJump)=0;
	virtual bool BufferWriteBack()=0;
	
	bool TestDevice();
	bool LowerFormatDevice(bool &bSupport);
	bool ResetDevice(uint_8 subCode=0);
	bool SetResetFlag();
	bool SwitchUsb3();
	bool PowerOffDevice();
	bool CheckChip();
	bool GetFlashInfo();
	virtual int EraseIDB()=0;
	virtual int EraseAllBlocks()=0;
	virtual int GetBlocksState()=0;
	virtual bool ReadCapability()=0;
	bool SetObject(CRKImage *pImage,CRKComm *pComm,CRKLog *pLog);
	string GetLayerString(uint_32 dwLocationID);
	CRKDevice(STRUCT_RKDEVICE_DESC &device);
	virtual ~CRKDevice();
protected:
	STRUCT_FLASH_INFO m_flashInfo;
	uint_8*	m_pFlashInfoData;
	uint_16	m_usFlashInfoDataOffset;
	uint_16  m_usFlashInfoDataLen;
	uint_32 m_idBlockOffset[IDB_BLOCKS];
	uint_8*  m_chipData;
	CRKImage *m_pImage;
	CRKComm  *m_pComm;
	CRKLog   *m_pLog;
	uint_8*  m_customData;
	uint_16 m_customDataSize;
	uint_16 m_customDataOffset;
	uint_8*  m_sn;
	uint_8   m_snSize;
	uint_8*  m_mac;
	uint_8*  m_blueTooth;
	uint_8*  m_uid;
	uint_8*  m_imei;
	uint_16 m_sysDiskSize;
	uint_16 m_cfgDiskSize;
	bool   m_bGetNewDiskSizeFlag;
	bool   m_bExistSector3Crc;
	uint_16 m_usSector3Crc;
	uint_16 m_usWriteBackCrc;
	uint_16 m_usWriteBackCustomDataOffset;
	uint_16 m_usWriteBackCustomDataSize;
	PSTRUCT_BLOCK_STATE m_pBlockState;
	uint_8   m_backupBuffer[SECTOR_SIZE+SPARE_SIZE];
	ProgressPromptCB m_callBackProc;
	bool m_bWriteBack;
	uint_32 m_uiWorkFlow;
	uint_32 m_uiIDBend;
	int m_error;
	bool m_bEraseInPrepare;
	bool m_bUidUseFlag;
	bool m_bUidWriteOK;
	bool  m_remallocDisk;
	bool m_bEmmc;
	bool m_bNor;
	bool m_bDirectLba;
	bool  m_bFirst4Access;
	bool m_bGptFlag;
	bool m_bNoCheckMode;
	bool m_bCloseIDBCheck;
	bool m_bUsb3Transfer;
	bool m_bNorSingleIDB;
	bool m_bReadIDBFlag;
	bool m_bSecureModeFlag;
	bool m_bNewIDBFlag;
	bool m_bSwitchStorageFlag;
	bool m_bSwitchUsb3Flag;
	bool m_bUsbDriver4m;

	uint_32 m_uiForceBand;
	ENUM_MISC_MODIFY_FLAG m_emMiscModifyFlag;
	bool BuildBlockStateMap(uint_8 bFlashCS);
	int ReadMutilSector(uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer);
	bool EraseMutilBlock(uint_8 bFlashCS,uint_32 dwPos,uint_32 dwCount,bool bForce);
	char FindValidBlocks(char bBegin, char bLen);
	uint_8 RandomByte(uint_8 bLowLimit,uint_8 bHighLimit);
	bool CheckCrc16(uint_8* pCheckData,uint_16 usDataLength,uint_16 usOldCrc);
	bool CheckUid(uint_8 uidSize,uint_8 *pUid);
	bool GetWriteBackData(uint_32 uiIDBCount,uint_8* lpBuf);
	bool GetIDBData(uint_32 uiIDBCount,uint_8* lpBuf,uint_32 uiSecCount);
	int EraseEmmcBlock(uint_8 ucFlashCS,uint_32 dwPos,uint_32 dwCount);
	int EraseEmmcByWriteLBA(uint_32 dwSectorPos,uint_32 dwCount);
	bool EraseEmmc();
	bool EraseEmmcIDB();
	virtual bool FindBackupBuffer()=0;
	virtual char FindAllIDB()=0;
	virtual char FindIDBlock(char pos,char &IDBlockPos)=0;
	virtual bool ReserveIDBlock(char iBlockIndex=0,char iIdblockPos=0)=0;
	virtual bool OffsetIDBlock(char pos)=0;
	virtual bool MakeSector0(uint_8* pSector)=0;
	virtual void MakeSector1(uint_8* pSector)=0;
	virtual bool MakeSector2(uint_8* pSector)=0;
	virtual bool MakeSector3(uint_8* pSector)=0;
	virtual int MakeIDBlockData(uint_8* lpIDBlock)=0;
	virtual bool MakeSpareData(uint_8* lpIDBlock,uint_32 dwSectorNum,uint_8* lpSpareBuffer)=0;
	virtual int WriteIDBlock(uint_8* lpIDBlock,uint_8* lpSpareData,uint_32 dwSectorNum,bool bErase)=0;
	
private:
	uint_16 m_vid;
	uint_16 m_pid;
	ENUM_RKDEVICE_TYPE m_device;
	ENUM_OS_TYPE m_os;
	ENUM_RKUSB_TYPE m_usb;
	ENUM_USB_SPEED m_usb_speed;
	uint_32 m_locationID;
	uint_16 m_bcdUsb;
protected:
	string  m_layerName;
	string m_linkName;
	string m_serialName;
	
};

uint_16 UshortToBCD(uint_16 num);
uint_8   ByteToBCD(uint_8 num);
extern uint_16 CRC_16(uint_8 * aData, uint_32 aSize);
extern uint_32 CRC_32(uint_8* pData, uint_32 ulSize);
extern void P_RC4(uint_8 * buf, uint_32 len);
extern void bch_encode(uint_8 *encode_in, uint_8 *encode_out);
extern uint_16 CRC_CCITT(uint_8 *p, uint_32 CalculateNumber);
extern void generate_gf();
extern void gen_poly();
#endif
