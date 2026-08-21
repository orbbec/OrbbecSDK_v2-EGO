#ifndef RKSCAN_HEADER
#define RKSCAN_HEADER
#include "../../DefineHeader.h"
#include "../../RKLog.h"


typedef struct  
{
	uint_16 usVid;
	uint_16 usPid;
	ENUM_RKDEVICE_TYPE emDeviceType;
}STRUCT_DEVICE_CONFIG,*PSTRUCT_DEVICE_CONFIG;

typedef vector<STRUCT_DEVICE_CONFIG> RKDEVICE_CONFIG_SET;
class CRKScan
{
public:
	
	uint_32 GetMSC_TIMEOUT();
	void SetMSC_TIMEOUT(uint_32 value);
 	property<CRKScan,uint_32,READ_WRITE> MSC_TIMEOUT;
	
	uint_32 GetRKUSB_TIMEOUT();
	void SetRKUSB_TIMEOUT(uint_32 value);
 	property<CRKScan,uint_32,READ_WRITE> RKUSB_TIMEOUT;
	
	int GetDEVICE_COUNTS();
 	property<CRKScan,int,READ_ONLY> DEVICE_COUNTS;

	CRKScan(uint_32 uiMscTimeout=30,uint_32 uiRKusbTimeout=5);
	void SetVidPid(uint_16 mscVid=0,uint_16 mscPid=0);
	void AddRockusbVidPid(uint_16 newVid,uint_16 newPid,uint_16 oldVid,uint_16 oldPid);
	bool FindRockusbVidPid(ENUM_RKDEVICE_TYPE type,uint_16 &usVid,uint_16 &usPid);
	int Search(uint_32 type);

	bool Wait(STRUCT_RKDEVICE_DESC &device,ENUM_RKUSB_TYPE usbType,uint_16 usVid=0,uint_16 usPid=0);
	bool Wait2(STRUCT_RKDEVICE_DESC &device,ENUM_RKUSB_TYPE usbType,uint_16 usVid=0,uint_16 usPid=0);
	int GetPos(uint_32 locationID);
	bool GetDevice(STRUCT_RKDEVICE_DESC &device,int pos);


	bool SetLogObject( CRKLog *pLog );
	~CRKScan();	
protected:
private:

	uint_32   m_waitRKusbSecond;
	uint_32   m_waitMscSecond;
	CRKLog *m_log;
	RKDEVICE_DESC_SET m_list;
	RKDEVICE_CONFIG_SET m_deviceConfigSet;
	RKDEVICE_CONFIG_SET m_deviceMscConfigSet;

	int FindConfigSetPos(RKDEVICE_CONFIG_SET &devConfigSet,uint_16 vid,uint_16 pid);
	int FindWaitSetPos(const RKDEVICE_CONFIG_SET &waitDeviceSet,uint_16 vid,uint_16 pid);
	void EnumerateUsbDevice(RKDEVICE_DESC_SET &list,uint_32 &uiTotalMatchDevices);
	void FreeDeviceList(RKDEVICE_DESC_SET &list);
	bool IsRockusbDevice(ENUM_RKDEVICE_TYPE &type,uint_16 vid,uint_16 pid);

};
#endif