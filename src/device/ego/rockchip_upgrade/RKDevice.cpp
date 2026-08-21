#include "RKDevice.h"

const char* szManufName[] =
{
	"SAMSUNG",
	"TOSHIBA",
	"HYNIX",
	"INFINEON",
	"MICRON",
	"RENESAS",
	"ST",
	"INTEL"
};


void CRKDevice::SetBlockStatePointer(PSTRUCT_BLOCK_STATE value)
{
	if (value)
	{
		m_pBlockState = value;
	}
}
PSTRUCT_BLOCK_STATE CRKDevice::GetBlockStatePointer()
{
	return m_pBlockState;
}

void CRKDevice::SetRemallocDisk(bool value)
{
	m_remallocDisk = value;
}
bool CRKDevice::GetRemallocDisk()
{
	return m_remallocDisk;
}

void CRKDevice::SetPrepareEraseFlag(bool value)
{
	m_bEraseInPrepare = value;
}

void CRKDevice::SetWorkFlow(uint_32 value)
{
	m_uiWorkFlow = value;
}

void CRKDevice::SetMiscModifyFlag(ENUM_MISC_MODIFY_FLAG value)
{
	m_emMiscModifyFlag = value;
}

void CRKDevice::SetGetNewDiskSizeFlag(bool value)
{
	m_bGetNewDiskSizeFlag = value;
}
void CRKDevice::SetVendorID(uint_16 value)
{
	m_vid = value;
}
void CRKDevice::SetProductID(uint_16 value)
{
	m_pid = value;
}
void CRKDevice::SetDeviceType(ENUM_RKDEVICE_TYPE value)
{
	m_device = value;
}
void CRKDevice::SetOsType(ENUM_OS_TYPE value)
{
	m_os = value;
}

void CRKDevice::SetUsbType(ENUM_RKUSB_TYPE value)
{
	m_usb = value;
}

void CRKDevice::SetUsbSpeed(ENUM_USB_SPEED value)
{
	m_usb_speed = value;
}

void CRKDevice::SetBcdUsb(uint_16 value)
{
	m_bcdUsb = value;
}
void CRKDevice::SetLayerName(string value)
{
	m_layerName = value;
}
void CRKDevice::SetLinkName(string value)
{
	m_linkName = value;
}
void CRKDevice::SetSerialName(string value)
{
	m_serialName = value;
}
void CRKDevice::SetLocationID(uint_32 value)
{
	m_locationID = value;
	m_layerName = GetLayerString(m_locationID);
}

void CRKDevice::SetCallBackPointer(ProgressPromptCB value)
{
	if (value)
	{
		m_callBackProc = value;
	}
}
CRKLog* CRKDevice::GetLogObjectPointer()
{
	return m_pLog;
}

CRKComm* CRKDevice::GetCommObjectPointer()
{
	return m_pComm;
}

uint_8* CRKDevice::GetCustomData()
{
	return m_customData;
}
uint_16 CRKDevice::GetCustomDataSize()
{
	return m_customDataSize;
}
uint_16 CRKDevice::GetCustomDataOffset()
{
	return m_customDataOffset;
}

void CRKDevice::SetCustomData(uint_8* value)
{
	if (value)
	{
		if (!m_customData)
		{
			m_customData = new uint_8[RKDEVICE_CUSTOMDATA_LEN];
			memset(m_customData,0,RKDEVICE_CUSTOMDATA_LEN);
		}
// 		if (m_customDataSize>0)
// 		{
// 			memcpy(m_customData,value+m_customDataOffset,m_customDataSize);
// 		}
// 		else
			memcpy(m_customData,value,SECTOR_SIZE);
	}
	else
	{
		if (m_customData)
		{
			delete []m_customData;
		}
		m_customData = value;
	}
	
}
void CRKDevice::SetCustomDataSize(uint_16 value)
{

	if ((m_customDataOffset+value)>=RKDEVICE_CUSTOMDATA_LEN)
	{
		m_customDataSize = RKDEVICE_CUSTOMDATA_LEN-m_customDataOffset;
	}
	else
		m_customDataSize = value;
}
void CRKDevice::SetCustomDataOffset(uint_16 value)
{
	if (value<RKDEVICE_CUSTOMDATA_LEN)
	{
		m_customDataOffset = value;
	}
	else
		m_customDataOffset = 0;
}

uint_8* CRKDevice::GetSn()
{
	return m_sn;
}
uint_8 CRKDevice::GetSnSize()
{
	return m_snSize;
}
void CRKDevice::SetSn(uint_8* value)
{
	if (value)
	{
		if (!m_sn)
		{
			m_sn = new uint_8[RKDEVICE_SN_LEN];
			memset(m_sn,0,RKDEVICE_SN_LEN);
		}
		memcpy(m_sn,value,m_snSize);
	}
	else
	{
		if (m_sn)
		{
			delete []m_sn;
		}
		m_sn = value;
	}
	
}
void CRKDevice::SetSnSize(uint_8 value)
{
	if (value>RKDEVICE_SN_LEN)
		m_snSize = RKDEVICE_SN_LEN;
	else
		m_snSize = value;
}
uint_8* CRKDevice::GetMac()
{
	return m_mac;
}
uint_8 CRKDevice::GetMacSize()
{
	return RKDEVICE_MAC_LEN;
}
void CRKDevice::SetMac(uint_8* value)
{
	if (value)
	{
		if (!m_mac)
		{
			m_mac = new uint_8[RKDEVICE_MAC_LEN];
			memset(m_mac,0,RKDEVICE_MAC_LEN);
		}
		memcpy(m_mac,value,RKDEVICE_MAC_LEN);
	}
	else
	{
		if (m_mac)
		{
			delete []m_mac;
		}
		m_mac = value;
	}
	
}

uint_8* CRKDevice::GetImei()
{
	return m_imei;
}
uint_8 CRKDevice::GetImeiSize()
{
	return RKDEVICE_IMEI_LEN;
}
void CRKDevice::SetImei(uint_8* value)
{
	if (value)
	{
		if (!m_imei)
		{
			m_imei = new uint_8[RKDEVICE_IMEI_LEN];
			memset(m_imei,0,RKDEVICE_IMEI_LEN);
		}
		memcpy(m_imei,value,RKDEVICE_IMEI_LEN);
	}
	else
	{
		if (m_imei)
		{
			delete []m_imei;
		}
		m_imei = value;
	}
	
}
bool CRKDevice::GetDirectLBA()
{
	return m_bDirectLba;
}
void CRKDevice::SetDirectLBA(bool value)
{
	m_bDirectLba = value;
}
bool CRKDevice::GetReadIDBConfigFlag()
{
	return m_bReadIDBFlag;
}
void CRKDevice::SetReadIDBConfigFlag(bool value)
{
	m_bReadIDBFlag = value;
}
bool CRKDevice::GetSecureModeFlag()
{
	return m_bSecureModeFlag;
}
void CRKDevice::SetSecureModeFlag(bool value)
{
	m_bSecureModeFlag = value;
}
bool CRKDevice::GetNewIDBFlag()
{
	return m_bNewIDBFlag;
}
void CRKDevice::SetNewIDBFlag(bool value)
{
	m_bNewIDBFlag = value;
}
bool CRKDevice::GetSwitchStorageFlag()
{
	return m_bSwitchStorageFlag;
}
void CRKDevice::SetSwitchStorageFlag(bool value)
{
	m_bSwitchStorageFlag = value;
}
bool CRKDevice::GetSwitchUsb3Flag()
{
	return m_bSwitchUsb3Flag;
}
void CRKDevice::SetSwitchUsb3Flag(bool value)
{
	m_bSwitchUsb3Flag = value;
}
bool CRKDevice::GetUsbDriver4M()
{
	return m_bUsbDriver4m;
}
void CRKDevice::SetUsbDriver4M(bool value)
{
	m_bUsbDriver4m = value;
}
bool CRKDevice::GetFirst4Access()
{
	return m_bFirst4Access;
}
void CRKDevice::SetFirst4Access(bool value)
{
	m_bFirst4Access = value;
}
bool CRKDevice::GetGptFlag()
{
	return m_bGptFlag;
}
void CRKDevice::SetGptFlag(bool value)
{
	m_bGptFlag = value;
}
bool CRKDevice::GetNoCheckMode()
{
	return m_bNoCheckMode;
}
void CRKDevice::SetNoCheckMode(bool value)
{
	m_bNoCheckMode = value;
}
bool CRKDevice::GetIDBCheckMode()
{
	return m_bCloseIDBCheck;
}
void CRKDevice::SetIDBCheckMode(bool value)
{
	m_bCloseIDBCheck = value;
}
bool CRKDevice::GetUsb3Transfer()
{
	return m_bUsb3Transfer;
}
void CRKDevice::SetUsb3Transfer(bool value)
{
	m_bUsb3Transfer = value;
}
bool CRKDevice::GetNorSingleIDB()
{
	return m_bNorSingleIDB;
}
void CRKDevice::SetNorSingleIDB(bool value)
{
	m_bNorSingleIDB = value;
}
uint_32 CRKDevice::GetForceDataBand()
{
	return m_uiForceBand;
}
void CRKDevice::SetForceDataBand(uint_32 value)
{
	m_uiForceBand = value;
}
uint_8* CRKDevice::GetUid()
{
	return m_uid;
}
uint_8 CRKDevice::GetUidSize()
{
	return RKDEVICE_UID_LEN;
}
uint_32 CRKDevice::GetIDBend()
{
	return m_uiIDBend;
}
bool CRKDevice::GetEmmcStor()
{
	return m_bEmmc;
}
bool CRKDevice::GetNorStor()
{
	return m_bNor;
}
int CRKDevice::GetError()
{
	return m_error;
}
void CRKDevice::SetUid(uint_8* value)
{
	if (value)
	{
		if (!m_uid)
		{
			m_uid = new uint_8[RKDEVICE_UID_LEN];
			memset(m_uid,0,RKDEVICE_UID_LEN);
		}
		memcpy(m_uid,value,RKDEVICE_UID_LEN);
	}
	else
	{
		if (m_uid)
		{
			delete []m_uid;
		}
		m_uid = value;
	}
	
}
uint_8* CRKDevice::GetBTAddr()
{
	return m_blueTooth;
}

uint_8 CRKDevice::GetBTSize()
{
	return RKDEVICE_BT_LEN;
}
void CRKDevice::SetBTAddr(uint_8* value)
{
	if (value)
	{
		if (!m_blueTooth)
		{
			m_blueTooth = new uint_8[RKDEVICE_BT_LEN];
			memset(m_blueTooth,0,RKDEVICE_BT_LEN);
		}
		memcpy(m_blueTooth,value,RKDEVICE_BT_LEN);
	}
	else
	{
		if (m_blueTooth)
		{
			delete []m_blueTooth;
		}
		m_blueTooth = value;
	}
	
}

bool CRKDevice::GetUidWriteOK()
{
	return m_bUidWriteOK;
}
uint_16 CRKDevice::GetVendorID()
{
	return m_vid;
}
uint_16 CRKDevice::GetProductID()
{
	return m_pid;
}
ENUM_RKDEVICE_TYPE CRKDevice::GetDeviceType()
{
	return m_device;
}
ENUM_OS_TYPE CRKDevice::GetOsType()
{
	return m_os;
}

ENUM_RKUSB_TYPE CRKDevice::GetUsbType()
{
	return m_usb;
}

ENUM_USB_SPEED CRKDevice::GetUsbSpeed()
{
	return m_usb_speed;
}

uint_16 CRKDevice::GetBcdUsb()
{
	return m_bcdUsb;
}
uint_32 CRKDevice::GetLocationID()
{
	return m_locationID;
}
string CRKDevice::GetLayerName()
{
	return m_layerName;
}
string CRKDevice::GetLinkName()
{
	return m_linkName;
}
string CRKDevice::GetSerialName()
{
	return m_serialName;
}

bool CRKDevice::FormatDisk(char,char *,CRKLog *)
{
	return false;
}
bool CRKDevice::CopyData(char,char *,CRKLog *)
{
	return false;
}
bool CRKDevice::SendExtCommand(char,uint_32,CRKLog *,uint_8*,uint_32)
{
	return false;
}
string CRKDevice::GetLayerString(uint_32 dwLocationID)
{
	string strLocation="";
	char szValue[10];
	int i;
	uint_8 value;
	for(i=0;i<8;i++){
		value = (dwLocationID >> 28) & 0xF;
		if (value>0){
			sprintf(szValue,"%d",value);
			strLocation += szValue;
			if (i<7)
				strLocation += "-";
		} else {
			if (strLocation != "") {
				sprintf(szValue,"%d",value);
				strLocation += szValue;
				if (i<7)
					strLocation += "-";
			}
		}
		dwLocationID <<= 4;
	}
	return strLocation;
}

CRKDevice::CRKDevice(STRUCT_RKDEVICE_DESC &device)
{
	VendorID.setContainer(this);
    VendorID.getter(&CRKDevice::GetVendorID);
 	VendorID.setter(&CRKDevice::SetVendorID);
	
	ProductID.setContainer(this);
    ProductID.getter(&CRKDevice::GetProductID);
 	ProductID.setter(&CRKDevice::SetProductID);
	
	DeviceType.setContainer(this);
    DeviceType.getter(&CRKDevice::GetDeviceType);
 	DeviceType.setter(&CRKDevice::SetDeviceType);
	
	UsbType.setContainer(this);
    UsbType.getter(&CRKDevice::GetUsbType);
 	UsbType.setter(&CRKDevice::SetUsbType);

	UsbSpeed.setContainer(this);
    UsbSpeed.getter(&CRKDevice::GetUsbSpeed);
 	UsbSpeed.setter(&CRKDevice::SetUsbSpeed);
	
	LayerName.setContainer(this);
    LayerName.getter(&CRKDevice::GetLayerName);
 	LayerName.setter(&CRKDevice::SetLayerName);

	LinkName.setContainer(this);
    LinkName.getter(&CRKDevice::GetLinkName);
 	LinkName.setter(&CRKDevice::SetLinkName);

	SerialName.setContainer(this);
    SerialName.getter(&CRKDevice::GetSerialName);
 	SerialName.setter(&CRKDevice::SetSerialName);
	
	BcdUsb.setContainer(this);
    BcdUsb.getter(&CRKDevice::GetBcdUsb);
 	BcdUsb.setter(&CRKDevice::SetBcdUsb);

	
	LocationID.setContainer(this);
    LocationID.getter(&CRKDevice::GetLocationID);
	LocationID.setter(&CRKDevice::SetLocationID);
	
	OsType.setContainer(this);
    OsType.getter(&CRKDevice::GetOsType);
 	OsType.setter(&CRKDevice::SetOsType);
	
	GetNewDiskSizeFlag.setContainer(this);
 	GetNewDiskSizeFlag.setter(&CRKDevice::SetGetNewDiskSizeFlag);
	
	BlockStatePointer.setContainer(this);
    BlockStatePointer.getter(&CRKDevice::GetBlockStatePointer);
 	BlockStatePointer.setter(&CRKDevice::SetBlockStatePointer);
	
	RemallocDisk.setContainer(this);
    RemallocDisk.getter(&CRKDevice::GetRemallocDisk);
 	RemallocDisk.setter(&CRKDevice::SetRemallocDisk);
	
	PrepareEraseFlag.setContainer(this);
 	PrepareEraseFlag.setter(&CRKDevice::SetPrepareEraseFlag);
	
	WorkFlow.setContainer(this);
 	WorkFlow.setter(&CRKDevice::SetWorkFlow);
	
	MiscModifyFlag.setContainer(this);
 	MiscModifyFlag.setter(&CRKDevice::SetMiscModifyFlag);
	
	UidWriteOK.setContainer(this);
 	UidWriteOK.getter(&CRKDevice::GetUidWriteOK);
	
	Sn.setContainer(this);
    Sn.getter(&CRKDevice::GetSn);
 	Sn.setter(&CRKDevice::SetSn);
	
	SnSize.setContainer(this);
    SnSize.getter(&CRKDevice::GetSnSize);
 	SnSize.setter(&CRKDevice::SetSnSize);
	
	Mac.setContainer(this);
    Mac.getter(&CRKDevice::GetMac);
 	Mac.setter(&CRKDevice::SetMac);
	
	MacSize.setContainer(this);
    MacSize.getter(&CRKDevice::GetMacSize);
	
	Imei.setContainer(this);
    Imei.getter(&CRKDevice::GetImei);
 	Imei.setter(&CRKDevice::SetImei);
	
	ImeiSize.setContainer(this);
   	ImeiSize.getter(&CRKDevice::GetImeiSize);
	
	BTAddr.setContainer(this);
    BTAddr.getter(&CRKDevice::GetBTAddr);
 	BTAddr.setter(&CRKDevice::SetBTAddr);
	
	BTSize.setContainer(this);
    BTSize.getter(&CRKDevice::GetBTSize);
	
	Uid.setContainer(this);
    Uid.getter(&CRKDevice::GetUid);
 	Uid.setter(&CRKDevice::SetUid);
	
	UidSize.setContainer(this);
    UidSize.getter(&CRKDevice::GetUidSize);

	IDBend.setContainer(this);
	IDBend.getter(&CRKDevice::GetIDBend);

	EmmcStor.setContainer(this);
	EmmcStor.getter(&CRKDevice::GetEmmcStor);

	NorStor.setContainer(this);
	NorStor.getter(&CRKDevice::GetNorStor);
	
	CustomData.setContainer(this);
    CustomData.getter(&CRKDevice::GetCustomData);
 	CustomData.setter(&CRKDevice::SetCustomData);
	
	CustomDataSize.setContainer(this);
    CustomDataSize.getter(&CRKDevice::GetCustomDataSize);
 	CustomDataSize.setter(&CRKDevice::SetCustomDataSize);
	
	CustomDataOffset.setContainer(this);
    CustomDataOffset.getter(&CRKDevice::GetCustomDataOffset);
 	CustomDataOffset.setter(&CRKDevice::SetCustomDataOffset);
	
	LogObjectPointer.setContainer(this);
    LogObjectPointer.getter(&CRKDevice::GetLogObjectPointer);
	
	CommObjectPointer.setContainer(this);
    CommObjectPointer.getter(&CRKDevice::GetCommObjectPointer);
	
	CallBackPointer.setContainer(this);
 	CallBackPointer.setter(&CRKDevice::SetCallBackPointer);

	DirectLBA.setContainer(this);
	DirectLBA.getter(&CRKDevice::GetDirectLBA);
	DirectLBA.setter(&CRKDevice::SetDirectLBA);

	ReadIDBConfigFlag.setContainer(this);
	ReadIDBConfigFlag.getter(&CRKDevice::GetReadIDBConfigFlag);
	ReadIDBConfigFlag.setter(&CRKDevice::SetReadIDBConfigFlag);

	SecureModeFlag.setContainer(this);
	SecureModeFlag.getter(&CRKDevice::GetSecureModeFlag);
	SecureModeFlag.setter(&CRKDevice::SetSecureModeFlag);

	NewIDBFlag.setContainer(this);
	NewIDBFlag.getter(&CRKDevice::GetNewIDBFlag);
	NewIDBFlag.setter(&CRKDevice::SetNewIDBFlag);

	SwitchStorageFlag.setContainer(this);
	SwitchStorageFlag.getter(&CRKDevice::GetSwitchStorageFlag);
	SwitchStorageFlag.setter(&CRKDevice::SetSwitchStorageFlag);

	SwitchUsb3Flag.setContainer(this);
	SwitchUsb3Flag.getter(&CRKDevice::GetSwitchUsb3Flag);
	SwitchUsb3Flag.setter(&CRKDevice::SetSwitchUsb3Flag);

	UsbDriver4M.setContainer(this);
	UsbDriver4M.getter(&CRKDevice::GetUsbDriver4M);
	UsbDriver4M.setter(&CRKDevice::SetUsbDriver4M);

	First4Access.setContainer(this);
	First4Access.getter(&CRKDevice::GetFirst4Access);
	First4Access.setter(&CRKDevice::SetFirst4Access);

	GptFlag.setContainer(this);
	GptFlag.getter(&CRKDevice::GetGptFlag);
	GptFlag.setter(&CRKDevice::SetGptFlag);

	NoCheckMode.setContainer(this);
	NoCheckMode.getter(&CRKDevice::GetNoCheckMode);
	NoCheckMode.setter(&CRKDevice::SetNoCheckMode);

	IDBCheckMode.setContainer(this);
	IDBCheckMode.getter(&CRKDevice::GetIDBCheckMode);
	IDBCheckMode.setter(&CRKDevice::SetIDBCheckMode);

	Usb3Transfer.setContainer(this);
	Usb3Transfer.getter(&CRKDevice::GetUsb3Transfer);
	Usb3Transfer.setter(&CRKDevice::SetUsb3Transfer);

	NorSingleIDB.setContainer(this);
	NorSingleIDB.getter(&CRKDevice::GetNorSingleIDB);
	NorSingleIDB.setter(&CRKDevice::SetNorSingleIDB);

	ForceDataBand.setContainer(this);
	ForceDataBand.getter(&CRKDevice::GetForceDataBand);
	ForceDataBand.setter(&CRKDevice::SetForceDataBand);
	
	Error.setContainer(this);
	Error.getter(&CRKDevice::GetError);

	m_vid = device.usVid;
	m_pid = device.usPid;
	m_usb = device.emUsbType;
	m_device = device.emDeviceType;
	m_usb_speed = device.emUsbSpeed;
	m_bcdUsb = device.usbcdUsb;
	m_locationID = device.uiLocationID;
	m_layerName = GetLayerString(m_locationID);
	m_linkName = device.strDevPath;
	m_serialName = device.strSerial;

	memset(m_idBlockOffset,0,sizeof(uint_32)*5);
	memset(m_flashInfo.blockState,0,IDBLOCK_TOP);
	m_flashInfo.usPhyBlokcPerIDB = 1;
	m_flashInfo.uiSecNumPerIDB = 0;
	m_callBackProc = NULL;
	m_chipData = NULL;
	m_pImage = NULL;
	m_pLog = NULL;
	m_pComm = NULL;
	m_customData = NULL;
	m_customDataSize = 0;
	m_customDataOffset = 0;
	m_sn = NULL;
	m_snSize = 0;
	m_mac = NULL;
	m_imei = NULL;
	m_blueTooth = NULL;
	m_uid = NULL;
	m_sysDiskSize = 0;
	m_cfgDiskSize = 0;
	m_bGetNewDiskSizeFlag = true;
	m_pBlockState = NULL;
	m_bWriteBack = false;
	m_pFlashInfoData = NULL;
	m_usFlashInfoDataLen = 0;
	m_usFlashInfoDataOffset = 0;
	memset(m_backupBuffer,0,SECTOR_SIZE);
	memset(m_backupBuffer+SECTOR_SIZE,0xFF,SPARE_SIZE);
	m_bUidWriteOK = false;
	m_remallocDisk = false;
	m_emMiscModifyFlag = MISC_MODIFY_NONE;
	m_bNoCheckMode = false;
	m_bCloseIDBCheck = false;
	m_bNorSingleIDB = false;
	m_bExistSector3Crc = false;
	m_bEmmc = false;
	m_bNor = false;
	m_usSector3Crc = 0;
	m_usWriteBackCrc = 0;
	m_usWriteBackCustomDataOffset = 0;
	m_usWriteBackCustomDataSize = 0;
	m_bDirectLba = false;
	m_bFirst4Access = false;
	m_bReadIDBFlag = false;
	m_bSecureModeFlag = false;
	m_bNewIDBFlag = false;
	m_bGptFlag = false;
	m_bUsbDriver4m = false;
	m_uiIDBend = 0;
	m_error = 0;
	m_uiForceBand = 0;
}
CRKDevice::~CRKDevice()
{
	if (m_pComm)
	{
		delete m_pComm;
		m_pComm = NULL;
	}
	if (m_chipData)
	{
		delete []m_chipData;
		m_chipData = NULL;
	}
	if (m_customData)
	{
		delete []m_customData;
		m_customData = NULL;
	}
	if (m_sn)
	{
		delete []m_sn;
		m_sn = NULL;
	}
	if (m_mac)
	{
		delete []m_mac;
		m_mac = NULL;
	}
	if (m_imei)
	{
		delete []m_imei;
		m_imei = NULL;
	}
	if (m_blueTooth)
	{
		delete []m_blueTooth;
		m_blueTooth = NULL;
	}
	if (m_uid)
	{
		delete []m_uid;
		m_uid = NULL;
	}

	if (m_pFlashInfoData)
	{
		delete []m_pFlashInfoData;
		m_pFlashInfoData = NULL;
	}
	
}
bool CRKDevice::SetObject(CRKImage *pImage,CRKComm *pComm,CRKLog *pLog)
{
	if (!pComm)
	{
		return false;
	}
	m_pImage = pImage;
	m_pComm = pComm;
	m_pLog = pLog;
	if (m_pImage)
	{
		m_os = m_pImage->OsType;
	}
	else
		m_os = RK_OS;
	return true;
}
int CRKDevice::EraseEmmcBlock(uint_8 ucFlashCS,uint_32 dwPos,uint_32 dwCount)
{
	int sectorOffset,nWrittenBlcok,iRet;
	uint_8 emptyData[4*(SECTOR_SIZE+SPARE_SIZE)];
	memset(emptyData,0xff,4*(SECTOR_SIZE+SPARE_SIZE));
	nWrittenBlcok = 0;
	while (dwCount>0)
	{
		sectorOffset = (ucFlashCS*m_flashInfo.uiBlockNum+dwPos+nWrittenBlcok)*m_flashInfo.uiSectorPerBlock;
		iRet = m_pComm->RKU_WriteSector(sectorOffset,4,emptyData);
		if ((iRet!=ERR_SUCCESS)&&(iRet!=ERR_FOUND_BAD_BLOCK))
		{
			m_error = ERR_COMM_RW;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:EraseEmmcBlock-->RKU_WriteSector failed,RetCode(%d)",iRet);
			}
			return iRet;
		}
		dwCount--;
		nWrittenBlcok++;
	}
	return ERR_SUCCESS;
}
int CRKDevice::EraseEmmcByWriteLBA(uint_32 dwSectorPos,uint_32 dwCount)
{
	int nWritten,iRet;
	uint_8 emptyData[32*SECTOR_SIZE];
	memset(emptyData,0xff,32*SECTOR_SIZE);

	while (dwCount>0)
	{
		nWritten = (dwCount<32)?dwCount:32;
		iRet = m_pComm->RKU_WriteLBA(dwSectorPos,nWritten,emptyData);
		if (iRet!=ERR_SUCCESS)
		{
			m_error = ERR_COMM_RW;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:EraseEmmcByWriteLBA-->RKU_WriteLBA failed,RetCode(%d)",iRet);
			}
			return iRet;
		}
		dwCount -= nWritten;
		dwSectorPos += nWritten;
	}
	return ERR_SUCCESS;
}
bool CRKDevice::EraseEmmcIDB()
{
	uint_32 dwCount=8*1024;//erase 4m bytes
	uint_32 dwErasePos,dwEraseOnce,dwErase;
	bool bSuccess = true;
	int iRet;
	if (!m_bNor) {
		iRet = m_pComm->RKU_EraseLBA(0,dwCount);

		if (iRet!=ERR_SUCCESS)
		{
			m_error = ERR_COMM_RW;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:EraseEmmcIDB-->RKU_EraseLBA failed,err=%d",iRet);
			}
			bSuccess = false;
		}
	} else {
		dwEraseOnce = 256;
		dwErasePos = 0;
		while (dwCount) {
			if (dwCount>=dwEraseOnce)
				dwErase = dwEraseOnce;
			else
				dwErase = dwCount;
			iRet = m_pComm->RKU_EraseLBA(dwErasePos,dwErase);

			if (iRet!=ERR_SUCCESS)
			{
				m_error = ERR_COMM_RW;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:EraseEmmcIDB-->RKU_EraseLBA failed,err=%d",iRet);
				}
				bSuccess = false;
				break;
			}
			dwCount -= dwErase;
			dwErasePos += dwErase;
		}
	}
	return bSuccess;
}
bool CRKDevice::EraseEmmc()
{
	uint_32 uiCount,uiEraseCount,uiSectorOffset,uiTotalCount;
	uint_32 uiErase=1024*32;
	int iRet=ERR_SUCCESS,iLoopTimes=0;
	uiTotalCount = uiCount = m_flashInfo.uiFlashSize*2*1024;
	uiSectorOffset = 0;
	uint_32 dwLayerID;
	dwLayerID = m_locationID;
	ENUM_CALL_STEP emCallStep=CALL_FIRST;
	if (m_bNor)
		uiErase = 256;

	while (uiCount)
	{
		if (uiCount>=uiErase)
		{
			uiEraseCount = uiErase;
		}
		else
			uiEraseCount = uiCount;
		iRet = m_pComm->RKU_EraseLBA(uiSectorOffset,uiEraseCount);
			
		if (iRet!=ERR_SUCCESS)
		{
			m_error = ERR_COMM_RW;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:EraseEmmc-->RKU_EraseLBA failed,RetCode(%d),offset=0x%x,count=0x%x",iRet,uiSectorOffset,uiEraseCount);
			}
			return false;
		}
		uiCount -= uiEraseCount;
		uiSectorOffset += uiEraseCount;
		iLoopTimes++;
		if (iLoopTimes%8==0)
		{
			if (m_callBackProc)
			{
				m_callBackProc(dwLayerID,ERASEFLASH_PROGRESS,uiTotalCount,uiSectorOffset,emCallStep);
				emCallStep = CALL_MIDDLE;
			}
		}
	}
	if (m_callBackProc)
	{
		emCallStep = CALL_LAST;
		m_callBackProc(dwLayerID,ERASEFLASH_PROGRESS,uiTotalCount,uiTotalCount,emCallStep);
	}
	return true;
}
bool CRKDevice::GetFlashInfo()
{
	STRUCT_FLASHINFO_CMD info;
	uint_8 flashID[5];
	int iRet;
	uint_32 uiRead;
	iRet = m_pComm->RKU_ReadFlashInfo((uint_8*)&info,&uiRead);
	if( ERR_SUCCESS == iRet )
	{
		if ((info.usBlockSize==0)||(info.bPageSize==0))
		{
			m_error = ERR_PAGE_ZERO;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:GetFlashInfo-->RKU_ReadFlashInfo failed,pagesize or blocksize is zero");
			}
			return false;
		}

		strcpy(m_flashInfo.szManufacturerName,"UNKNOWN");
		
		if (info.bManufCode > 200)
			m_flashInfo.uiLogicalBlockSize = info.bManufCode - 200;
		else
			m_flashInfo.uiLogicalBlockSize = 1;

		m_flashInfo.uiFlashSizeSector = info.uiFlashSize;
		m_flashInfo.usBlockSizeSector = info.usBlockSize;
		m_flashInfo.bPageSizeSector = info.bPageSize;
		m_flashInfo.bPageAvailSizeSector = info.bPageAvailSize;
		
		m_flashInfo.uiFlashSize = info.uiFlashSize/2/1024;//MB
		m_flashInfo.uiPageSize = info.bPageSize/2;//KB
		m_flashInfo.usBlockSize = info.usBlockSize/2;//KB
		m_flashInfo.bECCBits = info.bECCBits;
		m_flashInfo.bAccessTime = info.bAccessTime;
		m_flashInfo.uiBlockNum = info.uiFlashSize/info.usBlockSize;
		m_flashInfo.uiSectorPerBlock = info.usBlockSize;
		m_flashInfo.bFlashCS = info.bFlashCS;
		m_flashInfo.usValidSecPerBlock = (info.usBlockSize/info.bPageSize)*4;
		if (m_pFlashInfoData)
		{
			delete []m_pFlashInfoData;
			m_pFlashInfoData = NULL;
		}
		m_usFlashInfoDataLen = BYTE2SECTOR(uiRead);
		m_pFlashInfoData = new uint_8[SECTOR_SIZE*m_usFlashInfoDataLen];
		memset(m_pFlashInfoData,0,SECTOR_SIZE*m_usFlashInfoDataLen);
		memcpy(m_pFlashInfoData,(uint_8*)&info,uiRead);
		if (m_pLog)
		{
			string strFlashInfo;
			m_pLog->PrintBuffer(strFlashInfo,m_pFlashInfoData,11);
			m_pLog->Record("INFO:FlashInfo:%s",strFlashInfo.c_str());
		}
	}
	else
	{
		m_error = ERR_COMM_RW;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:GetFlashInfo-->RKU_ReadFlashInfo failed,RetCode(%d)",iRet);
		}
		return false;
	}
	iRet = m_pComm->RKU_ReadFlashID(flashID);
	if( ERR_SUCCESS == iRet )
	{
		uint_32 *pID=(uint_32 *)flashID;
		if (*pID==0x434d4d45)//emmc
		{
			if (m_pLog)
				m_pLog->Record("INFO:Emmc Storage");
			m_bEmmc = true;
		}
		else{
			m_bEmmc = false;
			if ((flashID[0]=='N') && (flashID[1]=='O') && (flashID[2]=='R'))
			{
				if (m_pLog)
					m_pLog->Record("INFO:Nor Storage");
				m_bNor = true;
			}
			else
				m_bNor = false;
		}
	}
	else
	{
		m_error = ERR_COMM_RW;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:GetFlashInfo-->RKU_ReadFlashID failed,RetCode(%d)",iRet);
		}
		return false;
	}
	
	return true;
}
bool CRKDevice::BuildBlockStateMap(uint_8 bFlashCS)
{
	uint_8 blockState[64];
	int iRet,i,j ;
	memset(blockState, 0, 64);
	iRet = m_pComm->RKU_TestBadBlock( bFlashCS, 0, MAX_TEST_BLOCKS, blockState);
	if(ERR_SUCCESS == iRet)
	{
		return true;
	}
	else if(ERR_FOUND_BAD_BLOCK == iRet)
	{
		for(i=0; i<64; i++)
		{
			for(j=0; j<8; j++)
			{
				if( blockState[i] & (1<<j) )
					m_flashInfo.blockState[i*8+j]=1;
				if (i*8+j>(IDBLOCK_TOP-2))
				{
					break;
				}
			}
			if (j<8)
			{
				break;
			}
		}
		return true;
	}
	else
	{
		m_error = ERR_COMM_RW;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:BuildBlockStateMap-->RKU_TestBadBlock failed,RetCode(%d)",iRet);
		}
		return false;
	}
}
int CRKDevice::ReadMutilSector(uint_32 dwPos,uint_32 dwCount,uint_8* lpBuffer)
{
	uint_32 dwReadSector=0,dwMaxReadWriteOnce;
	int iUsedSecCount,iUsedBlockCount,iValidSecCount;
	int iRet=0,iCurPos;
	iUsedBlockCount = dwPos / m_flashInfo.uiSectorPerBlock;
	iUsedSecCount = dwPos - (iUsedBlockCount*m_flashInfo.uiSectorPerBlock);
	iValidSecCount = m_flashInfo.usValidSecPerBlock-iUsedSecCount;
	
	if ((DeviceType==RKNANO_DEVICE)||(DeviceType==RKSMART_DEVICE))
	{
		dwMaxReadWriteOnce = 4;
	}
	else
		dwMaxReadWriteOnce = MAX_WRITE_SECTOR;
		
	while(dwCount>0)
	{
		dwReadSector = (dwCount>=dwMaxReadWriteOnce) ? dwMaxReadWriteOnce : dwCount;
		if (dwReadSector > static_cast<uint_32>(iValidSecCount))
		{
			dwReadSector = iValidSecCount;
		}
		iCurPos = iUsedBlockCount*m_flashInfo.uiSectorPerBlock+(m_flashInfo.usValidSecPerBlock-iValidSecCount);
		iRet = m_pComm->RKU_ReadSector(iCurPos, dwReadSector,lpBuffer);
		if( iRet!=ERR_SUCCESS )
		{
			m_error = ERR_COMM_RW;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:ReadMutilSector-->RKU_ReadSector failed,RetCode(%d)",iRet);
			}
			break;
		}
		
		dwCount -= dwReadSector;
		iUsedSecCount += dwReadSector;
		iValidSecCount -= dwReadSector;
		if (iValidSecCount<=0)
		{
			iUsedBlockCount++;
			iValidSecCount = m_flashInfo.usValidSecPerBlock;
		}
		lpBuffer += dwReadSector * 528;
	}
	return iRet;
}
bool CRKDevice::EraseMutilBlock(uint_8 bFlashCS,uint_32 dwPos,uint_32 dwCount,bool bForce)
{
	uint_32 dwTimes = 0;
	uint_8 eraseType;
	eraseType = bForce?ERASE_FORCE:ERASE_NORMAL;
	int iRet;
	while(dwCount >= MAX_ERASE_BLOCKS)
	{
		iRet = m_pComm->RKU_EraseBlock( bFlashCS, dwPos+dwTimes*MAX_ERASE_BLOCKS, MAX_ERASE_BLOCKS, eraseType);
		if(ERR_FOUND_BAD_BLOCK == iRet)
		{
			dwCount -= MAX_ERASE_BLOCKS;
			++dwTimes;
		}else if(ERR_SUCCESS == iRet)
		{
			dwCount -= MAX_ERASE_BLOCKS;
			++dwTimes;
		}
		else
		{
			m_error = ERR_COMM_RW;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:EraseMutilBlock-->RKU_EraseBlock failed,RetCode(%d)",iRet);
			}
			return false;
		}
	}
	if(dwCount>0)
	{
		iRet = m_pComm->RKU_EraseBlock( bFlashCS, dwPos+dwTimes*MAX_ERASE_BLOCKS, dwCount, eraseType);
		if(ERR_SUCCESS == iRet)
		{
			dwCount = 0;
		}
		else if(ERR_FOUND_BAD_BLOCK == iRet)
		{
			dwCount = 0;
		}
		else
		{
			m_error = ERR_COMM_RW;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:EraseMutilBlock-->RKU_EraseBlock failed,RetCode(%d)",iRet);
			}
			return false;
		}
	}
	return true;
}
bool CRKDevice::TestDevice()
{
	int iResult = ERR_FAILED, iTryCount;//,iZeroCount=0;
	uint_32 dwTotal = 0, dwCurrent = 0, dwLayerID;
	dwLayerID = m_locationID;
	ENUM_CALL_STEP emCallStep=CALL_FIRST;
	do 
	{
		iTryCount = 3;
		while (iTryCount>0)
		{
			iResult = m_pComm->RKU_TestDeviceReady( &dwTotal,&dwCurrent );
			if ((iResult==ERR_SUCCESS)||(iResult==ERR_DEVICE_UNREADY))
			{
				break;
			}
			if (m_pLog)
			{
				m_pLog->Record("ERROR:TestDevice-->RKU_TestDeviceReady failed,RetCode(%d)",iResult);
			}
			iTryCount--;
			sleep_s(1);
		}
		if (iTryCount<=0)
		{
			m_error = ERR_COMM_RW;
			return false;
		}
		
		if (iResult==ERR_SUCCESS)
		{
			if (emCallStep==CALL_MIDDLE)
			{
				if (m_callBackProc)
				{
					dwCurrent = dwTotal;
					emCallStep=CALL_LAST;
					m_callBackProc(dwLayerID,TESTDEVICE_PROGRESS,dwTotal,dwCurrent,emCallStep);
				}
			}
			break;
		}
		if (dwCurrent>dwTotal)
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:TestDevice-->RKU_TestDeviceReady failed,Total=%d,Current=%d",dwTotal,dwCurrent);
			}
			return false;
		}
		if (UsbType==RKUSB_LOADER)
		{
			if (dwTotal==0)
			{
//				string strFile;
				if ((m_pImage)&&(m_pImage->SignFlag))
				{//sign image go on unlock loader
					uint_8 md5Data[SECTOR_SIZE];
					uint_8 *md5=NULL,*signMd5=NULL;
					m_pImage->GetMd5Data(md5,signMd5);
					memset(md5Data,0,SECTOR_SIZE);
					memcpy(md5Data,signMd5,256);
					memcpy(md5Data+256,md5,32);
//					if (m_pLog)
//					{
//						strFile = m_pLog->LogSavePath;
//						strFile += "data_in.bin";
//						m_pLog->SaveBuffer(strFile,md5Data,SECTOR_SIZE);
//					}
					//unlock loader
					iResult = m_pComm->RKU_WriteLBA( 0xFFFFF000,1,md5Data );
					if (iResult!=ERR_SUCCESS)
					{
						m_error = ERR_COMM_RW;
						if (m_pLog)
						{
							m_pLog->Record("ERROR:TestDevice-->RKU_WriteLBA failed,RetCode(%d)",iResult);
						}
						return false;
					}
					sleep_s(1);
					//check loader result
					iResult = m_pComm->RKU_ReadLBA( 0xFFFFF000,1,md5Data );
					if (iResult!=ERR_SUCCESS)
					{
						m_error = ERR_COMM_RW;
						if (m_pLog)
						{
							m_pLog->Record("ERROR:TestDevice-->RKU_ReadLBA failed,RetCode(%d)",iResult);
						}
						return false;
					}
//					if (m_pLog)
//					{
//						strFile = m_pLog->LogSavePath;
//						strFile += "data_out.bin";
//						m_pLog->SaveBuffer(strFile,md5Data,SECTOR_SIZE);
//					}
					if (memcmp(md5Data,md5Data+256,256)!=0)
					{
						m_error = ERR_SIGN_UNLOCK;
						if (m_pLog)
						{
							m_pLog->Record("ERROR:TestDevice-->Loader check failed!");
						}
						return false;
					}
				}
				else
				{
					m_error = ERR_FW_UNSIGN;
					if (m_pLog)
					{
						m_pLog->Record("ERROR:TestDevice-->RKU_TestDeviceReady failed,Total is zero");
					}
					return false;
				}
				continue;
			}
		}
		if (m_callBackProc)
		{
			m_callBackProc(dwLayerID,TESTDEVICE_PROGRESS,dwTotal,dwCurrent,emCallStep);
			emCallStep=CALL_MIDDLE;
		}
		sleep_s(1);
	}while( iResult==ERR_DEVICE_UNREADY );
	return true;
}
bool CRKDevice::LowerFormatDevice(bool &bSupport)
{
	int iResult = ERR_FAILED, iTryCount;
	uint_32 dwTotal = 0, dwCurrent = 0, dwLayerID;
	dwLayerID = m_locationID;
	ENUM_CALL_STEP emCallStep=CALL_FIRST;
	uint_8 subCode=TU_LOWERFORMAT_SUBCODE;
	bSupport = true;
	do 
	{
		iTryCount = 3;
		while (iTryCount>0)
		{
			iResult = m_pComm->RKU_TestDeviceReady( &dwTotal,&dwCurrent,subCode );
			if ((iResult==ERR_SUCCESS)||(iResult==ERR_DEVICE_UNREADY))
			{
				break;
			}
			if (m_pLog)
			{
				m_pLog->Record("ERROR:LowerFormatDevice-->RKU_TestDeviceReady failed,subCode(0x%x),RetCode(%d)",subCode,iResult);
			}
			iTryCount--;
			sleep_s(1);
		}
		if (iTryCount<=0)
		{
			m_error = ERR_COMM_RW;
			return false;
		}
		if (iResult==ERR_SUCCESS)
		{
			if (subCode==TU_LOWERFORMAT_SUBCODE)
			{
				if (m_pLog)
				{
					m_pLog->Record("ERROR:LowerFormatDevice-->Not support lowerformat");
				}
				bSupport = false;
				return true;
			}
			if (emCallStep==CALL_MIDDLE)
			{
				if (m_callBackProc)
				{
					dwCurrent = dwTotal;
					emCallStep=CALL_LAST;
					m_callBackProc(dwLayerID,LOWERFORMAT_PROGRESS,dwTotal,dwCurrent,emCallStep);
				}
			}
			break;
		}
		if (subCode==TU_LOWERFORMAT_SUBCODE)
		{
			subCode = TU_NONE_SUBCODE;
		}
		if (m_callBackProc)
		{
			m_callBackProc(dwLayerID,LOWERFORMAT_PROGRESS,dwTotal,dwCurrent,emCallStep);
			emCallStep=CALL_MIDDLE;
		}
		sleep_s(1);
	}while( iResult==ERR_DEVICE_UNREADY );
	return true;
}
bool CRKDevice::ResetDevice(uint_8 subCode)
{
	int iRet;
	iRet = m_pComm->RKU_ResetDevice(subCode);
	if (iRet==ERR_SUCCESS)
	{
		return true;
	}
	else
	{
		bool bRet=false;
		if ((iRet==-2)||(iRet==-4))
		{
			bRet = true;
		}
		else
			m_error = ERR_COMM_RW;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:ResetDevice-->RKU_ResetDevice failed,RetCode(%d)",iRet);
		}
		return bRet;
	}
}
bool CRKDevice::SetResetFlag()
{
	int iRet;
	iRet = m_pComm->RKU_ResetDevice(RST_RESETMSC_SUBCODE);
	if (iRet==ERR_SUCCESS)
	{
		return true;
	}
	else
	{
		m_error = ERR_COMM_RW;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:SetResetFlag-->RKU_ResetDevice failed,RetCode(%d)",iRet);
		}
		return false;
	}
}
bool CRKDevice::SwitchUsb3()
{
	int iRet;
	string strSn;

	strSn = m_layerName;
	strSn.erase(remove(strSn.begin(),strSn.end(),'-'),strSn.end());

	iRet = m_pComm->RKU_SwitchUsb3((char *)strSn.c_str());

	if (iRet == ERR_SUCCESS)
	{
		return true;
	}
	else
	{
		m_error = ERR_COMM_RW;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:SwitchUsb3-->RKU_SwitchUsb3 failed,RetCode(%d)", iRet);
		}
		return false;
	}
}
bool CRKDevice::PowerOffDevice()
{
	int iRet;
	iRet = m_pComm->RKU_ResetDevice(RST_POWEROFF_SUBCODE);
	if (iRet==ERR_SUCCESS)
	{
		return true;
	}
	else
	{
		m_error = ERR_COMM_RW;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:PowerOffDevice-->RKU_ResetDevice failed,RetCode(%d)",iRet);
		}
		return false;
	}
}
bool CRKDevice::CheckChip()
{
	int iRet;
	uint_8 bChipInfo[CHIPINFO_LEN];
	ENUM_RKDEVICE_TYPE curDeviceType=RKNONE_DEVICE;
	memset(bChipInfo,0,CHIPINFO_LEN);
	iRet = m_pComm->RKU_ReadChipInfo(bChipInfo);
	if (iRet==ERR_SUCCESS)
	{
		if (!m_chipData)
		{
			m_chipData = new uint_8[CHIPINFO_LEN];
		}
		memset(m_chipData,0,CHIPINFO_LEN);
		memcpy(m_chipData,bChipInfo,CHIPINFO_LEN);
		uint_32 *pValue;
		pValue = (uint_32 *)(&bChipInfo[0]);
		
		if ((ENUM_RKDEVICE_TYPE)(*pValue)==m_device)
		{
			return true;
		}
		
		if (*pValue==0x524B3237)
		{
			curDeviceType = RK27_DEVICE;
		}
		else if (*pValue==0x32373341)
		{
			curDeviceType = RKCAYMAN_DEVICE;
		}
		else if (*pValue==0x524B3238)
		{
			curDeviceType = RK28_DEVICE;
		}
		else if (*pValue==0x32383158)
		{
			curDeviceType = RK281X_DEVICE;
		}
		else if (*pValue==0x32383242)
		{
			curDeviceType = RKPANDA_DEVICE;
		}
		else if (*pValue==0x32393058)
		{
			curDeviceType = RK29_DEVICE;
		}
		else if (*pValue==0x32393258)
		{
			curDeviceType = RK292X_DEVICE;
		}
		else if (*pValue==0x33303041)
		{
			curDeviceType = RK30_DEVICE;
		}
		else if (*pValue==0x33313041)
		{
			curDeviceType = RK30B_DEVICE;
		}
		else if (*pValue==0x33313042)
		{
			curDeviceType = RK31_DEVICE;
		}
		else if (*pValue==0x33323041)
		{
			curDeviceType = RK32_DEVICE;
		}
		else if (*pValue==0x32363243)
		{
			curDeviceType = RKSMART_DEVICE;
		}
		else if (*pValue==0x6E616E6F)
		{
			curDeviceType = RKNANO_DEVICE;
		}
		else if (*pValue==0x4E4F5243)
		{
			curDeviceType = RKCROWN_DEVICE;
		}

		if (curDeviceType==m_device)
		{
			return true;
		}
		else
		{
			m_error = ERR_WRONG_CHIP;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:CheckChip-->Chip is not match,firmware(0x%x),device(0x%x)",m_device,*pValue);
			}
			return false;
		}
	}
	else
	{
		m_error = ERR_COMM_RW;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:CheckChip-->RKU_ReadChipInfo failed,RetCode(%d)",iRet);
		}
		return false;
	}
}
char CRKDevice::FindValidBlocks(char bBegin, char bLen)
{
	char bCount = 0;
	char bIndex = bBegin;
	while(bBegin < IDBLOCK_TOP)
	{
		if(0 == m_flashInfo.blockState[static_cast<int>(bBegin++)])
			++bCount;
		else
		{
			bCount = 0;
			bIndex = bBegin;
		}
		if(bCount >= bLen)
			break;
	}
	if(bBegin >= IDBLOCK_TOP)
		bIndex = -1;
	
	return bIndex;
}
uint_16 UshortToBCD(uint_16 num)
{
	uint_16 bcd = 0;
	bcd = (num % 10) | ( ((num/10 )% 10)<< 4 )|( ((num/100) %10) << 8)|( ((num/1000) %10) << 12);
	return bcd;
}
uint_8   ByteToBCD(uint_8 num)
{
	uint_8 bcd = 0;
	bcd = (num % 10) | ( ((num/10 )% 10)<< 4 );
	return bcd;
}
uint_8 CRKDevice::RandomByte(uint_8 bLowLimit,uint_8 bHighLimit)
{
	uint_8 k;
	double d;
	
	d = (double)rand() / ((double)RAND_MAX+1);
	k = (uint_8)( d*(bHighLimit-bLowLimit+1) );
	return (bLowLimit+k);
}
bool CRKDevice::CheckCrc16(uint_8* pCheckData,uint_16 usDataLength,uint_16 usOldCrc)
{
	uint_16 usNewCrc;
	usNewCrc = CRC_16(pCheckData,usDataLength);
	return (usNewCrc==usOldCrc)?true:false;
}
bool CRKDevice::CheckUid(uint_8 uidSize,uint_8 *pUid)
{
	if (uidSize!=RKDEVICE_UID_LEN)
	{
		return false;
	}
	uint_16 oldCrc,newCrc;
	oldCrc = *(uint_16 *)(pUid+RKDEVICE_UID_LEN-2);
	newCrc = CRC_CCITT(pUid,RKDEVICE_UID_LEN-2);
	if (oldCrc!=newCrc)
	{
		return false;
	}
	return true;
}

bool CRKDevice::GetIDBData(uint_32 uiIDBCount,uint_8* lpBuf,uint_32 uiSecCount)
{
	uint_8* pIDB;
	pIDB = new uint_8[uiSecCount*528];
	memset( pIDB, 0, uiSecCount*528 );
	int i,j,iResult;
	int nSrc=-1,nDst=-1;
	bool bRet;
	for (i=0;static_cast<uint_32>(i)<uiIDBCount;i++)
	{
		if (nSrc==-1)
		{
			iResult = ReadMutilSector( m_flashInfo.uiSectorPerBlock*m_idBlockOffset[i], uiSecCount, lpBuf );
			if (iResult!=ERR_SUCCESS)
			{
				if (m_pLog)
				{
					m_pLog->Record("ERROR:GetIDBData-->RKU_ReadSector failed,RetCode(%d)",iResult);
				}
				continue;
			}
			nSrc = i;
			continue;
		}
		if (nDst==-1)
		{
			iResult = ReadMutilSector( m_flashInfo.uiSectorPerBlock*m_idBlockOffset[i], uiSecCount, pIDB );
			if (iResult!=ERR_SUCCESS)
			{
				if (m_pLog)
				{
					m_pLog->Record("ERROR:GetIDBData-->RKU_ReadSector failed,RetCode(%d)",iResult);
				}
				continue;
			}
			nDst = i;
		}
		
		bRet = true;

		if ((DeviceType==RKNANO_DEVICE)||(DeviceType==RKSMART_DEVICE))
		{

			uint_32 dwCount=uiSecCount;
			j=0;
			while (dwCount>0)
			{
				if (dwCount<4)
				{
					bRet = memcmp(lpBuf+528*j,pIDB+528*j,512*dwCount)==0;
					j+=dwCount;
					dwCount = 0;
				}
				else
				{
					bRet = memcmp(lpBuf+528*j,pIDB+528*j,512*4)==0;
					j+=4;
					dwCount -= 4;
				}
				if (!bRet)
				{
					break;
				}
			}
		}
		else
		{
			for(j=0;static_cast<uint_32>(j)<uiSecCount;j++)
			{
				bRet = memcmp(lpBuf+528*j,pIDB+528*j,512)==0;
				if (!bRet)
					break;
			}
		}
		
		
		if (bRet)
		{//��ͬ
			delete []pIDB;
			pIDB = NULL;
			return true;
		}
		else
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:GetIDBData-->memcmp failed,src(%d),Dst(%d)",nSrc,nDst);
			}
			memcpy(lpBuf,pIDB,528*uiSecCount);
			nSrc = nDst;
			nDst = -1;
			continue;
		}
	}
	delete []pIDB;
	pIDB = NULL;
	if (nSrc!=-1)
	{
		return true;
	}
	return false;
}

bool CRKDevice::GetWriteBackData(uint_32 uiIDBCount,uint_8* lpBuf)
{
	bool bRet;
	bRet = GetIDBData(uiIDBCount,lpBuf,4);
	return bRet;
}
