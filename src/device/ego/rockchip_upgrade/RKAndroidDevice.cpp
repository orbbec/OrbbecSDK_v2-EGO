#include "RKAndroidDevice.h"
#define tole(x)		(x)
/*factor is 0xedb88320*/
unsigned int crc32table_le[] = {
	tole(0x00000000L), tole(0x77073096L), tole(0xee0e612cL), tole(0x990951baL),
	tole(0x076dc419L), tole(0x706af48fL), tole(0xe963a535L), tole(0x9e6495a3L),
	tole(0x0edb8832L), tole(0x79dcb8a4L), tole(0xe0d5e91eL), tole(0x97d2d988L),
	tole(0x09b64c2bL), tole(0x7eb17cbdL), tole(0xe7b82d07L), tole(0x90bf1d91L),
	tole(0x1db71064L), tole(0x6ab020f2L), tole(0xf3b97148L), tole(0x84be41deL),
	tole(0x1adad47dL), tole(0x6ddde4ebL), tole(0xf4d4b551L), tole(0x83d385c7L),
	tole(0x136c9856L), tole(0x646ba8c0L), tole(0xfd62f97aL), tole(0x8a65c9ecL),
	tole(0x14015c4fL), tole(0x63066cd9L), tole(0xfa0f3d63L), tole(0x8d080df5L),
	tole(0x3b6e20c8L), tole(0x4c69105eL), tole(0xd56041e4L), tole(0xa2677172L),
	tole(0x3c03e4d1L), tole(0x4b04d447L), tole(0xd20d85fdL), tole(0xa50ab56bL),
	tole(0x35b5a8faL), tole(0x42b2986cL), tole(0xdbbbc9d6L), tole(0xacbcf940L),
	tole(0x32d86ce3L), tole(0x45df5c75L), tole(0xdcd60dcfL), tole(0xabd13d59L),
	tole(0x26d930acL), tole(0x51de003aL), tole(0xc8d75180L), tole(0xbfd06116L),
	tole(0x21b4f4b5L), tole(0x56b3c423L), tole(0xcfba9599L), tole(0xb8bda50fL),
	tole(0x2802b89eL), tole(0x5f058808L), tole(0xc60cd9b2L), tole(0xb10be924L),
	tole(0x2f6f7c87L), tole(0x58684c11L), tole(0xc1611dabL), tole(0xb6662d3dL),
	tole(0x76dc4190L), tole(0x01db7106L), tole(0x98d220bcL), tole(0xefd5102aL),
	tole(0x71b18589L), tole(0x06b6b51fL), tole(0x9fbfe4a5L), tole(0xe8b8d433L),
	tole(0x7807c9a2L), tole(0x0f00f934L), tole(0x9609a88eL), tole(0xe10e9818L),
	tole(0x7f6a0dbbL), tole(0x086d3d2dL), tole(0x91646c97L), tole(0xe6635c01L),
	tole(0x6b6b51f4L), tole(0x1c6c6162L), tole(0x856530d8L), tole(0xf262004eL),
	tole(0x6c0695edL), tole(0x1b01a57bL), tole(0x8208f4c1L), tole(0xf50fc457L),
	tole(0x65b0d9c6L), tole(0x12b7e950L), tole(0x8bbeb8eaL), tole(0xfcb9887cL),
	tole(0x62dd1ddfL), tole(0x15da2d49L), tole(0x8cd37cf3L), tole(0xfbd44c65L),
	tole(0x4db26158L), tole(0x3ab551ceL), tole(0xa3bc0074L), tole(0xd4bb30e2L),
	tole(0x4adfa541L), tole(0x3dd895d7L), tole(0xa4d1c46dL), tole(0xd3d6f4fbL),
	tole(0x4369e96aL), tole(0x346ed9fcL), tole(0xad678846L), tole(0xda60b8d0L),
	tole(0x44042d73L), tole(0x33031de5L), tole(0xaa0a4c5fL), tole(0xdd0d7cc9L),
	tole(0x5005713cL), tole(0x270241aaL), tole(0xbe0b1010L), tole(0xc90c2086L),
	tole(0x5768b525L), tole(0x206f85b3L), tole(0xb966d409L), tole(0xce61e49fL),
	tole(0x5edef90eL), tole(0x29d9c998L), tole(0xb0d09822L), tole(0xc7d7a8b4L),
	tole(0x59b33d17L), tole(0x2eb40d81L), tole(0xb7bd5c3bL), tole(0xc0ba6cadL),
	tole(0xedb88320L), tole(0x9abfb3b6L), tole(0x03b6e20cL), tole(0x74b1d29aL),
	tole(0xead54739L), tole(0x9dd277afL), tole(0x04db2615L), tole(0x73dc1683L),
	tole(0xe3630b12L), tole(0x94643b84L), tole(0x0d6d6a3eL), tole(0x7a6a5aa8L),
	tole(0xe40ecf0bL), tole(0x9309ff9dL), tole(0x0a00ae27L), tole(0x7d079eb1L),
	tole(0xf00f9344L), tole(0x8708a3d2L), tole(0x1e01f268L), tole(0x6906c2feL),
	tole(0xf762575dL), tole(0x806567cbL), tole(0x196c3671L), tole(0x6e6b06e7L),
	tole(0xfed41b76L), tole(0x89d32be0L), tole(0x10da7a5aL), tole(0x67dd4accL),
	tole(0xf9b9df6fL), tole(0x8ebeeff9L), tole(0x17b7be43L), tole(0x60b08ed5L),
	tole(0xd6d6a3e8L), tole(0xa1d1937eL), tole(0x38d8c2c4L), tole(0x4fdff252L),
	tole(0xd1bb67f1L), tole(0xa6bc5767L), tole(0x3fb506ddL), tole(0x48b2364bL),
	tole(0xd80d2bdaL), tole(0xaf0a1b4cL), tole(0x36034af6L), tole(0x41047a60L),
	tole(0xdf60efc3L), tole(0xa867df55L), tole(0x316e8eefL), tole(0x4669be79L),
	tole(0xcb61b38cL), tole(0xbc66831aL), tole(0x256fd2a0L), tole(0x5268e236L),
	tole(0xcc0c7795L), tole(0xbb0b4703L), tole(0x220216b9L), tole(0x5505262fL),
	tole(0xc5ba3bbeL), tole(0xb2bd0b28L), tole(0x2bb45a92L), tole(0x5cb36a04L),
	tole(0xc2d7ffa7L), tole(0xb5d0cf31L), tole(0x2cd99e8bL), tole(0x5bdeae1dL),
	tole(0x9b64c2b0L), tole(0xec63f226L), tole(0x756aa39cL), tole(0x026d930aL),
	tole(0x9c0906a9L), tole(0xeb0e363fL), tole(0x72076785L), tole(0x05005713L),
	tole(0x95bf4a82L), tole(0xe2b87a14L), tole(0x7bb12baeL), tole(0x0cb61b38L),
	tole(0x92d28e9bL), tole(0xe5d5be0dL), tole(0x7cdcefb7L), tole(0x0bdbdf21L),
	tole(0x86d3d2d4L), tole(0xf1d4e242L), tole(0x68ddb3f8L), tole(0x1fda836eL),
	tole(0x81be16cdL), tole(0xf6b9265bL), tole(0x6fb077e1L), tole(0x18b74777L),
	tole(0x88085ae6L), tole(0xff0f6a70L), tole(0x66063bcaL), tole(0x11010b5cL),
	tole(0x8f659effL), tole(0xf862ae69L), tole(0x616bffd3L), tole(0x166ccf45L),
	tole(0xa00ae278L), tole(0xd70dd2eeL), tole(0x4e048354L), tole(0x3903b3c2L),
	tole(0xa7672661L), tole(0xd06016f7L), tole(0x4969474dL), tole(0x3e6e77dbL),
	tole(0xaed16a4aL), tole(0xd9d65adcL), tole(0x40df0b66L), tole(0x37d83bf0L),
	tole(0xa9bcae53L), tole(0xdebb9ec5L), tole(0x47b2cf7fL), tole(0x30b5ffe9L),
	tole(0xbdbdf21cL), tole(0xcabac28aL), tole(0x53b39330L), tole(0x24b4a3a6L),
	tole(0xbad03605L), tole(0xcdd70693L), tole(0x54de5729L), tole(0x23d967bfL),
	tole(0xb3667a2eL), tole(0xc4614ab8L), tole(0x5d681b02L), tole(0x2a6f2b94L),
	tole(0xb40bbe37L), tole(0xc30c8ea1L), tole(0x5a05df1bL), tole(0x2d02ef8dL)
};
char CRKAndroidDevice::FindIDBlock(char pos,char &IDBlockPos)
{
	uint_8 bData[(SECTOR_SIZE+SPARE_SIZE)*4];
	int iRet = ERR_SUCCESS;
	int i = FindValidBlocks(pos,1);
	if ( i<0 )
		return -1;
	for( ;i<IDBLOCK_TOP;i=FindValidBlocks(i+1,1) )
	{
		if ( i<0 )
		{
			break;
		}
		iRet = m_pComm->RKU_ReadSector( i*m_flashInfo.uiSectorPerBlock, 4, bData);
		
		if(ERR_SUCCESS!=iRet)
		{
			m_error = ERR_COMM_RW;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:FindIDBlock-->RKU_ReadSector failed,RetCode(%d)",iRet);
			}
			return -1;
		}
		RKANDROID_IDB_SEC0 *pSec0;
		pSec0 = (RKANDROID_IDB_SEC0 *)bData;
		P_RC4((uint_8 *)pSec0,SECTOR_SIZE);
 //       if (bData[514]==0x69)//0x69='i'
		if (pSec0->dwTag==0x0FF0AA55)
		{
			RKANDROID_IDB_SEC1 *pSec;
			pSec = (RKANDROID_IDB_SEC1 *)(bData+SECTOR_SIZE+SPARE_SIZE);
			if (pSec->uiChipTag==0x38324B52)
			{
				IDBlockPos = i;
				return 0;
			}
			else
			{
				continue;
			}
        }
		
	}
	return -1;
}
char CRKAndroidDevice::FindAllIDB()
{
	signed char iRet;
	char iIndex,iStart=0;
	uint_8 i;
	m_oldIDBCounts = 0;
	for( i=0; i<5; i++)
	{
		iRet = FindIDBlock( iStart,iIndex );
		if ( iRet<0 )
		{
			return m_oldIDBCounts;
		}
		
		m_idBlockOffset[i] = iIndex;
		m_oldIDBCounts++;
		iStart = iIndex+1;
	}

	return m_oldIDBCounts;
}
bool CRKAndroidDevice::ReserveIDBlock(char iBlockIndex,char iIdblockPos)
{
	uint_8 i;
	for(i=iIdblockPos; i<IDB_BLOCKS; i++)
	{
		if( (signed char)(iBlockIndex=FindValidBlocks(iBlockIndex,m_flashInfo.usPhyBlokcPerIDB))<0 )
		{
			return false;
		}
		m_idBlockOffset[i] = iBlockIndex;
		iBlockIndex += m_flashInfo.usPhyBlokcPerIDB;
	}	
	return true;
}
bool CRKAndroidDevice::CalcIDBCount(bool bUpdateLoader)
{
	bool bRet;
	uint_32 uiIdSectorNum;
	if (bUpdateLoader)
	{
		if (NewIDBFlag)
		{
			bRet = GetLoaderHeadSize();
			if (!bRet)
			{
				m_error = ERR_FLASHHEAD;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:CalcIDBCount-->GetLoaderHeadSize failed");
				}
				return false;
			}
			uiIdSectorNum = m_usFlashHeadSec;
		}
		else {
			bRet = GetLoaderSize();
			if (!bRet)
			{
				m_error = ERR_FLASHBOOT;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:CalcIDBCount-->GetLoaderSize failed");
				}
				return false;
			}
			bRet = GetLoaderDataSize();
			if (!bRet)
			{
				m_error = ERR_FLASHDATA;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:CalcIDBCount-->GetLoaderDataSize failed");
				}
				return false;
			}
			uiIdSectorNum = 4 + m_usFlashDataSec + m_usFlashBootSec;
			if (m_pFlashInfoData)
			{
				m_usFlashInfoDataOffset = uiIdSectorNum;
				uiIdSectorNum+=(PAGEALIGN(m_usFlashInfoDataLen)*4);
			}
		}
	}
	else
	{
		if (m_oldSec0)
		{
			m_usFlashDataSec = m_oldSec0->usBootDataSize;
			m_usFlashBootSec = m_oldSec0->usBootCodeSize - m_usFlashDataSec;
		}
		else
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:CalcIDBCount-->No found old idb data");
			}
			return false;
		}
		m_usFlashInfoDataOffset = 0;
		m_usFlashInfoDataLen = 0;
		if ((m_oldSec1->usFlashInfoOffset>0)&&((m_oldSec1->usFlashInfoOffset%4)==0))
		{
			if ((m_oldSec1->usFlashInfoLen>0)&&(m_oldSec1->usFlashInfoLen<=512))
			{
				m_usFlashInfoDataOffset = m_oldSec1->usFlashInfoOffset;
				m_usFlashInfoDataLen = m_oldSec1->usFlashInfoLen;
				uiIdSectorNum = m_usFlashInfoDataOffset;
				uiIdSectorNum+=(PAGEALIGN(m_usFlashInfoDataLen)*4);
			}
			else
				uiIdSectorNum = m_oldSec0->usBootCodeSize+4;
		}
		else
			uiIdSectorNum = m_oldSec0->usBootCodeSize+4;
	}
	
	m_flashInfo.uiSecNumPerIDB = uiIdSectorNum;
	m_flashInfo.usPhyBlokcPerIDB = CALC_UNIT(uiIdSectorNum,m_flashInfo.usValidSecPerBlock);
	return true;
}
bool CRKAndroidDevice::OffsetIDBlock(char pos)
{
	int iBlockIndex,i;

	for ( i=0;i<m_flashInfo.usPhyBlokcPerIDB;i++)
	{
		m_flashInfo.blockState[m_idBlockOffset[static_cast<int>(pos)]+i]=1;
	}
	iBlockIndex = m_idBlockOffset[static_cast<int>(pos)]+m_flashInfo.usPhyBlokcPerIDB;
	for(i=pos; i<5; i++)
	{
		iBlockIndex = FindValidBlocks(iBlockIndex, m_flashInfo.usPhyBlokcPerIDB);
		if ( iBlockIndex<0 )
		{
			return false;
		}
		m_idBlockOffset[i] = iBlockIndex;
		iBlockIndex += m_flashInfo.usPhyBlokcPerIDB;
	}
	
	return true;
}
bool CRKAndroidDevice::GetLoaderSize()
{
	if (!m_pImage)
	{
		return false;
	}
	signed char index;
	bool bRet;
	char loaderName[]="FlashBoot";
	index = m_pImage->m_bootObject->GetIndexByName(ENTRYLOADER,loaderName);
	if (index==-1)
	{
		return false;
	}
	uint_32 dwDelay;
	bRet = m_pImage->m_bootObject->GetEntryProperty(ENTRYLOADER,index,m_dwLoaderSize,dwDelay);
	if (bRet)
	{
		m_usFlashBootSec = PAGEALIGN(BYTE2SECTOR(m_dwLoaderSize))*4;
	}
	return bRet;
}
bool CRKAndroidDevice::GetLoaderDataSize()
{
	if (!m_pImage)
	{
		return false;
	}
	signed char index;
	bool bRet;
	char loaderName[]="FlashData";
	index = m_pImage->m_bootObject->GetIndexByName(ENTRYLOADER,loaderName);
	if (index==-1)
	{
		return false;
	}
	uint_32 dwDelay;
	bRet = m_pImage->m_bootObject->GetEntryProperty(ENTRYLOADER,index,m_dwLoaderDataSize,dwDelay);
	if (bRet)
	{
		m_usFlashDataSec = PAGEALIGN(BYTE2SECTOR(m_dwLoaderDataSize))*4;
	}
	return bRet;
}
bool CRKAndroidDevice::GetLoaderHeadSize()
{
	if (!m_pImage)
	{
		return false;
	}
	signed char index;
	bool bRet;
	char loaderName[]="FlashHead";
	index = m_pImage->m_bootObject->GetIndexByName(ENTRYLOADER,loaderName);
	if (index==-1)
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:GetLoaderHeadSize-->no found flashhead entry");
		}
		return false;
	}
	uint_32 dwDelay,dwCount,dwEntrySize;
	dwCount = m_pImage->m_bootObject->EntryLoaderCount;
	m_usFlashHeadSec = 0;
	for (size_t i=0;i<dwCount;i++)
	{
		bRet = m_pImage->m_bootObject->GetEntryProperty(ENTRYLOADER,i,dwEntrySize,dwDelay);
		if (!bRet)
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:GetLoaderHeadSize-->failed to get NO%d entry",i+1);
			}
			return false;
		}
		m_usFlashHeadSec += (PAGEALIGN(BYTE2SECTOR(dwEntrySize))*4);
	}

	return true;
}
bool CRKAndroidDevice::GetNewDiskSize()
{
	if (!m_pImage)
	{
		return false;
	}
	uint_8* lpReserved=NULL;
	uint_16 usSize;
	m_pImage->GetReservedData(lpReserved,usSize);
	m_sysDiskSize = (*(uint_16 *)lpReserved);
	m_cfgDiskSize = (*(uint_16 *)(lpReserved+2));
	return true;

}
CRKAndroidDevice::CRKAndroidDevice(STRUCT_RKDEVICE_DESC &device):CRKDevice(device)
{
	m_oldSec0 = NULL;
	m_oldSec1 = NULL;
	m_oldSec2 = NULL;
	m_oldSec3 = NULL;
	m_dwLoaderSize = 0;
	m_dwLoaderDataSize = 0;
	m_dwLoaderHeadSize = 0;
	m_oldIDBCounts = 0;
	m_usFlashBootSec = 0;
	m_usFlashDataSec = 0;
	m_usFlashHeadSec = 0;
	m_bNeedLowFormat = false;
	m_bHasEnv = false;
	m_paramBuffer = NULL;
	m_uiLBATimes = 1;
	m_gptBuffer = NULL;
}
CRKAndroidDevice::~CRKAndroidDevice()
{
	if (m_oldSec0)
	{
		delete m_oldSec0;
		m_oldSec0 = NULL;
	}
	if (m_oldSec1)
	{
		delete m_oldSec1;
		m_oldSec1 = NULL;
	}
	if (m_oldSec2)
	{
		delete m_oldSec2;
		m_oldSec2 = NULL;
	}
	if (m_oldSec3)
	{
		delete m_oldSec3;
		m_oldSec3 = NULL;
	}
	if (m_paramBuffer)
	{
		delete []m_paramBuffer;
		m_paramBuffer = NULL;
	}
	if (m_gptBuffer)
	{
		delete []m_gptBuffer;
		m_gptBuffer = NULL;
	}
}
bool CRKAndroidDevice::GetOldSectorData()
{
	uint_8 bData[SECTOR_SIZE*4+SPARE_SIZE*4];
//	int iRet = ERR_SUCCESS;
	if (m_oldIDBCounts<=0)
	{
		return false;
	}

//	iRet = m_pComm->RKU_ReadSector(LinkName,m_idBlockOffset[0]*m_flashInfo.uiSectorPerBlock,4,bData);
//	if(ERR_SUCCESS!=iRet)
	if (!GetWriteBackData(m_oldIDBCounts,bData))
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:GetOldSectorData-->GetWriteBackData failed");
		}
		return false;
	}
	uint_8* pSec;
	if (!m_oldSec0)
	{
		m_oldSec0 = new RKANDROID_IDB_SEC0;
	}
	pSec = (uint_8*)(m_oldSec0);
	memset(pSec,0,SECTOR_SIZE);
	memcpy(pSec,bData,SECTOR_SIZE);
	P_RC4(pSec,SECTOR_SIZE);
	
	if (!m_oldSec1)
	{
		m_oldSec1 = new RKANDROID_IDB_SEC1;
	}
	pSec = (uint_8*)(m_oldSec1);
	memset(pSec,0,SECTOR_SIZE);
	memcpy(pSec,bData+528,SECTOR_SIZE);
	
	if (!m_oldSec2)
	{
		m_oldSec2 = new RKANDROID_IDB_SEC2;
	}
	pSec = (uint_8*)(m_oldSec2);
	memset(pSec,0,SECTOR_SIZE);
	memcpy(pSec,bData+528*2,SECTOR_SIZE);
	P_RC4(pSec,SECTOR_SIZE);

	if (!m_oldSec3)
	{
		m_oldSec3 = new RKANDROID_IDB_SEC3;
	}
	pSec = (uint_8*)(m_oldSec3);
	memset(pSec,0,SECTOR_SIZE);
	memcpy(pSec,bData+528*3,SECTOR_SIZE);
	P_RC4(pSec,SECTOR_SIZE);

	return true;

}
bool CRKAndroidDevice::MakeSector0(uint_8* pSector)
{
	PRKANDROID_IDB_SEC0 pSec0;
	memset(pSector,0,SECTOR_SIZE);
	pSec0 = (PRKANDROID_IDB_SEC0)pSector;
	
	pSec0->dwTag = 0x0FF0AA55;
	if (m_pImage->m_bootObject->Rc4DisableFlag)
	{
		pSec0->uiRc4Flag = 1;
	}
	pSec0->usBootCode1Offset = 0x4;
	pSec0->usBootCode2Offset = 0x4;
	pSec0->usBootDataSize = m_usFlashDataSec;
	pSec0->usBootCodeSize = m_usFlashDataSec + m_usFlashBootSec;
	
//	pSec0->usCrc = CRC_16(pSector,SECTOR_SIZE-2);
	return true;
}

void CRKAndroidDevice::MakeSector1(uint_8* pSector)
{
	PRKANDROID_IDB_SEC1 pSec1;
	memset(pSector,0,SECTOR_SIZE);
	pSec1 = (PRKANDROID_IDB_SEC1)pSector;
	uint_16 usSysReserved;
	if (DirectLBA)
	{
		pSec1->usSysReservedBlock = 12;
		pSec1->uiChipTag = 0x38324B52;
		if (m_oldSec1)
		{
			pSec1->usReadWriteTimes = m_oldSec1->usReadWriteTimes+1;
		}
		return;
	}
	if ((m_idBlockOffset[4]+1)%12==0)
	{
		usSysReserved=m_idBlockOffset[4]+13;
	}
	else
	{
		usSysReserved =((m_idBlockOffset[4]+1)/12+1)*12;
	}
	if (usSysReserved>IDBLOCK_TOP)
	{
		usSysReserved = IDBLOCK_TOP;
	}
	pSec1->usSysReservedBlock = usSysReserved;

	if ((DeviceType==RK281X_DEVICE)||(DeviceType==RK28_DEVICE))
	{
		pSec1->usDisk0Size = m_sysDiskSize;
	}
	else
	{
		if (m_pImage->BackupSize<=0)
		{
			pSec1->usDisk0Size = m_sysDiskSize;
		}
		else
		{
			if (m_oldSec1)
			{
				if (m_oldSec1->usDisk0Size<m_pImage->BackupSize)
				{
					m_bNeedLowFormat = true;
				}
			}
			pSec1->usDisk0Size = m_pImage->BackupSize;
		}
	}
	
	pSec1->usDisk1Size = m_cfgDiskSize;
	pSec1->usDisk2Size = 0;
	pSec1->usDisk3Size = 0;
	pSec1->uiChipTag = 0x38324B52;
	pSec1->uiMachineId = 0;
	pSec1->usLoaderYear = UshortToBCD(((STRUCT_RKTIME)m_pImage->m_bootObject->ReleaseTime).usYear);
	pSec1->usLoaderDate = ByteToBCD(((STRUCT_RKTIME)m_pImage->m_bootObject->ReleaseTime).ucMonth);
	pSec1->usLoaderDate = (pSec1->usLoaderDate<<8)|ByteToBCD(((STRUCT_RKTIME)m_pImage->m_bootObject->ReleaseTime).ucDay);
	pSec1->usLoaderVer =  m_pImage->m_bootObject->Version;
	if (m_oldSec1)
	{
		pSec1->usLastLoaderVer = m_oldSec1->usLoaderVer;
		pSec1->usReadWriteTimes = m_oldSec1->usReadWriteTimes+1;
	}
	else
	{
		pSec1->usLastLoaderVer = 0;
		pSec1->usReadWriteTimes = 1;
	}
	pSec1->uiFlashSize = m_flashInfo.uiFlashSize*2*1024;
	pSec1->usBlockSize = m_flashInfo.usBlockSize*2;
	pSec1->bPageSize = m_flashInfo.uiPageSize*2;
	pSec1->bECCBits = m_flashInfo.bECCBits;
	pSec1->bAccessTime = m_flashInfo.bAccessTime;
	if (m_pFlashInfoData)
	{
		pSec1->usFlashInfoLen = m_usFlashInfoDataLen;
		pSec1->usFlashInfoOffset = m_usFlashInfoDataOffset;
	}

	pSec1->usIdBlock0 = m_idBlockOffset[0];
	pSec1->usIdBlock1 = m_idBlockOffset[1];
	pSec1->usIdBlock2 = m_idBlockOffset[2];
	pSec1->usIdBlock3 = m_idBlockOffset[3];
	pSec1->usIdBlock4 = m_idBlockOffset[4];
}
bool CRKAndroidDevice::MakeSector2(uint_8* pSector)
{
	PRKANDROID_IDB_SEC2 pSec2;
//	memset(pSector,0,SECTOR_SIZE);
	pSec2 = (PRKANDROID_IDB_SEC2)pSector;

	pSec2->usInfoSize = CHIPINFO_LEN;
	if (m_chipData)
	{
		memcpy(pSec2->bChipInfo,m_chipData,CHIPINFO_LEN);
	}
	else
	{
		int iRet;
		m_chipData = new uint_8[CHIPINFO_LEN];
		memset(m_chipData,0,CHIPINFO_LEN);
		iRet = m_pComm->RKU_ReadChipInfo(m_chipData);
		if (iRet==ERR_SUCCESS)
		{
			memcpy(pSec2->bChipInfo,m_chipData,CHIPINFO_LEN);
		}
		else
		{
			m_error = ERR_COMM_RW;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:MakeSector2-->RKU_ReadChipInfo failed,RetCode(%d)",iRet);
			}
			delete []m_chipData;
			m_chipData = NULL;
			return false;
		}
	}
	if (m_oldSec2)
	{
		memcpy(pSec2->reserved,m_oldSec2->reserved,RKANDROID_SEC2_RESERVED_LEN);
		pSec2->usSec3CustomDataOffset = m_oldSec2->usSec3CustomDataOffset;
		pSec2->usSec3CustomDataSize = m_oldSec2->usSec3CustomDataSize;
	}
	else
	{
		memset(pSec2->reserved,0,RKANDROID_SEC2_RESERVED_LEN);
		pSec2->usSec3CustomDataOffset = m_usWriteBackCustomDataOffset;
		pSec2->usSec3CustomDataSize = m_usWriteBackCustomDataSize;
	}
	
	strcpy(pSec2->szVcTag,"VC");
	strcpy(pSec2->szCrcTag,"CRC");
	return true;
}
bool CRKAndroidDevice::MakeSector3(uint_8* pSector)
{
	PRKANDROID_IDB_SEC3 pSec3;
	memset(pSector,0,SECTOR_SIZE);
	pSec3 = (PRKANDROID_IDB_SEC3)pSector;

	if (m_oldSec3)
	{
		memcpy(pSector,(uint_8*)m_oldSec3,SECTOR_SIZE);
	}
	else
	{
		if (m_backupBuffer[0])
		{
			memcpy(pSector,(uint_8*)m_backupBuffer,SECTOR_SIZE);
		}
	}

	if (m_sn)
	{
		if (m_snSize>RKDEVICE_SN_LEN)
		{
			return false;
		}
		pSec3->usSNSize = m_snSize;
		memcpy(pSec3->sn,m_sn,m_snSize);
	}

	if (m_blueTooth)
	{
		pSec3->blueToothSize = RKDEVICE_BT_LEN;
		memcpy(pSec3->blueToothAddr,m_blueTooth,RKDEVICE_BT_LEN);
	}

	if (m_mac)
	{
		pSec3->macSize = RKDEVICE_MAC_LEN;
		memcpy(pSec3->macAddr,m_mac,RKDEVICE_MAC_LEN);
	}

	if (m_imei)
	{
		pSec3->imeiSize = RKDEVICE_IMEI_LEN;
		memcpy(pSec3->imei,m_imei,RKDEVICE_IMEI_LEN);
	}

	if (m_uid)
	{
		if ((m_oldSec3)||(m_backupBuffer[0]))
		{
			if (!CheckUid(pSec3->uidSize,pSec3->uid))
			{
				pSec3->uidSize = RKDEVICE_UID_LEN;
				memcpy(pSec3->uid,m_uid,RKDEVICE_UID_LEN);
				m_bUidWriteOK = true;
			}
		}
		else
		{
			pSec3->uidSize = RKDEVICE_UID_LEN;
			memcpy(pSec3->uid,m_uid,RKDEVICE_UID_LEN);
			m_bUidWriteOK = true;
		}
	}

	return true; 
}
int CRKAndroidDevice::MakeIDBlockData(uint_8* lpIDBlock)
{
 	RKANDROID_IDB_SEC0 sector0Info;
	RKANDROID_IDB_SEC1 sector1Info;
	RKANDROID_IDB_SEC2 sector2Info;
	RKANDROID_IDB_SEC3 sector3Info;

	if (!m_pImage)
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:MakeIDBlockData-->Image is invalid");
		}
		return -1;
	}
	signed char index;
	char loaderCodeName[]="FlashBoot";
	index = m_pImage->m_bootObject->GetIndexByName(ENTRYLOADER,loaderCodeName);
	if (index==-1)
	{
		m_error = ERR_FLASHBOOT;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:MakeIDBlockData-->Get LoaderCode Entry failed");
		}
		return -2;
	}
	uint_8* loaderCodeBuffer;
	loaderCodeBuffer = new uint_8[m_dwLoaderSize];
	memset(loaderCodeBuffer,0,m_dwLoaderSize);
	if ( !m_pImage->m_bootObject->GetEntryData(ENTRYLOADER,index,loaderCodeBuffer) )
	{
		m_error = ERR_FLASHBOOT;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:MakeIDBlockData-->Get LoaderCode Data failed");
		}
		delete []loaderCodeBuffer;
		return -3;
	}

	char loaderDataName[]="FlashData";
	index = m_pImage->m_bootObject->GetIndexByName(ENTRYLOADER,loaderDataName);
	if (index==-1)
	{
		m_error = ERR_FLASHDATA;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:MakeIDBlockData-->Get LoaderData Entry failed");
		}
		delete []loaderCodeBuffer;
		return -4;
	}
	uint_8* loaderDataBuffer;
	loaderDataBuffer = new uint_8[m_dwLoaderDataSize];
	memset(loaderDataBuffer,0,m_dwLoaderDataSize);
	if ( !m_pImage->m_bootObject->GetEntryData(ENTRYLOADER,index,loaderDataBuffer) )
	{
		m_error = ERR_FLASHDATA;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:MakeIDBlockData-->Get LoaderData Data failed");
		}
		delete []loaderDataBuffer;
		delete []loaderCodeBuffer;
		return -5;
	}
		

	uint_32 i;
	MakeSector0((uint_8*)&sector0Info);
	MakeSector1((uint_8*)&sector1Info);
	if (!MakeSector2((uint_8*)&sector2Info))
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:MakeIDBlockData-->MakeSector2 failed");
		}
		return -6;
	}
	if (!MakeSector3((uint_8*)&sector3Info))
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:MakeIDBlockData-->MakeSector3 failed");
		}
		return -7;
	}
	sector2Info.usSec0Crc = CRC_16((uint_8*)&sector0Info,SECTOR_SIZE);
	sector2Info.usSec1Crc = CRC_16((uint_8*)&sector1Info,SECTOR_SIZE);
	sector2Info.usSec3Crc = CRC_16((uint_8*)&sector3Info,SECTOR_SIZE);

	memcpy(lpIDBlock, &sector0Info, SECTOR_SIZE);
	memcpy(lpIDBlock+SECTOR_SIZE, &sector1Info, SECTOR_SIZE);
//	memcpy(lpIDBlock+SECTOR_SIZE*2, &sector2Info, SECTOR_SIZE);
	memcpy(lpIDBlock+SECTOR_SIZE*3, &sector3Info, SECTOR_SIZE);

	if (sector0Info.uiRc4Flag)
	{//close rc4 encryption
		for (i=0;i<m_dwLoaderDataSize/SECTOR_SIZE;i++)
		{
			P_RC4(loaderDataBuffer+SECTOR_SIZE*i,SECTOR_SIZE);
		}
		for (i=0;i<m_dwLoaderSize/SECTOR_SIZE;i++)
		{
			P_RC4(loaderCodeBuffer+SECTOR_SIZE*i,SECTOR_SIZE);
		}
	}
	
	memcpy(lpIDBlock+SECTOR_SIZE*4, loaderDataBuffer, m_dwLoaderDataSize);
	memcpy(lpIDBlock+SECTOR_SIZE*(4+m_usFlashDataSec), loaderCodeBuffer, m_dwLoaderSize);
	
	sector2Info.uiBootCodeCrc = CRC_32((uint_8*)(lpIDBlock+SECTOR_SIZE*4),sector0Info.usBootCodeSize*SECTOR_SIZE);
	memcpy(lpIDBlock+SECTOR_SIZE*2, &sector2Info, SECTOR_SIZE);

    for(i=0; i<4; i++)
	{
        if(i == 1)
		{
            continue;
        }
        else
		{
			P_RC4(lpIDBlock+SECTOR_SIZE*i, SECTOR_SIZE);
        }
    }
	if (m_pFlashInfoData)
	{
		memcpy(lpIDBlock+SECTOR_SIZE*m_usFlashInfoDataOffset,m_pFlashInfoData,SECTOR_SIZE*m_usFlashInfoDataLen);
	}
	
	delete []loaderDataBuffer;
	delete []loaderCodeBuffer;
	return 0;
}
int CRKAndroidDevice::MakeNewIDBlockData(uint_8* lpIDBlock)
{
	int i,ret=0;
	uint_32 j;
	if (!m_pImage)
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:MakeNewIDBlockData-->Image is invalid");
		}
		return -1;
	}
	signed char index;
	uint8_t *buffer = NULL;
	char loaderHeadName[]="FlashHead";
	uint32_t dwEntrySize,dwDelay,dwOffset;
	index = m_pImage->m_bootObject->GetIndexByName(ENTRYLOADER,loaderHeadName);
	if (index==-1)
	{
		m_error = ERR_FLASHHEAD;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:MakeNewIDBlockData-->Get FlashHead Entry failed");
		}
		ret = -2;
		goto Exit_NewIDBlock;
	}
	if (!m_pImage->m_bootObject->GetEntryProperty(ENTRYLOADER,index,dwEntrySize,dwDelay))
	{
		m_error = ERR_FLASHHEAD;
		ret = -2;
		goto Exit_NewIDBlock;
	}
	dwEntrySize = PAGEALIGN(BYTE2SECTOR(dwEntrySize))*RK_IMAGE_PAGE_SIZE;
	buffer = new uint8_t[dwEntrySize];
	memset(buffer,0,dwEntrySize);
	if ( !m_pImage->m_bootObject->GetEntryData(ENTRYLOADER,index,buffer) )
	{
		m_error = ERR_FLASHHEAD;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:MakeNewIDBlockData-->Get FlashHead Data failed");
		}
		ret = -3;
		goto Exit_NewIDBlock;
	}
	if (m_pImage->m_bootObject->Rc4DisableFlag)
	{//close rc4 encryption
		for (j=0;j<dwEntrySize/SECTOR_SIZE;j++)
		{
			P_RC4(buffer+SECTOR_SIZE*j,SECTOR_SIZE);
		}
	}
	memcpy(lpIDBlock, buffer, dwEntrySize);
	dwOffset = dwEntrySize;
	for (i=0;i<m_pImage->m_bootObject->EntryLoaderCount;i++)
	{
		if (i==index)
		{
			continue;
		}
		if (buffer)
		{
			delete []buffer;
			buffer = NULL;
		}
		if (!m_pImage->m_bootObject->GetEntryProperty(ENTRYLOADER,i,dwEntrySize,dwDelay))
		{
			m_error = ERR_FLASHHEAD;
			ret = -4;
			goto Exit_NewIDBlock;
		}
		dwEntrySize = PAGEALIGN(BYTE2SECTOR(dwEntrySize))*RK_IMAGE_PAGE_SIZE;
		buffer = new uint8_t[dwEntrySize];
		memset(buffer,0,dwEntrySize);
		if ( !m_pImage->m_bootObject->GetEntryData(ENTRYLOADER,i,buffer) )
		{
			m_error = ERR_FLASHHEAD;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:MakeNewIDBlockData-->Get Entry%d Data failed",i+1);
			}
			ret = -4;
			goto Exit_NewIDBlock;
		}
		if (m_pImage->m_bootObject->Rc4DisableFlag)
		{//close rc4 encryption
			for (j=0;j<dwEntrySize/SECTOR_SIZE;j++)
			{
				P_RC4(buffer+SECTOR_SIZE*j,SECTOR_SIZE);
			}
		}
		memcpy(lpIDBlock+dwOffset, buffer, dwEntrySize);
		dwOffset += dwEntrySize;
	}

Exit_NewIDBlock:
	if (buffer)
	{
		delete []buffer;
	}
	return ret;
}


bool CRKAndroidDevice::MakeSpareData(uint_8* lpIDBlock,uint_32 dwSectorNum,uint_8* lpSpareBuffer)
{
	uint_32 i = 0;
	uint_8 bchOutBuf[512+3+13];
	uint_8 bchInBuf[512+3];

	for (i=0; i<dwSectorNum; i++)
	{
		memcpy(bchInBuf, lpIDBlock+512*i, 512);
		bchInBuf[514] = ((i==0) ? 'i' : 0xff);
		bchInBuf[512] = 0xff;
		bchInBuf[513] = 0xff;
		bch_encode(bchInBuf, bchOutBuf);
		memcpy(lpSpareBuffer+i*16+3, bchOutBuf+515, 13);
	}
    lpSpareBuffer[2] = 'i';
	return true;
}

int CRKAndroidDevice::WriteIDBlock(uint_8* lpIDBlock,uint_8* lpSpareData,uint_32 dwSectorNum,bool bErase)
{
//	uint_8 *pIDBlockData = NULL;

	uint_8 writeBuf[MAX_WRITE_SECTOR*(SECTOR_SIZE+SPARE_SIZE)];
	uint_8 readBuf[MAX_WRITE_SECTOR*(SECTOR_SIZE+SPARE_SIZE)];
	uint_32 sysSectorAddr = 0;
	int iRet,iCurSec,i;
	uint_32 j,k;
	
	uint_32 uiNeedWriteSector,uiWriteSector;
	for(i=0; i<5; i++)
	{
		uiNeedWriteSector = dwSectorNum;
		uiWriteSector = 0;
		int iUsedSecCount,iUsedBlockCount;
		uint_32 iValidSecCount;
		iUsedSecCount=iUsedBlockCount=0;iValidSecCount = m_flashInfo.usValidSecPerBlock;
		if (bErase)
		{
			iRet = m_pComm->RKU_EraseBlock( 0, m_idBlockOffset[i], m_flashInfo.usPhyBlokcPerIDB, ERASE_NORMAL);
			if ( iRet!=ERR_SUCCESS )
			{
				m_error = ERR_COMM_RW;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:WriteIDBlock-->RKU_EraseBlock failed,RetCode(%d)",iRet);
				}
				if ( OffsetIDBlock(i) )
				{
					return -1;
				}
				return -2;
			}
		}
		
		sysSectorAddr = m_idBlockOffset[i]*m_flashInfo.uiSectorPerBlock;
		while(uiNeedWriteSector>0)
		{
			uiWriteSector = (uiNeedWriteSector>MAX_WRITE_SECTOR)?MAX_WRITE_SECTOR:uiNeedWriteSector;
			if (uiWriteSector>iValidSecCount)
			{
				uiWriteSector = iValidSecCount;
			}
			memset(writeBuf, 0xff, MAX_WRITE_SECTOR*(SECTOR_SIZE+SPARE_SIZE));
			for (j=0; j<uiWriteSector; j++)
			{
				memcpy(writeBuf+j*(SECTOR_SIZE+SPARE_SIZE), lpIDBlock+iUsedSecCount*SECTOR_SIZE+SECTOR_SIZE*j, SECTOR_SIZE);
				memcpy(writeBuf+j*(SECTOR_SIZE+SPARE_SIZE)+SECTOR_SIZE, lpSpareData+iUsedSecCount*SPARE_SIZE+SPARE_SIZE*j, SPARE_SIZE);
			}
			iCurSec = sysSectorAddr+iUsedBlockCount*m_flashInfo.uiSectorPerBlock+(m_flashInfo.usValidSecPerBlock-iValidSecCount);
			iRet = m_pComm->RKU_WriteSector( iCurSec, uiWriteSector, writeBuf);
			if (ERR_SUCCESS!=iRet)
			{
				m_error = ERR_COMM_RW;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:WriteIDBlock-->RKU_WriteSector failed,RetCode(%d)",iRet);
				}
				iRet = m_pComm->RKU_EraseBlock( 0, m_idBlockOffset[i], m_flashInfo.usPhyBlokcPerIDB, ERASE_NORMAL);
				if ( (iRet==ERR_SUCCESS) || (iRet==ERR_FOUND_BAD_BLOCK) )
				{
					if ( OffsetIDBlock(i) )
					{
						return -1;
					}
					return -3;
				}
				else
				{
					if (m_pLog)
					{
						m_pLog->Record("ERROR:WriteIDBlock-->RKU_WriteSector-->RKU_EraseBlock failed,RetCode(%d)",iRet);
					}
					return -4;
				}
			}

			memset(readBuf, 0xff, MAX_WRITE_SECTOR*(SECTOR_SIZE+SPARE_SIZE));

			iRet = m_pComm->RKU_ReadSector( iCurSec, uiWriteSector, readBuf);
			if (ERR_SUCCESS != iRet)
			{
				m_error = ERR_COMM_RW;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:WriteIDBlock-->RKU_ReadSector failed,RetCode(%d)",iRet);
				}
				iRet = m_pComm->RKU_EraseBlock( 0, m_idBlockOffset[i], m_flashInfo.usPhyBlokcPerIDB, ERASE_NORMAL);
				if ( (iRet==ERR_SUCCESS) || (iRet==ERR_FOUND_BAD_BLOCK) )
				{
					if ( OffsetIDBlock(i) )
					{
						return -1;
					}
					
					return -5;
				}
				else
				{
					if (m_pLog)
					{
						m_pLog->Record("ERROR:WriteIDBlock-->RKU_ReadSector-->RKU_EraseBlock failed,RetCode(%d)",iRet);
					}
					return -6;
				}
			}

			for (k = 0; k<uiWriteSector; k++)
			{
				if( 0!=memcmp(writeBuf+528*k, readBuf+528*k, SECTOR_SIZE) )
				{
					m_error = ERR_CHECK_IDB;
					if (m_pLog)
					{
						char szDateTime[100];
						string strFile;
						struct tm timeNow;
						timeNow = get_now_time();
						sprintf(szDateTime,"%02d:%02d:%02d \t",timeNow.tm_hour,timeNow.tm_min,timeNow.tm_sec);
						uint_8 *pFile,*pFlash;
						pFile = writeBuf+528*k;
						pFlash = readBuf+528*k;
						strFile = m_pLog->LogSavePath;
						strFile += szDateTime;
						strFile += "IDBfile.bin";
						m_pLog->SaveBuffer( strFile,pFile,SECTOR_SIZE );
						strFile = m_pLog->LogSavePath;
						strFile += szDateTime;
						strFile += "IDBflash.bin";
						m_pLog->SaveBuffer( strFile,pFlash,SECTOR_SIZE );
						m_pLog->Record("ERROR:WriteIDBlock-->memcmp failed");
					}
					
					iRet = m_pComm->RKU_EraseBlock( 0, m_idBlockOffset[i], m_flashInfo.usPhyBlokcPerIDB, ERASE_NORMAL);
					if ( (iRet==ERR_SUCCESS) || (iRet==ERR_FOUND_BAD_BLOCK) )
					{
						if ( OffsetIDBlock(i) )
						{
							return -1;
						}
						
						return -7;
					}
					else
					{
						if (m_pLog)
						{
							m_pLog->Record("ERROR:WriteIDBlock-->CheckData-->RKU_EraseBlock failed,RetCode(%d)",iRet);
						}
						return -8;
					}
				}
			}
			
			iUsedSecCount += uiWriteSector;
			uiNeedWriteSector -= uiWriteSector;
			iValidSecCount -= uiWriteSector;
			if (iValidSecCount<=0)
			{
				iUsedBlockCount++;
				iValidSecCount = m_flashInfo.usValidSecPerBlock;
			}
		}
	}

	return 0;
}
int CRKAndroidDevice::DownloadBoot()
{
	uint_8 i;
	uint_32 dwSize,dwDelay;
	uint_8* pBuffer=NULL;
	for ( i=0;i<m_pImage->m_bootObject->Entry471Count;i++ )
	{
		if ( !m_pImage->m_bootObject->GetEntryProperty(ENTRY471,i,dwSize,dwDelay) )
		{
			m_error = ERR_BOOT_471;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:DownloadBoot-->GetEntry471Property failed,index(%d)",i);
			}
			return -2;	
		}
		if (dwSize>0)
		{
			pBuffer = new uint_8[dwSize];
			if ( !m_pImage->m_bootObject->GetEntryData(ENTRY471,i,pBuffer) )
			{
				m_error = ERR_BOOT_471;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:DownloadBoot-->GetEntry471Data failed,index(%d)",i);
				}
				delete []pBuffer;
				return -3;
			}
			if ( !Boot_VendorRequest(0x0471,pBuffer,dwSize) )
			{
				m_error = ERR_RUN_BOOT471;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:DownloadBoot-->Boot_VendorRequest471 failed,index(%d)",i);
				}
				delete []pBuffer;
				return -4;
			}
			delete []pBuffer;
			pBuffer = NULL;
			if (dwDelay>0)
			{
				sleep_ms(dwDelay);
			}

		}
	}

	for ( i=0;i<m_pImage->m_bootObject->Entry472Count;i++ )
	{
		if ( !m_pImage->m_bootObject->GetEntryProperty(ENTRY472,i,dwSize,dwDelay) )
		{
			m_error = ERR_BOOT_472;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:DownloadBoot-->GetEntry472Property failed,index(%d)",i);
			}
			return -2;	
		}
		if (dwSize>0)
		{
			pBuffer = new uint_8[dwSize];
			if ( !m_pImage->m_bootObject->GetEntryData(ENTRY472,i,pBuffer) )
			{
				m_error = ERR_BOOT_472;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:DownloadBoot-->GetEntry472Data failed,index(%d)",i);
				}
				delete []pBuffer;
				return -3;
			}
			if ( !Boot_VendorRequest(0x0472,pBuffer,dwSize) )
			{
				m_error = ERR_RUN_BOOT472;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:DownloadBoot-->Boot_VendorRequest472 failed,index(%d)",i);
				}
				delete []pBuffer;
				return -4;
			}
			delete []pBuffer;
			pBuffer = NULL;
			if (dwDelay>0)
			{
				sleep_ms(dwDelay);
			}
			
		}
	}
	sleep_s(1);
	return 0;

}
bool CRKAndroidDevice::Boot_VendorRequest( uint_32 requestCode, uint_8* pBuffer, uint_32 dwDataSize)
{
	int iRet;
	iRet = m_pComm->RKU_DeviceRequest(requestCode,pBuffer,dwDataSize,m_pImage->m_bootObject->BootCrcOff);
	return (iRet==ERR_SUCCESS)?true:false;
}
int CRKAndroidDevice::PrepareIDB(bool &bNeedJump)
{
	int i,iRet;
	generate_gf();
	gen_poly();

	bool bFirstCS=false;
	m_uiIDBend = 0;
	for(i=0; i<8; i++)
	{
		if( m_flashInfo.bFlashCS&(1<<i) )
		{
			if (i==0)
			{
				bFirstCS = true;
			}
			if (m_pLog)
			{
				m_pLog->Record("INFO:CS(%d)\t\t(%dMB)\t\t(%s)",i+1,m_flashInfo.uiFlashSize,m_flashInfo.szManufacturerName);
			}
		}
	}
	if (!bFirstCS)
	{
		m_error = ERR_NO_FLASH_CS;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:PrepareIDB-->No Found 1st Flash CS");
		}
		return -2;
	}
	if (DirectLBA)
	{
		if (NewIDBFlag)
			return 0;
			
		if (!GetOldIDBData())
		{
			return -3;
		}
		if (m_oldSec3)
		{
			m_usWriteBackCrc = CRC_16((uint_8*)m_oldSec2,SECTOR_SIZE);
			m_usWriteBackCustomDataOffset = m_oldSec2->usSec3CustomDataOffset;
			m_usWriteBackCustomDataSize = m_oldSec2->usSec3CustomDataSize;
			memcpy(m_backupBuffer,(uint_8*)m_oldSec3,SECTOR_SIZE);
		}
		else
			FindBackupSector3();
		return 0;
	}
	m_sysDiskSize = 0;
	m_cfgDiskSize = 0;
	if ( !BuildBlockStateMap(0) )
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:PrepareIDB-->BuildBlockStateMap failed");
		}
		return -5;
	}
	
	m_bUidWriteOK = false;

	FindAllIDB();
	
	memset(m_backupBuffer,0,SECTOR_SIZE);
	memset(m_backupBuffer+SECTOR_SIZE,0xFF,SPARE_SIZE);

	if (m_oldIDBCounts>0)
	{
		uint_8 buffer[4*(SECTOR_SIZE+SPARE_SIZE)];
		PRKANDROID_IDB_SEC3 pSec;
		PRKANDROID_IDB_SEC2 pSec2;
		pSec2 = (PRKANDROID_IDB_SEC2)(buffer+2*(SECTOR_SIZE+SPARE_SIZE));
		pSec = (PRKANDROID_IDB_SEC3)(buffer+3*(SECTOR_SIZE+SPARE_SIZE));

		if (!GetWriteBackData(m_oldIDBCounts,buffer))
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:PrepareIDB-->GetWriteBackData failed");
			}
			return -1;
		}
		P_RC4((uint_8*)pSec2,SECTOR_SIZE);
		P_RC4((uint_8*)pSec,SECTOR_SIZE);
		IsExistSector3Crc(pSec2);
		
		m_usWriteBackCrc = CRC_16((uint_8*)pSec,SECTOR_SIZE);
		if (m_bExistSector3Crc)
		{
			m_usWriteBackCustomDataOffset = pSec2->usSec3CustomDataOffset;
			m_usWriteBackCustomDataSize = pSec2->usSec3CustomDataSize;
			if (m_usSector3Crc!=m_usWriteBackCrc)
			{
				if (m_pLog)
				{
					m_pLog->Record("ERROR:PrepareIDB-->Check sector3 crc failed");
				}
//				return -1;
			}
		}
		memcpy(m_backupBuffer,pSec,SECTOR_SIZE);
	}
	else
	{
		FindBackupBuffer();
	}
	
	bool bWriteIDB,bUpdateLoader;
	switch (m_uiWorkFlow)
	{
	case WF_UPGRADE:
	case WF_RESTORE:
	case WF_DOWNLOADBOOT:
		bWriteIDB = true;
		bUpdateLoader = true;
		break;
	case WF_WRITEBT:
	case WF_WRITEMAC:
	case WF_WRITESN:
	case WF_WRITEIMEI:
	case WF_WRITECUSTOMDATA:
		bWriteIDB = true;
		bUpdateLoader = false;
		break;
	default:
		bWriteIDB = false;
		bUpdateLoader = false;
		break;
	}

// 	if ( !CalcIDBCount() )
// 	{
// 		if (m_pLog)
// 		{
// 			m_pLog->Record("ERROR:PrepareIDB-->CalcIDBCount failed");
// 		}
// 		return -14;
// 	}
	if (UsbType==RKUSB_MASKROM)
	{
		if (m_oldIDBCounts>0)
		{
			if ( !GetOldSectorData() )
			{
				if (m_pLog)
				{
					m_pLog->Record("ERROR:PrepareIDB-->Maskrom-->GetOldSectorData failed");
				}
				return -6;
			}
			if (bWriteIDB)
			{
				for ( i=0;i<m_oldIDBCounts;i++ )
				{
					iRet = m_pComm->RKU_EraseBlock(0,m_idBlockOffset[i],1,ERASE_NORMAL);
					if (iRet!=ERR_SUCCESS)
					{
						m_error = ERR_COMM_RW;
						if (m_pLog)
						{
							m_pLog->Record("ERROR:PrepareIDB-->Maskrom-->RKU_EraseBlock failed,RetCode(%d)",iRet);
						}
						return -7;
					}
				}
			}
		}
		else
			bNeedJump = true;
		if (bWriteIDB)
		{
			if ( !CalcIDBCount(true) )
			{
				if (m_pLog)
				{
					m_pLog->Record("ERROR:PrepareIDB-->Maskrom-->CalcIDBCount failed");
				}
				return -14;
			}
			if ( !ReserveIDBlock() )
			{
				if (m_pLog)
				{
					m_pLog->Record("ERROR:PrepareIDB-->Maskrom-->ReserveIDBlock failed");
				}
				return -8;
			}
		}
	}
	else if (UsbType==RKUSB_LOADER)
	{
		if (m_oldIDBCounts>0)
		{
			if ( !GetOldSectorData() )
			{
				if (m_pLog)
				{
					m_pLog->Record("ERROR:PrepareIDB-->Loader-->GetOldSectorData failed");
				}
				return -9;
			}
			if (bWriteIDB)
			{
				if ( !CalcIDBCount(bUpdateLoader) )
				{
					if (m_pLog)
					{
						m_pLog->Record("ERROR:PrepareIDB-->Loader-->CalcIDBCount failed");
					}
					return -16;
				}
				uint_16 usOldPhyBlockPerIDB;
				uint_32 uiOldSecNum;
				if ((m_oldSec1->usFlashInfoOffset>0)&&((m_oldSec1->usFlashInfoOffset%4)==0))
				{
					if ((m_oldSec1->usFlashInfoLen>0)&&(m_oldSec1->usFlashInfoLen<=512))
					{
						uiOldSecNum = m_oldSec1->usFlashInfoOffset;
						uiOldSecNum += (PAGEALIGN(m_oldSec1->usFlashInfoLen)*4);;
					}
					else
						uiOldSecNum = m_oldSec0->usBootCodeSize+4;
				}
				else
					uiOldSecNum = m_oldSec0->usBootCodeSize+4;
				usOldPhyBlockPerIDB = CALC_UNIT(uiOldSecNum,m_flashInfo.usValidSecPerBlock);

				if (m_oldIDBCounts<IDB_BLOCKS)
				{
					
					char iIdbPos=0,iBlockStart=0;					
					if (usOldPhyBlockPerIDB==m_flashInfo.usPhyBlokcPerIDB)
					{
						iIdbPos = m_oldIDBCounts;
						iBlockStart = m_idBlockOffset[m_oldIDBCounts-1]+m_flashInfo.usPhyBlokcPerIDB;
					}
					else
					{
						if (!bUpdateLoader)
						{
							if (m_pLog)
							{
								m_pLog->Record("ERROR:PrepareIDB-->Check PhyBlokcPerIDB failed");
							}
							return -18;
						}
					}
					if ( !ReserveIDBlock(iBlockStart,iIdbPos) )
					{
						if (m_pLog)
						{
							m_pLog->Record("ERROR:PrepareIDB-->Loader-->ReserveIDBlock failed");
						}
						return -11;
					}
					
				}
				else
				{
					if (usOldPhyBlockPerIDB!=m_flashInfo.usPhyBlokcPerIDB)
					{
						if (!bUpdateLoader)
						{
							if (m_pLog)
							{
								m_pLog->Record("ERROR:PrepareIDB-->Check PhyBlokcPerIDB2 failed");
							}
							return -19;
						}
						if ( !ReserveIDBlock() )
						{
							if (m_pLog)
							{
								m_pLog->Record("ERROR:PrepareIDB-->Loader-->ReserveIDBlock2 failed");
							}
							return -15;
						}
					}
					
				}
				if (!bUpdateLoader)
				{
					if (m_idBlockOffset[4]>=m_oldSec1->usSysReservedBlock)
					{
						if (m_pLog)
						{
							m_pLog->Record("ERROR:PrepareIDB-->ReserveIDBlock beyond reserved block limit");
						}
						return -17;
					}
				}
			}
		}
		else
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:PrepareIDB-->Loader-->No Found IDB");
			}
			return -12;
		}
	}
	else
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:PrepareIDB-->UsbType is invalid");
		}
		return -13;
	}
	return 0;
}

int CRKAndroidDevice::DownloadIDBlock(bool &bNeedJump)
{
	uint_32 dwSectorNum;
	bool bFlag;
	dwSectorNum = m_flashInfo.uiSecNumPerIDB;

	if (DirectLBA)
	{
		if (NewIDBFlag)
		{
			if (m_pLog)
			{
				m_pLog->Record("INFO:DownloadIDBlock-->New IDB");
			}
		}
		if (IsEnvFirmware(bFlag))
		{
			if (bFlag)
			{
				m_bHasEnv = true;
				if (m_pLog)
				{
					m_pLog->Record("INFO:DownloadIDBlock-->Env Skip");
				}
				bNeedJump = true;
				return 0;
			}
		}
		
		if (!CalcIDBCount(true))
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:DownloadIDBlock-->CalcIDBCount failed");
			}
			return -1;
		}
		if (WriteIDBWithDirect()!=0)
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:DownloadIDBlock-->WriteIDBWithDirect failed");
			}
			return -2;
		}
		bNeedJump = true;
		return 0;
	}
	
	uint_8 *pIDBData,*pSpareData;
	pIDBData = new uint_8[dwSectorNum*SECTOR_SIZE];
	pSpareData = new uint_8[dwSectorNum*SPARE_SIZE];
	int iRet=0,iTryCount=3;

	while (iTryCount>0)
	{
		memset(pIDBData,0,dwSectorNum*SECTOR_SIZE);
		memset(pSpareData,0xff,dwSectorNum*SPARE_SIZE);
		
		iRet = MakeIDBlockData(pIDBData);
		if ( iRet!=0 )
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:DownloadIDBlock-->MakeIDBlockData,RetCode(%d)",iRet);
			}
			return -2;
		}
		if ( !MakeSpareData(pIDBData,dwSectorNum,pSpareData) )
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:DownloadIDBlock-->MakeSpareData failed");
			}
			return -3;
		}
		iRet = WriteIDBlock(pIDBData,pSpareData,dwSectorNum,true);
		if (iRet==0)
		{
			delete []pIDBData;
			delete []pSpareData;
			if (m_bNeedLowFormat)
			{
				bool bSupport,bRet;
				bRet = LowerFormatDevice(bSupport);
				if (!bRet)
				{
					if (m_pLog)
					{
						m_pLog->Record("ERROR:DownloadIDBlock-->LowerFormatDevice failed");
					}
					return -6;
				}
			}
			return 0;
		}
		else if (iRet!=-1)
		{
			delete []pIDBData;
			delete []pSpareData;
			return -4;
		}
		iTryCount--;
	}
	delete []pIDBData;
	delete []pSpareData;
	return -5;
}
int CRKAndroidDevice::DownloadImage()
{
	uint_32 dwLayerID;
	long long dwFwOffset;
	bool  bRet;
	ENUM_CALL_STEP emCallStep=CALL_FIRST;
	STRUCT_RKIMAGE_HDR rkImageHead;
	STRUCT_RKIMAGE_ITEM *pDownloadItem;
	uint_8 headerData[64*1024];
	uint_8* pHeaderData=headerData;
	int iHeadSize,iHeadMetaSize;
	dwLayerID = LocationID;
	
	dwFwOffset = m_pImage->FWOffset;
	iHeadSize = sizeof(STRUCT_RKIMAGE_HDR);
	
	
	bRet = m_pImage->GetData(dwFwOffset,iHeadSize,(uint_8*)&rkImageHead);
	if ( !bRet )
	{
		m_error = ERR_FW_READ;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:DownloadImage-->GetData failed");
		}
		return -1;
	}
	if ((rkImageHead.manufacturer[56]==0x55)&&(rkImageHead.manufacturer[57]==0x66))
	{
		uint_16 *pItemRemain;
		pItemRemain = (uint_16 *)(&rkImageHead.manufacturer[58]);
		rkImageHead.item_count += *pItemRemain;
	}
	iHeadMetaSize = iHeadSize = sizeof(STRUCT_RKIMAGE_HDR)-sizeof(STRUCT_RKIMAGE_ITEM)*MAX_PACKAGE_FILES;
	iHeadSize += rkImageHead.item_count*sizeof(STRUCT_RKIMAGE_ITEM);
	iHeadSize = CALC_UNIT(iHeadSize, RK_IMAGE_PAGE_SIZE)*RK_IMAGE_PAGE_SIZE;

 	bRet = m_pImage->GetData(dwFwOffset,iHeadSize,(uint_8*)pHeaderData);
 	if ( !bRet )
 	{
		m_error = ERR_FW_READ;
 		if (m_pLog)
 		{
 			m_pLog->Record("ERROR:DownloadImage-->GetData for header failed");
 		}
 		return -1;
 	}
	if ( rkImageHead.item_count<=0 )
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:DownloadImage-->No Found item");
		}
		return -2;
	}
	
	int i;
	bool bFound=false;
	int iParamPos=-1;
	uint_64 uiTotalSize=0;
	uint_64 ulItemSize;
	uint_64 uiSparseFlag = 0;
	uint_32 uiFirstWriteOff = 0;
	string strFirstWriteName;
	for ( i=0;i<rkImageHead.item_count;i++ )
	{
		pDownloadItem = (STRUCT_RKIMAGE_ITEM *)(pHeaderData+iHeadMetaSize+i*sizeof(STRUCT_RKIMAGE_ITEM));
		if ( pDownloadItem->flash_offset!=0xFFFFFFFF )
		{
			if ( strcmp(pDownloadItem->name,PARTNAME_PARAMETER)==0)
			{
				bFound = true;
				iParamPos = i;
			}
			else
			{
				if (uiFirstWriteOff == 0) {
					uiFirstWriteOff = pDownloadItem->flash_offset;
					strFirstWriteName = pDownloadItem->name;
				} else {
					if (uiFirstWriteOff>pDownloadItem->flash_offset) {
						uiFirstWriteOff = pDownloadItem->flash_offset;
						strFirstWriteName = pDownloadItem->name;
					}
				}
				if ((strcmp(pDownloadItem->name,PARTNAME_ENV)==0)&&(pDownloadItem->flash_offset==0))
				{
					m_bHasEnv = true;
				}

				if (IsSparseImage(*pDownloadItem))
				{
					uiSparseFlag += ((uint_64)1<<i);
					ulItemSize = GetSparseImageSize(*pDownloadItem);
					if (ulItemSize==(unsigned long long)-1)
					{
						if (m_pLog)
						{
							m_pLog->Record("ERROR:DownloadImage-->get sparse size failed,file=%s",pDownloadItem->name);
						}
						return -14;
					}
				}
				else
				{
					if (pDownloadItem->file[55]=='H')
					{
						ulItemSize = *((uint_32 *)(&pDownloadItem->file[56]));
						ulItemSize <<= 32;
						ulItemSize += pDownloadItem->size;
					}
					else
						ulItemSize = pDownloadItem->size;
				}
				
				uiTotalSize += ulItemSize;
			}
		}
	}
	if (!m_bHasEnv) {
		if ( !bFound )
		{
			m_error = ERR_NOT_EXIST_PARAMETER;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:DownloadImage-->No Found Parameter file");
			}
			return -3;
		}
		if ((m_uiIDBend>0)&&(!m_bCloseIDBCheck))
		{
			if (uiFirstWriteOff<m_uiIDBend)
			{
				m_error = ERR_CROSS_IDBLOCK;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:DownloadImage-->%s at 0x%08X is front of idblock at 0x%08X,idblock will be destroied",strFirstWriteName.c_str(),uiFirstWriteOff,m_uiIDBend);
				}
				return -17;
			}
		}
		pDownloadItem = (STRUCT_RKIMAGE_ITEM *)(pHeaderData+iHeadMetaSize+iParamPos*sizeof(STRUCT_RKIMAGE_ITEM));
		if (!MakeParamFileBuffer(*pDownloadItem))
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:DownloadImage-->MakeParamFileBuffer failed");
			}
			return -12;
		}
		bRet = GetParameterGptFlag(*pDownloadItem);
		if (!bRet)
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:DownloadImage-->GetParameterGptFlag failed,err=%d",errno);
			}
			return -16;
		}
		if ((GptFlag)&&(!DirectLBA))
		{
			m_error = ERR_GPT_DIRECTLBA;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:DownloadImage-->device did not support directLBA to write gpt");
			}
			return -14;
		}
		if (!GptFlag)
		{
			if (!CheckParamPartSize(pHeaderData+iHeadMetaSize,iParamPos,rkImageHead.item_count))
			{
				if (m_pLog)
				{
					m_pLog->Record("ERROR:DownloadImage-->CheckParamPartSize failed");
				}
				return -13;
			}
			uiTotalSize += (8*m_uiParamFileSize);
		}
		else
			uiTotalSize += SECTOR_SIZE * calculate_gpt_sector_count(m_flashInfo.uiLogicalBlockSize);
	}
	
	m_uiLBATimes = 1;
	int iRet;
	uint_32 dwTotal,dwCurrent;
	uint_8 subCode=0;
	dwTotal = dwCurrent = 0;
	if (m_uiForceBand == 0) {
		if (m_bUsbDriver4m)
			subCode = TU_USBTRANSFER4M_SUBCODE;
		iRet = m_pComm->RKU_TestDeviceReady(&dwTotal,&dwCurrent,subCode);
		if (iRet!=ERR_SUCCESS)
		{
			m_error = ERR_COMM_RW;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:DownloadImage-->RKU_TestDeviceReady failed,Err=%d",iRet);
			}
			return -11;
		}
		else
		{
			if (dwCurrent>0)
			{
				m_uiLBATimes = m_uiLBATimes<<dwCurrent;
			}
		}
	} else
		m_uiLBATimes = m_uiLBATimes << m_uiForceBand;
#ifndef _WIN32
	if (m_uiLBATimes>256)
		m_uiLBATimes = 256;//limit 4m transfer buffer on non-windows
#endif
	
	ReadCapability();
	// if (!GetFlashInfo())
	// {
	// 	if (m_pLog)
	// 	{
	// 		m_pLog->Record("ERROR:DownloadImage-->GetFlashInfo failed");
	// 	}
	// 	return -15;
	// }

	if (m_pLog)
	{
		m_pLog->Record("INFO:Start download image,total=%lld,band=%d,DirectLBA=%d,First4Access=%d,Gpt=%d,Env=%d",uiTotalSize,m_uiLBATimes,m_bDirectLba,m_bFirst4Access,m_bGptFlag,m_bHasEnv);
	}
	uint_8 ucLoopCount=0;
	uint_64 uiCurrentByte=0;
	for ( i=0;i<rkImageHead.item_count;i++ )
	{
		pDownloadItem = (STRUCT_RKIMAGE_ITEM *)(pHeaderData+iHeadMetaSize+i*sizeof(STRUCT_RKIMAGE_ITEM));
		if ( pDownloadItem->flash_offset==0xFFFFFFFF )
		{
			continue;
		}
		if (i==iParamPos)
		{
			if (GptFlag)
			{
				bRet = RKA_Gpt_Download(*pDownloadItem,ucLoopCount,uiCurrentByte,uiTotalSize,emCallStep);
				if ( !bRet )
				{
					if (m_pLog)
					{
						m_pLog->Record("ERROR:DownloadImage-->RKA_Gpt_Download failed");
					}
					return -4;
				}
			}
			else
			{
				bRet = RKA_Param_Download(*pDownloadItem,ucLoopCount,uiCurrentByte,uiTotalSize,emCallStep);
				if ( !bRet )
				{
					if (m_pLog)
					{
						m_pLog->Record("ERROR:DownloadImage-->RKA_Param_Download failed");
					}
					return -4;
				}
			}
		}
		else
		{
			if (pDownloadItem->file[55]=='H')
			{
				ulItemSize = *((uint_32 *)(&pDownloadItem->file[56]));
				ulItemSize <<= 32;
				ulItemSize += pDownloadItem->size;
			}
			else
				ulItemSize = pDownloadItem->size;
			
			if (ulItemSize>0)
			{
				if (uiSparseFlag & ((uint_64)1<<i))
				{
					bRet = RKA_SparseFile_Download(*pDownloadItem,ucLoopCount,uiCurrentByte,uiTotalSize,emCallStep);
				}
				else
					bRet = RKA_File_Download(*pDownloadItem,ucLoopCount,uiCurrentByte,uiTotalSize,emCallStep);
				if ( !bRet )
				{
					if (m_pLog)
					{
						m_pLog->Record("ERROR:DownloadImage-->RKA_File_Download failed(%s)",pDownloadItem->name);
					}
					return -5;
				}
			}
		}
	}
	if (uiCurrentByte>=uiTotalSize)
	{
		if (m_callBackProc)
		{
			emCallStep = CALL_LAST;
			m_callBackProc(dwLayerID,DOWNLOADIMAGE_PROGRESS,uiTotalSize,uiCurrentByte,emCallStep);
		}
	}
	if (NoCheckMode)
	{
		return 0;
	}
		
	ucLoopCount = 0;uiCurrentByte = 0;emCallStep=CALL_FIRST;
	for ( i=0;i<rkImageHead.item_count;i++ )
	{
		pDownloadItem = (STRUCT_RKIMAGE_ITEM *)(pHeaderData+iHeadMetaSize+i*sizeof(STRUCT_RKIMAGE_ITEM));
		if ( pDownloadItem->flash_offset==0xFFFFFFFF )
		{
			continue;
		}
		if (i==iParamPos)
		{
			if (GptFlag)
			{
				bRet = RKA_Gpt_Check(*pDownloadItem,ucLoopCount,uiCurrentByte,uiTotalSize,emCallStep);
				if ( !bRet )
				{
					if (m_pLog)
					{
						m_pLog->Record("ERROR:DownloadImage-->RKA_Gpt_Check failed");
					}
					return -6;
				}
			}
			else
			{
				bRet = RKA_Param_Check(*pDownloadItem,ucLoopCount,uiCurrentByte,uiTotalSize,emCallStep);
				if ( !bRet )
				{
					if (m_pLog)
					{
						m_pLog->Record("ERROR:DownloadImage-->RKA_Param_Check failed");
					}
					return -6;
				}
			}
		}
		else
		{
			if (pDownloadItem->file[55]=='H')
			{
				ulItemSize = *((uint_32 *)(&pDownloadItem->file[56]));
				ulItemSize <<= 32;
				ulItemSize += pDownloadItem->size;
			}
			else
				ulItemSize = pDownloadItem->size;
			if (ulItemSize>0)
			{
				if (uiSparseFlag & ((uint_64)1<<i))
				{
					bRet = RKA_SparseFile_Check(*pDownloadItem,ucLoopCount,uiCurrentByte,uiTotalSize,emCallStep);
				}
				else
					bRet = RKA_File_Check(*pDownloadItem,ucLoopCount,uiCurrentByte,uiTotalSize,emCallStep);
				if ( !bRet )
				{
					if (m_pLog)
					{
						m_pLog->Record("ERROR:DownloadImage-->RKA_File_Check failed(%s)",pDownloadItem->name);
					}
					return -7;
				}
			}
			
		}
	}
	
	if (uiCurrentByte>=uiTotalSize)
	{
		if (m_callBackProc)
		{
			emCallStep = CALL_LAST;
			m_callBackProc(dwLayerID,CHECKIMAGE_PROGRESS,uiTotalSize,uiCurrentByte,emCallStep);
		}
	}
	return 0;
}
int CRKAndroidDevice::EraseIDB()
{
	uint_32 dwEraseCounts;
	if ( m_oldIDBCounts>0 )
	{
		dwEraseCounts = m_oldSec1->usSysReservedBlock;
	}
	else
	{
		dwEraseCounts = IDBLOCK_TOP;
	}
	if (m_bEmmc || DirectLBA)
	{
		if(EraseDirectLBABlock(0,dwEraseCounts)!=ERR_SUCCESS)
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:EraseIDB-->EraseDirectLBABlock failed");
			}
			return -1;
		}
		// if (DirectLBA) {
		// 	if (!EraseEmmcIDB())
		// 	{
		// 		if (m_pLog)
		// 		{
		// 			m_pLog->Record("ERROR:EraseIDB-->EraseEmmcIDB failed");
		// 		}
		// 		return -1;
		// 	}
		// }
	}
	else
	{
		if(EraseEmmcBlock(0,0,dwEraseCounts)!=ERR_SUCCESS)
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:EraseIDB-->EraseEmmcBlock failed");
			}
			return -1;
		}

		if ( !EraseMutilBlock(0,0,dwEraseCounts,false) )
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:EraseIDB-->EraseMutilBlock failed");
			}
			return -1;
		}
	}
	
	return 0;
}
int CRKAndroidDevice::EraseAllBlocks()
{
	int i;
	uint_32 uiBlockCount;
	int iRet=ERR_SUCCESS,iErasePos=0,iEraseBlockNum=0,iEraseTimes=0,iCSIndex=0;
	uint_8 bCSCount=0;
	for (i=0;i<8;i++)
	{
		if ( m_flashInfo.bFlashCS & (1<<i) )
		{
			bCSCount++;
		}
	}
	uint_32 dwLayerID;
	dwLayerID = LocationID;
	ENUM_CALL_STEP emCallStep=CALL_FIRST;
	if ((m_bEmmc)||(DirectLBA))
	{
		if (!EraseEmmc())
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:EraseAllBlocks-->EraseEmmc failed");
			}
			return -1;
		}
		return 0;
	}
	for (i=0;i<8;i++)
	{
		if ( m_flashInfo.bFlashCS & (1<<i) )
		{
			uiBlockCount = m_flashInfo.uiBlockNum;
			iErasePos=0;iEraseTimes=0;
			while (uiBlockCount>0)
			{
				iEraseBlockNum = (uiBlockCount<MAX_ERASE_BLOCKS)?uiBlockCount:MAX_ERASE_BLOCKS;
				
				iRet = m_pComm->RKU_EraseBlock(i,iErasePos,iEraseBlockNum,ERASE_FORCE);
				if ((iRet!=ERR_SUCCESS)&&(iRet!=ERR_FOUND_BAD_BLOCK))
				{
					m_error = ERR_COMM_RW;
					if (m_pLog)
					{
						m_pLog->Record("ERROR:EraseAllBlocks-->RKU_EraseBlock failed,RetCode(%d)",iRet);
					}
					return -1;
				}
				
				
				iErasePos += iEraseBlockNum;
				uiBlockCount -= iEraseBlockNum;
				iEraseTimes++;
				if (iEraseTimes%8==0)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,ERASEFLASH_PROGRESS,m_flashInfo.uiBlockNum*bCSCount,iCSIndex*m_flashInfo.uiBlockNum+iErasePos,emCallStep);
						emCallStep = CALL_MIDDLE;
					}
				}
			}
			iCSIndex++;
		}
	}
	
	if (m_callBackProc)
	{
		emCallStep = CALL_LAST;
		m_callBackProc(dwLayerID,ERASEFLASH_PROGRESS,m_flashInfo.uiBlockNum*bCSCount,iCSIndex*m_flashInfo.uiBlockNum,emCallStep);
	}
	return 0;
}
int CRKAndroidDevice::GetBlocksState()
{
	if (!m_pBlockState)
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:GetBlocksState-->No buffer to save block state");
		}
		return -1;
	}
	m_pBlockState->bFlashCS = m_flashInfo.bFlashCS;
	m_pBlockState->uiBlockNum = m_flashInfo.uiBlockNum;
	m_pBlockState->usBlockStateSize = ((m_pBlockState->uiBlockNum-1)/512+1)*64;
	uint_8 bCSCount=0;
	int i;
	for (i=0;i<8;i++)
	{
		if ( m_pBlockState->bFlashCS & (1<<i) )
		{
			bCSCount++;
		}
	}
	m_pBlockState->pBlockStateData = new uint_8[m_pBlockState->usBlockStateSize*bCSCount];
	uint_32 dwLayerID;
	dwLayerID = LocationID;
	ENUM_CALL_STEP emCallStep=CALL_FIRST;
	uint_32 uiBlockCount;
	int iRet=ERR_SUCCESS,iTestTimes=0,iCSIndex=0,iTestPos=0,iTestBlockNum=0;
	for (i=0;i<8;i++)
	{
		if ( m_pBlockState->bFlashCS & (1<<i) )
		{
			uiBlockCount = m_pBlockState->uiBlockNum;
			iTestTimes = 0;iTestPos=0;
			while (uiBlockCount>0)
			{
				iTestBlockNum = (uiBlockCount<MAX_TEST_BLOCKS)?uiBlockCount:MAX_TEST_BLOCKS;
				iRet = m_pComm->RKU_TestBadBlock(i,iTestPos,iTestBlockNum,m_pBlockState->pBlockStateData+iCSIndex*m_pBlockState->usBlockStateSize+iTestTimes*64);
				if (iRet!=ERR_SUCCESS)
				{
					m_error = ERR_COMM_RW;
					if (m_pLog)
					{
						m_pLog->Record("ERROR:GetBlocksState-->RKU_TestBadBlock failed,RetCode(%d)",iRet);
					}
					return -1;
				}
				iTestPos += iTestBlockNum;
				uiBlockCount -= iTestBlockNum;
				iTestTimes++;
				if (iTestTimes%8==0)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,TESTBLOCK_PROGRESS,m_pBlockState->uiBlockNum*bCSCount,iCSIndex*m_pBlockState->uiBlockNum+iTestPos,emCallStep);
						emCallStep = CALL_MIDDLE;
					}
				}
			}
			iCSIndex++;
		}
	}

	if (m_callBackProc)
	{
		emCallStep = CALL_LAST;
		m_callBackProc(dwLayerID,TESTBLOCK_PROGRESS,m_pBlockState->uiBlockNum*bCSCount,iCSIndex*m_pBlockState->uiBlockNum,emCallStep);
	}
	return 0;
}


bool CRKAndroidDevice::BufferWriteBack()
{
	if (DirectLBA)
	{
		if (m_oldSec3)
		{
			delete []m_oldSec3;
			m_oldSec3 = NULL;
		}
		GetOldIDBData();
		if (m_oldSec3)
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:BufferWriteBack-->Found IDB");
			}
			return true;
		}
		uint_8 pWriteBackBuffer[2*SECTOR_SIZE];
		char *pszTag = (char *)pWriteBackBuffer;
		uint_16 *pValue = (uint_16 *)(pWriteBackBuffer+4);
		int iRet;
		memset(pWriteBackBuffer,0,2*SECTOR_SIZE);

		if ((m_usWriteBackCustomDataOffset==0)&&(m_usWriteBackCustomDataSize==0)&&(memcmp(m_backupBuffer,pWriteBackBuffer,SECTOR_SIZE)==0))
		{
			return true;
		}
		strcpy(pszTag,"CRC");
		*pValue = m_usWriteBackCrc;
		pValue++;
		*pValue = m_usWriteBackCustomDataOffset;
		pValue++;
		*pValue = m_usWriteBackCustomDataSize;
		memcpy(pWriteBackBuffer+SECTOR_SIZE,m_backupBuffer,SECTOR_SIZE);
		iRet = m_pComm->RKU_WriteLBA(SECTOR3_BACKUP_LBA,2,pWriteBackBuffer);
		if (iRet!=ERR_SUCCESS)
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:BufferWriteBack-->RKU_WriteLBA failed,RetCode(%d)",iRet);
			}
		}
		return true;
	}
	FindAllIDB();
	if (m_oldIDBCounts>0)
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:BufferWriteBack-->Found IDB");
		}
		return true;
	}
	if (m_usWriteBackCrc==0)
	{
		if (m_pLog)
		{
			m_pLog->Record("INFO:BufferWriteBack-->Crc is zero");
		}
		return true;
	}
	uint_8 pWriteBackBuffer[2*(SECTOR_SIZE+SPARE_SIZE)];
	char *pszTag = (char *)pWriteBackBuffer;
	uint_16 *pValue = (uint_16 *)(pWriteBackBuffer+4);

	memset(pWriteBackBuffer,0,SECTOR_SIZE);
	memset(pWriteBackBuffer+SECTOR_SIZE,0xFF,SPARE_SIZE);
	memset(pWriteBackBuffer+SECTOR_SIZE+SPARE_SIZE,0,SECTOR_SIZE);
	memset(pWriteBackBuffer+2*SECTOR_SIZE+SPARE_SIZE,0xFF,SPARE_SIZE);
	
	strcpy(pszTag,"CRC");
	*pValue = m_usWriteBackCrc;
	pValue++;
	*pValue = m_usWriteBackCustomDataOffset;
	pValue++;
	*pValue = m_usWriteBackCustomDataSize;
	memcpy(pWriteBackBuffer+(SECTOR_SIZE+SPARE_SIZE),m_backupBuffer,SECTOR_SIZE);
	MakeSpareData(pWriteBackBuffer,1,pWriteBackBuffer+SECTOR_SIZE);
	MakeSpareData(pWriteBackBuffer+SECTOR_SIZE+SPARE_SIZE,1,pWriteBackBuffer+2*SECTOR_SIZE+SPARE_SIZE);
	int iRet,i;
	for (i=WBBUFFER_BOTTOM;i<WBBUFFER_TOP;i++)
	{
		iRet = m_pComm->RKU_EraseBlock( 0, i, 1, ERASE_NORMAL);
		if ( iRet!=ERR_SUCCESS )
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:BufferWriteBack-->RKU_EraseBlock failed,Block(%d),RetCode(%d)",i,iRet);
			}
			continue;
		}
		iRet = m_pComm->RKU_WriteSector(i*m_flashInfo.uiSectorPerBlock,2,pWriteBackBuffer);
		if (iRet!=ERR_SUCCESS)
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:BufferWriteBack-->RKU_WriteSector failed,RetCode(%d)",iRet);
			}
		}
		else
			break;
	}
	if (i>=WBBUFFER_TOP)
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:BufferWriteBack failed");
		}
		return true;
	}

	return true;
}
bool CRKAndroidDevice::FindBackupBuffer()
{
	int i,iRet;
	bool bRet;
	uint_8 buffer[2*(SECTOR_SIZE+SPARE_SIZE)];
	for (i=WBBUFFER_BOTTOM;i<WBBUFFER_TOP;i++)
	{
		memset(buffer,0,2*(SECTOR_SIZE+SPARE_SIZE));
		iRet = m_pComm->RKU_ReadSector(i*m_flashInfo.uiSectorPerBlock,2,buffer);
		if (iRet!=ERR_SUCCESS)
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:FindBackupBuffer-->RKU_ReadSector failed,RetCode(%d)",iRet);
			}
			continue;
		}
		else
		{
			PSTRUCT_RKANDROID_WBBUFFER pWriteBack;
			pWriteBack = (PSTRUCT_RKANDROID_WBBUFFER)buffer;
			char *pszCrcTag = (char *)buffer;
			if (pWriteBack->dwTag==0x38324B52)
			{
				bRet = CheckCrc16(buffer,SECTOR_SIZE-2,pWriteBack->usCrc);
				if (!bRet)
				{
					if (m_pLog)
					{
						m_pLog->Record("ERROR:FindBackupBuffer-->Check Crc Failed");
					}
//					continue;
				}
				PRKANDROID_IDB_SEC3 pSec = (PRKANDROID_IDB_SEC3)m_backupBuffer;
				pSec->usSNSize = pWriteBack->usSnSize;
				memcpy(pSec->sn,pWriteBack->btSnData,RKDEVICE_SN_LEN);
				memset(pSec->reserved,0,RKANDROID_SEC3_RESERVED_LEN);
				memcpy(pSec->reserved+6,pWriteBack->btReserve,RKANDROID_SEC3_RESERVED_LEN-6);
				pSec->imeiSize = pWriteBack->btImeiSize;
				memcpy(pSec->imei,pWriteBack->btImeiData,RKDEVICE_IMEI_LEN);
				pSec->uidSize = pWriteBack->btUidSize;
				memcpy(pSec->uid,pWriteBack->btUidData,RKDEVICE_UID_LEN);
				pSec->blueToothSize = pWriteBack->btBlueToothSize;
				memcpy(pSec->blueToothAddr,pWriteBack->btBlueToothData,RKDEVICE_BT_LEN);
				pSec->macSize = pWriteBack->btMacSize;
				memcpy(pSec->macAddr,pWriteBack->btMacData,RKDEVICE_MAC_LEN);
				m_usWriteBackCrc = CRC_16(m_backupBuffer,SECTOR_SIZE);
			}
			else if (strcmp(pszCrcTag,"CRC")==0)
			{
				m_usWriteBackCrc = *((uint_16 *)(buffer+4));
				m_usWriteBackCustomDataOffset = *((uint_16 *)(buffer+6));
				m_usWriteBackCustomDataSize = *((uint_16 *)(buffer+8));
				bRet = CheckCrc16(buffer+SECTOR_SIZE+SPARE_SIZE,SECTOR_SIZE,m_usWriteBackCrc);
				if (!bRet)
				{
					if (m_pLog)
					{
						m_pLog->Record("ERROR:FindBackupBuffer-->Check Crc Failed");
					}
					m_usWriteBackCrc = CRC_16(buffer+SECTOR_SIZE+SPARE_SIZE,SECTOR_SIZE);
//					continue;
				}
				memcpy(m_backupBuffer,buffer+SECTOR_SIZE+SPARE_SIZE,SECTOR_SIZE);
			}
			else
			{
				if (m_pLog)
				{
					m_pLog->Record("INFO:FindBackupBuffer-->No Found Tag");
				}
				continue;
			}
			
			break;
		}
	}
	if ( i<WBBUFFER_TOP )
	{
		return true;
	}
	else
		return false;
}

bool CRKAndroidDevice::RKA_File_Download(STRUCT_RKIMAGE_ITEM &entry,uint_8 &loopCount,uint_64 &currentByte,uint_64 totalByte,ENUM_CALL_STEP &emCallStep)
{
	uint_32 uiLBATransferSize=(LBA_TRANSFER_SIZE)*m_uiLBATimes;
	uint_32 uiLBASector = uiLBATransferSize/SECTOR_SIZE;
	uint_32 uiPartSizeSector,ubiImagePageSize;
	int iRet;
	bool bRet;
	uint_32 uiBufferSize=uiLBATransferSize,uiFlashOffset;
	long long uifileBufferSize;
	long long ulEntryStartOffset;
	uint_32 dwLayerID;

	dwLayerID = LocationID;
	if (entry.file[50]=='H')
	{
		ulEntryStartOffset = *((uint_32 *)(&entry.file[51]));
		ulEntryStartOffset <<= 32;
		ulEntryStartOffset += entry.offset;
		ulEntryStartOffset += m_pImage->FWOffset;
	}
	else
	{
		ulEntryStartOffset = m_pImage->FWOffset;
		ulEntryStartOffset += entry.offset;
	}
	if (entry.file[55]=='H')
	{
		uifileBufferSize = *((uint_32 *)(&entry.file[56]));
		uifileBufferSize <<= 32;
		uifileBufferSize += entry.size;
	}
	else
		uifileBufferSize = entry.size;

	if ((!m_bHasEnv)&&(!GptFlag))
	{
		if (First4Access)
		{
			uiFlashOffset = entry.flash_offset + 0x2000;//offset 4m
		}
		else
			uiFlashOffset = entry.flash_offset;
	}
	else
		uiFlashOffset = entry.flash_offset;
	
	if (uiFlashOffset>=m_flashInfo.uiFlashSizeSector)
	{
		m_error = ERR_PARTITION_CROSS_FLASH;
		if (m_pLog)
			m_pLog->Record("ERROR:RKA_File_Download-->Partition offset is larger than flash size, partition=%s,partition_offset=0x%08X,flash_size=0x%08X", entry.name,uiFlashOffset,m_flashInfo.uiFlashSizeSector);
		return false;
	}
	if (entry.part_size==0xFFFFFFFF)
	{
		uiPartSizeSector = m_flashInfo.uiFlashSizeSector - uiFlashOffset;
		if (GptFlag)
        {
        	if (m_flashInfo.uiFlashSizeSector>=0x800000)
			{//exclude backup gpt
				uiPartSizeSector -= 64;
			}
			else
				uiPartSizeSector -= 33;
		}
	}
	else
		uiPartSizeSector = entry.part_size;

	if ((uiFlashOffset+uiPartSizeSector)>m_flashInfo.uiFlashSizeSector)
	{
		m_error = ERR_PARTITION_CROSS_FLASH;
		if (m_pLog)
			m_pLog->Record("ERROR:RKA_File_Download-->Partition ending is larger than flash size, partition=%s,partition_end=0x%08X,flash_size=0x%08X", entry.name,(uiFlashOffset+uiPartSizeSector),m_flashInfo.uiFlashSizeSector);
		return false;
	}

	if (uifileBufferSize > (long long)uiPartSizeSector * SECTOR_SIZE)
	{
		m_error = ERR_PARTITION_NOT_ENOUGH;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_File_Download-->imagesize is larger than partition ,partition=%s,image_size=%llu,part_size=%llu",entry.name,uifileBufferSize,(long long)uiPartSizeSector * SECTOR_SIZE);
		}
		return false;
	}
	
	if (m_pLog)
	{
		m_pLog->Record("INFO:Start to download %s,offset=0x%x,size=%lld",entry.name,entry.flash_offset,uifileBufferSize);
	}

	if (IsUbiImage(entry)) {
		if (m_pLog)
		{
			m_pLog->Record("INFO:It's a ubi image");
		}
		bRet = m_pImage->GetData(ulEntryStartOffset+0x10,4,(uint_8 *)&ubiImagePageSize);
		if ( !bRet )
		{
			m_error = ERR_FW_READ;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:RKA_File_Download-->GetFileData failed on reading ubi pagesize");
			}
			return false;
		}
		ubiImagePageSize = Endian32_Swap(ubiImagePageSize);

		if (m_flashInfo.bPageSizeSector*SECTOR_SIZE != ubiImagePageSize)
		{
			m_error = ERR_UBI_PAGE_CHECK;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:RKA_File_Download-->Check UBI page failed,page from image:%u,page from device:%u",ubiImagePageSize,m_flashInfo.bPageSizeSector*SECTOR_SIZE);
			}
			return false;
		}
		if (!EraseUbiRegion(uiFlashOffset,entry.part_size))
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:RKA_File_Download-->EraseUbiRegion failed");
			}
			return false;
		}
	} else if (IsJffsImage(entry))
	{
		if (m_pLog)
		{
			m_pLog->Record("INFO:It's a jffs image");
		}
		if (!EraseSparseRegion(uiFlashOffset, uiPartSizeSector, 256))
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:RKA_File_Download-->Erase jffs region failed");
			}
			return false;
		}
	} else if (IsDharaImage(entry)) {
		if (m_pLog)
		{
			m_pLog->Record("INFO:It's a dhara image");
		}
		if (!EraseUbiRegion(uiFlashOffset,entry.part_size))
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:RKA_File_Download-->EraseUbiRegion failed");
			}
			return false;
		}
	}
	
	uint_8 byRWMethod=RWMETHOD_IMAGE;
	
	uint_8* pBuffer=NULL;
	pBuffer = new uint_8[uiBufferSize];
	if (!pBuffer)
	{
		m_error = ERR_NEW_BUFFER;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_File_Download-->New memory failed");
		}
		return false;
	}

	bool bMisc=false,bUser=false;
	if (strcmp(entry.name,PARTNAME_MISC)==0)
	{
		bMisc = true;
	}
	if (strcmp(entry.name,PARTNAME_USER)==0)
	{
		bUser = true;
	}
	
	uint_32 uiBegin,uiLen,uiWriteByte;
	long long uiEntryOffset;
	uiBegin = uiFlashOffset;
	uiLen = 0;uiWriteByte = 0;uiEntryOffset=0;
	while ( uifileBufferSize>0 )
	{
		memset(pBuffer,0,uiBufferSize);
		if ( uifileBufferSize<uiBufferSize )
		{
			uiWriteByte = uifileBufferSize;
			uiLen = ( (uiWriteByte%SECTOR_SIZE==0) ? (uiWriteByte/SECTOR_SIZE) : (uiWriteByte/SECTOR_SIZE+1) );
		}
		else
		{
			uiWriteByte = uiBufferSize;
			uiLen = uiLBASector;
		}
		bRet = m_pImage->GetData(ulEntryStartOffset+uiEntryOffset,uiWriteByte,pBuffer);
		if ( !bRet )
		{
			m_error = ERR_FW_READ;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:RKA_File_Download-->GetFileData failed");
			}
			delete []pBuffer;
			pBuffer = NULL;
			return false;
		}
		if (bUser)
		{
			if ((pBuffer[0]==0xEB)&&(pBuffer[1]==0x58)&&(pBuffer[2]==0x90))
			{//fat user image
				iRet = m_pComm->RKU_TestDeviceReady((uint_32 *)&m_uiUserSectors,NULL,TU_GETUSERSECTOR_SUBCODE);
				if (iRet!=ERR_SUCCESS)
				{
					m_error = ERR_COMM_RW;
					if (m_pLog)
					{
						m_pLog->Record("ERROR:RKA_File_Download-->Get user sectors failed,RetCode(%d)",iRet);
					}

					delete []pBuffer;
					pBuffer = NULL;
					return false;
				}
				if ((m_uiUserSectors==0)||(m_uiUserSectors==(uint_32)-1))
				{
					if (m_pLog)
					{
						m_pLog->Record("ERROR:RKA_File_Download-->User size is wrong,value=0x%x",m_uiUserSectors);
					}

					delete []pBuffer;
					pBuffer = NULL;
					return false;
				}
				if (m_uiUserSectors<=uiBegin)
				{
					if (m_pLog)
					{
						m_pLog->Record("ERROR:RKA_File_Download-->Available total is smaller than user offset");
					}

					delete []pBuffer;
					pBuffer = NULL;
					return false;
				}
				m_uiUserSectors -= uiBegin;
				
				uint_8 *pDbr,*pCopyDbr;
				pDbr = pBuffer;
				pCopyDbr = pBuffer + SECTOR_SIZE*6;
				if (*(uint_32 *)(pDbr+32)<m_uiUserSectors)
				{
					if (m_pLog)
					{
						m_pLog->Record("ERROR:RKA_File_Download-->Original size is smaller than current user size");
					}

					delete []pBuffer;
					pBuffer = NULL;
					return false;
				}

				(*(uint_32 *)(pDbr+32)) = m_uiUserSectors;
				(*(uint_32 *)(pCopyDbr+32)) = m_uiUserSectors;
			}
				
			bUser = false;
		}
		
		if ((bMisc)&&(m_emMiscModifyFlag!=MISC_MODIFY_NONE))
		{
			if (uiLBATransferSize % 0x4000==0)
			{
				if ((uiEntryOffset+uiWriteByte)>0x4000)
				{
					if (m_emMiscModifyFlag==MISC_MODIFY_WIPE_ALL)
					{
						if (uiWriteByte>0x4000)
						{
							memcpy(pBuffer+0x4040,Wipe_All,20);
						}
						else
							memcpy(pBuffer+0x40,Wipe_All,20);
					}
					else if (m_emMiscModifyFlag==MISC_MODIFY_WIPE_DATA)
					{
						if (uiWriteByte>0x4000)
						{
							memcpy(pBuffer+0x4040,Wipe_Data,21);
						}
						else
							memcpy(pBuffer+0x40,Wipe_Data,21);
					}
					bMisc = false;
				}
			}
		}
		
		iRet = m_pComm->RKU_WriteLBA(uiBegin,uiLen,pBuffer,byRWMethod);
		if( iRet!=ERR_SUCCESS )
		{
			m_error = ERR_COMM_RW;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:RKA_File_Download-->RKU_WriteLBA failed,Written(%lld),RetCode(%d)",uiEntryOffset,iRet);
			}
			
			delete []pBuffer;
			pBuffer = NULL;
			return false;
		}
		uifileBufferSize -= uiWriteByte;
		uiEntryOffset += uiWriteByte;
		uiBegin += uiLen;
		currentByte += uiWriteByte;
		
		uint_8 curLoopCount = (uint_8)(currentByte*100/totalByte);
		if (curLoopCount > loopCount) {
			loopCount = curLoopCount;
			if (m_callBackProc)
			{
				m_callBackProc(dwLayerID,DOWNLOADIMAGE_PROGRESS,totalByte,currentByte,emCallStep);
				if (emCallStep != CALL_MIDDLE)
					emCallStep = CALL_MIDDLE;
			}
		}
	}
	delete []pBuffer;
	pBuffer = NULL;
	return true;
}
bool CRKAndroidDevice::EraseSparseRegion(uint_32 uiOffset,uint_32 uiSize,uint_32 uiErase)
{
	bool bSuccess = true;
	int iRet;

	while (uiSize)
	{
		if (uiSize>=uiErase)
		{
			iRet = m_pComm->RKU_EraseLBA(uiOffset,uiErase);
			uiSize -= uiErase;
			uiOffset += uiErase;
		}
		else
		{
			iRet = m_pComm->RKU_EraseLBA(uiOffset,uiSize);
			uiSize = 0;
			uiOffset += uiSize;
		}
		if (iRet!=ERR_SUCCESS)
		{
			m_error = ERR_COMM_RW;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:ErasePartition-->erase partition failed,err=%d",iRet);
			}
			bSuccess = false;
			break;
		}
	}
	return bSuccess;
}
bool CRKAndroidDevice::EraseUbiRegion(uint_32 uiOffset,uint_32 uiSize)
{
	uint_8 flashID[5];
	uint_32 uiStartBlock,uiEraseBlock,uiBlockCount,uiErasePos;
	int iRet;
	bool bLast=false;
	iRet = m_pComm->RKU_ReadFlashID(flashID);
	if(iRet!=ERR_SUCCESS)
	{
		m_error = ERR_COMM_RW;
		if (m_pLog)
		{
			m_pLog->Record("Error:EraseUbiRegion-->RKU_ReadFlashID failed,RetCode(%d)",iRet);
		}
		return false;
	}
	uint_32 *pID=(uint_32 *)flashID;
	if (*pID==0x434d4d45)//emmc
	{
		return true;
	}

	if (uiSize==0xFFFFFFFF) {
		bLast = true;
		uiSize = m_flashInfo.uiFlashSizeSector - uiOffset;
	}
		

	uiStartBlock = uiOffset / m_flashInfo.usBlockSizeSector;
	if ((uiSize % m_flashInfo.usBlockSizeSector) == 0)
		uiEraseBlock = uiSize / m_flashInfo.usBlockSizeSector;
	else
		uiEraseBlock = uiSize / m_flashInfo.usBlockSizeSector + 1;
	
	if (bLast)
		uiEraseBlock--;//subtract backup gpt

	uiErasePos=uiStartBlock;
	while (uiEraseBlock>0)
	{
		uiBlockCount = (uiEraseBlock<MAX_ERASE_BLOCKS)?uiEraseBlock:MAX_ERASE_BLOCKS;

		iRet = m_pComm->RKU_EraseBlock(0,uiErasePos,uiBlockCount,ERASE_FORCE);
		if ((iRet!=ERR_SUCCESS)&&(iRet!=ERR_FOUND_BAD_BLOCK))
		{
			m_error = ERR_COMM_RW;
			if (m_pLog)
			{
				m_pLog->Record("Error:EraseUbiRegion-->RKU_EraseBlock failed,RetCode(%d)",iRet);
			}
			return false;
		}

		uiErasePos += uiBlockCount;
		uiEraseBlock -= uiBlockCount;
	}
	return true;

}
bool CRKAndroidDevice::RKA_SparseFile_Download(STRUCT_RKIMAGE_ITEM &entry,uint_8 &loopCount,uint_64 &currentByte,uint_64 totalByte,ENUM_CALL_STEP &emCallStep)
{
	uint_32 uiLBATransferSize=(LBA_TRANSFER_SIZE)*m_uiLBATimes;
	uint_32 uiLBASector = uiLBATransferSize/SECTOR_SIZE;
	int iRet;
	bool bRet;
	uint_32 uiBufferSize=uiLBATransferSize,uiFlashOffset;
	uint_32 uiPartSize=entry.part_size,uiSparseSector;
	uint_64 uifileBufferSize;
	uint_32 dwLayerID;
	uint_64 ulEntryStartOffset;
	sparse_header header;
	chunk_header chunk;
	dwLayerID = LocationID;;
	if (entry.file[50]=='H')
	{
		ulEntryStartOffset = *((uint_32 *)(&entry.file[51]));
		ulEntryStartOffset <<= 32;
		ulEntryStartOffset += entry.offset;
		ulEntryStartOffset += m_pImage->FWOffset;
	}
	else
	{
		ulEntryStartOffset = m_pImage->FWOffset;
		ulEntryStartOffset += entry.offset;
	}
	if (entry.file[55]=='H')
	{
		uifileBufferSize = *((uint_32 *)(&entry.file[56]));
		uifileBufferSize <<= 32;
		uifileBufferSize += entry.size;
	}
	else
		uifileBufferSize = entry.size;

	if ((!m_bHasEnv)&&(!GptFlag))
	{
		if (First4Access)
		{
			uiFlashOffset = entry.flash_offset + 0x2000;//offset 4m
		}
		else
			uiFlashOffset = entry.flash_offset;
	}
	else
		uiFlashOffset = entry.flash_offset;
	
	//get sparse header
	bRet = m_pImage->GetData(ulEntryStartOffset,sizeof(header),(uint_8*)&header);
	if (!bRet)
	{
		m_error = ERR_FW_READ;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_SparseFile_Download-->get sparse header failed,partition=%s",entry.name);
		}
		return false;
	}
	
	if (uiFlashOffset>=m_flashInfo.uiFlashSizeSector)
	{
		m_error = ERR_PARTITION_CROSS_FLASH;
		if (m_pLog)
			m_pLog->Record("ERROR:RKA_SparseFile_Download-->Partition offset is larger than flash size, partition=%s,partition_offset=0x%08X,flash_size=0x%08X", entry.name,uiFlashOffset,m_flashInfo.uiFlashSizeSector);
		return false;
	}
	uifileBufferSize = header.blk_sz * (uint_64)header.total_blks;
	
	if (uiPartSize==0xFFFFFFFF) {
		uiPartSize = m_flashInfo.uiFlashSizeSector - uiFlashOffset;
		if (GptFlag)
		{
			if (m_flashInfo.uiFlashSizeSector>=0x800000)
			{//exclude backup gpt
				uiPartSize -= 64;
			}
			else
				uiPartSize -= 33;
		}
	}
		
	if ((uiFlashOffset+uiPartSize)>m_flashInfo.uiFlashSizeSector)
	{
		m_error = ERR_PARTITION_CROSS_FLASH;
		if (m_pLog)
			m_pLog->Record("ERROR:RKA_SparseFile_Download-->Partition ending is larger than flash size, partition=%s,partition_end=0x%08X,flash_size=0x%08X", entry.name,(uiFlashOffset+uiPartSize),m_flashInfo.uiFlashSizeSector);
		return false;
	}
	uiSparseSector = uifileBufferSize/SECTOR_SIZE;
	if (uiSparseSector > uiPartSize) {
		m_error = ERR_PARTITION_NOT_ENOUGH;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_SparseFile_Download-->unsparse size is larger than partition ,partition=%s,unsparse=%d,part_size=%d",entry.name,uiSparseSector,uiPartSize);
		}
		return false;
	}
	if (m_pLog)
	{
		m_pLog->Record("INFO:Start to download %s,offset=0x%x,size=%lld",entry.name,entry.flash_offset,uifileBufferSize);
	}
	//erase partition
	if (!EraseSparseRegion(uiFlashOffset,uifileBufferSize/SECTOR_SIZE))
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_SparseFile_Download-->EraseSparseRegion failed");
		}
		return false;
	}
	//erase last 32k to destroy verity meta
	if (!EraseSparseRegion(uiFlashOffset+uiPartSize-64,64))
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_SparseFile_Download-->EraseSparseRegion last failed");
		}
		return false;
	}

	if (m_pLog)
	{
		m_pLog->Record("INFO:It's a sparse image");
	}

	uint_8 byRWMethod=RWMETHOD_IMAGE;

	uint_8* pBuffer=NULL;
	pBuffer = new uint_8[uiBufferSize];
	if (!pBuffer)
	{
		m_error = ERR_NEW_BUFFER;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_SparseFile_Download-->New memory failed");
		}
		return false;
	}

	uint_32 uiBegin,uiLen,uiWriteByte,uiCurChunk,uiFillByte,uiCrc;
	uint_64 uiChunkDataSize;
	uint_64 uiEntryOffset;
	uint_8 curLoopCount;
	uiCurChunk = 0;
	uiBegin = uiFlashOffset;
	uiLen = 0;uiWriteByte = 0;uiEntryOffset=sizeof(header);
	while ( uiCurChunk<header.total_chunks)
	{
		bRet = m_pImage->GetData(ulEntryStartOffset+uiEntryOffset,sizeof(chunk),(uint_8*)&chunk);
		if (!bRet)
		{
			m_error = ERR_FW_READ;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:RKA_SparseFile_Download-->get sparse chunk failed");
			}
			delete []pBuffer;
			pBuffer = NULL;
			return false;
		}
		uiCurChunk++;
		uiEntryOffset += sizeof(chunk);
		switch (chunk.chunk_type)
		{
		case CHUNK_TYPE_RAW:
			uiChunkDataSize = chunk.total_sz - sizeof(chunk_header);
			while (uiChunkDataSize>0)
			{
				memset(pBuffer,0,uiBufferSize);
				if ( uiChunkDataSize<uiBufferSize )
				{
					uiWriteByte = uiChunkDataSize;
					uiLen = ( (uiWriteByte%SECTOR_SIZE==0) ? (uiWriteByte/SECTOR_SIZE) : (uiWriteByte/SECTOR_SIZE+1) );
				}
				else
				{
					uiWriteByte = uiBufferSize;
					uiLen = uiLBASector;
				}
				if (!m_pImage->GetData(ulEntryStartOffset+uiEntryOffset,uiWriteByte,pBuffer))
				{
					m_error = ERR_FW_READ;
					if (m_pLog)
					{
						m_pLog->Record("ERROR:RKA_SparseFile_Download-->get chunk data failed,chunk=%d,0ffset=0x%llx,size=0x%x,err=%d",\
						uiCurChunk,\
						ulEntryStartOffset+uiEntryOffset,\
						uiWriteByte,\
						m_pImage->GetError());
					}
					delete []pBuffer;
					pBuffer = NULL;
					return false;
				}
				uiEntryOffset += uiWriteByte;
				iRet = m_pComm->RKU_WriteLBA(uiBegin,uiLen,pBuffer,byRWMethod);
				if( iRet!=ERR_SUCCESS )
				{
					m_error = ERR_COMM_RW;
					if (m_pLog)
					{
						m_pLog->Record("ERROR:RKA_SparseFile_Download-->RKU_WriteLBA failed,RetCode(%d),chunk=%d",iRet,uiCurChunk);
					}

					delete []pBuffer;
					pBuffer = NULL;
					return false;
				}
				uiBegin += uiLen;
				currentByte += uiWriteByte;
				uiChunkDataSize -= uiWriteByte;
				curLoopCount = (uint_8)(currentByte*100/totalByte);
				if (curLoopCount > loopCount) {
					loopCount = curLoopCount;
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,DOWNLOADIMAGE_PROGRESS,totalByte,currentByte,emCallStep);
						if (emCallStep != CALL_MIDDLE)
							emCallStep = CALL_MIDDLE;
					}
				}
			}
			break;
		case CHUNK_TYPE_FILL:
			uiChunkDataSize = (uint_64)chunk.chunk_sz * header.blk_sz;
			if (!m_pImage->GetData(ulEntryStartOffset+uiEntryOffset,4,(uint_8*)&uiFillByte))
			{
				m_error = ERR_FW_READ;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:RKA_SparseFile_Download-->get fill byte failed,chunk=%d",uiCurChunk);
				}
				delete []pBuffer;
				pBuffer = NULL;
				return false;
			}
			uiEntryOffset += 4;
			if (uiFillByte==0)
			{
				currentByte += uiChunkDataSize;
				uiLen = ( (uiChunkDataSize%SECTOR_SIZE==0) ? (uiChunkDataSize/SECTOR_SIZE) : (uiChunkDataSize/SECTOR_SIZE+1) );
				uiBegin += uiLen;
				curLoopCount = (uint_8)(currentByte*100/totalByte);
				if (curLoopCount>loopCount)
				{
					loopCount = curLoopCount;
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,DOWNLOADIMAGE_PROGRESS,totalByte,currentByte,emCallStep);
						if (emCallStep!=CALL_MIDDLE)
							emCallStep = CALL_MIDDLE;
					}
				}
			}
			else
			{
				while (uiChunkDataSize>0)
				{
					memset(pBuffer,0,uiBufferSize);
					if ( uiChunkDataSize<uiBufferSize )
					{
						uiWriteByte = uiChunkDataSize;
						uiLen = ( (uiWriteByte%SECTOR_SIZE==0) ? (uiWriteByte/SECTOR_SIZE) : (uiWriteByte/SECTOR_SIZE+1) );
					}
					else
					{
						uiWriteByte = uiBufferSize;
						uiLen = uiLBASector;
					}
					for (uint_32 wordIndex = 0; wordIndex < uiWriteByte / 4; wordIndex++)
					{
						*(uint_32 *)(pBuffer + wordIndex * 4) = uiFillByte;
					}

					iRet = m_pComm->RKU_WriteLBA(uiBegin,uiLen,pBuffer,byRWMethod);
					if( iRet!=ERR_SUCCESS )
					{
						m_error = ERR_COMM_RW;
						if (m_pLog)
						{
							m_pLog->Record("ERROR:RKA_SparseFile_Download-->RKU_WriteLBA failed,RetCode(%d),chunk=%d",iRet,uiCurChunk);
						}

						delete []pBuffer;
						pBuffer = NULL;
						return false;
					}
					uiBegin += uiLen;
					currentByte += uiWriteByte;
					uiChunkDataSize -= uiWriteByte;
					curLoopCount = (uint_8)(currentByte*100/totalByte);
					if (curLoopCount>loopCount)
					{
						loopCount = curLoopCount;
						if (m_callBackProc)
						{
							m_callBackProc(dwLayerID,DOWNLOADIMAGE_PROGRESS,totalByte,currentByte,emCallStep);
							if (emCallStep!=CALL_MIDDLE)
								emCallStep = CALL_MIDDLE;
						}
					}
				}
			}
			break;
		case CHUNK_TYPE_DONT_CARE:
			uiChunkDataSize = (uint_64)chunk.chunk_sz * header.blk_sz;
			currentByte += uiChunkDataSize;
			uiLen = ( (uiChunkDataSize%SECTOR_SIZE==0) ? (uiChunkDataSize/SECTOR_SIZE) : (uiChunkDataSize/SECTOR_SIZE+1) );
			uiBegin += uiLen;
			curLoopCount = (uint_8)(currentByte*100/totalByte);
			if (curLoopCount>loopCount)
			{
				loopCount = curLoopCount;
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,DOWNLOADIMAGE_PROGRESS,totalByte,currentByte,emCallStep);
					if (emCallStep!=CALL_MIDDLE)
						emCallStep = CALL_MIDDLE;
				}
			}
			break;
		case CHUNK_TYPE_CRC32:
			bRet = m_pImage->GetData(ulEntryStartOffset+uiEntryOffset,4,(uint_8*)&uiCrc);
			uiEntryOffset += 4;
			break;
		default:
			m_error = ERR_FW_READ;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:RKA_SparseFile_Download-->unsupported sparse chunk type=0x%x,chunk=%d", chunk.chunk_type, uiCurChunk);
			}
			delete []pBuffer;
			pBuffer = NULL;
			return false;
		}
	}
	delete []pBuffer;
	pBuffer = NULL;

	return true;
}

bool CRKAndroidDevice::RKA_File_Check(STRUCT_RKIMAGE_ITEM &entry,uint_8 &loopCount,uint_64 &currentByte,uint_64 totalByte,ENUM_CALL_STEP &emCallStep)
{
	uint_32 uiLBATransferSize=(LBA_TRANSFER_SIZE)*m_uiLBATimes;
	uint_32 uiLBASector = uiLBATransferSize/SECTOR_SIZE;
	int iRet;
	bool bRet;
	uint_32 uiBufferSize=uiLBATransferSize,uiFlashOffset;
	long long uifileBufferSize;
	long long ulEntryStartOffset;
	uint_32 dwLayerID;
	dwLayerID = LocationID;
	if (entry.file[50]=='H')
	{
		ulEntryStartOffset = *((uint_32 *)(&entry.file[51]));
		ulEntryStartOffset <<= 32;
		ulEntryStartOffset += entry.offset;
		ulEntryStartOffset += m_pImage->FWOffset;
	}
	else
	{
		ulEntryStartOffset = m_pImage->FWOffset;
		ulEntryStartOffset += entry.offset;
	}
	if (entry.file[55]=='H')
	{
		uifileBufferSize = *((uint_32 *)(&entry.file[56]));
		uifileBufferSize <<= 32;
		uifileBufferSize += entry.size;
	}
	else
		uifileBufferSize = entry.size;
	if ((!m_bHasEnv)&&(!GptFlag))
	{
		if (First4Access)
		{
			uiFlashOffset = entry.flash_offset + 0x2000;//offset 4m
		}
		else
			uiFlashOffset = entry.flash_offset;
	}
	else
		uiFlashOffset = entry.flash_offset;

	uint_8 byRWMethod=RWMETHOD_IMAGE;

	uint_8* pBufferFromFile=NULL;
	pBufferFromFile = new uint_8[uiBufferSize];
	if (!pBufferFromFile)
	{
		m_error = ERR_NEW_BUFFER;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_File_Check-->New memory failed");
		}
		return false;
	}
	uint_8* pBufferFromFlash=NULL;
	pBufferFromFlash = new uint_8[uiBufferSize];
	if (!pBufferFromFlash)
	{
		m_error = ERR_NEW_BUFFER;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_File_Check-->New memory failed");
		}
		delete []pBufferFromFile;
		return false;
	}
	
	bool bMisc=false,bUser=false;
	if (strcmp(entry.name,PARTNAME_MISC)==0)
	{
		bMisc = true;
	}
	if (strcmp(entry.name,PARTNAME_USER)==0)
	{
		bUser = true;
		if ((entry.name[PART_NAME-2]=='N')&&(entry.name[PART_NAME-1]=='C'))
		{//no check user
			currentByte += uifileBufferSize;
			delete []pBufferFromFile;
			delete []pBufferFromFlash;
			return true;
		}
	}
	if (m_pLog)
	{
		m_pLog->Record("INFO:Start to check %s,offset=0x%x,size=%lld",entry.name,entry.flash_offset,uifileBufferSize);
	}
	uint_32 uiBegin,uiLen,uiWriteByte;
	long long uiEntryOffset;
	uiBegin = uiFlashOffset;
	uiLen = 0;uiWriteByte = 0;uiEntryOffset=0;
	while ( uifileBufferSize>0 )
	{
		if ( uifileBufferSize<uiBufferSize )
		{
			uiWriteByte = uifileBufferSize;
			uiLen = ( (uiWriteByte%SECTOR_SIZE==0) ? (uiWriteByte/SECTOR_SIZE) : (uiWriteByte/SECTOR_SIZE+1) );
		}
		else
		{
			uiWriteByte = uiBufferSize;
			uiLen = uiLBASector;
		}

	
		memset(pBufferFromFile,0,uiBufferSize);
		memset(pBufferFromFlash,0,uiBufferSize);
		
		iRet = m_pComm->RKU_ReadLBA(uiBegin,uiLen,pBufferFromFlash,byRWMethod);
		if( iRet!=ERR_SUCCESS )
		{
			m_error = ERR_COMM_RW;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:RKA_File_Check-->RKU_ReadLBA failed,Read(%d),RetCode(%d)",uiEntryOffset,iRet);
			}
			delete []pBufferFromFile;
			delete []pBufferFromFlash;
			return false;
		}
		bRet = m_pImage->GetData(ulEntryStartOffset+uiEntryOffset,uiWriteByte,pBufferFromFile);
		if ( !bRet )
		{
			m_error = ERR_FW_READ;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:RKA_File_Check-->GetFileData failed");
			}
			delete []pBufferFromFile;
			delete []pBufferFromFlash;
			return false;
		}
		if (bUser)
		{
			if ((pBufferFromFile[0]==0xEB)&&(pBufferFromFile[1]==0x58)&&(pBufferFromFile[2]==0x90))
			{//fat user image		
				uint_8 *pDbr,*pCopyDbr;
				pDbr = pBufferFromFile;
				pCopyDbr = pBufferFromFile + SECTOR_SIZE*6;

				(*(uint_32 *)(pDbr+32)) = m_uiUserSectors;
				(*(uint_32 *)(pCopyDbr+32)) = m_uiUserSectors;
			}

			bUser = false;
		}
		
		if ((bMisc)&&(m_emMiscModifyFlag!=MISC_MODIFY_NONE))
		{
			if (uiLBATransferSize % 0x4000==0)
			{
				if ((uiEntryOffset+uiWriteByte)>0x4000)
				{
					if (m_emMiscModifyFlag==MISC_MODIFY_WIPE_ALL)
					{
						if (uiWriteByte>0x4000)
						{
							memcpy(pBufferFromFile+0x4040,Wipe_All,20);
						}
						else
							memcpy(pBufferFromFile+0x40,Wipe_All,20);
					}
					else if (m_emMiscModifyFlag==MISC_MODIFY_WIPE_DATA)
					{
						if (uiWriteByte>0x4000)
						{
							memcpy(pBufferFromFile+0x4040,Wipe_Data,21);
						}
						else
							memcpy(pBufferFromFile+0x40,Wipe_Data,21);
					}
					bMisc = false;
				}
			}
		}
	
		if ( memcmp(pBufferFromFile,pBufferFromFlash,uiWriteByte)!=0 )
		{
			m_error = ERR_FW_CHECK;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:RKA_File_Check-->Memcmp failed,Read(%d)",uiEntryOffset);
			}
			int iTestCount;
			uint_32 dwTotal=0,dwCurrent=0;
			ENUM_CALL_STEP emCallStep2=CALL_FIRST;
			do 
			{
				iTestCount = 3;
				while (iTestCount>0)
				{
					iRet = m_pComm->RKU_TestDeviceReady( &dwTotal,&dwCurrent );
					if ((iRet==ERR_SUCCESS)||(iRet==ERR_DEVICE_UNREADY))
					{
						break;
					}
					if (m_pLog)
					{
						m_pLog->Record("ERROR:RKA_File_Check-->Memcmp-->RKU_TestDeviceReady failed,Read(%d),RetCode(%d)",uiEntryOffset,iRet);
					}
					iTestCount--;
					sleep_s(1);
				}
				if (iTestCount<=0)
				{
					break;
				}
				if (iRet==ERR_SUCCESS)
				{
					if (emCallStep2==CALL_MIDDLE)
					{
						if (m_callBackProc)
						{
							emCallStep2 = CALL_LAST;
							m_callBackProc(dwLayerID,TAGBADBLOCK_PROGRESS,dwTotal,dwCurrent,emCallStep2);
						}
					}
					break;
				}
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,TAGBADBLOCK_PROGRESS,dwTotal,dwCurrent,emCallStep2);
					emCallStep2 = CALL_MIDDLE;
				}
				sleep_s(1);
			}while( iRet==ERR_DEVICE_UNREADY );
			if (m_pLog)
			{
				char szDateTime[100];
				string strFile;
				struct tm timeNow;
				timeNow = get_now_time();
				sprintf(szDateTime,"%02d-%02d-%02d",timeNow.tm_hour,timeNow.tm_min,timeNow.tm_sec);
				strFile = m_pLog->LogSavePath;
				strFile += szDateTime;
				strFile += "file.bin";
				m_pLog->SaveBuffer( strFile,pBufferFromFile,uiWriteByte );
				strFile = m_pLog->LogSavePath;
				strFile += szDateTime;
				strFile += "flash.bin";
				m_pLog->SaveBuffer( strFile,pBufferFromFlash,uiWriteByte );
			}
			delete []pBufferFromFile;
			delete []pBufferFromFlash;
			return false;
		}
		
		
		
		currentByte += uiWriteByte;
		uiEntryOffset += uiWriteByte;
		uifileBufferSize -= uiWriteByte;
		uiBegin += uiLen;
		
		uint_8 curLoopCount = (uint_8)(currentByte*100/totalByte);
		if (curLoopCount>loopCount)
		{
			loopCount = curLoopCount;
			if (m_callBackProc)
			{
				m_callBackProc(dwLayerID,CHECKIMAGE_PROGRESS,totalByte,currentByte,emCallStep);
				if (emCallStep!=CALL_MIDDLE)
					emCallStep = CALL_MIDDLE;
			}
		}
	}

	delete []pBufferFromFile;
	delete []pBufferFromFlash;
	return true;
}
bool CRKAndroidDevice::RKA_SparseFile_Check(STRUCT_RKIMAGE_ITEM &entry,uint_8 &loopCount,uint_64 &currentByte,uint_64 totalByte,ENUM_CALL_STEP &emCallStep)
{
	uint_32 uiLBATransferSize=(LBA_TRANSFER_SIZE)*m_uiLBATimes;
	uint_32 uiLBASector = uiLBATransferSize/SECTOR_SIZE;
	int iRet;
	bool bRet;
	uint_32 uiBufferSize=uiLBATransferSize,uiFlashOffset;
	uint_64 uifileBufferSize;
	uint_32 dwLayerID;
	uint_64 ulEntryStartOffset;
	sparse_header header;
	chunk_header chunk;
	dwLayerID = LocationID;
	if (entry.file[50]=='H')
	{
		ulEntryStartOffset = *((uint_32 *)(&entry.file[51]));
		ulEntryStartOffset <<= 32;
		ulEntryStartOffset += entry.offset;
		ulEntryStartOffset += m_pImage->FWOffset;
	}
	else
	{
		ulEntryStartOffset = m_pImage->FWOffset;
		ulEntryStartOffset += entry.offset;
	}
	if (entry.file[55]=='H')
	{
		uifileBufferSize = *((uint_32 *)(&entry.file[56]));
		uifileBufferSize <<= 32;
		uifileBufferSize += entry.size;
	}
	else
		uifileBufferSize = entry.size;
	if ((!m_bHasEnv)&&(!GptFlag))
	{
		if (First4Access)
		{
			uiFlashOffset = entry.flash_offset + 0x2000;//offset 4m
		}
		else
			uiFlashOffset = entry.flash_offset;
	}
	else
		uiFlashOffset = entry.flash_offset;
	//get sparse header
	bRet = m_pImage->GetData(ulEntryStartOffset,sizeof(header),(uint_8*)&header);
	if (!bRet)
	{
		m_error = ERR_FW_READ;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_SparseFile_Check-->get sparse header failed,partition=%s",entry.name);
		}
		return false;
	}
	uifileBufferSize = header.blk_sz * (uint_64)header.total_blks;
	if (m_pLog)
	{
		m_pLog->Record("INFO:Start to check %s,offset=0x%x,size=%lld",entry.name,entry.flash_offset,uifileBufferSize);
	}

	uint_8 byRWMethod=RWMETHOD_IMAGE;

	uint_8* pBufferFromFile=NULL;
	pBufferFromFile = new uint_8[uiBufferSize];
	if (!pBufferFromFile)
	{
		m_error = ERR_NEW_BUFFER;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_SparseFile_Check-->New memory failed");
		}
		return false;
	}
	uint_8* pBufferFromFlash=NULL;
	pBufferFromFlash = new uint_8[uiBufferSize];
	if (!pBufferFromFlash)
	{
		m_error = ERR_NEW_BUFFER;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_SparseFile_Check-->New memory failed");
		}
		delete []pBufferFromFile;
		return false;
	}

	uint_32 uiBegin,uiLen,uiWriteByte,uiCurChunk,uiFillByte,uiCrc;
	uint_64 uiEntryOffset,uiChunkDataSize;
	uint_8 curLoopCount;
	uiBegin = uiFlashOffset;
	uiCurChunk = 0;
	uiLen = 0;uiWriteByte = 0;uiEntryOffset=sizeof(header);

	while ( uiCurChunk<header.total_chunks)
	{
		bRet = m_pImage->GetData(ulEntryStartOffset+uiEntryOffset,sizeof(chunk),(uint_8*)&chunk);
		if (!bRet)
		{
			m_error = ERR_FW_READ;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:RKA_SparseFile_Check-->get sparse chunk failed");
			}
			delete []pBufferFromFile;
			delete []pBufferFromFlash;
			return false;
		}
		uiCurChunk++;
		uiEntryOffset += sizeof(chunk);
		switch (chunk.chunk_type)
		{
		case CHUNK_TYPE_RAW:
			uiChunkDataSize = chunk.total_sz - sizeof(chunk_header);
			while (uiChunkDataSize)
			{
				if ( uiChunkDataSize<uiBufferSize )
				{
					uiWriteByte = uiChunkDataSize;
					uiLen = ( (uiWriteByte%SECTOR_SIZE==0) ? (uiWriteByte/SECTOR_SIZE) : (uiWriteByte/SECTOR_SIZE+1) );
				}
				else
				{
					uiWriteByte = uiBufferSize;
					uiLen = uiLBASector;
				}
				memset(pBufferFromFile,0,uiBufferSize);
				memset(pBufferFromFlash,0,uiBufferSize);
				if (!m_pImage->GetData(ulEntryStartOffset+uiEntryOffset,uiWriteByte,pBufferFromFile))
				{
					m_error = ERR_FW_READ;
					if (m_pLog)
					{
						m_pLog->Record("ERROR:RKA_SparseFile_Check-->get chunk data failed,chunk=%d",uiCurChunk);
					}
					delete []pBufferFromFile;
					delete []pBufferFromFlash;
					return false;
				}
				uiEntryOffset += uiWriteByte;
				iRet = m_pComm->RKU_ReadLBA(uiBegin,uiLen,pBufferFromFlash,byRWMethod);
				if( iRet!=ERR_SUCCESS )
				{
					m_error = ERR_COMM_RW;
					if (m_pLog)
					{
						m_pLog->Record("ERROR:RKA_SparseFile_Check-->RKU_ReadLBA failed,RetCode(%d),chunk=%d",iRet,uiCurChunk);
					}
					delete []pBufferFromFile;
					delete []pBufferFromFlash;
					return false;
				}
				if ( memcmp(pBufferFromFile,pBufferFromFlash,uiWriteByte)!=0 )
				{
					m_error = ERR_FW_CHECK;
					if (m_pLog)
					{
						m_pLog->Record("ERROR:RKA_SparseFile_Check-->Memcmp failed,chunk=%d,type=%d",uiCurChunk,chunk.chunk_type);
					}
					if (m_pLog)
					{
						char szDateTime[100];
						string strFile;
						struct tm timeNow;

						timeNow = get_now_time();
						sprintf(szDateTime,"%02d-%02d-%02d",timeNow.tm_hour,timeNow.tm_min,timeNow.tm_sec);
						strFile = m_pLog->LogSavePath;
						strFile += szDateTime;
						strFile += "file.bin";
						m_pLog->SaveBuffer( strFile,pBufferFromFile,uiWriteByte );
						strFile = m_pLog->LogSavePath;
						strFile += szDateTime;
						strFile += "flash.bin";
						m_pLog->SaveBuffer( strFile,pBufferFromFlash,uiWriteByte );
					}
					delete []pBufferFromFile;
					delete []pBufferFromFlash;
					return false;
				}
				currentByte += uiWriteByte;
				uiChunkDataSize -= uiWriteByte;
				uiBegin += uiLen;

				curLoopCount = (uint_8)(currentByte*100/totalByte);
				if (curLoopCount>loopCount)
				{
					loopCount = curLoopCount;
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,CHECKIMAGE_PROGRESS,totalByte,currentByte,emCallStep);
						if (emCallStep!=CALL_MIDDLE)
							emCallStep = CALL_MIDDLE;
					}
				}
			}
			break;
		case CHUNK_TYPE_FILL:
			uiChunkDataSize = (uint_64)chunk.chunk_sz * header.blk_sz;
			if (!m_pImage->GetData(ulEntryStartOffset+uiEntryOffset,4,(uint_8*)&uiFillByte))
			{
				m_error = ERR_FW_READ;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:RKA_SparseFile_Check-->get fill byte failed,chunk=%d",uiCurChunk);
				}
				delete []pBufferFromFile;
				delete []pBufferFromFlash;
				return false;
			}
			uiEntryOffset += 4;
			if(uiFillByte == 0)
			{
				currentByte += uiChunkDataSize;
				uiLen = ( (uiChunkDataSize%SECTOR_SIZE==0) ? (uiChunkDataSize/SECTOR_SIZE) : (uiChunkDataSize/SECTOR_SIZE+1) );
				uiBegin += uiLen;
				curLoopCount = (uint_8)(currentByte*100/totalByte);
				if (curLoopCount>loopCount)
				{
					loopCount = curLoopCount;
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,CHECKIMAGE_PROGRESS,totalByte,currentByte,emCallStep);
						if (emCallStep!=CALL_MIDDLE)
							emCallStep = CALL_MIDDLE;
					}
				}
			}
			else
			{
				while (uiChunkDataSize)
				{
					if ( uiChunkDataSize<uiBufferSize )
					{
						uiWriteByte = uiChunkDataSize;
						uiLen = ( (uiWriteByte%SECTOR_SIZE==0) ? (uiWriteByte/SECTOR_SIZE) : (uiWriteByte/SECTOR_SIZE+1) );
					}
					else
					{
						uiWriteByte = uiBufferSize;
						uiLen = uiLBASector;
					}
					memset(pBufferFromFile,0,uiBufferSize);
					memset(pBufferFromFlash,0,uiBufferSize);
					for (uint_32 wordIndex = 0; wordIndex < uiWriteByte / 4; wordIndex++)
					{
						*(uint_32 *)(pBufferFromFile + wordIndex * 4) = uiFillByte;
					}
					iRet = m_pComm->RKU_ReadLBA(uiBegin,uiLen,pBufferFromFlash,byRWMethod);
					if( iRet!=ERR_SUCCESS )
					{
						m_error = ERR_COMM_RW;
						if (m_pLog)
						{
							m_pLog->Record("ERROR:RKA_SparseFile_Check-->RKU_ReadLBA failed,RetCode(%d),chunk=%d",iRet,uiCurChunk);
						}
						delete []pBufferFromFile;
						delete []pBufferFromFlash;
						return false;
					}
					if ( memcmp(pBufferFromFile,pBufferFromFlash,uiWriteByte)!=0 )
					{
						m_error = ERR_FW_CHECK;
						if (m_pLog)
						{
							m_pLog->Record("ERROR:RKA_SparseFile_Check-->Memcmp failed,chunk=%d,type=%d",uiCurChunk,chunk.chunk_type);
						}
						if (m_pLog)
						{
							char szDateTime[100];
							string strFile;
							struct tm timeNow;

							timeNow = get_now_time();
							sprintf(szDateTime,"%02d-%02d-%02d",timeNow.tm_hour,timeNow.tm_min,timeNow.tm_sec);
							strFile = m_pLog->LogSavePath;
							strFile += szDateTime;
							strFile += "file.bin";
							m_pLog->SaveBuffer( strFile,pBufferFromFile,uiWriteByte );
							strFile = m_pLog->LogSavePath;
							strFile += szDateTime;
							strFile += "flash.bin";
							m_pLog->SaveBuffer( strFile,pBufferFromFlash,uiWriteByte );
						}
						delete []pBufferFromFile;
						delete []pBufferFromFlash;
						return false;
					}
					currentByte += uiWriteByte;
					uiChunkDataSize -= uiWriteByte;
					uiBegin += uiLen;

					curLoopCount = (uint_8)(currentByte*100/totalByte);
					if (curLoopCount>loopCount)
					{
						loopCount = curLoopCount;
						if (m_callBackProc)
						{
							m_callBackProc(dwLayerID,CHECKIMAGE_PROGRESS,totalByte,currentByte,emCallStep);
							if (emCallStep!=CALL_MIDDLE)
								emCallStep = CALL_MIDDLE;
						}
					}
				}
			}
			break;
		case CHUNK_TYPE_DONT_CARE:
			uiChunkDataSize = (uint_64)chunk.chunk_sz * header.blk_sz;
			currentByte += uiChunkDataSize;
			uiLen = ( (uiChunkDataSize%SECTOR_SIZE==0) ? (uiChunkDataSize/SECTOR_SIZE) : (uiChunkDataSize/SECTOR_SIZE+1) );
			uiBegin += uiLen;
			curLoopCount = (uint_8)(currentByte*100/totalByte);
			if (curLoopCount>loopCount)
			{
				loopCount = curLoopCount;
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,CHECKIMAGE_PROGRESS,totalByte,currentByte,emCallStep);
					if (emCallStep!=CALL_MIDDLE)
						emCallStep = CALL_MIDDLE;
				}
			}
			break;
		case CHUNK_TYPE_CRC32:
			bRet = m_pImage->GetData(ulEntryStartOffset+uiEntryOffset,4,(uint_8*)&uiCrc);
			uiEntryOffset += 4;
			break;
		default:
			m_error = ERR_FW_READ;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:RKA_SparseFile_Check-->unsupported sparse chunk type=0x%x,chunk=%d", chunk.chunk_type, uiCurChunk);
			}
			delete []pBufferFromFile;
			delete []pBufferFromFlash;
			return false;
		}
	}

	delete []pBufferFromFile;
	delete []pBufferFromFlash;
	return true;
}
bool CRKAndroidDevice::GetReservedBlockByte()
{
	int iRet;
	uint_8 bData[SECTOR_SIZE*4+SPARE_SIZE*4];
	iRet = m_pComm->RKU_ReadSector(m_idBlockOffset[0]*m_flashInfo.uiSectorPerBlock,4,bData);
	if(ERR_SUCCESS!=iRet)
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:GetReservedBlockByte-->RKU_ReadSector failed,RetCode(%d)",iRet);
		}
		return false;
	}
	PRKANDROID_IDB_SEC1 pSec1=(PRKANDROID_IDB_SEC1)(bData+528);
	m_uiResevedBlockSize = pSec1->usSysReservedBlock*m_flashInfo.usBlockSize*1024;
	return true;
}
bool CRKAndroidDevice::RKA_Param_Download(STRUCT_RKIMAGE_ITEM &entry,uint_8 &loopCount,uint_64 &currentByte,uint_64 totalByte,ENUM_CALL_STEP &emCallStep)
{
	uint_32 uiLBATransferSize=(LBA_TRANSFER_SIZE)*m_uiLBATimes;
	uint_32 uiLBASector = uiLBATransferSize/SECTOR_SIZE,uiFlashOffset;
	int  iRet,i;
	uint_32 dwLayerID;
	uint_8 buf[8*SECTOR_SIZE];
	uint_8 byRWMethod=RWMETHOD_IMAGE;

	dwLayerID = LocationID;
	
	uint_32 uiTransfer;
	uint_32 uiStepSec=entry.part_size/8;
	
	uint_32 uiLen,uiWriteByte,uiFileSize;
	uint_32 uiBegin;
	if (First4Access)
	{
		uiFlashOffset = entry.flash_offset + 0x2000;//offset 4m
	}
	else
		uiFlashOffset = entry.flash_offset;
	for ( i=0;i<8;i++ )
	{
		uiFileSize = m_uiParamFileSize;

		uiBegin = uiFlashOffset+uiStepSec*i;
		uiLen = 0;
		uiWriteByte = 0;
		uiTransfer = 0;
		while (uiFileSize>0)
		{
			if ( uiFileSize<uiLBATransferSize )
			{
				uiWriteByte = uiFileSize;
				uiLen = ( (uiWriteByte%512==0) ? (uiWriteByte/512) : (uiWriteByte/512+1) );
			}
			else
			{
				uiWriteByte = uiLBATransferSize;
				uiLen = uiLBASector;
			}
			iRet = m_pComm->RKU_WriteLBA(uiBegin,uiLen,m_paramBuffer+uiTransfer,byRWMethod);
			if( iRet!=ERR_SUCCESS )
			{
				m_error = ERR_COMM_RW;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:RKA_Param_Download-->RKU_WriteLBA failed,Written(%d),RetCode(%d)",uiTransfer,iRet);
				}
				
				return false;
			}
			
			uiTransfer += uiWriteByte;
			currentByte += uiWriteByte;
			uiBegin += uiLen;
			uiFileSize -= uiWriteByte;
			uint_8 curLoopCount = (uint_8)(currentByte*100/totalByte);
			if (curLoopCount > loopCount) {
				loopCount = curLoopCount;
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,DOWNLOADIMAGE_PROGRESS,totalByte,currentByte,emCallStep);
					if (emCallStep != CALL_MIDDLE)
						emCallStep = CALL_MIDDLE;
				}
			}
		}
	}
	iRet = m_pComm->RKU_ReadLBA(0,4,buf,byRWMethod);
	if (iRet==ERR_SUCCESS)
	{
		legacy_mbr *mbr = (legacy_mbr *)(buf);
		if (mbr->signature == MSDOS_MBR_SIGNATURE)
		{
			memset(buf,0,sizeof(buf));
			m_pComm->RKU_WriteLBA(0,8,buf,byRWMethod);//erase gpt partition
		}
	}
	return true;
}
bool CRKAndroidDevice::RKA_Param_Check(STRUCT_RKIMAGE_ITEM &entry,uint_8 &loopCount,uint_64 &currentByte,uint_64 totalByte,ENUM_CALL_STEP &emCallStep)
{
	uint_32 uiLBATransferSize=(LBA_TRANSFER_SIZE)*m_uiLBATimes;
	uint_32 uiLBASector = uiLBATransferSize/SECTOR_SIZE;
	int iRet,i;
	uint_32 uiReadBufferSize=uiLBATransferSize,uiFlashOffset;
	uint_32 dwLayerID = LocationID;
	uint_8 byRWMethod=RWMETHOD_IMAGE;
	uint_8* pRead=NULL;
	if (First4Access)
	{
		uiFlashOffset = entry.flash_offset + 0x2000;//offset 4m
	}
	else
		uiFlashOffset = entry.flash_offset;
	pRead = new uint_8[uiReadBufferSize];
	if (!pRead)
	{
		m_error = ERR_NEW_BUFFER;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_Param_Check-->New ReadBuffer failed");
		}
		return false;
	}
	
	uint_32 uiTransfer;
	uint_32 uiStepSec=entry.part_size/8;
	
	uint_32 uiLen,uiWriteByte,uiFileSize;
	uint_32 uiBegin;
	for ( i=0;i<8;i++ )
	{
		uiFileSize = m_uiParamFileSize;
		uiBegin = uiFlashOffset+uiStepSec*i;
		uiLen = 0;
		uiWriteByte = 0;
		uiTransfer = 0;
		while (uiFileSize>0)
		{
			memset(pRead,0,uiReadBufferSize);
			if ( uiFileSize<uiLBATransferSize )
			{
				uiWriteByte = uiFileSize;
				uiLen = ( (uiWriteByte%512==0) ? (uiWriteByte/512) : (uiWriteByte/512+1) );
			}
			else
			{
				uiWriteByte = uiLBATransferSize;
				uiLen = uiLBASector;
			}
			iRet = m_pComm->RKU_ReadLBA(uiBegin,uiLen,pRead,byRWMethod);
			if( iRet!=ERR_SUCCESS )
			{
				m_error = ERR_COMM_RW;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:RKA_Param_Check-->RKU_ReadLBA failed,Read(%d),RetCode(%d)",uiTransfer,iRet);
				}
				delete []pRead;
				return false;
			}
			if ( memcmp(pRead,m_paramBuffer+uiTransfer,uiWriteByte)!=0 )
			{
				m_error = ERR_FW_CHECK;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:RKA_Param_Check-->Memcmp failed,Read(%d)",uiTransfer);
				}
				int iTestCount;
				uint_32 dwTotal=0,dwCurrent=0;
				ENUM_CALL_STEP emCallStep2=CALL_FIRST;
				do 
				{
					iTestCount = 3;
					while (iTestCount>0)
					{
						iRet = m_pComm->RKU_TestDeviceReady( &dwTotal,&dwCurrent );
						if ((iRet==ERR_SUCCESS)||(iRet==ERR_DEVICE_UNREADY))
						{
							break;
						}
						if (m_pLog)
						{
							m_pLog->Record("ERROR:RKA_Param_Check-->Memcmp-->RKU_TestDeviceReady failed,Read(%d),RetCode(%d)",uiTransfer,iRet);
						}
						iTestCount--;
						sleep_s(1);
					}
					if (iTestCount<=0)
					{
						break;
					}
					if (iRet==ERR_SUCCESS)
					{
						if (emCallStep2==CALL_MIDDLE)
						{
							if (m_callBackProc)
							{
								emCallStep2 = CALL_LAST;
								m_callBackProc(dwLayerID,TAGBADBLOCK_PROGRESS,dwTotal,dwCurrent,emCallStep2);
							}
						}
						break;
					}
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,TAGBADBLOCK_PROGRESS,dwTotal,dwCurrent,emCallStep2);
						emCallStep2 = CALL_MIDDLE;
					}
					sleep_s(1);
				}while( iRet==ERR_DEVICE_UNREADY );
				if (m_pLog)
				{
					char szDateTime[100];
					string strFile;
					struct tm timeNow;

					timeNow = get_now_time();
					sprintf(szDateTime,"%02d-%02d-%02d",timeNow.tm_hour+1,timeNow.tm_min+1,timeNow.tm_sec+1);
					strFile = m_pLog->LogSavePath;
					strFile += szDateTime;
					strFile += "file.bin";
					m_pLog->SaveBuffer( strFile,m_paramBuffer+uiTransfer,uiWriteByte );
					strFile = m_pLog->LogSavePath;
					strFile += szDateTime;
					strFile += "flash.bin";
					m_pLog->SaveBuffer( strFile,pRead,uiWriteByte );
				}
				
				delete []pRead;
				return false;
				
			}
			uiTransfer += uiWriteByte;
			currentByte += uiWriteByte;
			uiBegin += uiLen;
			uiFileSize -= uiWriteByte;
			
			uint_8 curLoopCount = (uint_8)(currentByte*100/totalByte);
			if (curLoopCount>loopCount)
			{
				loopCount = curLoopCount;
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,CHECKIMAGE_PROGRESS,totalByte,currentByte,emCallStep);
					if (emCallStep!=CALL_MIDDLE)
						emCallStep = CALL_MIDDLE;
				}
			}
		}
	}
	
	delete []pRead;
	return true;
}
bool CRKAndroidDevice::RKA_Gpt_Download(STRUCT_RKIMAGE_ITEM &entry,uint_8 &loopCount,uint_64 &currentByte,uint_64 totalByte,ENUM_CALL_STEP &emCallStep)
{
	int  iRet,nGptSector,nLBsector,nGptEntryLB,nMasterGptSector;
	bool bRet,bGrowAlign;
	uint_32 dwLayerID = LocationID;
	uint_32 i,nPartOffset;
	PARAM_ITEM_VECTOR vecItems;
	CONFIG_ITEM_VECTOR vecUuids;
	uint_8 *backup_gpt;

	if (m_flashInfo.uiLogicalBlockSize != 1) {
		nLBsector = m_flashInfo.uiLogicalBlockSize;
		nGptEntryLB = CALC_UNIT(128, (nLBsector * 4));
		nGptSector = (3 + nGptEntryLB * 2) * nLBsector;
		nMasterGptSector = (2 + nGptEntryLB) * nLBsector;
	}
	else {
		nLBsector = 1;
		nGptEntryLB = 32;
		nGptSector = 67;
		nMasterGptSector = 34;
	}

	if (m_gptBuffer) {
		delete []m_gptBuffer;
		m_gptBuffer = NULL;
	}


	m_gptBuffer = new uint_8[nGptSector * SECTOR_SIZE];
	if (!m_gptBuffer)
	{
		m_error = ERR_NEW_BUFFER;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_Gpt_Download-->new memory failed,err=%d)",errno);
		}
		return false;
	}
	
	memset(m_gptBuffer,0,nGptSector * SECTOR_SIZE);
	*(uint_32 *)(m_paramBuffer+(entry.size-4)) = 0;//clear crc to fix parse uuid bug
	bRet = parse_parameter((char *)(m_paramBuffer+8),vecItems);
	if (!bRet)
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_Gpt_Download-->parse_parameter failed)");
		}
		return false;
	}
	get_uuid_from_parameter((char *)(m_paramBuffer+8),vecUuids);
	//check partition if overlapped
	nPartOffset = 0;
	for ( i = 0; i < vecItems.size(); i++)
	{
		if (nPartOffset > vecItems[i].uiItemOffset)
			break;
		nPartOffset = vecItems[i].uiItemOffset + vecItems[i].uiItemSize;
	}
	if (i < vecItems.size()) {
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_Gpt_Download->Fail to check parameter, %s was overlapped",  vecItems[i].szItemName);
		}
		return false;
	}

	backup_gpt = m_gptBuffer+nMasterGptSector*SECTOR_SIZE;
	bRet = get_grow_align_flag((char *)(m_paramBuffer+8),bGrowAlign);
	if (bRet)
		create_gpt_buffer(m_gptBuffer,vecItems,vecUuids,m_flashInfo.uiFlashSizeSector,nLBsector,&bGrowAlign);
	else
		create_gpt_buffer(m_gptBuffer,vecItems,vecUuids,m_flashInfo.uiFlashSizeSector,nLBsector);
	
	memcpy(backup_gpt, m_gptBuffer + 2 * nLBsector * SECTOR_SIZE, nGptEntryLB * nLBsector * SECTOR_SIZE);
	memcpy(backup_gpt + nGptEntryLB * nLBsector * SECTOR_SIZE, m_gptBuffer + nLBsector * SECTOR_SIZE, nLBsector * SECTOR_SIZE);
	prepare_gpt_backup(m_gptBuffer, backup_gpt, nLBsector);

	iRet = m_pComm->RKU_WriteLBA(0,nMasterGptSector,m_gptBuffer);
	if (iRet!=ERR_SUCCESS)
	{
		m_error = ERR_COMM_RW;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_Gpt_Download-->write gpt master failed,RetCode(%d)",iRet);
		}
		return false;
	}
	iRet = m_pComm->RKU_WriteLBA(m_flashInfo.uiFlashSizeSector-(nGptSector - nMasterGptSector), (nGptSector - nMasterGptSector),backup_gpt);
	if (iRet!=ERR_SUCCESS)
	{
		m_error = ERR_COMM_RW;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_Gpt_Download-->write gpt backup failed,RetCode(%d)",iRet);
		}
		return false;
	}
	currentByte += (nGptSector * SECTOR_SIZE);
	uint_8 curLoopCount = (uint_8)(currentByte*100/totalByte);
	if (curLoopCount > loopCount) {
		loopCount = curLoopCount;
		if (m_callBackProc)
		{
			m_callBackProc(dwLayerID,DOWNLOADIMAGE_PROGRESS,totalByte,currentByte,emCallStep);
			if (emCallStep != CALL_MIDDLE)
				emCallStep = CALL_MIDDLE;
		}
	}

	return true;
}
bool CRKAndroidDevice::RKA_Gpt_Check(STRUCT_RKIMAGE_ITEM &,uint_8 &loopCount,uint_64 &currentByte,uint_64 totalByte,ENUM_CALL_STEP &emCallStep)
{
	int iRet;
	uint_32 dwLayerID = LocationID;
	uint_32 nGptEntryLB, nGptSector, nMasterGptSector, nLBsector;
	uint_8* pRead=NULL;

	nLBsector = m_flashInfo.uiLogicalBlockSize;
	nGptEntryLB = CALC_UNIT(128, (nLBsector * 4));
	nGptSector = (3 + nGptEntryLB * 2) * nLBsector;
	nMasterGptSector = (2 + nGptEntryLB) * nLBsector;

	pRead = new uint_8[nMasterGptSector * SECTOR_SIZE];
	if (!pRead)
	{
		m_error = ERR_NEW_BUFFER;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_Gpt_Check-->New ReadBuffer failed,err=%d",errno);
		}
		return false;
	}
	
	iRet = m_pComm->RKU_ReadLBA(0,nMasterGptSector,pRead);
	if (iRet!=ERR_SUCCESS)
	{
		m_error = ERR_COMM_RW;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_Gpt_Download-->read gpt master failed,RetCode(%d)",iRet);
		}
		delete []pRead;
		return false;
	}
	if (memcmp(m_gptBuffer,pRead,nMasterGptSector*SECTOR_SIZE)!=0)
	{
		m_error = ERR_FW_CHECK;
		if (m_pLog)
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:RKA_Gpt_Download-->compare gpt master failed");
			}
			char szDateTime[100];
			string strFile;
			struct tm timeNow;

			timeNow = get_now_time();
			sprintf(szDateTime,"%02d-%02d-%02d",timeNow.tm_hour+1,timeNow.tm_min+1,timeNow.tm_sec+1);
			strFile = m_pLog->LogSavePath;
			strFile += szDateTime;
			strFile += "file.bin";
			m_pLog->SaveBuffer( strFile,m_gptBuffer,nMasterGptSector*SECTOR_SIZE );
			strFile = m_pLog->LogSavePath;
			strFile += szDateTime;
			strFile += "flash.bin";
			m_pLog->SaveBuffer( strFile,pRead,nMasterGptSector*SECTOR_SIZE );
		}
		delete []pRead;
		return false;
	}
	iRet = m_pComm->RKU_ReadLBA(m_flashInfo.uiFlashSizeSector-(nGptSector - nMasterGptSector),(nGptSector - nMasterGptSector),pRead);
	if (iRet!=ERR_SUCCESS)
	{
		m_error = ERR_COMM_RW;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:RKA_Gpt_Download-->read gpt backup failed,RetCode(%d)",iRet);
		}
		delete []pRead;
		return false;
	}
	if (memcmp(m_gptBuffer+nMasterGptSector*SECTOR_SIZE,pRead,(nGptSector - nMasterGptSector)*SECTOR_SIZE)!=0)
	{
		if (m_pLog)
		{
			// Some EgoPro Loader revisions do not return a stable backup GPT
			// region at the end of eMMC. The primary GPT was verified above;
			// continue so the actual firmware partitions can be verified.
			m_pLog->Record("WARN:RKA_Gpt_Check-->compare gpt backup failed; primary GPT verified, continuing");
		}
	}
	currentByte += (nGptSector * SECTOR_SIZE);
	uint_8 curLoopCount = (uint_8)(currentByte*100/totalByte);
	if (curLoopCount>loopCount)
	{
		loopCount = curLoopCount;
		if (m_callBackProc)
		{
			m_callBackProc(dwLayerID,CHECKIMAGE_PROGRESS,totalByte,currentByte,emCallStep);
			if (emCallStep!=CALL_MIDDLE)
				emCallStep = CALL_MIDDLE;
		}
	}
	
	delete []pRead;
	return true;
}


bool CRKAndroidDevice::ParsePartitionInfo(string &strPartInfo,string &strName,uint_32 &uiOffset,uint_32 &uiLen)
{
	string::size_type pos,prevPos;
	string strOffset,strLen;
	int iCount;
	prevPos = pos = 0;
	if (strPartInfo.size()<=0)
	{
		return false;
	}
	pos = strPartInfo.find('@');
	if (pos==string::npos)
	{
		return false;
	}
	strLen = strPartInfo.substr(prevPos,pos-prevPos);
	strLen.erase(0,strLen.find_first_not_of(" "));
	strLen.erase(strLen.find_last_not_of(" ")+1);
	if (strchr(strLen.c_str(),'-'))
	{
		uiLen = 0xFFFFFFFF;
	}
	else
	{
		iCount = sscanf(strLen.c_str(),"0x%x",&uiLen);
		if (iCount!=1)
		{
			return false;
		}
	}
	
	prevPos = pos +1;
	pos = strPartInfo.find('(',prevPos);
	if (pos==string::npos)
	{
		return false;
	}
	strOffset = strPartInfo.substr(prevPos,pos-prevPos);
	strOffset.erase(0,strOffset.find_first_not_of(" "));
	strOffset.erase(strOffset.find_last_not_of(" ")+1);
	iCount = sscanf(strOffset.c_str(),"0x%x",&uiOffset);
	if (iCount!=1)
	{
		return false;
	}
	
	prevPos = pos +1;
	pos = strPartInfo.find(')',prevPos);
	if (pos==string::npos)
	{
		return false;
	}
	strName = strPartInfo.substr(prevPos,pos-prevPos);
	strName.erase(0,strName.find_first_not_of(" "));
	strName.erase(strName.find_last_not_of(" ")+1);
	
	return true;
}
bool CRKAndroidDevice::EraseSystemPart(uint_32 dwLayerID)
{
	int iTestCount, iRet = ERR_FAILED;
	uint_32 dwTotal=0,dwCurrent=0;
	ENUM_CALL_STEP emCallStep=CALL_FIRST;
	do 
	{
		iTestCount = 3;
		while (iTestCount>0)
		{
			iRet = m_pComm->RKU_TestDeviceReady( &dwTotal,&dwCurrent,TU_ERASESYSTEM_SUBCODE);
			if ((iRet==ERR_SUCCESS)||(iRet==ERR_DEVICE_UNREADY))
			{
				break;
			}
			if (m_pLog)
			{
				m_pLog->Record("ERROR:EraseSystemPart-->RKU_TestDeviceReady failed,RetCode(%d)",iRet);
			}
			iTestCount--;
			sleep_s(1);
		}
		if (iTestCount<=0)
		{
			return false;
		}
		if (iRet==ERR_SUCCESS)
		{
			if (emCallStep==CALL_MIDDLE)
			{
				if (m_callBackProc)
				{
					emCallStep = CALL_LAST;
					m_callBackProc(dwLayerID,ERASESYSTEM_PROGRESS,dwTotal,dwCurrent,emCallStep);
				}
			}
			break;
		}
		if (m_callBackProc)
		{
			m_callBackProc(dwLayerID,ERASESYSTEM_PROGRESS,dwTotal,dwCurrent,emCallStep);
			emCallStep = CALL_MIDDLE;
		}
		sleep_s(1);
	}while( iRet==ERR_DEVICE_UNREADY );
	return true;
}
bool CRKAndroidDevice::EraseUserDataPart(uint_32 dwLayerID)
{
	int iTestCount, iRet = ERR_FAILED;
	uint_32 dwTotal=0,dwCurrent=0;
	ENUM_CALL_STEP emCallStep=CALL_FIRST;
	do 
	{
		iTestCount = 3;
		while (iTestCount>0)
		{
			iRet = m_pComm->RKU_TestDeviceReady( &dwTotal,&dwCurrent,TU_ERASEUSERDATA_SUBCODE);
			if ((iRet==ERR_SUCCESS)||(iRet==ERR_DEVICE_UNREADY))
			{
				break;
			}
			if (m_pLog)
			{
				m_pLog->Record("ERROR:EraseUserDataPart-->RKU_TestDeviceReady failed,RetCode(%d)",iRet);
			}
			iTestCount--;
			sleep_s(1);
		}
		if (iTestCount<=0)
		{
			return false;
		}
		if (iRet==ERR_SUCCESS)
		{
			if (emCallStep==CALL_MIDDLE)
			{
				if (m_callBackProc)
				{
					emCallStep = CALL_LAST;
					m_callBackProc(dwLayerID,ERASEUSERDATA_PROGRESS,dwTotal,dwCurrent,emCallStep);
				}
			}
			break;
		}
		if (m_callBackProc)
		{
			m_callBackProc(dwLayerID,ERASEUSERDATA_PROGRESS,dwTotal,dwCurrent,emCallStep);
			emCallStep = CALL_MIDDLE;
		}
		sleep_s(1);
	}while( iRet==ERR_DEVICE_UNREADY );
	return true;
}

bool CRKAndroidDevice::MakeParamFileBuffer(STRUCT_RKIMAGE_ITEM &entry)
{
	bool bRet;
	uint_32 uiFileBufferSize;
	uint_32 dwFWOffset;
	
	dwFWOffset = m_pImage->FWOffset;
	uiFileBufferSize = 2*entry.size;
	m_uiParamFileSize = entry.size;
	uint_8* pBuffer=NULL;
	pBuffer = new uint_8[uiFileBufferSize];
	if (!pBuffer)
	{
		m_error = ERR_NEW_BUFFER;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:MakeParamFileBuffer-->New memory failed");
		}
		return false;
	}
	memset(pBuffer,0,uiFileBufferSize);
	bRet = m_pImage->GetData(dwFWOffset+entry.offset,entry.size,pBuffer);
	if (!bRet)
	{
		m_error = ERR_FW_READ;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:MakeParamFileBuffer-->GetFileData failed");
		}
		delete[] pBuffer;
		pBuffer = NULL;
		return false;
	}
	
	uint_32 uiParamSec;
	if (m_uiParamFileSize%512==0)
	{
		uiParamSec = m_uiParamFileSize/512;
	}
	else
		uiParamSec = (m_uiParamFileSize+512)/512;

	if (m_paramBuffer)
	{
		delete []m_paramBuffer;
		m_paramBuffer = NULL;
	}
	m_paramBuffer = new uint_8[uiParamSec*512];
	if (!m_paramBuffer)
	{
		m_error = ERR_NEW_BUFFER;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:MakeParamFileBuffer-->new memory failed");
		}
		delete []pBuffer;
		return false;
	}
	memset(m_paramBuffer,0,uiParamSec*512);
	memcpy(m_paramBuffer,pBuffer,m_uiParamFileSize);
	delete []pBuffer;
	return true;
}
bool CRKAndroidDevice::GetParameterPartOffset(STRUCT_RKIMAGE_ITEM &paramItem)
{
	uint_8* pParamBuf=NULL;
	pParamBuf = new uint_8[paramItem.size-12+1];
	if (!pParamBuf)
	{
		return false;
	}
	memset(pParamBuf,0,paramItem.size-12+1);
	bool bRet;
	bRet = m_pImage->GetData(m_pImage->FWOffset+paramItem.offset+8,paramItem.size-12,pParamBuf);
	if (!bRet)
	{
		delete []pParamBuf;
		return false;
	}
	string strParamFile = (char *)pParamBuf;
	stringstream paramStream(strParamFile);
	delete []pParamBuf;

	string strLine,strParam;
	string::size_type line_size,pos,posColon,posParam;
	uint_32 uiParamOffset;
	while (!paramStream.eof())
	{
		getline(paramStream,strLine);
		line_size = strLine.size();
		if (line_size<=0)
		{
			continue;
		}
		if (strLine[line_size-1]=='\r')
		{
			strLine = strLine.substr(0,line_size-1);
		}
		if (strLine.size()<=0)
		{
			continue;
		}
		if (strLine[0]=='#')
		{
			continue;
		}
		pos = strLine.find("mtdparts");
		if (pos==string::npos)
		{
			continue;
		}
		posColon = strLine.find(':',pos);
		if (posColon==string::npos)
		{
			continue;
		}
		posParam = strLine.find("mtdpart.addr=");
		if (posParam==string::npos)
		{
			uiParamOffset = 0;
		}
		else
		{
			strParam = strLine.substr(posParam+13);
			strParam.erase(0,strParam.find_first_not_of(" "));
			posParam = strParam.find(' ');
			if (posParam!=string::npos)
			{
				strParam.erase(posParam);
			}
			uiParamOffset = strtoul(strParam.c_str(),NULL,0);
		}
		paramItem.part_size -= uiParamOffset;
		paramItem.flash_offset = uiParamOffset;
		return true;
	}
	return false;
}
bool CRKAndroidDevice::CheckParamPartSize(uint_8* pItemBuffer,int iParamPos,int iItemCount)
{
	uint_32 uiParamPartSize;
	STRUCT_RKIMAGE_ITEM *pItem;
	int i;
	uiParamPartSize = 0xFFFFFFFF;
	for (i=0;i<iItemCount;i++)
	{
		pItem = (STRUCT_RKIMAGE_ITEM *)(pItemBuffer + i*sizeof(STRUCT_RKIMAGE_ITEM));
		if (i!=iParamPos)
		{
			if (pItem->flash_offset<uiParamPartSize)
			{
				uiParamPartSize = pItem->flash_offset;
			}
		}
	}
	pItem = (STRUCT_RKIMAGE_ITEM *)(pItemBuffer + iParamPos*sizeof(STRUCT_RKIMAGE_ITEM));
	if (!GetParameterPartOffset(*pItem))
		return false;
	if (m_uiParamFileSize>pItem->part_size/8*512)
	{
		return false;
	}

	return true;
}
bool CRKAndroidDevice::IsExistSector3Crc(PRKANDROID_IDB_SEC2 pSec)
{
	if (!pSec)
	{
		return false;
	}
	
	if (strcmp(pSec->szCrcTag,"CRC")==0)
	{
		m_bExistSector3Crc = true;
		m_usSector3Crc = pSec->usSec3Crc;
	}
	return true;
}
bool CRKAndroidDevice::CheckIDBData(uint_8* pIDBData,bool bCheckBootCode)
{
	RKANDROID_IDB_SEC0 *pSec0 = (RKANDROID_IDB_SEC0 *)(pIDBData);
	RKANDROID_IDB_SEC1 *pSec1 = (RKANDROID_IDB_SEC1 *)(pIDBData + SECTOR_SIZE);
	RKANDROID_IDB_SEC2 *pSec2 = (RKANDROID_IDB_SEC2 *)(pIDBData + SECTOR_SIZE*2);
	RKANDROID_IDB_SEC3 *pSec3 = (RKANDROID_IDB_SEC3 *)(pIDBData + SECTOR_SIZE*3);
	uint_16 usNewCrc;
	
	if (strcmp(pSec2->szVcTag,"VC")==0)
	{
		usNewCrc = CRC_16((uint_8*)pSec0,SECTOR_SIZE);
		if (usNewCrc!=pSec2->usSec0Crc)
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:CheckIDBData-->Check sector0 data failed !");
			}
			return false;
		}
		usNewCrc = CRC_16((uint_8*)pSec1,SECTOR_SIZE);
		if (usNewCrc!=pSec2->usSec1Crc)
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:CheckIDBData-->Check sector1 data failed !");
			}
			return false;
		}
		
		if (bCheckBootCode)
		{
			uint_32 dwNewCrc;
			dwNewCrc = CRC_32((uint_8*)(pIDBData+SECTOR_SIZE*4),pSec0->usBootCodeSize*SECTOR_SIZE);
			if (dwNewCrc!=pSec2->uiBootCodeCrc)
			{
				if (m_pLog)
				{
					m_pLog->Record("ERROR:CheckIDBData-->Check Bootcode data failed !");
				}
				return false;
			}
		}	
	}
	else
	{
		if ((pSec0->usBootCode1Offset!=4)||(pSec0->usBootCode2Offset!=4))
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:CheckIDBData-->Boot Offset is wrong !");
			}
			return false;
		}
	}
	
	if (strcmp(pSec2->szCrcTag,"CRC")==0)
	{
		
		usNewCrc = CRC_16((uint_8*)pSec3,SECTOR_SIZE);
		if (usNewCrc!=pSec2->usSec3Crc)
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:CheckIDBData-->Check sector3 data failed !");
			}
			return false;
		}
	}
	
	return true;
}
void CRKAndroidDevice::IDB528To512(uint_8* pIDB528,uint_8* pIDB512,uint_8* pSpare,uint_32 dwSectorNum)
{
	uint_32 i;
	for (i=0;i<dwSectorNum;i++)
	{
		memcpy(pIDB512+i*512,pIDB528+i*528,512);
		if (pSpare)
		{
			memcpy(pSpare+i*16,pIDB528+i*528+512,16);
		}
	}
}
void CRKAndroidDevice::IDBRC4(uint_8* pIDB)
{
	int i;
	for (i=0;i<4;i++)
	{
		if(i!=1)
		{
			P_RC4(pIDB+SECTOR_SIZE*i, SECTOR_SIZE);
		}
	}
}
bool CRKAndroidDevice::IsEnvFirmware(bool &envFlag)
{
	long long dwFwOffset;
	bool  bRet;
	STRUCT_RKIMAGE_HDR rkImageHead;
	STRUCT_RKIMAGE_ITEM *pDownloadItem;
	uint_8 headerData[64*1024];
	uint_8* pHeaderData=headerData;
	int iHeadSize,iHeadMetaSize;
	
	envFlag = false;
	if (m_pImage->SingleBootFlag)
	{
		return true;
	}
	
	dwFwOffset = m_pImage->FWOffset;
	iHeadSize = sizeof(STRUCT_RKIMAGE_HDR);
	
	
	bRet = m_pImage->GetData(dwFwOffset,iHeadSize,(uint_8*)&rkImageHead);
	if ( !bRet )
	{
		m_error = ERR_FW_READ;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:IsEnvFirmware-->GetData failed");
		}
		return false;
	}
	if ((rkImageHead.manufacturer[56]==0x55)&&(rkImageHead.manufacturer[57]==0x66))
	{
		uint_16 *pItemRemain;
		pItemRemain = (uint_16 *)(&rkImageHead.manufacturer[58]);
		rkImageHead.item_count += *pItemRemain;
	}
	iHeadMetaSize = iHeadSize = sizeof(STRUCT_RKIMAGE_HDR)-sizeof(STRUCT_RKIMAGE_ITEM)*MAX_PACKAGE_FILES;
	iHeadSize += rkImageHead.item_count*sizeof(STRUCT_RKIMAGE_ITEM);
	iHeadSize = CALC_UNIT(iHeadSize, RK_IMAGE_PAGE_SIZE)*RK_IMAGE_PAGE_SIZE;

 	bRet = m_pImage->GetData(dwFwOffset,iHeadSize,(uint_8*)pHeaderData);
 	if ( !bRet )
 	{
		m_error = ERR_FW_READ;
 		if (m_pLog)
 		{
 			m_pLog->Record("ERROR:IsEnvFirmware-->GetData for header failed");
 		}
 		return false;
 	}
	if ( rkImageHead.item_count<=0 )
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:IsEnvFirmware-->No Found item");
		}
		return false;
	}
	
	int i;

	for ( i=0;i<rkImageHead.item_count;i++ )
	{
		pDownloadItem = (STRUCT_RKIMAGE_ITEM *)(pHeaderData+iHeadMetaSize+i*sizeof(STRUCT_RKIMAGE_ITEM));
		if ( pDownloadItem->flash_offset==0 ) {
			if ( stricmp(pDownloadItem->name,PARTNAME_ENV)==0)
			{
				envFlag = true;
				break;
			}
		}
	}

	return true;
}
bool CRKAndroidDevice::IsSparseImage(STRUCT_RKIMAGE_ITEM &entry)
{
	bool bRet;
	sparse_header header;
	uint_64 ulEntryStartOffset;
	if (entry.file[50]=='H')
	{
		ulEntryStartOffset = *((uint_32 *)(&entry.file[51]));
		ulEntryStartOffset <<= 32;
		ulEntryStartOffset += entry.offset;
		ulEntryStartOffset += m_pImage->FWOffset;
	}
	else
	{
		ulEntryStartOffset = m_pImage->FWOffset;
		ulEntryStartOffset += entry.offset;
	}
	bRet = m_pImage->GetData(ulEntryStartOffset,sizeof(header),(uint_8*)&header);
	if (!bRet)
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:IsSparseImage-->getdata failed");
		}
		return false;
	}
	if (header.magic!=SPARSE_HEADER_MAGIC)
	{
		return false;
	}
	return true;
}
bool CRKAndroidDevice::IsDharaImage(STRUCT_RKIMAGE_ITEM &entry)
{
	bool bRet;
	uint_32 magic;
	uint_64 ulEntryStartOffset,ulEntryFileSize;
	if (entry.file[50]=='H')
	{
		ulEntryStartOffset = *((uint_32 *)(&entry.file[51]));
		ulEntryStartOffset <<= 32;
		ulEntryStartOffset += entry.offset;
		ulEntryStartOffset += m_pImage->FWOffset;
	}
	else
	{
		ulEntryStartOffset = m_pImage->FWOffset;
		ulEntryStartOffset += entry.offset;
	}
	if (entry.file[55]=='H')
	{
		ulEntryFileSize = *((uint_32 *)(&entry.file[56]));
		ulEntryFileSize <<= 32;
		ulEntryFileSize += entry.size;
	}
	else
		ulEntryFileSize = entry.size;
	if (ulEntryFileSize <= 0x7800)
		return false;

	bRet = m_pImage->GetData(ulEntryStartOffset+0x7800,sizeof(uint_32),(uint_8*)&magic);
	if (!bRet)
	{
		m_error = ERR_FW_READ;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:IsDharaImage-->getdata failed");
		}
		return false;
	}
	if (magic!=DHARA_HEADER_MAGIC)
	{
		return false;
	}
	return true;
}
bool CRKAndroidDevice::IsUbiImage(STRUCT_RKIMAGE_ITEM &entry)
{
	bool bRet;
	uint_32 magic;
	uint_64 ulEntryStartOffset;
	if (entry.file[50]=='H')
	{
		ulEntryStartOffset = *((uint_32 *)(&entry.file[51]));
		ulEntryStartOffset <<= 32;
		ulEntryStartOffset += entry.offset;
		ulEntryStartOffset += m_pImage->FWOffset;
	}
	else
	{
		ulEntryStartOffset = m_pImage->FWOffset;
		ulEntryStartOffset += entry.offset;
	}
	bRet = m_pImage->GetData(ulEntryStartOffset,sizeof(uint_32),(uint_8*)&magic);
	if (!bRet)
	{
		m_error = ERR_FW_READ;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:IsUbiImage-->getdata failed");
		}
		return false;
	}
	if (magic!=UBI_HEADER_MAGIC)
	{
		return false;
	}
	return true;
}
bool CRKAndroidDevice::IsJffsImage(STRUCT_RKIMAGE_ITEM &entry)
{
	bool bRet;
	uint_32 magic;
	uint_64 ulEntryStartOffset;
	if (entry.file[50]=='H')
	{
		ulEntryStartOffset = *((uint_32 *)(&entry.file[51]));
		ulEntryStartOffset <<= 32;
		ulEntryStartOffset += entry.offset;
		ulEntryStartOffset += m_pImage->FWOffset;
	}
	else
	{
		ulEntryStartOffset = m_pImage->FWOffset;
		ulEntryStartOffset += entry.offset;
	}
	bRet = m_pImage->GetData(ulEntryStartOffset,sizeof(uint_32),(uint_8*)&magic);
	if (!bRet)
	{
		m_error = ERR_FW_READ;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:IsJffsImage-->getdata failed");
		}
		return false;
	}
	if ((magic!=JFFS_HEADER_MAGIC) && (magic!=JFFS_HEADER_MAGIC2))
	{
		return false;
	}
	return true;
}
uint_64 CRKAndroidDevice::GetSparseImageSize(STRUCT_RKIMAGE_ITEM &entry)
{
	bool bRet;
	sparse_header header;
	uint_64 ulEntryStartOffset;
	if (entry.file[50]=='H')
	{
		ulEntryStartOffset = *((uint_32 *)(&entry.file[51]));
		ulEntryStartOffset <<= 32;
		ulEntryStartOffset += entry.offset;
		ulEntryStartOffset += m_pImage->FWOffset;
	}
	else
	{
		ulEntryStartOffset = m_pImage->FWOffset;
		ulEntryStartOffset += entry.offset;
	}
	bRet = m_pImage->GetData(ulEntryStartOffset,sizeof(header),(uint_8*)&header);
	if (!bRet)
	{
		m_error = ERR_FW_READ;
		if (m_pLog)
		{
			m_pLog->Record("ERROR:IsSparseImage-->getdata failed");
		}
		return -1;
	}
	return (uint_64)header.blk_sz * header.total_blks;

}
bool CRKAndroidDevice::GetOldIDBData()
{
	uint_8 bData[SECTOR_SIZE*4];
	int i,ret;
	RKANDROID_IDB_SEC0 *pSec0;
	RKANDROID_IDB_SEC1 *pSec;
	bool bFound = false;
	m_oldIDBCounts = 0;
	for (i=0;i<5;i++)
	{
		ret = m_pComm->RKU_ReadLBA(IDBLOCK_LBA+i*IDBLOCK_SPAN,4,bData);
		if (ret != ERR_SUCCESS)
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:GetOldIDBData-->RKU_ReadLBA failed,err(%d)",ret);
			}
			return false;
		}
		pSec0 = (RKANDROID_IDB_SEC0 *)bData;
		P_RC4((uint_8 *)pSec0,SECTOR_SIZE);
		if (pSec0->dwTag==0x0FF0AA55)
		{
			pSec = (RKANDROID_IDB_SEC1 *)(bData+SECTOR_SIZE);
			if (pSec->uiChipTag==0x38324B52)
			{
				bFound = true;
				m_oldIDBCounts++;
			}
		}
	}
	if (bFound)
	{
		uint_8* pSectorData;
		if (!m_oldSec0)
		{
			m_oldSec0 = new RKANDROID_IDB_SEC0;
		}
		pSectorData = (uint_8*)(m_oldSec0);
		memset(pSectorData,0,SECTOR_SIZE);
		memcpy(pSectorData,bData,SECTOR_SIZE);

		if (!m_oldSec1)
		{
			m_oldSec1 = new RKANDROID_IDB_SEC1;
		}
		pSectorData = (uint_8*)(m_oldSec1);
		memset(pSectorData,0,SECTOR_SIZE);
		memcpy(pSectorData,bData+SECTOR_SIZE,SECTOR_SIZE);

		if (!m_oldSec2)
		{
			m_oldSec2 = new RKANDROID_IDB_SEC2;
		}
		pSectorData = (uint_8*)(m_oldSec2);
		memset(pSectorData,0,SECTOR_SIZE);
		memcpy(pSectorData,bData+SECTOR_SIZE*2,SECTOR_SIZE);
		P_RC4(pSectorData,SECTOR_SIZE);

		if (!m_oldSec3)
		{
			m_oldSec3 = new RKANDROID_IDB_SEC3;
		}
		pSectorData = (uint_8*)(m_oldSec3);
		memset(pSectorData,0,SECTOR_SIZE);
		memcpy(pSectorData,bData+SECTOR_SIZE*3,SECTOR_SIZE);
		P_RC4(pSectorData,SECTOR_SIZE);
	}
	return true;
}
bool CRKAndroidDevice::FindBackupSector3()
{
	int iRet;
	bool bRet;
	uint_8 buffer[2*SECTOR_SIZE];

	memset(buffer,0,2*SECTOR_SIZE);
	iRet = m_pComm->RKU_ReadLBA(SECTOR3_BACKUP_LBA,2,buffer);
	if (iRet!=ERR_SUCCESS)
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:FindBackupSector3-->RKU_ReadLBA failed,RetCode(%d)",iRet);
		}
		return false;
	}
	else
	{
		char *pszCrcTag = (char *)buffer;
		if (strcmp(pszCrcTag,"CRC")==0)
		{
			m_usWriteBackCrc = *((uint_16 *)(buffer+4));
			m_usWriteBackCustomDataOffset = *((uint_16 *)(buffer+6));
			m_usWriteBackCustomDataSize = *((uint_16 *)(buffer+8));
			bRet = CheckCrc16(buffer+SECTOR_SIZE,SECTOR_SIZE,m_usWriteBackCrc);
			if (!bRet)
			{
				if (m_pLog)
				{
					m_pLog->Record("ERROR:FindBackupSector3-->Check Crc Failed");
				}
				m_usWriteBackCrc = CRC_16(buffer+SECTOR_SIZE,SECTOR_SIZE);
			}
			memcpy(m_backupBuffer,buffer+SECTOR_SIZE,SECTOR_SIZE);
		}
		else
		{
			if (m_pLog)
			{
				m_pLog->Record("INFO:FindBackupSector3-->No Found");
			}
		}
	}
	return true;
}
int  CRKAndroidDevice::WriteIDBWithDirect(uint_8* pIDB)
{
	int ret = 0;
	uint_32 i;
	uint_32 nCopyCount=2,idbStartSector=IDBLOCK_LBA,idbSpanSector=IDBLOCK_SPAN;
	uint_32 dwSectorNum, dwOffset, dwRemain, dwWrtie;
	uint_8 *pIDBData = NULL,*pTempIDB=NULL;
	bool bForceFree = false;
	dwSectorNum = m_flashInfo.uiSecNumPerIDB;
	if (pIDB)
	{
		pIDBData = pIDB;
	}
	else
	{
		if (ReadIDBConfigFlag)
		{//nand device
			if (dwSectorNum>m_flashInfo.uiSectorPerBlock)
			{
				dwSectorNum = CALC_UNIT(dwSectorNum,m_flashInfo.uiSectorPerBlock) * m_flashInfo.uiSectorPerBlock;//align with block
			}
			else
				dwSectorNum = m_flashInfo.uiSectorPerBlock;
			idbStartSector = m_flashInfo.uiSectorPerBlock;
			idbSpanSector = dwSectorNum;
			//nCopyCount = (7*m_flashInfo.uiSectorPerBlock)/dwSectorNum;//insert idb between [1Block,8Block]
		}
		pIDBData = new uint_8[dwSectorNum*SECTOR_SIZE];
		if (!pIDBData)
		{
			m_error = ERR_NEW_BUFFER;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:WriteIDBWithDirect-->new memory failed,err(%d)",errno);
			}
			ret = -1;
			goto Exit_WriteIDBDirect;
		}
		memset(pIDBData,0,dwSectorNum*SECTOR_SIZE);
		if (NewIDBFlag)
		{
			ret = MakeNewIDBlockData(pIDBData);
		}
		else
			ret = MakeIDBlockData(pIDBData);
		if ( ret!=0 )
		{
			if (m_pLog)
			{
				m_pLog->Record("ERROR:WriteIDBWithDirect-->MakeIDBlockData,RetCode(%d)",ret);
			}
			ret = -2;
			goto Exit_WriteIDBDirect;
		}
	}
	if (m_bNor)
	{
		if (m_bNorSingleIDB)
		{
			nCopyCount = 1;
		}
		else
		{
			// nCopyCount = 2;
			uint32_t dwSecondOffset;
			dwSecondOffset = idbStartSector + idbSpanSector;
			if (dwSecondOffset>640)
				dwSecondOffset = CALC_UNIT(dwSecondOffset,128)*128;//align with 64k
			else
				dwSecondOffset = 640;//the second idb on 320k
			idbSpanSector = dwSecondOffset - idbStartSector;
		}
	}
	if (m_flashInfo.bPageAvailSizeSector && (m_flashInfo.bPageAvailSizeSector < m_flashInfo.bPageSizeSector)) {
		dwOffset = CALC_UNIT(dwSectorNum, m_flashInfo.bPageAvailSizeSector);
		dwRemain = dwOffset*m_flashInfo.bPageSizeSector;
		pTempIDB = new uint_8[dwRemain*SECTOR_SIZE];
		if (pTempIDB)
		{
			memset(pTempIDB, 0, dwRemain * SECTOR_SIZE);
			dwWrtie = 0;
			for (i = 0; i < dwOffset; i++)
			{
				if ((i + 1) < dwOffset) {
					memcpy(pTempIDB + i * m_flashInfo.bPageSizeSector * SECTOR_SIZE, pIDBData + i * m_flashInfo.bPageAvailSizeSector * SECTOR_SIZE, m_flashInfo.bPageAvailSizeSector * SECTOR_SIZE);
				}
				else {
					memcpy(pTempIDB + i * m_flashInfo.bPageSizeSector * SECTOR_SIZE, pIDBData + i * m_flashInfo.bPageAvailSizeSector * SECTOR_SIZE, (dwSectorNum - dwWrtie) * SECTOR_SIZE);
				}
				dwWrtie += m_flashInfo.bPageAvailSizeSector;
			}
			bForceFree = true;
			if (!pIDB)
				delete []pIDBData;
			pIDBData = pTempIDB;
			pTempIDB = NULL;
			dwSectorNum = dwRemain;
			if (idbSpanSector < dwSectorNum)
				idbSpanSector = CALC_UNIT(dwSectorNum, m_flashInfo.uiSectorPerBlock) * m_flashInfo.uiSectorPerBlock;//align with block
		}
		else {
			m_error = ERR_NEW_BUFFER;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:WriteIDBWithDirect-->new memory failed,err(%d)",errno);
			}
			ret = -1;
			goto Exit_WriteIDBDirect;
		}
	}
	for (i=0;i<nCopyCount;i++)
	{
		ret = m_pComm->RKU_WriteLBA(idbStartSector+i*idbSpanSector,dwSectorNum,pIDBData);
		if (ret!=ERR_SUCCESS)
		{
			m_error = ERR_COMM_RW;
			if (m_pLog)
			{
				m_pLog->Record("ERROR:WriteIDBWithDirect-->RKU_WriteLBA failed,ret(%d),lba(%d)",ret,idbStartSector+i*idbSpanSector);
			}
			ret = -3;
			goto Exit_WriteIDBDirect;
		}
	}
	m_uiIDBend = idbStartSector + (nCopyCount-1)*idbSpanSector + dwSectorNum;
Exit_WriteIDBDirect:
	if (bForceFree)
		delete []pIDBData;
	else {
		if ((pIDBData) && (!pIDB))
		{
			delete []pIDBData;
		}
	}
	
	return ret;
}
int CRKAndroidDevice::EraseDirectLBABlock(uint_32 dwPos,uint_32 dwCount)
{
	int nWriteSec,nSectors,nBegin,iRet;
	uint_8 emptyData[32*SECTOR_SIZE];
	memset(emptyData,0,32*SECTOR_SIZE);
	while (dwCount>0)
	{
		nSectors = m_flashInfo.uiSectorPerBlock;
		nBegin = dwPos * m_flashInfo.uiSectorPerBlock;
		while(nSectors>0)
		{
			if (nSectors>=32)
				nWriteSec = 32;
			else
				nWriteSec = nSectors;
			iRet = m_pComm->RKU_WriteLBA(nBegin,nWriteSec,emptyData);
			if (iRet!=ERR_SUCCESS)
			{
				m_error = ERR_COMM_RW;
				if (m_pLog)
				{
					m_pLog->Record("ERROR:EraseDirectLBABlock-->RKU_WriteLBA failed,RetCode(%d)",iRet);
				}
				return iRet;
			}
			nSectors -= nWriteSec;
			nBegin += nWriteSec;
		}
		dwCount--;
		dwPos++;
	}
	return ERR_SUCCESS;
}
bool CRKAndroidDevice::ReadCapability()
{
	int ret;
	uint_8 data[8];
	ret = m_pComm->RKU_ReadCapability(data);
	if (ret!=ERR_SUCCESS)
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:ReadCapability-->RKU_ReadCapability failed,err(%d)",ret);
		}
		return false;
	}
	if (data[0] & 0x1)
	{
		DirectLBA = true;
	}
	else
		DirectLBA = false;
	if (data[0] & 0x4)
	{
		First4Access = true;
	}
	else
		First4Access = false;
	if (data[0] & 0x40)
	{
		ReadIDBConfigFlag = true;
	}
	else
		ReadIDBConfigFlag = false;
	if (data[0] & 0x80)
	{
		SecureModeFlag = true;
	}
	else
		SecureModeFlag = false;
	if (data[1] & 0x1)
	{
		NewIDBFlag = true;
	}
	else
		NewIDBFlag = false;
	if (data[1] & 0x2)
	{
		SwitchStorageFlag = true;
	}
	else
		SwitchStorageFlag = false;
	if (data[1] & 0x10)
	{
		SwitchUsb3Flag = true;
	}
	else
		SwitchUsb3Flag = false;
	return true;
}
bool CRKAndroidDevice::GetParameterGptFlag(STRUCT_RKIMAGE_ITEM &paramItem)
{
	uint_8* pParamBuf=NULL;
	GptFlag = false;
	pParamBuf = new uint_8[paramItem.size-12+1];
	if (!pParamBuf)
	{
		m_error = ERR_NEW_BUFFER;
		return false;
	}
	memset(pParamBuf,0,paramItem.size-12+1);
	bool bRet;
	bRet = m_pImage->GetData(m_pImage->FWOffset+paramItem.offset+8,paramItem.size-12,pParamBuf);
	if (!bRet)
	{
		m_error = ERR_FW_READ;
		delete []pParamBuf;
		return false;
	}
	string strParamFile = (char *)pParamBuf;
	stringstream paramStream(strParamFile);
	delete []pParamBuf;
	

	string strLine;
	string::size_type pos,line_size;
	while (!paramStream.eof())
	{
		getline(paramStream,strLine);
		line_size = strLine.size();
		if (line_size<=0)
		{
			continue;
		}
		if (strLine[line_size-1]=='\r')
		{
			strLine = strLine.substr(0,line_size-1);
		}
		if (strLine.size()<=0)
		{
			continue;
		}
		if (strLine[0]=='#')
		{
			continue;
		}
		transform(strLine.begin(),strLine.end(),strLine.begin(),(int(*)(int))toupper);
		pos = strLine.find("TYPE");
		if (pos==string::npos)
		{
			continue;
		}
		if ((pos = strLine.find("GPT"))!=string::npos)
		{
			GptFlag = true;
			break;
		}
		else
			continue;
	}
	return true;
}
#define DO_CRC(x) crc = tab[ (crc ^ (x)) & 255 ] ^ (crc>>8)
unsigned int crc32_le(unsigned int crc, unsigned char *p, unsigned int len)
{
	unsigned int      *b =(unsigned int *)p;
	unsigned int      *tab = crc32table_le;
	crc = crc ^ 0xFFFFFFFF;
	if((((long)b)&3 && len)){
		do {
			unsigned char *bytePointer = (unsigned char *)b;
			DO_CRC(*bytePointer++);
			b = (unsigned int *)bytePointer;
		} while ((--len) && ((long)b)&3 );
	}
	if((len >= 4)){
		unsigned int save_len = len & 3;
		len = len >> 2;
		--b;
		do {
			crc ^= *++b;
			DO_CRC(0);
			DO_CRC(0);
			DO_CRC(0);
			DO_CRC(0);
		} while (--len);
		b++;
		len = save_len;
	}
	if(len){
		do {
			unsigned char *bytePointer = (unsigned char *)b;
			DO_CRC(*bytePointer++);
			b = (unsigned int *)bytePointer;
		} while (--len);
	}
	crc = crc ^ 0xFFFFFFFF;
	return crc;
}
unsigned int calculate_gpt_sector_count(unsigned int nLogicalBlock)
{
	unsigned int nGptEntryLB, nGptSector;
	nGptEntryLB = CALC_UNIT(128, (nLogicalBlock * 4));
	nGptSector = (3 + nGptEntryLB * 2) * nLogicalBlock;
	return nGptSector;
}
void prepare_gpt_backup(u8 *master, u8 *backup, u32 logicalBlockSectors)
{
	gpt_header *gptMasterHead;
	gpt_header *gptBackupHead;
	u32 calc_crc32;
	u64 val;
	u32 nGptEntryLB;

	nGptEntryLB = CALC_UNIT(128, (logicalBlockSectors * 4));

	gptMasterHead = (gpt_header*)(master + logicalBlockSectors * SECTOR_SIZE);
	gptBackupHead = (gpt_header*)(backup + nGptEntryLB * logicalBlockSectors * SECTOR_SIZE);

	/* recalculate the values for the Backup GPT Header */
	val = le64_to_cpu(gptMasterHead->my_lba);
	gptBackupHead->my_lba = gptMasterHead->alternate_lba;
	gptBackupHead->alternate_lba = cpu_to_le64(val);
	gptBackupHead->partition_entry_lba = cpu_to_le64(le64_to_cpu(gptMasterHead->alternate_lba) - nGptEntryLB); 
	gptBackupHead->header_crc32 = 0;

	calc_crc32 = crc32_le(0, (unsigned char *)gptBackupHead, le32_to_cpu(gptBackupHead->header_size));
	gptBackupHead->header_crc32 = cpu_to_le32(calc_crc32);
}

void gen_rand_uuid(unsigned char *uuid_bin)
{
	efi_guid_t id;
	unsigned int *ptr = (unsigned int *)&id;
	unsigned int i;

	/* Set all fields randomly */
	for (i = 0; i < sizeof(id) / sizeof(*ptr); i++)
		*(ptr + i) = cpu_to_be32(rand());

	id.uuid.time_hi_and_version = (id.uuid.time_hi_and_version & 0x0FFF) | 0x4000;
	id.uuid.clock_seq_hi_and_reserved = id.uuid.clock_seq_hi_and_reserved | 0x80;

	memcpy(uuid_bin, id.raw, sizeof(id));
}
void create_gpt_buffer(u8 *gpt, PARAM_ITEM_VECTOR &vecParts, CONFIG_ITEM_VECTOR &vecUuid, u64 diskSectors, u32 logicalBlockSectors, bool *align_flag)
{
	legacy_mbr *mbr = (legacy_mbr *)gpt;
	gpt_header *gptHead = (gpt_header *)(gpt + logicalBlockSectors * SECTOR_SIZE);
	gpt_entry *gptEntry = (gpt_entry *)(gpt + 2 * logicalBlockSectors * SECTOR_SIZE);
	u32 i,j;
	int pos,backup_gpt_secs;
	u32 nGptEntryLB,nDiskLB,nMasterGptLB;
	string strPartName;
	string::size_type iPos;

	nDiskLB = CALC_UNIT(diskSectors, logicalBlockSectors);
	nGptEntryLB = CALC_UNIT(128, (logicalBlockSectors * 4));
	nMasterGptLB = 2 + nGptEntryLB;

	backup_gpt_secs = 1 + nGptEntryLB;
	if (align_flag)
	{
		if (*align_flag)
		{
			backup_gpt_secs = CALC_UNIT(64, logicalBlockSectors);
		}
	}else {
		bool b_exist_cache = false;
		for (i = 0; i < vecParts.size(); i++) {
			if (stricmp(vecParts[i].szItemName,"cache")==0)
			{
				b_exist_cache = true;
				break;
			}
		}
		if (b_exist_cache)
		{//check if the system is android by finding cache partition
			backup_gpt_secs = CALC_UNIT(64, logicalBlockSectors);
		}
	}

	/*1.protective mbr*/
	memset(gpt, 0, logicalBlockSectors * SECTOR_SIZE);
	mbr->signature = MSDOS_MBR_SIGNATURE;
	mbr->partition_record[0].sys_ind = EFI_PMBR_OSTYPE_EFI_GPT;
	mbr->partition_record[0].start_sect = 1;
	mbr->partition_record[0].nr_sects = (u32)-1;
	/*2.gpt header*/
	memset(gpt + logicalBlockSectors * SECTOR_SIZE, 0, logicalBlockSectors * SECTOR_SIZE);
	gptHead->signature = cpu_to_le64(GPT_HEADER_SIGNATURE);
	gptHead->revision = cpu_to_le32(GPT_HEADER_REVISION_V1);
	gptHead->header_size = cpu_to_le32(sizeof(gpt_header));
	gptHead->my_lba = cpu_to_le64(1);
	gptHead->alternate_lba = cpu_to_le64(nDiskLB - 1);
	gptHead->first_usable_lba = cpu_to_le64(nMasterGptLB);
	gptHead->last_usable_lba = cpu_to_le64(nDiskLB - nMasterGptLB);
	gptHead->partition_entry_lba = cpu_to_le64(2);
	gptHead->num_partition_entries = cpu_to_le32(GPT_ENTRY_NUMBERS);
	gptHead->sizeof_partition_entry = cpu_to_le32(GPT_ENTRY_SIZE);
	gptHead->header_crc32 = 0;
	gptHead->partition_entry_array_crc32 = 0;
	gen_rand_uuid(gptHead->disk_guid.raw);

	/*3.gpt partition entry*/
	memset(gpt + 2 * logicalBlockSectors * SECTOR_SIZE, 0, nGptEntryLB * logicalBlockSectors * SECTOR_SIZE);
	for (i = 0; i < vecParts.size(); i++) {
		gen_rand_uuid(gptEntry->partition_type_guid.raw);
		gen_rand_uuid(gptEntry->unique_partition_guid.raw);
		gptEntry->starting_lba = cpu_to_le64(CALC_UNIT(vecParts[i].uiItemOffset, logicalBlockSectors));
		gptEntry->ending_lba = cpu_to_le64(gptEntry->starting_lba + CALC_UNIT(vecParts[i].uiItemSize,logicalBlockSectors) - 1);
		gptEntry->attributes.raw = 0;
		strPartName = vecParts[i].szItemName;
		iPos = strPartName.find(':');
		if (iPos != string::npos) {
			transform(strPartName.begin(),strPartName.end(),strPartName.begin(),(int(*)(int))tolower);
			if (strPartName.find("bootable") != string::npos)
				gptEntry->attributes.raw = PART_PROPERTY_BOOTABLE;
			if (strPartName.find("efi") != string::npos) {
				gptEntry->attributes.raw = PART_PROPERTY_BOOTABLE;
				string_to_uuid(EFI_PARTITION_GUID,(char *)gptEntry->partition_type_guid.raw);
			}
			if (strPartName.find("grow") != string::npos)
				gptEntry->ending_lba = cpu_to_le64(nDiskLB - backup_gpt_secs - 1);
			strPartName = strPartName.substr(0,iPos);
			vecParts[i].szItemName[strPartName.size()] = 0;
		}
		for (j = 0; j < strlen(vecParts[i].szItemName); j++)
			gptEntry->partition_name[j] = vecParts[i].szItemName[j];
		if ((pos = find_uuid_item(vecUuid, vecParts[i].szItemName)) != -1)
			memcpy(gptEntry->unique_partition_guid.raw, vecUuid[pos].szItemValue, 16);
		gptEntry++;
	}

	gptHead->partition_entry_array_crc32 = cpu_to_le32(crc32_le(0, gpt + 2 * logicalBlockSectors * SECTOR_SIZE, GPT_ENTRY_SIZE * GPT_ENTRY_NUMBERS));
	gptHead->header_crc32 = cpu_to_le32(crc32_le(0, gpt + logicalBlockSectors * SECTOR_SIZE, sizeof(gpt_header)));
}

bool ParsePartitionInfo(string &strPartInfo,string &strName,uint_32 &uiOffset,uint_32 &uiLen)
{
	string::size_type pos,prevPos;
	string strOffset,strLen;
	int iCount;
	prevPos = pos = 0;
	if (strPartInfo.size()<=0)
	{
		return false;
	}
	pos = strPartInfo.find('@');
	if (pos==string::npos)
	{
		return false;
	}
	strLen = strPartInfo.substr(prevPos,pos-prevPos);
	strLen.erase(0,strLen.find_first_not_of(" "));
	strLen.erase(strLen.find_last_not_of(" ")+1);
	if (strchr(strLen.c_str(),'-'))
	{
		uiLen = 0xFFFFFFFF;
	}
	else
	{
		iCount = sscanf(strLen.c_str(),"0x%x",&uiLen);
		if (iCount!=1)
		{
			return false;
		}
	}

	prevPos = pos +1;
	pos = strPartInfo.find('(',prevPos);
	if (pos==string::npos)
	{
		return false;
	}
	strOffset = strPartInfo.substr(prevPos,pos-prevPos);
	strOffset.erase(0,strOffset.find_first_not_of(" "));
	strOffset.erase(strOffset.find_last_not_of(" ")+1);
	iCount = sscanf(strOffset.c_str(),"0x%x",&uiOffset);
	if (iCount!=1)
	{
		return false;
	}
	
	prevPos = pos +1;
	pos = strPartInfo.find(')',prevPos);
	if (pos==string::npos)
	{
		return false;
	}
	strName = strPartInfo.substr(prevPos,pos-prevPos);
	strName.erase(0,strName.find_first_not_of(" "));
	strName.erase(strName.find_last_not_of(" ")+1);

	return true;
}
bool ParseUuidInfo(string &strUuidInfo, string &strName, string &strUUid)
{
	string::size_type pos(0);
	
	if (strUuidInfo.size() <= 0) {
		return false;
	}
	pos = strUuidInfo.find('=');
	if (pos == string::npos) {
		return false;
	}
	strName = strUuidInfo.substr(0, pos);
	strName.erase(0, strName.find_first_not_of(" "));
	strName.erase(strName.find_last_not_of(" ") + 1);

	strUUid = strUuidInfo.substr(pos+1);
	strUUid.erase(0, strUUid.find_first_not_of(" "));
	strUUid.erase(strUUid.find_last_not_of(" ") + 1);
	
	while(true) { 
		pos = 0;
		if( (pos = strUUid.find("-")) != string::npos) 
			strUUid.replace(pos,1,""); 
		else 
			break; 
	}
	if (strUUid.size() != 32)
		return false;
	return true;
}
void string_to_uuid(string strUUid, char *uuid)
{
	unsigned int i;
	char value;
	memset(uuid, 0, 16);
	for (i =0; i < strUUid.size(); i++) {
		value = 0;
		if ((strUUid[i] >= '0')&&(strUUid[i] <= '9'))
			value = strUUid[i] - '0';
		if ((strUUid[i] >= 'a')&&(strUUid[i] <= 'f'))
			value = strUUid[i] - 'a' + 10;
		if ((strUUid[i] >= 'A')&&(strUUid[i] <= 'F'))
			value = strUUid[i] - 'A' + 10;
		if ((i % 2) == 0)
			uuid[i / 2] += (value << 4);
		else
			uuid[i / 2] += value;
	}
	unsigned int *p32;
	unsigned short *p16;
	p32 = (unsigned int*)uuid;
	*p32 = cpu_to_be32(*p32);
	p16 = (unsigned short *)(uuid + 4);
	*p16 = cpu_to_be16(*p16);
	p16 = (unsigned short *)(uuid + 6);
	*p16 = cpu_to_be16(*p16);
}
bool parse_parameter(char *pParameter,PARAM_ITEM_VECTOR &vecItem)
{
	stringstream paramStream(pParameter);
	bool bRet,bFind=false;
	string strLine,strPartition,strPartInfo,strPartName;
	string::size_type line_size,pos,posColon,posComma;
	uint_32 uiPartOffset,uiPartSize;
	STRUCT_PARAM_ITEM item;
	vecItem.clear();
	while (!paramStream.eof())
	{
		getline(paramStream,strLine);
		line_size = strLine.size();
		if (line_size==0)
			continue;
		if (strLine[line_size-1]=='\r')
		{
			strLine = strLine.substr(0,line_size-1);
		}
		pos = strLine.find("mtdparts");
		if (pos==string::npos)
		{
			continue;
		}
		bFind = true;
		posColon = strLine.find(':',pos);
		if (posColon==string::npos)
		{
			continue;
		}		
		strPartition = strLine.substr(posColon+1);
		pos = 0;
		posComma = strPartition.find(',',pos);
		while (posComma!=string::npos)
		{
			strPartInfo = strPartition.substr(pos,posComma-pos);
			bRet = ParsePartitionInfo(strPartInfo,strPartName,uiPartOffset,uiPartSize);
			if (bRet)
			{			
				strcpy(item.szItemName,strPartName.c_str());
				item.uiItemOffset = uiPartOffset;
				item.uiItemSize = uiPartSize;
				vecItem.push_back(item);
			}
			pos = posComma+1;
			posComma = strPartition.find(',',pos);
		}
		strPartInfo = strPartition.substr(pos);
		if (strPartInfo.size()>0)
		{
			bRet = ParsePartitionInfo(strPartInfo,strPartName,uiPartOffset,uiPartSize);
			if (bRet)
			{
				strcpy(item.szItemName,strPartName.c_str());
				item.uiItemOffset = uiPartOffset;
				item.uiItemSize = uiPartSize;
				vecItem.push_back(item);
			}
		}
		break;
	}
	return bFind;
}
bool get_uuid_from_parameter(char *pParameter,CONFIG_ITEM_VECTOR &vecItem)
{
	stringstream paramStream(pParameter);
	bool bRet;
	string strLine,strUUid,strPartInfo,strPartName;
	string::size_type line_size,pos;
	STRUCT_CONFIG_ITEM uuid_item;
	vecItem.clear();
	while (!paramStream.eof())
	{
		getline(paramStream,strLine);
		line_size = strLine.size();
		if (line_size==0)
			continue;
		if (strLine[line_size-1]=='\r')
		{
			strLine = strLine.substr(0,line_size-1);
		}
		pos = strLine.find("uuid:");
		if (pos != string::npos) {
			strPartInfo = strLine.substr(pos+5);
			bRet = ParseUuidInfo(strPartInfo, strPartName, strUUid);
			if (bRet) {
				strcpy(uuid_item.szItemName, strPartName.c_str());
				string_to_uuid(strUUid,uuid_item.szItemValue);
				vecItem.push_back(uuid_item);
			}
		}
	}
	return (vecItem.size()>0);
}
bool get_grow_align_flag(char *pParam, bool &align_flag)
{
	bool bret=false;
	string strParamFile = pParam;
	stringstream paramStream(strParamFile);

	string strLine,strAlignFlag;
	string::size_type pos,line_size;
	while (!paramStream.eof())
	{
		getline(paramStream,strLine);
		line_size = strLine.size();
		if (line_size<=0)
		{
			continue;
		}
		if (strLine[line_size-1]=='\r')
		{
			strLine = strLine.substr(0,line_size-1);
		}
		if (strLine.size()<=0)
		{
			continue;
		}
		if (strLine[0]=='#')
		{
			continue;
		}
		transform(strLine.begin(),strLine.end(),strLine.begin(),(int(*)(int))toupper);
		pos = strLine.find("GROW_ALIGN:");
		if (pos==string::npos)
		{
			continue;
		}
		bret = true;
		align_flag = false;
		strAlignFlag = strLine.substr(pos+11);
		if (strAlignFlag.size())
		{
			strAlignFlag.erase(0, strAlignFlag.find_first_not_of(" "));
			strAlignFlag.erase(strAlignFlag.find_last_not_of(" ") + 1);
			if (strAlignFlag=="1")
			{
				align_flag = true;
			}
		}
	}
	return bret;
}
bool parse_gpt(unsigned char *pGpt,PARAM_ITEM_VECTOR &vecItem,u32 logicalBlockSectors)
{
	gpt_header *gptMasterHead = (gpt_header *)(pGpt + logicalBlockSectors * 512);
	gpt_entry  *gptEntry  = NULL;
	u32 i,j;
	STRUCT_PARAM_ITEM item;
	u8 zerobuf[GPT_ENTRY_SIZE];
	vecItem.clear();
	memset(zerobuf,0,GPT_ENTRY_SIZE);
	if (gptMasterHead->signature != le64_to_cpu(GPT_HEADER_SIGNATURE))
	{
		return false;
	}

	for (i = 0; i < le32_to_cpu(gptMasterHead->num_partition_entries); i++) {

		gptEntry = (gpt_entry *)(pGpt + 2 * logicalBlockSectors * SECTOR_SIZE + i * GPT_ENTRY_SIZE);
		if (memcmp(zerobuf, (u8 *)gptEntry, GPT_ENTRY_SIZE) == 0)
			break;
		j = 0;
		memset(item.szItemName,0,sizeof(item.szItemName));
		while(gptEntry->partition_name[j]!=0)
		{
			item.szItemName[j] = (char)gptEntry->partition_name[j];
			j++;
		}
		
		item.uiItemOffset = le64_to_cpu(gptEntry->starting_lba);
		item.uiItemOffset *= logicalBlockSectors;
		item.uiItemSize = le64_to_cpu(gptEntry->ending_lba) - le64_to_cpu(gptEntry->starting_lba)+1;
		item.uiItemSize *= logicalBlockSectors;
		vecItem.push_back(item);
	}
	return true;
}
int find_uuid_item(CONFIG_ITEM_VECTOR &vecItems, char *pszName)
{
	unsigned int i;
	for(i = 0; i < vecItems.size(); i++){
		if (stricmp(pszName, vecItems[i].szItemName) == 0){
			return i;
		}
	}
	return -1;
}
uint_32 convert_sector_unit(string strValue)
{
	string strUnit;
	char *pszEnd=NULL;
	uint_32 value;

	strUnit = strValue[strValue.size()-1];
	strValue.erase(strValue.end()-1);
	value = strtoul(strValue.c_str(),&pszEnd,0);
	if (*pszEnd)
		return -1;
	if (stricmp(strUnit.c_str(), "k")==0) {
		value *= 2;
	} else if (stricmp(strUnit.c_str(), "m")==0) {
		value *= (2 * 1024);
	} else if (stricmp(strUnit.c_str(), "g")==0) {
		value *= (2 * 1024 * 1024);
	}
	
	return value;
}
bool parse_mcu(unsigned char *pBuf,PARAM_ITEM_VECTOR &vecItem)
{
	STRUCT_PARAM_ITEM item;
	uint_32 entry_data_offset, entry_size, entry_count;

	entry_data_offset = *(uint_32 *)(pBuf + 20);
	entry_data_offset *= SECTOR_SIZE;
	entry_size = *(uint_32 *)(pBuf + 28);
	entry_count = *(uint_32 *)(pBuf + 32);

	for (size_t i = 0; i < entry_count; i++)
	{
		uint_8 *entry = pBuf + entry_data_offset + i * entry_size;

		memset(item.szItemName,0,sizeof(item.szItemName));
		strcpy(item.szItemName, (const char *)entry);
		item.uiItemOffset = *(uint_32 *)(entry + 36);
		item.uiItemSize = *(uint_32 *)(entry + 40);
		
		vecItem.push_back(item);
	}
	

	return true;
}
bool parse_env(unsigned char *pEnv,PARAM_ITEM_VECTOR &vecItem)
{
	STRUCT_PARAM_ITEM item;
	string strLine,strKey,strValue,strPartition,strPartInfo,strPartName,strTmp;
	string::size_type line_size,pos,posColon,posComma;
	uint_8 btResult=0;
	uint_32 uiPartOffset,uiPartSize;
	uint_64 ullCurrentOff;
	vector<string> vecLines;
	char *token = (char *)(pEnv+4);//skip 4 bytes crc

	do
	{
		strLine = token;
		if (strLine.size() == 0)
			break;
		token += strLine.size();
		token++;
		vecLines.push_back(strLine);
	} while (1);
	

	for (size_t i = 0; i < vecLines.size(); i++)
	{
		strLine = vecLines[i];
		line_size = strLine.size();
		if (line_size<=0)
			continue;
		strLine.erase(0,strLine.find_first_not_of(' '));
		strLine.erase(0,strLine.find_first_not_of('\t'));
		strLine.erase(strLine.find_last_not_of(' ')+1);
		strLine.erase(strLine.find_last_not_of('\t')+1);
		if (strLine.size()<=0)
			continue;
		pos = strLine.find("=");
		if (pos==string::npos)
			continue;
		strKey = strLine.substr(0,pos);
		strValue = strLine.substr(pos+1);
		if ((stricmp(strKey.c_str(),"mtdparts")==0) || (stricmp(strKey.c_str(),"blkdevparts")==0))
		{
			btResult |= 0x10;
			posColon = strValue.find(':');
			if (posColon==string::npos)
			{
				continue;
			}
			ullCurrentOff = 0;
			strPartition = strValue.substr(posColon+1);
			pos = 0;
			posComma = strPartition.find(',',pos);
			while (posComma!=string::npos)
			{
				strPartInfo = strPartition.substr(pos,posComma-pos);
				pos = strPartInfo.find('(');
				strValue = strPartInfo.substr(0,pos);
				strPartName = strPartInfo.substr(pos+1);
				strPartName.erase(strPartName.end()-1);
				pos = strValue.find('@');
				if (pos== string::npos) {
					uiPartOffset = ullCurrentOff;
					if (strValue=="-")
					{
						uiPartSize = 0xFFFFFFFF;
					} else {
						uiPartSize = convert_sector_unit(strValue);
						ullCurrentOff += uiPartSize;
					}
				} else {
					strKey = strValue.substr(pos+1);
					uiPartOffset = convert_sector_unit(strKey);
					strKey = strValue.substr(0,pos);
					uiPartSize = convert_sector_unit(strKey);
					ullCurrentOff = uiPartOffset + uiPartSize;
				}
				memset(item.szItemName,0,sizeof(item.szItemName));
				if (strPartName.size()>=sizeof(item.szItemName))
					strncpy(item.szItemName,strPartName.c_str(),sizeof(item.szItemName)-1);
				else
					strcpy(item.szItemName,strPartName.c_str());
				item.uiItemOffset = uiPartOffset;
				item.uiItemSize = uiPartSize;
				vecItem.push_back(item);
				pos = posComma+1;
				posComma = strPartition.find(',',pos);
			}
			strPartInfo = strPartition.substr(pos);
			if (strPartInfo.size()>0)
			{
				strPartInfo = strPartition.substr(pos,posComma-pos);
				pos = strPartInfo.find('(');
				strValue = strPartInfo.substr(0,pos);
				strPartName = strPartInfo.substr(pos+1);
				strPartName.erase(strPartName.end()-1);
				pos = strValue.find('@');
				if (pos== string::npos) {
					uiPartOffset = ullCurrentOff;
					if (strValue=="-")
					{
						uiPartSize = 0xFFFFFFFF;
					} else {
						uiPartSize = convert_sector_unit(strValue);
						ullCurrentOff += uiPartSize;
					}
				} else {
					strKey = strValue.substr(pos+1);
					uiPartOffset = convert_sector_unit(strKey);
					strKey = strValue.substr(0,pos);
					uiPartSize = convert_sector_unit(strKey);
					ullCurrentOff = uiPartOffset + uiPartSize;
				}
				memset(item.szItemName,0,sizeof(item.szItemName));
				if (strPartName.size()>=sizeof(item.szItemName))
					strncpy(item.szItemName,strPartName.c_str(),sizeof(item.szItemName)-1);
				else
					strcpy(item.szItemName,strPartName.c_str());
				item.uiItemOffset = uiPartOffset;
				item.uiItemSize = uiPartSize;
				vecItem.push_back(item);
			}
		}
		else
		{
			continue;
		}
	}

	if ((btResult & 0x10)==0)
	{
		return false;
	}
	return true;
}
