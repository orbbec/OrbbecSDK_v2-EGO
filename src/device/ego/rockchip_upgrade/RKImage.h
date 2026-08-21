#ifndef RKIMAGE_HEADER
#define RKIMAGE_HEADER
#include "DefineHeader.h"
#include "RKBoot.h"
#define  IMAGE_RESERVED_SIZE 57
#define  IMAGE_ERROR_BASE 200
/*image error list*/
#define ERR_NEW_MEMORY (IMAGE_ERROR_BASE+0)
#define ERR_OPEN_IMAGE (IMAGE_ERROR_BASE+1)
#define ERR_READ_IMAGE (IMAGE_ERROR_BASE+2)
#define ERR_WRONG_TAG (IMAGE_ERROR_BASE+3)
#define ERR_WRONG_HASH (IMAGE_ERROR_BASE+4)
#define ERR_NO_PARAMETER (IMAGE_ERROR_BASE+5)
#define ERR_INVALID_READ (IMAGE_ERROR_BASE+6)
#define ERR_BOOT_SIZE (IMAGE_ERROR_BASE+7)
#define ERR_BOOT_CRC (IMAGE_ERROR_BASE+8)
#define ERR_BOOT_TAG (IMAGE_ERROR_BASE+9)
#define ERR_BOOT_VER (IMAGE_ERROR_BASE+10)
#pragma pack(1)
typedef struct  
{
	uint_32 uiTag;
	uint_16 usSize;
	uint_32  dwVersion;
	uint_32  dwMergeVersion;
	STRUCT_RKTIME stReleaseTime;
	ENUM_RKDEVICE_TYPE emSupportChip;
	uint_32  dwBootOffset;
	uint_32  dwBootSize;
	uint_32  dwFWOffset;
	uint_32  dwFWSize;
	uint_8   reserved[IMAGE_RESERVED_SIZE];
	uint_32  dwBootStorage;
}STRUCT_RKIMAGE_HEAD,*PSTRUCT_RKIMAGE_HEAD;
#pragma pack()
class CRKImage
{
public:
	uint_32 GetVersion();
 	property<CRKImage,uint_32,READ_ONLY> Version;
	uint_32 GetMergeVersion();
 	property<CRKImage,uint_32,READ_ONLY> MergeVersion;
	STRUCT_RKTIME GetReleaseTime();
 	property<CRKImage,STRUCT_RKTIME,READ_ONLY> ReleaseTime;
	ENUM_RKDEVICE_TYPE GetSupportDevice();
 	property<CRKImage,ENUM_RKDEVICE_TYPE,READ_ONLY> SupportDevice;
	ENUM_OS_TYPE GetOsType();
 	property<CRKImage,ENUM_OS_TYPE,READ_ONLY> OsType;
	unsigned short GetBackupSize();
 	property<CRKImage,unsigned short,READ_ONLY> BackupSize;
	uint_32 GetBootOffset();
 	property<CRKImage,uint_32,READ_ONLY> BootOffset;
	uint_32 GetBootSize();
 	property<CRKImage,uint_32,READ_ONLY> BootSize;
	uint_32 GetFWOffset();
 	property<CRKImage,uint_32,READ_ONLY> FWOffset;
	long long GetFWSize();
 	property<CRKImage,long long,READ_ONLY> FWSize;
	bool GetSignFlag();
	property<CRKImage,bool,READ_ONLY> SignFlag;
	bool GetSingleBootFlag();
	property<CRKImage,bool,READ_ONLY> SingleBootFlag;
	uint_32 GetStorage();
	void SetStorage(uint_32 value);
 	property<CRKImage,uint_32,READ_WRITE> Storage;
	int GetError();
 	property<CRKImage,int,READ_ONLY> Error;


	CRKBoot *m_bootObject;
	bool Md5Check(long long nCheckSize);
	bool SaveBootFile(string filename);
	bool SaveFWFile(string filename);
	bool GetData(long long dwOffset,uint_32 dwSize,uint_8* lpBuffer);
	void GetReservedData(uint_8* &lpData,uint_16 &usSize);
	int GetMd5Data(uint_8* &lpMd5,uint_8* &lpSignMd5);
	long long GetImageSize();
	int ObjectInit(string filename,bool bDoCheck=true,uint_64 baseOffset=-1,uint_64 baseSize=-1);
	CRKImage();
	~CRKImage();
protected:

private:
	uint_32 m_version;
	uint_32 m_mergeVersion;
	STRUCT_RKTIME m_releaseTime;
	ENUM_RKDEVICE_TYPE m_supportDevice;
	uint_32 m_bootOffset;
	uint_32 m_bootSize;
	uint_32 m_fwOffset;
	long long m_fwSize;
	uint_64 m_baseOffset;
	uint_64 m_baseFileSize;
	uint_8  m_md5[32];
	uint_8  m_signMd5[512];
	uint_8  m_reserved[IMAGE_RESERVED_SIZE];
	bool  m_bSignFlag;
	bool  m_bSingleBootFlag;
	int   m_signMd5Size;
	uint_32 m_storage;
	int m_error;
	FILE *m_pFile;
	long long m_fileSize;
};
#endif