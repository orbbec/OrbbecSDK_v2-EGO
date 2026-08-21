#ifndef RKBOOT_HEADER
#define RKBOOT_HEADER
#include "DefineHeader.h"


#define  BOOT_RESERVED_SIZE 55
#pragma pack(1)
typedef struct  
{
	uint_32 uiTag;
	uint_16 usSize;
	uint_32  dwVersion;
	uint_32  dwMergeVersion;
	STRUCT_RKTIME stReleaseTime;
	ENUM_RKDEVICE_TYPE emSupportChip;
	uint_8 uc471EntryCount;
	uint_32 dw471EntryOffset;
	uint_8 uc471EntrySize;
	uint_8 uc472EntryCount;
	uint_32 dw472EntryOffset;
	uint_8 uc472EntrySize;
	uint_8 ucLoaderEntryCount;
	uint_32 dwLoaderEntryOffset;
	uint_8 ucLoaderEntrySize;
	uint_8 ucSignFlag;
	uint_8 ucRc4Flag;
	uint_8 uc471Rc4Off;
	uint_8 ucCrcOff;
	uint_8 reserved[BOOT_RESERVED_SIZE];
}STRUCT_RKBOOT_HEAD,*PSTRUCT_RKBOOT_HEAD;

typedef struct  
{
	uint_8 ucSize;
	ENUM_RKBOOTENTRY emType;
	uint_16 szName[20];
	uint_32 dwDataOffset;
	uint_32 dwDataSize;
	uint_32 dwDataDelay;
}STRUCT_RKBOOT_ENTRY,*PSTRUCT_RKBOOT_ENTRY;


#pragma pack()
class CRKBoot
{
public:
	bool GetBootCrcOff();
 	property<CRKBoot,bool,READ_ONLY> BootCrcOff;
	bool GetBootRc4Off();
 	property<CRKBoot,bool,READ_ONLY> BootRc4Off;
	bool GetRc4DisableFlag();
 	property<CRKBoot,bool,READ_ONLY> Rc4DisableFlag;
	bool GetSignFlag();
 	property<CRKBoot,bool,READ_ONLY> SignFlag;
	uint_32 GetVersion();
 	property<CRKBoot,uint_32,READ_ONLY> Version;
	uint_32 GetMergeVersion();
 	property<CRKBoot,uint_32,READ_ONLY> MergeVersion;
	STRUCT_RKTIME GetReleaseTime();
 	property<CRKBoot,STRUCT_RKTIME,READ_ONLY> ReleaseTime;
	ENUM_RKDEVICE_TYPE GetSupportDevice();
 	property<CRKBoot,ENUM_RKDEVICE_TYPE,READ_ONLY> SupportDevice;
	unsigned char GetEntry471Count();
 	property<CRKBoot,unsigned char,READ_ONLY> Entry471Count;
	unsigned char GetEntry472Count();
 	property<CRKBoot,unsigned char,READ_ONLY> Entry472Count;
	unsigned char GetEntryLoaderCount();
 	property<CRKBoot,unsigned char,READ_ONLY> EntryLoaderCount;
	bool CrcCheck();
	bool SaveEntryFile(ENUM_RKBOOTENTRY type,uint_8 ucIndex,string fileName);
	bool GetEntryProperty(ENUM_RKBOOTENTRY type,uint_8 ucIndex,uint_32 &dwSize,uint_32 &dwDelay);
	char GetIndexByName(ENUM_RKBOOTENTRY type,char *pName);
	bool GetEntryData(ENUM_RKBOOTENTRY type,uint_8 ucIndex,uint_8* lpData);
	bool GetEntryName(ENUM_RKBOOTENTRY type,uint_8 ucIndex,char* name);
	int ObjectInit();
	CRKBoot(uint_8* lpBootData,uint_32 dwBootSize);
	~CRKBoot();
protected:
private:
	bool m_bBootCrcOff;
	bool m_bBootRc4Off;
	bool m_bRc4Disable;
	bool m_bSignFlag;
	uint_32 m_version;
	uint_32 m_mergeVersion;
	STRUCT_RKTIME m_releaseTime;
	ENUM_RKDEVICE_TYPE m_supportDevice;
	uint_32 m_471Offset;
	uint_8 m_471Size;
	uint_8 m_471Count;
	uint_32 m_472Offset;
	uint_8 m_472Size;
	uint_8 m_472Count;
	uint_32 m_loaderOffset;
	uint_8 m_loaderSize;
	uint_8 m_loaderCount;
	uint_8  m_crc[4];
	uint_8* m_BootData;
	uint_32 m_BootSize;
	uint_16 m_BootHeadSize;
	void wchar_To_char(uint_16 *src,char *dst,int len);
};

#endif