#include "RKBoot.h"
extern uint_32 CRC_32(uint_8* pData, uint_32 ulSize);
#define LDR_FORMAT_VER  0x1000000
bool CRKBoot::GetBootCrcOff()
{
	return m_bBootCrcOff;
}
bool CRKBoot::GetBootRc4Off()
{
	return m_bBootRc4Off;
}
bool CRKBoot::GetRc4DisableFlag()
{
	return m_bRc4Disable;
}
bool CRKBoot::GetSignFlag()
{
	return m_bSignFlag;
}
uint_32 CRKBoot::GetVersion()
{
	return m_version;
}
uint_32 CRKBoot::GetMergeVersion()
{
	return m_mergeVersion;
}
STRUCT_RKTIME CRKBoot::GetReleaseTime()
{
	return m_releaseTime;
}
ENUM_RKDEVICE_TYPE CRKBoot::GetSupportDevice()
{
	return m_supportDevice;
}
uint_8 CRKBoot::GetEntry471Count()
{
	return m_471Count;
}
uint_8 CRKBoot::GetEntry472Count()
{
	return m_472Count;
}
uint_8 CRKBoot::GetEntryLoaderCount()
{
	return m_loaderCount;
}
bool CRKBoot::CrcCheck()
{
	uint_32*pOldCrc,ulNewCrc;
	pOldCrc = (uint_32*)(m_BootData+(m_BootSize-4));
	ulNewCrc = CRC_32(m_BootData,m_BootSize-4);
	return (*pOldCrc==ulNewCrc)?true:false;
}
bool CRKBoot::SaveEntryFile(ENUM_RKBOOTENTRY type,uint_8 ucIndex,string fileName)
{
	uint_32 dwOffset;
	uint_8 ucCount,ucSize;
	switch ( type )
	{
	case ENTRY471:
		dwOffset = m_471Offset;
		ucCount = m_471Count;
		ucSize = m_471Size;
		break;
	case ENTRY472:
		dwOffset = m_472Offset;
		ucCount = m_472Count;
		ucSize = m_472Size;
		break;
	case ENTRYLOADER:
		dwOffset = m_loaderOffset;
		ucCount = m_loaderCount;
		ucSize = m_loaderSize;
		break;
	default:
		return false;
	}
	if( ucIndex>=ucCount )
	{
		return false;
	}
	PSTRUCT_RKBOOT_ENTRY pEntry;
	pEntry = (PSTRUCT_RKBOOT_ENTRY)(m_BootData+dwOffset+(ucSize*ucIndex));
	FILE *file=NULL;
	file = fopen(fileName.c_str(),"wb+");
	if ( !file )
	{
		return false;
	}
	fwrite(m_BootData+pEntry->dwDataOffset,1,pEntry->dwDataSize,file);
	fclose(file);
	return true;
}
bool CRKBoot::GetEntryProperty(ENUM_RKBOOTENTRY type,uint_8 ucIndex,uint_32 &dwSize,uint_32 &dwDelay)
{
	uint_32 dwOffset;
	uint_8 ucCount,ucSize;
	switch ( type )
	{
	case ENTRY471:
		dwOffset = m_471Offset;
		ucCount = m_471Count;
		ucSize = m_471Size;
		break;
	case ENTRY472:
		dwOffset = m_472Offset;
		ucCount = m_472Count;
		ucSize = m_472Size;
		break;
	case ENTRYLOADER:
		dwOffset = m_loaderOffset;
		ucCount = m_loaderCount;
		ucSize = m_loaderSize;
		break;
	default:
		return false;
	}
	if( ucIndex>=ucCount )
	{
		return false;
	}
	PSTRUCT_RKBOOT_ENTRY pEntry;
	pEntry = (PSTRUCT_RKBOOT_ENTRY)(m_BootData+dwOffset+(ucSize*ucIndex));
	dwDelay = pEntry->dwDataDelay;
	dwSize = pEntry->dwDataSize;
	
	return true;
}
bool CRKBoot::GetEntryData(ENUM_RKBOOTENTRY type,uint_8 ucIndex,uint_8* lpData)
{
	uint_32 dwOffset;
	uint_8 ucCount,ucSize;
	switch ( type )
	{
	case ENTRY471:
		dwOffset = m_471Offset;
		ucCount = m_471Count;
		ucSize = m_471Size;
		break;
	case ENTRY472:
		dwOffset = m_472Offset;
		ucCount = m_472Count;
		ucSize = m_472Size;
		break;
	case ENTRYLOADER:
		dwOffset = m_loaderOffset;
		ucCount = m_loaderCount;
		ucSize = m_loaderSize;
		break;
	default:
		return false;
	}
	if( ucIndex>=ucCount )
	{
		return false;
	}
	PSTRUCT_RKBOOT_ENTRY pEntry;
	pEntry = (PSTRUCT_RKBOOT_ENTRY)(m_BootData+dwOffset+(ucSize*ucIndex));
	memcpy(lpData,m_BootData+pEntry->dwDataOffset,pEntry->dwDataSize);
	return true;
}
bool CRKBoot::GetEntryName(ENUM_RKBOOTENTRY type,uint_8 ucIndex,char* name)
{
	uint_32 dwOffset;
	uint_8 ucCount,ucSize;
	switch ( type )
	{
	case ENTRY471:
		dwOffset = m_471Offset;
		ucCount = m_471Count;
		ucSize = m_471Size;
		break;
	case ENTRY472:
		dwOffset = m_472Offset;
		ucCount = m_472Count;
		ucSize = m_472Size;
		break;
	case ENTRYLOADER:
		dwOffset = m_loaderOffset;
		ucCount = m_loaderCount;
		ucSize = m_loaderSize;
		break;
	default:
		return false;
	}
	if( ucIndex>=ucCount )
	{
		return false;
	}
	PSTRUCT_RKBOOT_ENTRY pEntry;
	pEntry = (PSTRUCT_RKBOOT_ENTRY)(m_BootData+dwOffset+(ucSize*ucIndex));

	if (name)
	{
		int i;
		for(i=0;i<20;i++)
			name[i] = (char)(pEntry->szName[i] & 0xFF);
		if (name[19]!=0)
		{
			name[19] = 0;
		}
	}
	
	return true;
}
char CRKBoot::GetIndexByName(ENUM_RKBOOTENTRY type,char *pName)
{
	uint_32 dwOffset;
	uint_8 ucCount,ucSize;
	switch ( type )
	{
	case ENTRY471:
		dwOffset = m_471Offset;
		ucCount = m_471Count;
		ucSize = m_471Size;
		break;
	case ENTRY472:
		dwOffset = m_472Offset;
		ucCount = m_472Count;
		ucSize = m_472Size;
		break;
	case ENTRYLOADER:
		dwOffset = m_loaderOffset;
		ucCount = m_loaderCount;
		ucSize = m_loaderSize;
		break;
	default:
		return -1;
	}

	for (uint_8 i=0;i<ucCount;i++)
	{
		PSTRUCT_RKBOOT_ENTRY pEntry;
		pEntry = (PSTRUCT_RKBOOT_ENTRY)(m_BootData+dwOffset+(ucSize*i));
		
		char szName[20];
		wchar_To_char(pEntry->szName,szName,20);
		if (stricmp(pName,szName)==0)
		{
			return i;
		}
	}
	return -1;
}
CRKBoot::~CRKBoot()
{
	if (m_BootData!=NULL)
	{
		delete []m_BootData;
	}
}
int CRKBoot::ObjectInit()
{
	if (m_BootData){
		if (!CrcCheck())
		{
			return -2;
		}
		PSTRUCT_RKBOOT_HEAD pBootHead;
		pBootHead = (PSTRUCT_RKBOOT_HEAD)(m_BootData);
		if (( pBootHead->uiTag!=0x544F4F42)&&(pBootHead->uiTag!=0x2052444C))
		{
			return -3;
		}
		if (pBootHead->uiTag==0x2052444C)
		{
			if (pBootHead->dwMergeVersion > LDR_FORMAT_VER)
			{
				return -4;
			}
		}
		if (pBootHead->ucCrcOff)
		{
			m_bBootCrcOff = true;
		}
		else
			m_bBootCrcOff = false;

		if (pBootHead->uc471Rc4Off)
		{
			m_bBootRc4Off = true;
		}
		else
			m_bBootRc4Off = false;

		if (pBootHead->ucRc4Flag)
		{
			m_bRc4Disable = true;
		}
		else
			m_bRc4Disable = false;
		if (pBootHead->ucSignFlag=='S')
		{
			m_bSignFlag = true;
		}
		else
			m_bSignFlag = false;
		m_version = pBootHead->dwVersion;
		m_mergeVersion = pBootHead->dwMergeVersion;
		m_BootHeadSize = pBootHead->usSize;
		m_releaseTime.usYear = pBootHead->stReleaseTime.usYear;
		m_releaseTime.ucMonth = pBootHead->stReleaseTime.ucMonth;
		m_releaseTime.ucDay = pBootHead->stReleaseTime.ucDay;
		m_releaseTime.ucHour = pBootHead->stReleaseTime.ucHour;
		m_releaseTime.ucMinute = pBootHead->stReleaseTime.ucMinute;
		m_releaseTime.ucSecond = pBootHead->stReleaseTime.ucSecond;
		m_supportDevice = pBootHead->emSupportChip;

		m_471Offset = pBootHead->dw471EntryOffset;
		m_471Count = pBootHead->uc471EntryCount;
		m_471Size = pBootHead->uc471EntrySize;

		m_472Offset = pBootHead->dw472EntryOffset;
		m_472Count = pBootHead->uc472EntryCount;
		m_472Size = pBootHead->uc472EntrySize;

		m_loaderOffset = pBootHead->dwLoaderEntryOffset;
		m_loaderCount = pBootHead->ucLoaderEntryCount;
		m_loaderSize = pBootHead->ucLoaderEntrySize;

		memcpy(m_crc,m_BootData+(m_BootSize-4),4);
		return 0;

	} else {
		return -1;
	}
}

CRKBoot::CRKBoot(uint_8* lpBootData,uint_32 dwBootSize)
{
	BootCrcOff.setContainer(this);
	BootCrcOff.getter(&CRKBoot::GetBootCrcOff);
	BootRc4Off.setContainer(this);
	BootRc4Off.getter(&CRKBoot::GetBootRc4Off);
	Rc4DisableFlag.setContainer(this);
	Rc4DisableFlag.getter(&CRKBoot::GetRc4DisableFlag);
	SignFlag.setContainer(this);
	SignFlag.getter(&CRKBoot::GetSignFlag);
	Version.setContainer(this);
	Version.getter(&CRKBoot::GetVersion);
	MergeVersion.setContainer(this);
	MergeVersion.getter(&CRKBoot::GetMergeVersion);
	ReleaseTime.setContainer(this);
	ReleaseTime.getter(&CRKBoot::GetReleaseTime);
	SupportDevice.setContainer(this);
	SupportDevice.getter(&CRKBoot::GetSupportDevice);
	Entry471Count.setContainer(this);
	Entry471Count.getter(&CRKBoot::GetEntry471Count);
	Entry472Count.setContainer(this);
	Entry472Count.getter(&CRKBoot::GetEntry472Count);
	EntryLoaderCount.setContainer(this);
	EntryLoaderCount.getter(&CRKBoot::GetEntryLoaderCount);
	if (lpBootData)
	{
		m_BootData=lpBootData;
		m_BootSize=dwBootSize;
	}
	else
	{
		m_BootData=NULL;
	}
}

void CRKBoot::wchar_To_char(uint_16 *src,char *dst,int len)
{
	int i;
	memset(dst,0,len*sizeof(char));
	for (i=0;i<len;i++)
	{
		memcpy(dst,src,1);
		src++;
		dst++;
	}
}

