#ifndef RKANDROIDDEVICE_HEADER
#define RKANDROIDDEVICE_HEADER
#include "RKDevice.h"
#include "gpt.h"

#pragma pack(1)
typedef	struct 
{
	uint_32	dwTag;
	uint_8	reserved[4];
	uint_32	uiRc4Flag;
	uint_16	usBootCode1Offset;
	uint_16	usBootCode2Offset;
	uint_8	reserved1[490];
	uint_16  usBootDataSize;
	uint_16	usBootCodeSize;
	uint_16	usCrc;
}RKANDROID_IDB_SEC0,*PRKANDROID_IDB_SEC0;

typedef struct 
{
	uint_16  usSysReservedBlock;                 
	uint_16  usDisk0Size;				
	uint_16  usDisk1Size;			
	uint_16  usDisk2Size;				
	uint_16  usDisk3Size;					
	uint_32	uiChipTag;
	uint_32	uiMachineId;
	uint_16	usLoaderYear;
	uint_16	usLoaderDate;
	uint_16	usLoaderVer;
	uint_16  usLastLoaderVer;
	uint_16  usReadWriteTimes;
	uint_32	dwFwVer;
	uint_16  usMachineInfoLen;
	uint_8	ucMachineInfo[30];
	uint_16	usManufactoryInfoLen;
	uint_8	ucManufactoryInfo[30];
	uint_16	usFlashInfoOffset;
	uint_16	usFlashInfoLen;
	uint_8	reserved[384];
	uint_32	uiFlashSize;//��sectorΪ��λ
	uint_8    reserved1;
	uint_8    bAccessTime;
	uint_16  usBlockSize;
	uint_8    bPageSize;
	uint_8    bECCBits;
	uint_8    reserved2[8];
	uint_16  usIdBlock0;
	uint_16  usIdBlock1;
	uint_16  usIdBlock2;
	uint_16  usIdBlock3;
	uint_16  usIdBlock4;
}RKANDROID_IDB_SEC1,*PRKANDROID_IDB_SEC1;

typedef struct
{
	uint_16  usInfoSize;                        
	uint_8    bChipInfo[CHIPINFO_LEN];
	uint_8    reserved[RKANDROID_SEC2_RESERVED_LEN];
	char    szVcTag[3];
	uint_16  usSec0Crc;
	uint_16  usSec1Crc;
	uint_32	uiBootCodeCrc;
	uint_16  usSec3CustomDataOffset;
	uint_16  usSec3CustomDataSize;
	char    szCrcTag[4];
	uint_16  usSec3Crc;
}RKANDROID_IDB_SEC2,*PRKANDROID_IDB_SEC2;

typedef struct
{
    	uint_16  usSNSize;                        
    	uint_8    sn[RKDEVICE_SN_LEN];
	uint_8    reserved[RKANDROID_SEC3_RESERVED_LEN];
	uint_8	imeiSize;
	uint_8	imei[RKDEVICE_IMEI_LEN];
	uint_8	uidSize;
	uint_8	uid[RKDEVICE_UID_LEN];
	uint_8    blueToothSize;
	uint_8	blueToothAddr[RKDEVICE_BT_LEN];
	uint_8	macSize;
	uint_8	macAddr[RKDEVICE_MAC_LEN];
}RKANDROID_IDB_SEC3,*PRKANDROID_IDB_SEC3;
typedef struct  
{
	uint_32  dwTag;
	uint_16 usSnSize;
	uint_8   btSnData[RKDEVICE_SN_LEN];
	uint_8   btReserve[RKANDROID_SEC3_RESERVED_LEN-6];
	uint_8   btImeiSize;
	uint_8   btImeiData[RKDEVICE_IMEI_LEN];
	uint_8   btUidSize;
	uint_8   btUidData[RKDEVICE_UID_LEN];
	uint_8   btBlueToothSize;
	uint_8   btBlueToothData[RKDEVICE_BT_LEN];
	uint_8   btMacSize;
	uint_8   btMacData[RKDEVICE_MAC_LEN];
	uint_16 usCrc;
	uint_8   btSpare[SPARE_SIZE];
}STRUCT_RKANDROID_WBBUFFER,*PSTRUCT_RKANDROID_WBBUFFER;
const uint_8 Wipe_Data[]={0x72,0x65,0x63,0x6F,0x76,0x65,
						0x72,0x79,0x0A,0x2D,0x2D,0x77,
						0x69,0x70,0x65,0x5F,0x64,0x61,0x74,0x61,0x00};
const uint_8 Wipe_All[]={0x72,0x65,0x63,0x6F,0x76,0x65,
						0x72,0x79,0x0A,0x2D,0x2D,0x77,
						0x69,0x70,0x65,0x5F,0x61,0x6C,0x6C,0x00};
#define LBA_TRANSFER_SIZE		16*1024
#define LBA_LOOP_SIZE	1024*1024
#define QUICK_CHECK_INTERVAL 512*1024
#define QUICK_CHECK_HEAD_SIZE 8*1024
#define QUICK_CHECK_REAR_SIZE 8*1024
#define MAX_PACKAGE_FILES			16
#define RKIMAGE_TAG				0x46414B52
#define PARTNAME_BOOTLOADER		"bootloader"
#define PARTNAME_PARAMETER		"parameter"
#define PARTNAME_KERNEL			"kernel"
#define PARTNAME_BOOT			"boot"
#define PARTNAME_RECOVERY		"recovery"
#define PARTNAME_SYSTEM			"system"
#define PARTNAME_MISC			"misc"
#define PARTNAME_BACKUP			"backup"
#define PARTNAME_USERDATA		"userdata"
#define PARTNAME_USER			"user"
#define PARTNAME_ENV			"env"
#define MAX_MANUFACTURER		60
#define MAX_MACHINE_INFO		30
#define MAX_MACHINE_MODEL		34
#define RELATIVE_PATH			60
#define PART_NAME				32
typedef struct 
{
	char name[PART_NAME];
	char file[RELATIVE_PATH];
	unsigned int part_size;
	unsigned int offset;
	unsigned int flash_offset;
	unsigned int usespace;
	unsigned int size;
}STRUCT_RKIMAGE_ITEM,*PSTRUCT_RKIMAGE_ITEM;

typedef struct tagRKIMAGE_HDR
{
	unsigned int tag;
	unsigned int size;
	char machine_model[MAX_MACHINE_MODEL];
	char machine_info[MAX_MACHINE_INFO];
	char manufacturer[MAX_MANUFACTURER];
	unsigned int dwFWVer;
	int item_count;
	STRUCT_RKIMAGE_ITEM item[MAX_PACKAGE_FILES];
}STRUCT_RKIMAGE_HDR,*PSTRUCT_RKIMAGE_HDR;

#pragma pack()

class CRKAndroidDevice :public CRKDevice
{
public:
	int DownloadBoot() override;
	int PrepareIDB(bool &bNeedJump) override;
	int DownloadIDBlock(bool &bNeedJump) override;
	int DownloadImage() override;
	int EraseIDB() override;
	int EraseAllBlocks() override;
	int GetBlocksState() override;
	bool BufferWriteBack() override;
	bool ReadCapability() override;
	CRKAndroidDevice(STRUCT_RKDEVICE_DESC &device);
	~CRKAndroidDevice() override;
protected:
private:
	uint_32  m_dwLoaderSize;
	uint_32  m_dwLoaderDataSize;
	uint_32  m_dwLoaderHeadSize;
	bool   m_bNeedLowFormat;
	bool   m_bHasEnv;
	char   m_oldIDBCounts;
	uint_16 m_usFlashDataSec;
	uint_16 m_usFlashBootSec;
	uint_16 m_usFlashHeadSec;
	uint_8   *m_paramBuffer;
	uint_32   m_uiParamFileSize;
	uint_32   m_uiResevedBlockSize;
	RKANDROID_IDB_SEC0 *m_oldSec0;
	RKANDROID_IDB_SEC1 *m_oldSec1;
	RKANDROID_IDB_SEC2 *m_oldSec2;
	RKANDROID_IDB_SEC3 *m_oldSec3;
	uint_32 m_uiLBATimes;
	uint_32 m_uiUserSectors;
	uint_8 *m_gptBuffer;
	bool GetNewDiskSize();
	bool GetLoaderSize();
	bool GetLoaderDataSize();
	bool GetLoaderHeadSize();
	bool GetOldSectorData();
	bool CalcIDBCount(bool bUpdateLoader); 
	bool IsExistSector3Crc(PRKANDROID_IDB_SEC2 pSec);
	bool CheckIDBData(uint_8* pIDBData,bool bCheckBootCode=false);
	void IDB528To512(uint_8* pIDB528,uint_8* pIDB512,uint_8* pSpare,uint_32 dwSectorNum);
	void IDBRC4(uint_8* pIDB);
	bool EraseSystemPart(uint_32 dwLayerID);
	bool EraseUserDataPart(uint_32 dwLayerID);
	bool FindBackupBuffer() override;
	char FindIDBlock(char pos,char &IDBlockPos) override;
	char FindAllIDB() override;
	bool ReserveIDBlock(char iBlockIndex=0,char iIdblockPos=0) override;
	bool OffsetIDBlock(char pos) override;
	bool MakeSector0(uint_8* pSector) override;
	void MakeSector1(uint_8* pSector) override;
	bool MakeSector2(uint_8* pSector) override;
	bool MakeSector3(uint_8* pSector) override;
	int MakeIDBlockData(uint_8* lpIDBlock) override;
	virtual int MakeNewIDBlockData(uint_8* lpIDBlock);
	bool MakeSpareData(uint_8* lpIDBlock,uint_32 dwSectorNum,uint_8* lpSpareBuffer) override;
	int WriteIDBlock(uint_8* lpIDBlock,uint_8* lpSpareData,uint_32 dwSectorNum,bool bErase) override;
	bool Boot_VendorRequest(uint_32 requestCode, uint_8* pBuffer, uint_32 dwDataSize);
	bool RKA_Param_Download(STRUCT_RKIMAGE_ITEM &entry,uint_8 &loopCount,uint_64 &currentByte,uint_64 totalByte,ENUM_CALL_STEP &emCallStep);
	bool RKA_Param_Check(STRUCT_RKIMAGE_ITEM &entry,uint_8 &loopCount,uint_64 &currentByte,uint_64 totalByte,ENUM_CALL_STEP &emCallStep);
	bool RKA_Gpt_Download(STRUCT_RKIMAGE_ITEM &entry,uint_8 &loopCount,uint_64 &currentByte,uint_64 totalByte,ENUM_CALL_STEP &emCallStep);
	bool RKA_Gpt_Check(STRUCT_RKIMAGE_ITEM &entry,uint_8 &loopCount,uint_64 &currentByte,uint_64 totalByte,ENUM_CALL_STEP &emCallStep);
	bool RKA_File_Download(STRUCT_RKIMAGE_ITEM &entry,uint_8 &loopCount,uint_64 &currentByte,uint_64 totalByte,ENUM_CALL_STEP &emCallStep);
	bool RKA_File_Check(STRUCT_RKIMAGE_ITEM &entry,uint_8 &loopCount,uint_64 &currentByte,uint_64 totalByte,ENUM_CALL_STEP &emCallStep);
	bool RKA_SparseFile_Download(STRUCT_RKIMAGE_ITEM &entry,uint_8 &loopCount,uint_64 &currentByte,uint_64 totalByte,ENUM_CALL_STEP &emCallStep);
	bool RKA_SparseFile_Check(STRUCT_RKIMAGE_ITEM &entry,uint_8 &loopCount,uint_64 &currentByte,uint_64 totalByte,ENUM_CALL_STEP &emCallStep);
	bool ParsePartitionInfo(string &strPartInfo,string &strName,uint_32 &uiOffset,uint_32 &uiLen);
	bool GetReservedBlockByte();
	bool MakeParamFileBuffer(STRUCT_RKIMAGE_ITEM &entry);
	bool CheckParamPartSize(uint_8* pItemBuffer,int iParamPos,int iItemCount);
	bool GetParameterPartOffset(STRUCT_RKIMAGE_ITEM &paramItem);
	bool IsSparseImage(STRUCT_RKIMAGE_ITEM &entry);
	bool IsUbiImage(STRUCT_RKIMAGE_ITEM &entry);
	bool IsJffsImage(STRUCT_RKIMAGE_ITEM &entry);
	bool IsDharaImage(STRUCT_RKIMAGE_ITEM &entry);
	uint_64 GetSparseImageSize(STRUCT_RKIMAGE_ITEM &entry);
	bool EraseSparseRegion(uint_32 uiOffset,uint_32 uiSize,uint_32 uiErase=32*1024);
	bool EraseUbiRegion(uint_32 uiOffset, uint_32 uiSize);
	bool GetOldIDBData();
	bool FindBackupSector3();
	int  WriteIDBWithDirect(uint_8* pIDB=NULL);
	int  EraseDirectLBABlock(uint_32 dwPos,uint_32 dwCount);
	bool GetParameterGptFlag(STRUCT_RKIMAGE_ITEM &paramItem);
	bool IsEnvFirmware(bool &envFlag);
};
u32 calculate_gpt_sector_count(unsigned int nLogicalBlock);
void create_gpt_buffer(u8 *gpt, PARAM_ITEM_VECTOR &vecParts, CONFIG_ITEM_VECTOR &vecUuid, u64 diskSectors, u32 logicalBlockSectors, bool *align_flag=NULL);
void prepare_gpt_backup(u8 *master, u8 *backup, u32 logicalBlockSectors);
void gen_rand_uuid(unsigned char *uuid_bin);
unsigned int crc32_le(unsigned int crc, unsigned char *p, unsigned int len);
bool parse_parameter(char *pParameter,PARAM_ITEM_VECTOR &vecItem);
bool get_grow_align_flag(char *pParam, bool &align_flag);
bool get_uuid_from_parameter(char *pParameter,CONFIG_ITEM_VECTOR &vecItem);
bool ParsePartitionInfo(string &strPartInfo,string &strName,uint_32 &uiOffset,uint_32 &uiLen);
bool ParseUuidInfo(string &strUuidInfo, string &strName, string &strUUid);
void string_to_uuid(string strUUid, char *uuid);
bool parse_gpt(unsigned char *pGpt,PARAM_ITEM_VECTOR &vecItem,u32 logicalBlockSectors=1);
int find_uuid_item(CONFIG_ITEM_VECTOR &vecItems, char *pszName);
bool parse_env(unsigned char *pEnv,PARAM_ITEM_VECTOR &vecItem);
bool parse_mcu(unsigned char *pBuf,PARAM_ITEM_VECTOR &vecItem);
#endif
