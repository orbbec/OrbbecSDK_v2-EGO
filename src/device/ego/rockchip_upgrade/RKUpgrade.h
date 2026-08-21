#ifndef RKUPGRADE_HEADER
#define RKUPGRADE_HEADER
#include "DefineHeader.h"
#include "RKDevice.h"
#include "RKAndroidDevice.h"

typedef enum
{
	DOWNLOADBOOT_STEP=1,
	DOWNLOADIDBLOCK_STEP=4,
	DOWNLOADIMAGE_STEP=7,
	TESTDEVICE_STEP=10,
	RESETDEVICE_STEP=13,
	FORMATDISK_STEP=16,
	COPYDATA_STEP=19,
	WAITMSC_STEP=22,
	WAITLOADER_STEP=25,
	WAITMASKROM_STEP=28,
	ERASEIDB_STEP=31,
	SWITCHMSC_STEP=34,
	CHECKCHIP_STEP=37,
	PREPAREIDB_STEP=40,
	MUTEXRESETDEVICE_STEP=43,
	GETOLDDISKSIZE_STEP=46,
	READSN_STEP=49,
	WRITESN_STEP=52,
	ERASEALLBLOCKS_STEP=55,
	GETBLOCKSTATE_STEP=58,
	GETFLASHINFO_STEP=61,
	WRITEBACK_STEP=64,
	FINDUSERDISK_STEP=67,
	SHOWUSERDISK_STEP=70,
	READMAC_STEP=73,
	WRITEMAC_STEP=76,
	READBT_STEP=79,
	WRITEBT_STEP=82,
	LOWERFORMAT_STEP=85,
	READIMEI_STEP=88,
	WRITEIMEI_STEP=91,
	SHOWDATADISK_STEP=94,
	FINDDATADISK_STEP=97,
	FORMATDATADISK_STEP=100,
	COPYDATADISK_STEP=103,
	READUID_STEP=106,
	READCUSTOMDATA_STEP=109,
	WRITECUSTOMDATA_STEP=112,
	SETRESETFLAG_STEP=115,
	POWEROFF_STEP=118,
	READALLINFO_STEP=121,
	WRITEALLINFO_STEP=124,
	RESETMSC_STEP=127,
	READWIFI_STEP=130,
	WRITEWIFI_STEP=133,
	READ_CAPABILITY_STEP=136,
	CLEAR_SECTOR3_STEP=139
}ENUM_UPGRADE_STEP;
typedef struct  
{
	ENUM_UPGRADE_STEP emStep;
	uint_32    uiSupport;
	void   *pMutexCS;
	bool    bMutex;
	bool    bSync;
	bool    bJump;
	uint_8    bJumpParam;
	bool    bWriteBack;
	uint_32	uiParam;
}STRUCT_UPGRADE_STEP,*PSTRUCT_UPGRADE_STEP;
typedef vector<STRUCT_UPGRADE_STEP>  RK_UPGRADE_SET;
class CDeviceFactory
{
public:
	static CRKDevice* CreateDevice(STRUCT_RKDEVICE_DESC &device);
	static void FreeDevice(CRKDevice *pDevice);
};
class CRKUpgrade
{
public:
	CRKDevice *m_pDevice;
	void SetCallBackPointer(UpgradeStepPromptCB value);
 	property<CRKUpgrade,UpgradeStepPromptCB,WRITE_ONLY> CallBackPointer;
	void SetDeviceObject(void *value);
 	property<CRKUpgrade,void *,WRITE_ONLY> DeviceObject;
	void SetScanObject(void *value);
 	property<CRKUpgrade,void *,WRITE_ONLY> ScanObject;

	CRKUpgrade(const RK_UPGRADE_SET &upgradeStepSet,ENUM_WORKFLOW workFlow);
	~CRKUpgrade();
	bool InitializeStep(const RK_UPGRADE_SET &upgradeStepSet,ENUM_WORKFLOW workFlow);
	bool SetObject(CRKDevice *pDevice,CRKScan *pScan);
	static bool CreateUpgradeStep(ENUM_RKDEVICE_TYPE type,RK_UPGRADE_SET& upgradeStepSet,uint_32 uiMscOper=MSC_NONE_OPER);
	static bool CreateRestoreStep(ENUM_RKDEVICE_TYPE type,RK_UPGRADE_SET& upgradeStepSet,uint_32 uiMscOper=MSC_NONE_OPER);
	static bool CreateDownloadIDBStep(ENUM_RKDEVICE_TYPE type,RK_UPGRADE_SET& upgradeStepSet);
	static bool CreateDownloadImageStep(ENUM_RKDEVICE_TYPE type,RK_UPGRADE_SET& upgradeStepSet);
	static bool CreateEraseAllBlocksStep(ENUM_RKDEVICE_TYPE type,RK_UPGRADE_SET& upgradeStepSet);
	bool SetStep(ENUM_UPGRADE_STEP emStep,uint_32 uiSupport);
	bool RemoveStep(ENUM_UPGRADE_STEP emStep);
	bool  Run();
protected:
private:
	CRKScan   *m_pScan;
	RK_UPGRADE_SET m_UpgradeProcedure;
	ENUM_WORKFLOW m_workFlow;
	UpgradeStepPromptCB m_callBackProc;
	
};
#define MAX_DRIVE_NUM	5
#endif