#ifndef RK_DISK_H
#define RK_DISK_H
#include "../../DefineHeader.h"
#define DISK_ERROR_NONE 0
#define DISK_ERROR_NEW_MEMORY -1
#define DISK_ERROR_HANDLE_EXIST -2
#define DISK_ERROR_NO_DISKNUM -3
#define DISK_ERROR_OPEN_FAIL -4
#define DISK_ERROR_GETSIZE_FAIL -5
#define DISK_ERROR_HANDLE_NOTEXIST -6
#define DISK_ERROR_GETALLVOLUME_FAIL -7
#define DISK_ERROR_NO_VOLUME -8
#define DISK_ERROR_READ_PARTIAL -9
#define DISK_ERROR_WRITE_PARTIAL -10
#define DISK_ERROR_LOAD_FMIFS -11
#define DISK_ERROR_CHAR_CONVERT -12
#define DISK_ERROR_FORMAT_FAIL -13
#define DISK_ERROR_EJECTMEDIA_FAIL -14
#define DISK_ERROR_LOADMEDIA_FAIL -15
typedef struct
{
	int diskNum;
	string diskPath;
	string diskDesc;
}STRUCT_DISK_BASE,*PSTRUCT_DISK_BASE;
typedef vector<STRUCT_DISK_BASE> CmDiskBaseVec;

class CRKDisk
{
public:
	static int GetAllDisk(CmDiskBaseVec &vecAllDisk);
	static int GetDiskFromVolume(string strVolume);
	static int GetAllVolumeFromDisk(int nDiskNum,STRING_VECTOR &vecAllVolume);
	static bool CheckDiskRemovable(int nDiskNum,bool &bRemoved);
	static uint64_t GetDriveSize(string strDrive);
	static bool FormatVolume(string strVolume);
	static string VolumeRoot(string strVolume);
	static int GetLastDiskError();

	CRKDisk(STRUCT_DISK_BASE disk);
	virtual ~CRKDisk();
	bool OpenDisk();
	bool ReadDisk(size_t nStart,int nCount,uint8_t *pReceiveBuf);
	bool WriteDisk(size_t nStart,int nCount,uint8_t *pSendBuf);
	void CloseDisk();
	bool GetAllDrive(STRING_VECTOR &vecAllDrive,bool bRefresh=false);
	bool LockAndDismount();
	bool IsRemovedDisk(bool &bRemoved);
	bool UpdateDiskLayout();
	uint64_t GetDiskSize();
private:
	int m_diskNum;
	int m_diskHandle;
	STRING_VECTOR m_vecAllVolume;
	string m_strDiskPath;
	string m_strDiskDesc;
	uint64_t m_diskSize;
	int m_bytePerSector;
	bool OffsetDisk(uint64_t nMoveBytes,uint32_t dwMovestart);
};
#endif