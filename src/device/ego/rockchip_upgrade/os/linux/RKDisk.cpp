#include "RKDisk.h"
typedef enum
{
	FILTER_NONE=0,
	FILTER_DISK=1<<0,
	FILTER_PART=1<<1
}LSBLK_FILTER_TYPE;
int g_lastDiskErr=0;

int RunCommand(string strCmd, string &stdInfo)
{
	int ret;
	char buffer[256];
	FILE* pipe = NULL;

	stdInfo = "";
	pipe = popen(strCmd.c_str(),"r");

	if (!pipe)
    {
		g_lastDiskErr = errno;
		return -1;
    }
    while (fgets(buffer, 256, pipe) != NULL) {
        stdInfo += buffer;
    }
    ret = pclose(pipe);
	if (ret!=0)
	{
		g_lastDiskErr = errno;
		return -2;
	}

	return ret;
}

void ParseMPResult(string strResult, string &strMountPoint)
{
	stringstream stream(strResult);
	string strLine;
	size_t line_size;
	STRING_VECTOR vecItem;

	strMountPoint = "";
	while (!stream.eof())
	{
		getline(stream,strLine);
		line_size = strLine.size();
		if (line_size<=0)
		{
			continue;
		}
		vecItem.clear();
		split_token(strLine," ",vecItem);
		if (vecItem.size() != 2)
			continue;
		strMountPoint = vecItem[1];
		return;
	}
}

bool CRKDisk::FormatVolume(string strVolume)
{
	bool bSuccess=false;
	string stdInfo;
	char cmd[256];
	int ret;

	sprintf(cmd,"mkfs.vfat %s",strVolume.c_str());
	ret = RunCommand(cmd, stdInfo);
	if (ret != 0)
	{
		bSuccess = false;
	}
	else
		bSuccess = true;


	return bSuccess;
}

string CRKDisk::VolumeRoot(string strVolume)
{
	int ret;
	string strMountPoint="";
	string cmd,outInfo;

	cmd = "lsblk -n -p -r -o name,mountpoint " + strVolume;
	ret = RunCommand(cmd,outInfo);
	if (ret!=0)
	{
		return "";
	}

	ParseMPResult(outInfo,strMountPoint);

	return strMountPoint;
}
int CRKDisk::GetLastDiskError()
{
	return g_lastDiskErr;
}

void ParseLSblkResult(string strResult, CmDiskBaseVec &vecAllDisk, uint32_t type=(uint32_t)FILTER_NONE)
{
	stringstream stream(strResult);
	string strLine,strVendor,strDevNum;
	size_t line_size,pos;
	STRING_VECTOR vecItem;
	STRUCT_DISK_BASE disk;
	int major,minor;
	bool bPass;

	while (!stream.eof())
	{
		getline(stream,strLine);
		line_size = strLine.size();
		if (line_size<=0)
		{
			continue;
		}
		vecItem.clear();
		split_token(strLine," ",vecItem);
		
		if (vecItem[2] == "0")
		{//not removable device
			continue;
		}
		bPass = false;
		if (type != FILTER_NONE)
		{
			if ((type & FILTER_DISK) == FILTER_DISK)
			{
				if (vecItem[1] == "disk")
				{
					bPass = true;
				}
			}
			if (!bPass) {
				if ((type & FILTER_PART) == FILTER_PART)
				{
					if (vecItem[1] == "part")
					{
						bPass = true;
					}
				}
			}
		}
		if (!bPass)
		{
			continue;
		}
		
		disk.diskPath = vecItem[0];
		if (vecItem.size()==5)
		{
			pos = vecItem[4].find("\\");
			if (pos != string::npos)
			{
				strVendor = vecItem[4].erase(pos);
			}
			else
				strVendor = vecItem[4];

			disk.diskDesc = strVendor;
		}
		else
			disk.diskDesc = "";
		sscanf(vecItem[3].c_str(),"%d:%d",&major,&minor);
		// printf("%s %d %d\r\n",vecItem[2].c_str(),major,minor);
		disk.diskNum = major*100 + minor;
		vecAllDisk.push_back(disk);
	}
}
void ParseSizeResult(string strResult, vector<uint64_t> &vecSize)
{
	stringstream stream(strResult);
	string strLine;
	size_t line_size;
	STRING_VECTOR vecItem;
	uint64_t volume_size;

	while (!stream.eof())
	{
		getline(stream,strLine);
		line_size = strLine.size();
		if (line_size<=0)
		{
			continue;
		}
		vecItem.clear();
		split_token(strLine," ",vecItem);

		unsigned long long ullVolumeSize = 0;
		sscanf(vecItem[1].c_str(),"%llu",&ullVolumeSize);
		volume_size = ullVolumeSize;
		vecSize.push_back(volume_size);
		//printf("%s %llu\r\n",vecItem[0].c_str(), volume_size);
	}
}


int CRKDisk::GetAllDisk(CmDiskBaseVec &vecAllDisk)
{
	string cmd,outInfo;
	int ret;


	cmd = "lsblk -n -p -r -o name,type,rm,maj:min,vendor";
	ret = RunCommand(cmd,outInfo);
	if (ret!=0)
	{
		return ret;
	}

	ParseLSblkResult(outInfo,vecAllDisk,FILTER_DISK);
	return vecAllDisk.size();

}
int CRKDisk::GetDiskFromVolume(string strVolume)
{
	int ret,i;
	string cmd,outInfo;
	CmDiskBaseVec vecItem;

	cmd = "lsblk -n -p -r -o name,type,rm,maj:min,vendor";
	ret = RunCommand(cmd,outInfo);
	if (ret!=0)
	{
		return ret;
	}

	ParseLSblkResult(outInfo,vecItem,FILTER_DISK);
	if (vecItem.size()==0)
	{
		return 0;
	}
	for (i = 0; static_cast<size_t>(i) < vecItem.size(); i++)
	{
		if (strncmp(vecItem[i].diskPath.c_str(),strVolume.c_str(),strVolume.size())==0)
		{
			return vecItem[i].diskNum;
		}
	}
	
	return 0;
}
int CRKDisk::GetAllVolumeFromDisk(int nDiskNum,STRING_VECTOR &vecAllVolume)
{
	int ret,i,search_major,search_minor,dest_minor;
	string strDisk;
	string cmd,outInfo;
	CmDiskBaseVec vecItem;
	char buffer[256];

	vecAllVolume.clear();
	search_major = (int)(nDiskNum / 100);
	search_minor = nDiskNum % 100;
	sprintf(buffer,"lsblk -n -I %d -p -r -o name,type,rm,maj:min,mountpoint", search_major);
	cmd = buffer;
	ret = RunCommand(cmd,outInfo);
	if (ret!=0)
	{
		return ret;
	}

	ParseLSblkResult(outInfo,vecItem,FILTER_PART);
	if (vecItem.size()==0)
	{
		return 0;
	}
	
	for (i = 0; static_cast<size_t>(i) < vecItem.size(); i++)
	{
		dest_minor = vecItem[i].diskNum % 100;
		if ((dest_minor>search_minor)&&(dest_minor<(search_minor+16)))
			vecAllVolume.push_back(vecItem[i].diskPath);
	}
	
	return vecAllVolume.size();
}
bool CRKDisk::CheckDiskRemovable(int nDiskNum,bool &bRemoved)
{
	(void)nDiskNum;
	bRemoved = true;
	return true;
}
uint64_t CRKDisk::GetDriveSize(string strDrive)
{
	int ret;
	string strDisk;
	string cmd,outInfo;
	vector<uint64_t> vecSize;

	cmd = "lsblk -n -p -r -b -o name,size " + strDrive;
	ret = RunCommand(cmd,outInfo);
	if (ret!=0)
	{
		return 0;
	}

	ParseSizeResult(outInfo,vecSize);
	if (vecSize.size()==0)
	{
		return 0;
	}
	
	return vecSize[0];
}
CRKDisk::CRKDisk(STRUCT_DISK_BASE disk)
{
	m_diskNum = disk.diskNum;
	m_diskHandle = -1;
	m_vecAllVolume.clear();
	m_strDiskPath = disk.diskPath;
	m_strDiskDesc = disk.diskDesc;
	m_diskSize = 0;
	m_bytePerSector = 0;
}
CRKDisk::~CRKDisk()
{
	if (m_diskHandle>=0)
	{
		close(m_diskHandle);
		m_diskHandle = -1;
	}
}
bool CRKDisk::OpenDisk()
{
	off_t disk_pos;
	m_diskHandle = open(m_strDiskPath.c_str(),O_RDWR|O_SYNC);
	if (m_diskHandle<0) {
		printf("open failed, %s err=%d\r\n",m_strDiskPath.c_str(),errno);
		g_lastDiskErr = errno;
		return false;
	}
	disk_pos = lseek(m_diskHandle, 0, SEEK_END);
	if (disk_pos == (off_t)-1)
	{
		printf("seek failed,%d %s err=%d\r\n",m_diskHandle, m_strDiskPath.c_str(),errno);
		return false;
	}
	m_diskSize = disk_pos;
	lseek(m_diskHandle,0,SEEK_SET);
	return true;
}

bool CRKDisk::ReadDisk(size_t nStart,int nCount,uint8_t *pReceiveBuf)
{
	if (m_diskNum<0)
	{
		g_lastDiskErr = DISK_ERROR_NO_DISKNUM;
		return false;
	}
	if (m_diskHandle<0)
	{
		g_lastDiskErr = DISK_ERROR_HANDLE_NOTEXIST;
		return false;
	}
	bool bRet;
	bRet = OffsetDisk(nStart,SEEK_SET);
	if (!bRet)
	{
		return false;
	}
	ssize_t dwRead;
	dwRead = read(m_diskHandle,pReceiveBuf,nCount);
	if (dwRead<0)
	{
		g_lastDiskErr = errno;
		return false;
	}
	if ((dwRead!=nCount)&&(dwRead!=0))
	{
		g_lastDiskErr = DISK_ERROR_READ_PARTIAL;
		return false;
	}

	return true;
}
bool CRKDisk::WriteDisk(size_t nStart,int nCount,uint8_t *pSendBuf)
{
	if (m_diskNum<0)
	{
		g_lastDiskErr = DISK_ERROR_NO_DISKNUM;
		return false;
	}
	if (m_diskHandle<0)
	{
		g_lastDiskErr = DISK_ERROR_HANDLE_NOTEXIST;
		return false;
	}
	bool bRet;
	bRet = OffsetDisk(nStart,SEEK_SET);
	if (!bRet)
	{
		return false;
	}
	ssize_t dwWrite;
	dwWrite = write(m_diskHandle,pSendBuf,nCount);
	if (dwWrite<0)
	{
		g_lastDiskErr = errno;
		return false;
	}
	if (dwWrite!=nCount)
	{
		g_lastDiskErr = DISK_ERROR_WRITE_PARTIAL;
		return false;
	}
	
	return true;
}
bool CRKDisk::OffsetDisk(uint64_t nMoveBytes,uint32_t dwMovestart)
{
	if (m_diskNum<0)
	{
		g_lastDiskErr = DISK_ERROR_NO_DISKNUM;
		return false;
	}
	if (m_diskHandle<0)
	{
		g_lastDiskErr = DISK_ERROR_HANDLE_NOTEXIST;
		return false;
	}
	off_t ret;
	ret = lseek(m_diskHandle,nMoveBytes,dwMovestart);
	if (ret==(off_t)-1)
	{
		g_lastDiskErr = errno;
		return false;
	}
	return true;
}
void CRKDisk::CloseDisk()
{
	if (m_diskHandle>=0)
	{
		close(m_diskHandle);
		m_diskHandle = -1;
	}
	m_diskNum = -1;
	m_vecAllVolume.clear();
	m_strDiskPath = m_strDiskDesc = "";
	m_diskSize = 0;
	m_bytePerSector = 0;
}

bool CRKDisk::LockAndDismount()
{
	int ret,nDriveCount,i;
	string strMountPoint,strDiskMP="";
	string cmd,outInfo;

	if (m_diskNum<0)
	{
		g_lastDiskErr = DISK_ERROR_NO_DISKNUM;
		return false;
	}
	if (m_diskHandle<0)
	{
		g_lastDiskErr = DISK_ERROR_HANDLE_NOTEXIST;
		return false;
	}

	nDriveCount = GetAllVolumeFromDisk(m_diskNum,m_vecAllVolume);
	if (nDriveCount<0)
	{
		g_lastDiskErr = DISK_ERROR_GETALLVOLUME_FAIL;
		return false;
	}

	strDiskMP = VolumeRoot(m_strDiskPath);
	

	for (i = 0; static_cast<size_t>(i) < m_vecAllVolume.size(); i++)
	{
		strMountPoint = VolumeRoot(m_vecAllVolume[i]);
		if (strMountPoint.size()>0)
		{
			cmd = "umount -A " + m_vecAllVolume[i];
			ret = RunCommand(cmd,outInfo);
			if (ret!=0)
			{
				return false;
			}
			if (strMountPoint == strDiskMP)
			{
				strDiskMP = "";
			}
		}
	}
	if (strDiskMP.size()>0)
	{
		cmd = "umount -A " + m_strDiskPath;
		ret = RunCommand(cmd,outInfo);
		if (ret!=0)
		{
			return false;
		}
	}
	return true;
}

bool CRKDisk::GetAllDrive(STRING_VECTOR &vecAllDrive,bool bRefresh)
{
	if (m_diskNum<0)
	{
		g_lastDiskErr = DISK_ERROR_NO_DISKNUM;
		return false;
	}
	if (m_diskHandle<0)
	{
		g_lastDiskErr = DISK_ERROR_HANDLE_NOTEXIST;
		return false;
	}
	if (bRefresh)
	{
		int nDriveCount ;
		nDriveCount = GetAllVolumeFromDisk(m_diskNum,m_vecAllVolume);
		if (nDriveCount<0)
		{
			g_lastDiskErr = DISK_ERROR_GETALLVOLUME_FAIL;
			return false;
		}
	}
	vecAllDrive.clear();
	int i;
	for (i=0;static_cast<size_t>(i)<m_vecAllVolume.size();i++)
	{
		vecAllDrive.push_back(m_vecAllVolume[i]);
	}
	
	return true;
}

uint64_t CRKDisk::GetDiskSize()
{
	if (m_diskNum<0)
	{
		g_lastDiskErr = DISK_ERROR_NO_DISKNUM;
		return 0;
	}
	if (m_diskHandle<0)
	{
		g_lastDiskErr = DISK_ERROR_HANDLE_NOTEXIST;
		return 0;
	}
	return m_diskSize;
}

bool CRKDisk::IsRemovedDisk(bool &bRemoved)
{
	bRemoved = true;
	return true;
}

bool CRKDisk::UpdateDiskLayout()
{
	int ret;
	string strDisk;
	string cmd,outInfo;

	if (m_diskNum<0)
	{
		g_lastDiskErr = DISK_ERROR_NO_DISKNUM;
		return false;
	}
	if (m_diskHandle<0)
	{
		g_lastDiskErr = DISK_ERROR_HANDLE_NOTEXIST;
		return false;
	}
	
	cmd = "partprobe " + m_strDiskPath;
	ret = RunCommand(cmd,outInfo);
	if (ret!=0)
	{
		return false;
	}
	
	return true;
}