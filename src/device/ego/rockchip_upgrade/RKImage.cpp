#include "RKImage.h"
#include "MD5Checksum.h"
#ifdef _WIN32
#include <windows.h>
#endif

uint_32 CRKImage::GetVersion()
{
	return m_version;
}
uint_32 CRKImage::GetMergeVersion()
{
	return m_mergeVersion;
}
STRUCT_RKTIME CRKImage::GetReleaseTime()
{
	return m_releaseTime;
}
ENUM_RKDEVICE_TYPE CRKImage::GetSupportDevice()
{
	return m_supportDevice;
}
ENUM_OS_TYPE CRKImage::GetOsType()
{
	uint_32 *pOsType;
	pOsType = (uint_32 *)&m_reserved[4];
	return (ENUM_OS_TYPE)*pOsType;
}
uint_16 CRKImage::GetBackupSize()
{
	uint_16 *pBackupSize;
	pBackupSize = (uint_16 *)&m_reserved[12];
	return *pBackupSize;
}
uint_32 CRKImage::GetBootOffset()
{
	return m_bootOffset;
}
uint_32 CRKImage::GetBootSize()
{
	return m_bootSize;
}
uint_32 CRKImage::GetFWOffset()
{
	return m_fwOffset;
}
long long CRKImage::GetFWSize()
{
	return m_fwSize;
}
bool CRKImage::Md5Check(long long nCheckSize)
{
	string strNewMd5="";
	strNewMd5 = CMD5Checksum::GetMD5(m_pFile,nCheckSize,m_baseOffset);
	if (strNewMd5.size()==32)
	{
		uint_8 newMd5[32];

		memcpy(newMd5,strNewMd5.c_str(),32);
	
		if ( memcmp(newMd5,m_md5,32)!=0 )
			return false;
		else
			return true;
	}
	else
		return false;
	
}
bool CRKImage::SaveBootFile(string filename)
{
	FILE *file=NULL;
	file = fopen(filename.c_str(),"wb+");
	if ( !file)
	{
		return false;
	}
	uint_8 buffer[1024];
	uint_32 dwBufferSize=1024;
	uint_32 dwBootSize=m_bootSize;
	uint_32 dwReadSize;
	fseek(m_pFile,m_baseOffset+m_bootOffset,SEEK_SET);
	do 
	{
		dwReadSize = (dwBootSize>=1024)?dwBufferSize:dwBootSize;
		size_t bootRead = fread(buffer,1,dwReadSize,m_pFile);
		(void)bootRead;
		fwrite(buffer,1,dwReadSize,file);
		dwBootSize -= dwReadSize;
	} while (dwBootSize>0);
	fclose(file);
	return true;
}
bool CRKImage::SaveFWFile(string filename)
{
	FILE *file=NULL;
	file = fopen(filename.c_str(),"wb+");
	if ( !file )
	{
		return false;
	}
	uint_8 buffer[1024];
	uint_32 dwBufferSize=1024;
	long long dwFWSize=m_fwSize;
	uint_32 dwReadSize;
	fseek(m_pFile,m_baseOffset+m_fwOffset,SEEK_SET);
	do 
	{
		dwReadSize = (dwFWSize>=1024)?dwBufferSize:dwFWSize;
		size_t fwRead = fread(buffer,1,dwReadSize,m_pFile);
		(void)fwRead;
		fwrite(buffer,1,dwReadSize,file);
		dwFWSize -= dwReadSize;
	} while (dwFWSize>0);
	fclose(file);
	return true;
}
bool CRKImage::GetData(long long dwOffset,uint_32 dwSize,uint_8* lpBuffer)
{
	if ( dwOffset<0 )
	{
		m_error = ERR_INVALID_READ;
		return false;
	}
	if ( dwOffset+dwSize >m_fileSize)
	{
		m_error = ERR_INVALID_READ;
		return false;
	}

	fseek(m_pFile,m_baseOffset+dwOffset,SEEK_SET);
	uint_32 uiActualRead;
	uiActualRead = fread(lpBuffer,1,dwSize,m_pFile);
	if (dwSize!=uiActualRead)
	{
		m_error = ERR_READ_IMAGE;
		return false;
	}
	return true;
}
void CRKImage::GetReservedData(uint_8* &lpData,uint_16 &usSize)
{
	lpData = m_reserved;
	usSize = IMAGE_RESERVED_SIZE;
}
uint_32 CRKImage::GetStorage()
{
	return m_storage;
}
void CRKImage::SetStorage(uint_32 value)
{
	m_storage = value;
}
int CRKImage::GetError()
{
	return m_error;
}
int CRKImage::ObjectInit(string filename,bool bDoCheck,uint_64 baseOffset,uint_64 baseSize)
{
	int ret;
	// A long path previously overflowed this fixed buffer and could crash the
	// RockUSB DLL before the firmware transfer began.
	const char *szName = filename.c_str();

	if (!exist_file(filename))
	{
		return -1;
	}
	
	transform(filename.begin(),filename.end(),filename.begin(),(int(*)(int))tolower);
	if (filename.find(".bin")!=string::npos)
	{
		m_bSingleBootFlag = true;
	}

	if (baseOffset==static_cast<uint_64>(-1))
	{
		m_baseOffset = 0;
	}
	else
		m_baseOffset = baseOffset;

	#ifdef _WIN32
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, szName, -1, NULL, 0);
    if(length == 0) {
        m_error = ERR_OPEN_IMAGE;
        return -3;
    }
    vector<wchar_t> widePath(length);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, szName, -1, widePath.data(), length);
    m_pFile = _wfopen(widePath.data(), L"rb");
#else
    m_pFile = fopen(szName,"rb");
#endif
	if ( !m_pFile)
	{
		m_error = ERR_OPEN_IMAGE;
		return -3;
	}

	m_baseFileSize = baseSize;
	if (baseSize==static_cast<uint_64>(-1))
	{
		fseek(m_pFile,0,SEEK_END);
		m_fileSize = ftell(m_pFile);
		fseek(m_pFile,0,SEEK_SET);
	}
	else
	{
		m_fileSize = baseSize;
	}
	
	
	int nMd5DataSize;
	long long ulFwSize;
	STRUCT_RKIMAGE_HEAD imageHead;
	if (!m_bSingleBootFlag)
	{
		fseek(m_pFile,m_baseOffset,SEEK_SET);
		size_t headRead = fread((uint_8*)(&imageHead),1,sizeof(STRUCT_RKIMAGE_HEAD),m_pFile);
		(void)headRead;
		
		if ( imageHead.uiTag!=0x57464B52 )
		{
			m_error = ERR_WRONG_TAG;
			return -4;
		}
		if ((imageHead.reserved[14]=='H')&&(imageHead.reserved[15]=='I'))
		{
			ulFwSize = *((uint_32 *)(&imageHead.reserved[16]));
			ulFwSize <<= 32;
			ulFwSize += imageHead.dwFWOffset;
			ulFwSize += imageHead.dwFWSize;
		}
		else
			ulFwSize = imageHead.dwFWOffset+imageHead.dwFWSize;
		nMd5DataSize = GetImageSize()-ulFwSize;
		if (nMd5DataSize>=96)
		{//sign image
			m_bSignFlag = true;
			m_signMd5Size = nMd5DataSize-32;
			fseek(m_pFile,m_baseOffset + ulFwSize,SEEK_SET);
			size_t md5Read = fread(m_md5,1,32,m_pFile);
			(void)md5Read;
			size_t signRead = fread(m_signMd5,1,nMd5DataSize-32,m_pFile);
			(void)signRead;
		}
		else
		{
			if (m_baseFileSize!=static_cast<uint_64>(-1))
			{
				fseek(m_pFile,m_baseOffset+m_baseFileSize-32,SEEK_SET);
			}
			else
				fseek(m_pFile,-32,SEEK_END);
			size_t md5Read = fread(m_md5,1,32,m_pFile);
			(void)md5Read;
		}
		if (bDoCheck){
			if (!Md5Check(ulFwSize))
			{
				m_error = ERR_WRONG_HASH;
				return -5;
			}
		}
		

		m_version = imageHead.dwVersion;
		m_mergeVersion = imageHead.dwMergeVersion;
		m_releaseTime.usYear = imageHead.stReleaseTime.usYear;
		m_releaseTime.ucMonth = imageHead.stReleaseTime.ucMonth;
		m_releaseTime.ucDay = imageHead.stReleaseTime.ucDay;
		m_releaseTime.ucHour = imageHead.stReleaseTime.ucHour;
		m_releaseTime.ucMinute = imageHead.stReleaseTime.ucMinute;
		m_releaseTime.ucSecond = imageHead.stReleaseTime.ucSecond;
		m_supportDevice = imageHead.emSupportChip;
		m_bootOffset = imageHead.dwBootOffset;
		m_bootSize = imageHead.dwBootSize;
		m_fwOffset = imageHead.dwFWOffset;
		m_fwSize = ulFwSize - m_fwOffset;
		m_storage = imageHead.dwBootStorage;
		
		memcpy(m_reserved,imageHead.reserved,IMAGE_RESERVED_SIZE);
	}
	else
	{
		m_bootOffset = 0;
		m_bootSize = m_fileSize;
	}

	
	uint_8* lpBoot;
	lpBoot = new uint_8[m_bootSize];
	fseek(m_pFile,m_baseOffset+m_bootOffset,SEEK_SET);
	size_t bootDataRead = fread(lpBoot,1,m_bootSize,m_pFile);
	(void)bootDataRead;
	m_bootObject = new CRKBoot(lpBoot,m_bootSize);
	ret = m_bootObject->ObjectInit();
	if (ret!=0)
	{
		if (ret==-2)
			m_error = ERR_BOOT_CRC;
		else if (ret==-3)
			m_error = ERR_BOOT_TAG;
		else if (ret==-4)
			m_error = ERR_BOOT_VER;
		return -6;
	}
	if (m_bSingleBootFlag)
	{
		m_supportDevice = m_bootObject->SupportDevice;
		uint_32 *pOsType;
		pOsType = (uint_32 *)&m_reserved[4];
		*pOsType = (uint_32)RK_OS;
		fclose(m_pFile);
		m_pFile = NULL;
	}
	return 0;

}
CRKImage::CRKImage()
{
	Version.setContainer(this);
	Version.getter(&CRKImage::GetVersion);
	MergeVersion.setContainer(this);
	MergeVersion.getter(&CRKImage::GetMergeVersion);
	ReleaseTime.setContainer(this);
	ReleaseTime.getter(&CRKImage::GetReleaseTime);
	SupportDevice.setContainer(this);
	SupportDevice.getter(&CRKImage::GetSupportDevice);
	OsType.setContainer(this);
	OsType.getter(&CRKImage::GetOsType);
	BackupSize.setContainer(this);
	BackupSize.getter(&CRKImage::GetBackupSize);
	BootOffset.setContainer(this);
	BootOffset.getter(&CRKImage::GetBootOffset);
	BootSize.setContainer(this);
	BootSize.getter(&CRKImage::GetBootSize);
	FWOffset.setContainer(this);
	FWOffset.getter(&CRKImage::GetFWOffset);
	FWSize.setContainer(this);
	FWSize.getter(&CRKImage::GetFWSize);
	SignFlag.setContainer(this);
	SignFlag.getter(&CRKImage::GetSignFlag);
	SingleBootFlag.setContainer(this);
	SingleBootFlag.getter(&CRKImage::GetSingleBootFlag);
	Storage.setContainer(this);
	Storage.getter(&CRKImage::GetStorage);
	Storage.setter(&CRKImage::SetStorage);
	Error.setContainer(this);
	Error.getter(&CRKImage::GetError);
	 
	m_bootObject = NULL;
	m_pFile = NULL;
	m_storage = 0;
	m_error = 0;
	m_bSignFlag = false;
	m_bSingleBootFlag = false;
	m_signMd5Size = 0;
	memset(m_md5,0,32);
	memset(m_signMd5,0,256);

}
CRKImage::~CRKImage()
{
	if (m_pFile)
	{
		fclose(m_pFile);
		m_pFile = NULL;
	}
	if (m_bootObject)
	{
		delete m_bootObject;
		m_bootObject = NULL;
	}
}

long long CRKImage::GetImageSize()
{
	return m_fileSize;
}
int CRKImage::GetMd5Data(uint_8* &lpMd5,uint_8* &lpSignMd5)
{
	lpMd5 = m_md5;
	lpSignMd5 = m_signMd5;
	return m_signMd5Size;
}
bool CRKImage::GetSignFlag()
{
	return m_bSignFlag;
}
bool CRKImage::GetSingleBootFlag()
{
	return m_bSingleBootFlag;
}