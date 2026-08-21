#include "RKUpgrade.h"



ENUM_RKDEVICE_TYPE g_arrayNanoDeviceType[] = {RKNANO_DEVICE,RKSMART_DEVICE,(ENUM_RKDEVICE_TYPE)0x262d};
set<ENUM_RKDEVICE_TYPE> g_setNanoDeviceType(g_arrayNanoDeviceType,g_arrayNanoDeviceType+3);


bool IsValidDeviceType(set<ENUM_RKDEVICE_TYPE> &searchSet,ENUM_RKDEVICE_TYPE type)
{
	set<ENUM_RKDEVICE_TYPE>::iterator iter;
	iter = searchSet.find(type);
	if (iter==searchSet.end())
	{
		return false;
	}
	return true;
}


void CRKUpgrade::SetDeviceObject(void *value)
{
	if (value)
	{
		m_pDevice = (CRKDevice *)value;
	}
}
void CRKUpgrade::SetScanObject(void *value)
{
	if (value)
	{
		m_pScan = (CRKScan *)value;
	}
}

void CRKUpgrade::SetCallBackPointer(UpgradeStepPromptCB value)
{
	if (value)
	{
		m_callBackProc = value;
	}
}
bool CRKUpgrade::InitializeStep(const RK_UPGRADE_SET &upgradeStepSet,ENUM_WORKFLOW workFlow)
{
	m_UpgradeProcedure.clear();
	int i;
	for ( i=0;static_cast<size_t>(i)<upgradeStepSet.size();i++ )
	{
		m_UpgradeProcedure.push_back(upgradeStepSet[i]);
	}
	m_workFlow = workFlow;
	return true;
}
CRKUpgrade::CRKUpgrade(const RK_UPGRADE_SET &upgradeStepSet,ENUM_WORKFLOW workFlow)
{
	CallBackPointer.setContainer(this);
 	CallBackPointer.setter(&CRKUpgrade::SetCallBackPointer);
	DeviceObject.setContainer(this);
 	DeviceObject.setter(&CRKUpgrade::SetDeviceObject);

	ScanObject.setContainer(this);
 	ScanObject.setter(&CRKUpgrade::SetScanObject);

	
	m_pDevice = NULL;
	m_pScan = NULL;
	m_callBackProc = NULL;
	InitializeStep(upgradeStepSet,workFlow);
}
CRKUpgrade::~CRKUpgrade()
{
	if (m_pDevice)
	{
		CDeviceFactory::FreeDevice(m_pDevice);	
	}
	if (m_pScan)
	{
		delete m_pScan;
		m_pScan = NULL;
	}
}

bool CRKUpgrade::CreateEraseAllBlocksStep(ENUM_RKDEVICE_TYPE type,RK_UPGRADE_SET& upgradeStepSet)
{
	STRUCT_UPGRADE_STEP step;

	if (IsValidDeviceType(g_setNanoDeviceType,type))
	{
		return false;
	}

	upgradeStepSet.clear();

	step.emStep = DOWNLOADBOOT_STEP;
	step.bMutex = true;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = WAITMASKROM_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	step.uiParam = RKUSB_NONE;
	upgradeStepSet.push_back(step);
	
	step.emStep = TESTDEVICE_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM|RKUSB_LOADER;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = GETFLASHINFO_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM|RKUSB_LOADER;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = READ_CAPABILITY_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM|RKUSB_LOADER;
	step.bJump = true;
	step.bJumpParam = 9;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
	
	
	step.emStep = PREPAREIDB_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM|RKUSB_LOADER;
	step.bJump = true;
	step.bJumpParam = 8;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
	
	step.emStep = ERASEIDB_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM|RKUSB_LOADER;
	step.bJump = false;
	step.bWriteBack = true;
	upgradeStepSet.push_back(step);

	step.emStep = WRITEBACK_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM|RKUSB_LOADER;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = RESETDEVICE_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM|RKUSB_LOADER;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
	
	step.emStep = WAITMASKROM_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM|RKUSB_LOADER;
	step.bJump = false;
	step.bWriteBack = false;
	step.uiParam = RKUSB_MASKROM;
	upgradeStepSet.push_back(step);
	

	step.emStep = DOWNLOADBOOT_STEP;
	step.bMutex = true;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = WAITMASKROM_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	step.uiParam = RKUSB_NONE;
	upgradeStepSet.push_back(step);
	
	step.emStep = TESTDEVICE_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
	
	step.emStep = GETFLASHINFO_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = ERASEALLBLOCKS_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = true;
	upgradeStepSet.push_back(step);

	step.emStep = WRITEBACK_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = RESETDEVICE_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	return true;
}


bool CRKUpgrade::CreateDownloadImageStep(ENUM_RKDEVICE_TYPE type,RK_UPGRADE_SET& upgradeStepSet)
{
	STRUCT_UPGRADE_STEP step;

	if (IsValidDeviceType(g_setNanoDeviceType,type))
	{
		return false;
	}

	upgradeStepSet.clear();

	step.emStep = TESTDEVICE_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_LOADER;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = CHECKCHIP_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_LOADER;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
	
	step.emStep = GETFLASHINFO_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_LOADER;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
	
	step.emStep = DOWNLOADIMAGE_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_LOADER;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
	
	step.emStep = RESETDEVICE_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_LOADER;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	return true;
}
bool CRKUpgrade::CreateDownloadIDBStep(ENUM_RKDEVICE_TYPE type,RK_UPGRADE_SET& upgradeStepSet)
{
	STRUCT_UPGRADE_STEP step;

	if (IsValidDeviceType(g_setNanoDeviceType,type))
	{
		return false;
	}
	uint_32 uiSupport = RKUSB_MASKROM|RKUSB_LOADER;
		

	upgradeStepSet.clear();
	
	step.emStep = DOWNLOADBOOT_STEP;
	step.bMutex = true;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = WAITMASKROM_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	step.uiParam = RKUSB_NONE;
	upgradeStepSet.push_back(step);

	step.emStep = TESTDEVICE_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = READ_CAPABILITY_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = CHECKCHIP_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = GETFLASHINFO_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = PREPAREIDB_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = DOWNLOADIDBLOCK_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = true;
	upgradeStepSet.push_back(step);


	step.emStep = RESETDEVICE_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM|RKUSB_LOADER;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

		
	return true;
}
bool CRKUpgrade::CreateUpgradeStep(ENUM_RKDEVICE_TYPE type,RK_UPGRADE_SET& upgradeStepSet,uint_32 uiMscOper)
{
	(void)uiMscOper;
	STRUCT_UPGRADE_STEP step;

	if (IsValidDeviceType(g_setNanoDeviceType,type))
	{
		return false;
	}

	uint_32 uiSupport = RKUSB_MASKROM|RKUSB_LOADER;

	upgradeStepSet.clear();

	step.emStep = DOWNLOADBOOT_STEP;
	step.bMutex = true;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = WAITMASKROM_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	step.uiParam = RKUSB_NONE;
	upgradeStepSet.push_back(step);

	step.emStep = TESTDEVICE_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = READ_CAPABILITY_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = CHECKCHIP_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = GETFLASHINFO_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = PREPAREIDB_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = DOWNLOADIDBLOCK_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = true;
	step.bJumpParam = 3;
	step.bWriteBack = true;
	upgradeStepSet.push_back(step);

	step.emStep = RESETDEVICE_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
	
	step.emStep = WAITLOADER_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
	
	step.emStep = TESTDEVICE_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_LOADER;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);


	step.emStep = DOWNLOADIMAGE_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = RESETDEVICE_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
		
	return true;
}
bool CRKUpgrade::CreateRestoreStep(ENUM_RKDEVICE_TYPE type,RK_UPGRADE_SET& upgradeStepSet,uint_32 uiMscOper)
{
	(void)uiMscOper;
	STRUCT_UPGRADE_STEP step;

	if (IsValidDeviceType(g_setNanoDeviceType,type))
	{
		return false;
	}

	uint_32 uiSupport = RKUSB_MASKROM|RKUSB_LOADER;
	
	upgradeStepSet.clear();
	
	
	step.emStep = DOWNLOADBOOT_STEP;
	step.bMutex = true;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = WAITMASKROM_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	step.uiParam = RKUSB_NONE;
	upgradeStepSet.push_back(step);
	
	step.emStep = TESTDEVICE_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = GETFLASHINFO_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = READ_CAPABILITY_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = true;
	step.bJumpParam = 9;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
	
	step.emStep = PREPAREIDB_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = true;
	step.bJumpParam = 8;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
	
	step.emStep = ERASEIDB_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = true;
	upgradeStepSet.push_back(step);

	step.emStep = WRITEBACK_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = RESETDEVICE_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
	
	step.emStep = WAITMASKROM_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	step.uiParam = RKUSB_MASKROM;
	upgradeStepSet.push_back(step);

	step.emStep = DOWNLOADBOOT_STEP;
	step.bMutex = true;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = WAITMASKROM_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	step.uiParam = RKUSB_NONE;
	upgradeStepSet.push_back(step);
	
	step.emStep = TESTDEVICE_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
	
	step.emStep = GETFLASHINFO_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = ERASEALLBLOCKS_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = true;
	upgradeStepSet.push_back(step);

	step.emStep = WRITEBACK_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = RESETDEVICE_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
	

	step.emStep = WAITMASKROM_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	step.uiParam = RKUSB_MASKROM;
	upgradeStepSet.push_back(step);

	step.emStep = DOWNLOADBOOT_STEP;
	step.bMutex = true;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = WAITMASKROM_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_MASKROM;
	step.bJump = false;
	step.bWriteBack = false;
	step.uiParam = RKUSB_NONE;
	upgradeStepSet.push_back(step);

	step.emStep = TESTDEVICE_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = READ_CAPABILITY_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = CHECKCHIP_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = GETFLASHINFO_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = PREPAREIDB_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = DOWNLOADIDBLOCK_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = true;
	step.bJumpParam = 3;
	step.bWriteBack = true;
	upgradeStepSet.push_back(step);

	step.emStep = RESETDEVICE_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
	
	step.emStep = WAITLOADER_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
	
	step.emStep = TESTDEVICE_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = RKUSB_LOADER;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);


	step.emStep = DOWNLOADIMAGE_STEP;
	step.bMutex = false;
	step.bSync = false;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);

	step.emStep = RESETDEVICE_STEP;
	step.bMutex = false;
	step.bSync = true;
	step.pMutexCS = NULL;
	step.uiSupport = uiSupport;
	step.bJump = false;
	step.bWriteBack = false;
	upgradeStepSet.push_back(step);
		
	return true;
}
bool  CRKUpgrade::Run()
{
	int i,iRet=0;
	bool bRet=false;
	bool bNeedJump=false;
	bool bNoError=true;
	bool bRunSetp=false;
	uint_32 dwLayerID=0;
	uint_32 uiSkip=0;
	string strSerialName;
	dwLayerID = m_pDevice->LocationID;
	STRUCT_RKDEVICE_DESC device;
	UINT_VECTOR existedDevice;
	CRKComm *pComm = NULL;
	strSerialName = m_pDevice->SerialName;
	for ( i=0;static_cast<size_t>(i)<m_UpgradeProcedure.size();i++ )
	{
		if (uiSkip>0)
		{
			uiSkip--;
			continue;
		}
		if ( (m_UpgradeProcedure[i].uiSupport&(uint_32)m_pDevice->UsbType)==(uint_32)m_pDevice->UsbType )
		{
			if (!bRunSetp)
			{
				bRunSetp = true;
			}
	
			switch ( m_UpgradeProcedure[i].emStep )
			{
			case DOWNLOADBOOT_STEP:
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,DOWNLOADBOOT_START,0);
				}
				
				if (strSerialName.size()==0)
				{
					iRet = m_pDevice->DownloadBoot();
				}
				else //usbplug skip downloadboot
				{
					m_pDevice->GetLogObjectPointer()->Record("Download Boot skip");
					iRet = 0;
				}
				
				bRet = (iRet==0)?true:false;
				if (!bRet)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,DOWNLOADBOOT_FAIL,0);
					}
				}
				else
				{
					// pComm = (CRKComm *)m_pDevice->CommObjectPointer;
					// pComm->Reset_Usb_Device();
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,DOWNLOADBOOT_PASS,0);
					}
				}
				break;
			case DOWNLOADIDBLOCK_STEP:
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,DOWNLOADIDBLOCK_START,0);
				}
				bNeedJump = false;
				iRet = m_pDevice->DownloadIDBlock(bNeedJump);
				bRet = (iRet==0)?true:false;
				if (!bRet)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,DOWNLOADIDBLOCK_FAIL,0);
					}
				}
				else
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,DOWNLOADIDBLOCK_PASS,0);
					}
				}
				break;
			case DOWNLOADIMAGE_STEP:
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,DOWNLOADIMAGE_START,0);
				}
				bRet = true;
				if (m_pDevice->Usb3Transfer)
				{
					if (m_pDevice->UsbSpeed != SUPER_SPEED) {
						if (m_pDevice->ReadCapability()) {
							if (m_pDevice->SwitchUsb3Flag) {//support to switch usb3
								if (m_callBackProc)
								{
									m_callBackProc(dwLayerID, SWITCHUSB3_START, 0);
								}
								if (!m_pDevice->SwitchUsb3())
								{
									if (m_callBackProc)
									{
										m_callBackProc(dwLayerID, SWITCHUSB3_FAIL, 0);
									}
									bRet = false;
								} else { //wait for usb3 reconnect
									strSerialName = m_pDevice->LayerName;
									strSerialName.erase(remove(strSerialName.begin(),strSerialName.end(),'-'),strSerialName.end());
									device.strSerial = strSerialName;
									if (!m_pScan->Wait2(device, m_pDevice->UsbType, m_pDevice->VendorID, m_pDevice->ProductID)) {
										if (m_callBackProc)
										{
											m_callBackProc(dwLayerID, SWITCHUSB3_FAIL, 0);
										}
										device.uiLocationID = m_pDevice->LocationID;
										if (!m_pScan->Wait(device, m_pDevice->UsbType,m_pDevice->VendorID, m_pDevice->ProductID))
											bRet = false;
										else {
											m_pDevice->UsbSpeed = device.emUsbSpeed;
											m_pDevice->BcdUsb = device.usbcdUsb;
											m_pDevice->LinkName = device.strDevPath;
											m_pDevice->SerialName = device.strSerial;
											pComm = (CRKComm *)m_pDevice->CommObjectPointer;
											pComm->Reset_Usb_Config(device);
										}
									}
									else {
										m_pDevice->VendorID = device.usVid;
										m_pDevice->ProductID = device.usPid;
										m_pDevice->UsbType = device.emUsbType;
										m_pDevice->UsbSpeed = device.emUsbSpeed;
										m_pDevice->BcdUsb = device.usbcdUsb;
										m_pDevice->LocationID = device.uiLocationID;
										m_pDevice->LinkName = device.strDevPath;
										m_pDevice->SerialName = device.strSerial;
										pComm = (CRKComm *)m_pDevice->CommObjectPointer;
										pComm->Reset_Usb_Config(device);
										if (m_callBackProc)
										{
											m_callBackProc(dwLayerID, SWITCHUSB3_PASS, 0);
										}
									}
								}
							}
						}
					}
				}
				if (bRet) {
					iRet = m_pDevice->DownloadImage();
					bRet = (iRet == 0) ? true : false;
				}
				if (!bRet)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,DOWNLOADIMAGE_FAIL,0);
					}
				}
				else
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,DOWNLOADIMAGE_PASS,0);
					}
				}
				break;
			case READ_CAPABILITY_STEP:
				bNeedJump = false;
				m_pDevice->ReadCapability();
				if ((m_pDevice->UsbType==RKUSB_MASKROM)&&(m_pDevice->EmmcStor || m_pDevice->NorStor))
				{
					bNeedJump = m_pDevice->DirectLBA;
				}
				bRet = true;
				break;
			case TESTDEVICE_STEP:
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,TESTDEVICE_START,0);
				}
				bRet = m_pDevice->TestDevice();
				if (!bRet)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,TESTDEVICE_FAIL,0);
					}
				}
				else
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,TESTDEVICE_PASS,0);
					}
				}
				break;
			case LOWERFORMAT_STEP:
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,LOWERFORMAT_START,0);
				}
				bRet = m_pDevice->LowerFormatDevice(bNeedJump);
				if (!bRet)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,LOWERFORMAT_FAIL,0);
					}
				}
				else
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,LOWERFORMAT_PASS,0);
					}
				}
				break;
			case RESETDEVICE_STEP:
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,RESETDEVICE_START,0);
				}
				bRet = m_pDevice->ResetDevice();
				if (!bRet)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,RESETDEVICE_FAIL,0);
					}
				}
				else
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,RESETDEVICE_PASS,0);
					}					
				}
				break;
			case SETRESETFLAG_STEP:
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,SETRESETFLAG_START,0);
				}
				bRet = m_pDevice->SetResetFlag();
				if (!bRet)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,SETRESETFLAG_FAIL,0);
					}
				}
				else
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,SETRESETFLAG_PASS,0);
					}
				}
				break;
			
			case WAITLOADER_STEP:
				device.uiLocationID = m_pDevice->LocationID;
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,WAITLOADER_START,0);
				}
//				printf("Waitloader before:locationID=0x%x\r\n\r\n",device.uiLocationID);
				bRet = m_pScan->Wait(device,RKUSB_LOADER/*,m_pDevice->VendorID,m_pDevice->ProductID*/);
				if (!bRet)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,WAITLOADER_FAIL,0);
					}
				}
				else
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,WAITLOADER_PASS,0);
					}
//					printf("Waitloader after:locationID=0x%x\r\n\r\n",device.uiLocationID);
					m_pDevice->VendorID = device.usVid;
					m_pDevice->ProductID = device.usPid;
					m_pDevice->UsbType = device.emUsbType;
					m_pDevice->UsbSpeed = device.emUsbSpeed;
					m_pDevice->BcdUsb = device.usbcdUsb;
					m_pDevice->LocationID = device.uiLocationID;
					m_pDevice->LinkName = device.strDevPath;
					m_pDevice->SerialName = device.strSerial;
					pComm = (CRKComm *)m_pDevice->CommObjectPointer;
					pComm->Reset_Usb_Config(device);
				}
				break;
			case WAITMASKROM_STEP:
				device.uiLocationID = m_pDevice->LocationID;
				device.emUsbType = (ENUM_RKUSB_TYPE)m_UpgradeProcedure[i].uiParam;
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,WAITMASKROM_START,0);
				}
				bRet = m_pScan->Wait(device,RKUSB_MASKROM/*,m_pDevice->VendorID,m_pDevice->ProductID*/);
				if (!bRet)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,WAITMASKROM_FAIL,0);
					}
				}
				else
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,WAITMASKROM_PASS,0);
					}
					m_pDevice->VendorID = device.usVid;
					m_pDevice->ProductID = device.usPid;
					m_pDevice->UsbType = device.emUsbType;
					m_pDevice->UsbSpeed = device.emUsbSpeed;
					m_pDevice->BcdUsb = device.usbcdUsb;
					m_pDevice->LocationID = device.uiLocationID;
					m_pDevice->LinkName = device.strDevPath;
					m_pDevice->SerialName = device.strSerial;
					pComm = (CRKComm *)m_pDevice->CommObjectPointer;
					pComm->Reset_Usb_Config(device);
				}
				break;
			case ERASEIDB_STEP:
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,ERASEIDB_START,0);
				}
				bNeedJump = false;
				if (m_UpgradeProcedure[i].bJump)
				{
					if (m_pDevice->EmmcStor)
					{
						bNeedJump = true;
					}
				}
				if (bNeedJump)
				{
					iRet = -1;
					bRet = m_pDevice->ResetDevice(RST_RESETMASKROM_SUBCODE);
					if (bRet)
					{
						device.uiLocationID = m_pDevice->LocationID;
						bRet = m_pScan->Wait(device,RKUSB_MASKROM/*,m_pDevice->VendorID,m_pDevice->ProductID*/);
						if (bRet)
						{
							m_pDevice->VendorID = device.usVid;
							m_pDevice->ProductID = device.usPid;
							m_pDevice->UsbType = device.emUsbType;
							m_pDevice->UsbSpeed = device.emUsbSpeed;
							m_pDevice->BcdUsb = device.usbcdUsb;
							m_pDevice->LocationID = device.uiLocationID;
							m_pDevice->LinkName = device.strDevPath;
							m_pDevice->SerialName = device.strSerial;
							pComm = (CRKComm *)m_pDevice->CommObjectPointer;
							pComm->Reset_Usb_Config(device);
							iRet = 0;
						}
					}
				}
				else
				{
					iRet = m_pDevice->EraseIDB();
				}
				bRet = (iRet==0)?true:false;
				if (!bRet)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,ERASEIDB_FAIL,0);
					}
				}
				else
				{
					if (m_callBackProc)
					{	
						m_callBackProc(dwLayerID,ERASEIDB_PASS,0);
					}
				}
				break;
			case SWITCHMSC_STEP:
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,SWITCHMSC_START,0);
				}
				bRet = m_pDevice->SendExtCommand(0,MSC_SWITCHROCKUSB);
				if (!bRet)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,SWITCHMSC_FAIL,0);
					}
				}
				else
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,SWITCHMSC_PASS,0);
					}
				}
				break;
			case RESETMSC_STEP:
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,RESETMSC_START,0);
				}
				//sleep for flushing data to device
				sleep_s(1);
				bRet = m_pDevice->SendExtCommand(0,MSC_RESETDEVICE,0);
				if (!bRet)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,RESETMSC_FAIL,0);
					}
				}
				else
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,RESETMSC_PASS,0);
					}
				}
				break;
			
			case CHECKCHIP_STEP:
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,CHECKCHIP_START,0);
				}
				bRet = m_pDevice->CheckChip();
				if (!bRet)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,CHECKCHIP_FAIL,0);
					}
				}
				else
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,CHECKCHIP_PASS,0);
					}
				}
				break;
			case PREPAREIDB_STEP:
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,PREPAREIDB_START,0);
				}
				bNeedJump = false;
				iRet = m_pDevice->PrepareIDB(bNeedJump);
				bRet = (iRet==0)?true:false;
				if (!bRet)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,PREPAREIDB_FAIL,0);
					}
				}
				else
				{
					if (m_callBackProc)
					{	
						m_callBackProc(dwLayerID,PREPAREIDB_PASS,0);
					}
				}
				break;
			
			case ERASEALLBLOCKS_STEP:
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,ERASEALLBLOCKS_START,0);
				}
				iRet = m_pDevice->EraseAllBlocks();
				bRet = (iRet==0)?true:false;
				if (!bRet)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,ERASEALLBLOCKS_FAIL,0);
					}
				}
				else
				{
					if (m_callBackProc)
					{	
						m_callBackProc(dwLayerID,ERASEALLBLOCKS_PASS,0);
					}
				}
				break;
			
			case GETFLASHINFO_STEP:
				if (m_callBackProc)
				{
					m_callBackProc(dwLayerID,GETFLASHINFO_START,0);
				}
				bRet = m_pDevice->GetFlashInfo();
				if (!bRet)
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,GETFLASHINFO_FAIL,0);
					}
				}
				else
				{
					if (m_callBackProc)
					{
						m_callBackProc(dwLayerID,GETFLASHINFO_PASS,0);
					}
				}
				break;
			case WRITEBACK_STEP:
				bRet = m_pDevice->BufferWriteBack();
				break;
			default:
				break;
			}

			if (!bRet)
			{
				bNoError = false;
				if (m_UpgradeProcedure[i].bWriteBack)
				{
					m_pDevice->BufferWriteBack();
				}
				break;
			}
			else
			{
				if ((m_UpgradeProcedure[i].bJump)&&(bNeedJump))
				{
					uiSkip = m_UpgradeProcedure[i].bJumpParam;
				}
			}
		}
	}
	if (!bRunSetp)
	{
		bNoError = false;
	}
	return bNoError;
}
bool CRKUpgrade::SetObject(CRKDevice *pDevice,CRKScan *pScan)
{

	if ( (!pDevice)||(!pScan))
	{
		return false;
	}

	m_pDevice = pDevice;
	m_pDevice->WorkFlow = m_workFlow;
	m_pScan = pScan;

	return true;
}

bool CRKUpgrade::SetStep(ENUM_UPGRADE_STEP emStep,uint_32 uiSupport)
{
	int i;
	for ( i=0;static_cast<size_t>(i)<m_UpgradeProcedure.size();i++ )
	{
		if ( m_UpgradeProcedure[i].emStep==emStep )
		{
			m_UpgradeProcedure[i].uiSupport = uiSupport;
			return true;
		}
	}
	return false;
}

bool CRKUpgrade::RemoveStep(ENUM_UPGRADE_STEP emStep)
{
	int i;
	for ( i=0;static_cast<size_t>(i)<m_UpgradeProcedure.size();i++ )
	{
		if ( m_UpgradeProcedure[i].emStep==emStep )
		{
			m_UpgradeProcedure.erase(m_UpgradeProcedure.begin()+i);
			return true;
		}
	}
	return false;
}

CRKDevice* CDeviceFactory::CreateDevice(STRUCT_RKDEVICE_DESC &device)
{
	CRKDevice *pDevice=NULL;
	pDevice = new CRKAndroidDevice(device);
	return pDevice;
}
void CDeviceFactory::FreeDevice(CRKDevice *pDevice)
{
	if (!pDevice)
	{
		return;
	}
	delete pDevice;
	pDevice = NULL;
}
