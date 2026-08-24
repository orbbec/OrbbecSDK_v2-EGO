#include "RKScan.h"

#if defined(__ANDROID__)
#include "Platform.hpp"
#include "usb/enumerator/UsbEnumeratorLibusb.hpp"
#include "usb/vendor/VendorUsbDevicePort.hpp"
#include <exception>
#endif

int CRKScan::GetDEVICE_COUNTS()
{
	return m_list.size();
}


uint_32 CRKScan::GetMSC_TIMEOUT()
{
	return m_waitMscSecond;
}
uint_32 CRKScan::GetRKUSB_TIMEOUT()
{
	return m_waitRKusbSecond;
}


void CRKScan::SetMSC_TIMEOUT(uint_32 value)
{
	m_waitMscSecond = value;
}

void CRKScan::SetRKUSB_TIMEOUT(uint_32 value)
{
	m_waitRKusbSecond = value;
}


CRKScan::CRKScan(uint_32 uiMscTimeout,uint_32 uiRKusbTimeout)
{
	DEVICE_COUNTS.setContainer(this);
    	DEVICE_COUNTS.getter(&CRKScan::GetDEVICE_COUNTS);
 	
 	MSC_TIMEOUT.setContainer(this);
    	MSC_TIMEOUT.getter(&CRKScan::GetMSC_TIMEOUT);
 	MSC_TIMEOUT.setter(&CRKScan::SetMSC_TIMEOUT);
	
	RKUSB_TIMEOUT.setContainer(this);
   	 RKUSB_TIMEOUT.getter(&CRKScan::GetRKUSB_TIMEOUT);
 	RKUSB_TIMEOUT.setter(&CRKScan::SetRKUSB_TIMEOUT);
	
	m_waitMscSecond = uiMscTimeout;
	m_waitRKusbSecond = uiRKusbTimeout;
	m_log = NULL;
	m_list.clear();
	m_deviceConfigSet.clear();
	m_deviceMscConfigSet.clear();
}
bool CRKScan::FindRockusbVidPid(ENUM_RKDEVICE_TYPE type,uint_16 &usVid,uint_16 &usPid)
{
	uint_32 i;
	bool bRet=false;
	for (i=0;i<m_deviceConfigSet.size();i++)
	{
		if (m_deviceConfigSet[i].emDeviceType==type)
		{
			usVid = m_deviceConfigSet[i].usVid;
			usPid = m_deviceConfigSet[i].usPid;
			bRet = true;
			break;
		}
	}
	return bRet;
}
void CRKScan::AddRockusbVidPid(uint_16 newVid,uint_16 newPid,uint_16 oldVid,uint_16 oldPid)
{
	if ((newVid==0)||(newPid==0)||(oldVid==0)||(oldPid==0))
	{
		return;
	}
	STRUCT_DEVICE_CONFIG config;
	int i;
	for (i=0;static_cast<size_t>(i)<m_deviceConfigSet.size();i++)
	{
		if ((m_deviceConfigSet[i].usVid==oldVid)&&(m_deviceConfigSet[i].usPid==oldPid))
		{
			config.usVid = newVid;
			config.usPid = newPid;
			config.emDeviceType = m_deviceConfigSet[i].emDeviceType;
			break;
		}
	}
	if (static_cast<size_t>(i)<m_deviceConfigSet.size())
		m_deviceConfigSet.push_back(config);
}

void CRKScan::SetVidPid(uint_16 mscVid,uint_16 mscPid)
{
	STRUCT_DEVICE_CONFIG config;
	m_deviceConfigSet.clear();
	
	config.emDeviceType = RK27_DEVICE;
	config.usPid = 0x3201;
	config.usVid = 0x071B;
	m_deviceConfigSet.push_back(config);
	
	config.emDeviceType = RK28_DEVICE;
	config.usPid = 0x3228;
	config.usVid = 0x071B;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RKNANO_DEVICE;
	config.usPid = 0x3226;
	config.usVid = 0x071B;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RKCROWN_DEVICE;
	config.usPid = 0x261A;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RK281X_DEVICE;
	config.usPid = 0x281A;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RKCAYMAN_DEVICE;
	config.usPid = 0x273A;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RK29_DEVICE;
	config.usPid = 0x290A;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RKPANDA_DEVICE;
	config.usPid = 0x282B;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RKSMART_DEVICE;
	config.usPid = 0x262C;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RK292X_DEVICE;
	config.usPid = 0x292A;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RK30_DEVICE;
	config.usPid = 0x300A;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RK30B_DEVICE;
	config.usPid = 0x300B;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RK31_DEVICE;
	config.usPid = 0x310B;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);

	config.emDeviceType = RK32_DEVICE;
	config.usPid = 0x320A;
	config.usVid = 0x2207;
	m_deviceConfigSet.push_back(config);


	m_deviceMscConfigSet.clear();

	config.emDeviceType = RKNONE_DEVICE;
	config.usPid = 0x3203;
	config.usVid = 0x071B;
	m_deviceMscConfigSet.push_back(config);

	config.emDeviceType = RKNONE_DEVICE;
	config.usPid = 0x3205;
	config.usVid = 0x071B;
	m_deviceMscConfigSet.push_back(config);

	config.emDeviceType = RKNONE_DEVICE;
	config.usPid = 0x2910;
	config.usVid = 0x0BB4;
	m_deviceMscConfigSet.push_back(config);

	config.emDeviceType = RKNONE_DEVICE;
	config.usPid = 0x0000;
	config.usVid = 0x2207;
	m_deviceMscConfigSet.push_back(config);

	config.emDeviceType = RKNONE_DEVICE;
	config.usPid = 0x0010;
	config.usVid = 0x2207;
	m_deviceMscConfigSet.push_back(config);

	config.emDeviceType = RKNONE_DEVICE;
	config.usPid = 0x0016;
	config.usVid = 0x2207;
	m_deviceMscConfigSet.push_back(config);

	if ((mscVid!=0)||(mscPid!=0))
	{
		if (FindConfigSetPos(m_deviceMscConfigSet,mscVid,mscPid)==-1)
		{
			config.emDeviceType = RKNONE_DEVICE;
			config.usPid = mscPid;
			config.usVid = mscVid;
			m_deviceMscConfigSet.push_back(config);
		}
	}
}
int CRKScan::FindWaitSetPos(const RKDEVICE_CONFIG_SET &waitDeviceSet,uint_16 vid,uint_16 pid)
{
	int pos=-1;
	uint_32 i;
	for ( i=0;i<waitDeviceSet.size();i++ )
	{
		if ( (vid==waitDeviceSet[i].usVid)&&(pid==waitDeviceSet[i].usPid) )
		{
			pos = i;
			break;
		}
	}
	return pos;
}
int CRKScan::FindConfigSetPos(RKDEVICE_CONFIG_SET &devConfigSet,uint_16 vid,uint_16 pid)
{
	int pos=-1;
	uint_32 i;
	for ( i=0;i<devConfigSet.size();i++ )
	{
		if ( (vid==devConfigSet[i].usVid)&&(pid==devConfigSet[i].usPid) )
		{
			pos = i;
			break;
		}
	}
	return pos;
}
void CRKScan::EnumerateUsbDevice(RKDEVICE_DESC_SET &list,uint_32 &uiTotalMatchDevices)
{
#if defined(__ANDROID__)
	uiTotalMatchDevices = 0;
	const libobsensor::SourcePortInfoList infos = libobsensor::Platform::getInstance()->queryUsbSourcePortInfos(false);
	for(const auto &baseInfo: infos) {
		auto info = std::dynamic_pointer_cast<const libobsensor::USBSourcePortInfo>(baseInfo);
		if(!info || info->portType != libobsensor::SOURCE_PORT_USB_VENDOR) {
			continue;
		}
		try {
			auto sourcePort = libobsensor::Platform::getInstance()->getUsbSourcePort(info);
			auto vendorPort = std::dynamic_pointer_cast<libobsensor::VendorUsbDevicePort>(sourcePort);
			if(!vendorPort) {
				continue;
			}
			auto usbDevice = std::dynamic_pointer_cast<libobsensor::UsbDeviceLibusb>(vendorPort->getUsbDevice());
			if(!usbDevice || !usbDevice->getLibusbDeviceHandle()) {
				continue;
			}
			libusb_device_handle *openedHandle = usbDevice->getLibusbDeviceHandle();
			libusb_device *device = libusb_get_device(openedHandle);
			libusb_device_descriptor descriptor;
			if(!device || libusb_get_device_descriptor(device, &descriptor) != 0) {
				continue;
			}
			STRUCT_RKDEVICE_DESC desc = {};
			desc.usVid = descriptor.idVendor;
			desc.usPid = descriptor.idProduct;
			desc.usbcdUsb = descriptor.bcdUSB;
			desc.emUsbType = (descriptor.bcdUSB & 0x1) ? RKUSB_LOADER : RKUSB_MASKROM;
			desc.emDeviceType = RKNONE_DEVICE;
			desc.emUsbSpeed = HIGH_SPEED;
			desc.pUsbHandle = device;
			desc.pOpenedUsbHandle = openedHandle;
			desc.strSerial = info->serial;
			desc.strDevPath = info->url;
			m_androidUsbDevices[info->url] = usbDevice;
			list.push_back(desc);
			++uiTotalMatchDevices;
		}
		catch(const std::exception &) {
			// Unrelated or unauthorized Android USB devices are not candidates.
		}
	}
	return;
#else
	STRUCT_RKDEVICE_DESC desc;
	struct libusb_device_descriptor descriptor;
	int ret,j,i,cnt;
	uint8_t ports[7];
	uint8_t nPort=0;
	char szSerial[60];
	uiTotalMatchDevices = 0;
	libusb_device **pDevs=NULL;
	libusb_device *dev;
	ret = libusb_get_device_list(NULL,&pDevs);
	if (ret<0)
	{
		if (m_log)
			m_log->Record("Error:EnumerateUsbDevice-->get_device_list failed,err=%d!",ret);
		return;
	}
	cnt = ret;
	for (i=0;i<cnt;i++)
	{
		dev = pDevs[i];
		if (dev)
		{
			ret = libusb_get_device_descriptor(dev,&descriptor);
			if (ret<0)
			{
				libusb_free_device_list(pDevs,1);
				if (m_log)
					m_log->Record("Error:EnumerateUsbDevice-->get_device_descriptor failed,err=%d!",ret);
				return;
			}
			desc.strDevPath = "";
			desc.strSerial = "";
			desc.emDeviceType = RKNONE_DEVICE;
			desc.emUsbType = RKUSB_NONE;
			desc.emUsbSpeed = HIGH_SPEED;
			desc.pUsbHandle = (void *)dev;
			desc.usbcdUsb = descriptor.bcdUSB;
			desc.usVid = descriptor.idVendor;
			desc.usPid = descriptor.idProduct;
			desc.uiLocationID = libusb_get_bus_number(dev);
			desc.uiLocationID &= 0xF;
			ret = libusb_get_port_numbers(dev,ports,7);
			if (ret>0) {
				nPort = ret;
				desc.uiLocationID <<= 4;
				for (j=0; j < nPort; j++) {
					desc.uiLocationID += (ports[j] & 0xF);
					if ((j+1) == nPort)
						continue;
					desc.uiLocationID <<= 4;
				}
			}
			
			ret = libusb_get_device_speed(dev);
			if (ret>0) {
				if (ret>3)
					desc.emUsbSpeed = SUPER_SPEED;
				else
					desc.emUsbSpeed = (ENUM_USB_SPEED)(ret-1);
			}	
			
			memset(szSerial,0,60);
			if (descriptor.iSerialNumber!=0) {
				libusb_device_handle *pUsbHandle;
				ret = libusb_open(dev,(libusb_device_handle **)&pUsbHandle);
				if (ret==0)
				{
					ret = libusb_get_string_descriptor_ascii(pUsbHandle,descriptor.iSerialNumber,(unsigned char*)szSerial,60);
					libusb_close(pUsbHandle);
					if (ret<0) {
						if (m_log)
							m_log->Record("Error:EnumerateUsbDevice-->get_string_descriptor failed,err=%d!",ret);
					}
					else
						desc.strSerial = szSerial;
				} else {
					if (m_log)
						m_log->Record("Error:EnumerateUsbDevice-->get_string_descriptor open failed,err=%d!",ret);
				}
				
			}
				
			libusb_ref_device(dev);
			uiTotalMatchDevices++;
			list.push_back(desc);
		}
			
	}
	libusb_free_device_list(pDevs,1);
#endif
    
}

void CRKScan::FreeDeviceList(RKDEVICE_DESC_SET &list)
{
	device_list_iter iter;
	 for (iter=list.begin(); iter!=list.end(); iter++) {
	#if !defined(__ANDROID__)
		if ((*iter).pUsbHandle) 
		{
			libusb_unref_device((libusb_device *)((*iter).pUsbHandle));
			(*iter).pUsbHandle = NULL;
		}
	#endif
	}
   	list.clear();
}
bool CRKScan::IsRockusbDevice(ENUM_RKDEVICE_TYPE &type,uint_16 vid,uint_16 pid)
{
	int iPos;
	iPos = FindConfigSetPos(m_deviceConfigSet,vid,pid);
	if (iPos!=-1)
	{
		type = m_deviceConfigSet[iPos].emDeviceType;
		return true;
	}
	if ( vid==0x2207)
	{
		if ((pid>>8)>0)
		{
			type = RKNONE_DEVICE;
			return true;
		}
	}
	return false;
}
int CRKScan::Search(uint_32 type)
{
	device_list_iter iter,new_iter;
	ENUM_RKDEVICE_TYPE devType;
	uint_32 uiTotalDevice;
	int iPos;

	FreeDeviceList(m_list);
    EnumerateUsbDevice( m_list,uiTotalDevice );



	for ( iter=m_list.begin();iter!=m_list.end(); )
	{
		if( (iPos=FindConfigSetPos(m_deviceMscConfigSet,(*iter).usVid,(*iter).usPid))!=-1 ) 
		{
			(*iter).emDeviceType = RKNONE_DEVICE;
			iter++;
			continue;
		}
		else if (IsRockusbDevice(devType,(*iter).usVid,(*iter).usPid) )
		{
			(*iter).emDeviceType = devType;
			iter++;
			continue;
		}
		else
		{
			if ((*iter).pUsbHandle) 
			{
				#if !defined(__ANDROID__)
				libusb_unref_device((libusb_device *)((*iter).pUsbHandle));
				#endif
				(*iter).pUsbHandle = NULL;
			}
			iter = m_list.erase(iter);
			uiTotalDevice--;
		}
	}

	if (m_list.size()<=0)
	{
		return 0;
	}

	if ( (type&RKUSB_MASKROM)==0 )
	{
		for ( iter=m_list.begin();iter!=m_list.end(); )
		{
			if( (IsRockusbDevice(devType,(*iter).usVid,(*iter).usPid))&&(((*iter).usbcdUsb&0x1)==0) )
			{
	            if ((*iter).pUsbHandle) 
				{
					#if !defined(__ANDROID__)
					libusb_unref_device((libusb_device *)((*iter).pUsbHandle));
					#endif
					(*iter).pUsbHandle = NULL;
				}
				iter = m_list.erase(iter);
	            uiTotalDevice--;
			}
			else
			{
				iter++;
				continue;
			}
		}
	}
	if (m_list.size()<=0)
	{
		return 0;
	}

	if ( (type&RKUSB_LOADER)==0 )
	{
		for ( iter=m_list.begin();iter!=m_list.end(); )
		{
			if( (IsRockusbDevice(devType,(*iter).usVid,(*iter).usPid))&&(((*iter).usbcdUsb&0x1)==1) )
			{
	            if ((*iter).pUsbHandle) 
				{
					#if !defined(__ANDROID__)
					libusb_unref_device((libusb_device *)((*iter).pUsbHandle));
					#endif
					(*iter).pUsbHandle = NULL;
				}
				iter = m_list.erase(iter);
	            uiTotalDevice--;
			}
			else
			{
				iter++;
				continue;
			}
		}
	}
	if (m_list.size()<=0)
	{
		return 0;
	}

	if ( (type&RKUSB_MSC)==0 )
	{
		for ( iter=m_list.begin();iter!=m_list.end(); )
		{
			if(FindConfigSetPos(m_deviceMscConfigSet,(*iter).usVid,(*iter).usPid)!=-1)
			{
                if ((*iter).pUsbHandle) 
				{
					#if !defined(__ANDROID__)
					libusb_unref_device((libusb_device *)((*iter).pUsbHandle));
					#endif
					(*iter).pUsbHandle = NULL;
				}
				iter = m_list.erase(iter);
                uiTotalDevice--;
			}
			else
			{
				iter++;
				continue;
			}
		}
	}
	if (m_list.size()<=0)
	{
		return 0;
	}

	for ( iter=m_list.begin();iter!=m_list.end();iter++ )
	{
		if(FindConfigSetPos(m_deviceMscConfigSet,(*iter).usVid,(*iter).usPid)!=-1)
		{
			(*iter).emUsbType = RKUSB_MSC;
		}
		else
		{
			uint_16 usTemp;
			usTemp = (*iter).usbcdUsb;
			usTemp= usTemp &0x1;
			if ( usTemp==0 )
				(*iter).emUsbType = RKUSB_MASKROM;
			else
				(*iter).emUsbType = RKUSB_LOADER;
		}
	}
	return m_list.size();
}


bool CRKScan::Wait(STRUCT_RKDEVICE_DESC &device,ENUM_RKUSB_TYPE usbType,uint_16 usVid,uint_16 usPid)
{
	RKDEVICE_DESC_SET deviceList;
	ENUM_RKUSB_TYPE curDeviceType;
	ENUM_RKDEVICE_TYPE devType;
	device_list_iter iter;
	uint_32 totalDevice;
	int uiWaitSecond;
	int iFoundCount=0;
	bool bRet=false;
	if (usbType==RKUSB_MSC)
		uiWaitSecond = m_waitMscSecond;
	else
		uiWaitSecond = m_waitRKusbSecond;
	time_t tmInit, tmNow;
	time(&tmInit);
	while( difftime(time(&tmNow),tmInit)<=uiWaitSecond )
	{
		FreeDeviceList(deviceList);
		EnumerateUsbDevice(deviceList, totalDevice);
		
		for ( iter=deviceList.begin();iter!=deviceList.end();iter++ )
		{
			bool sameDevice = ((*iter).uiLocationID==device.uiLocationID);
#if defined(__ANDROID__)
			// Android does not expose a stable bus/port topology through the USB
			// Host API. A RockUSB loader restart can also change its device URL,
			// so prefer the USB serial captured before reset. If no serial is
			// available, retain the conservative URL match rather than selecting a
			// different device with the same VID/PID.
			sameDevice = !device.strSerial.empty() ? ((*iter).strSerial == device.strSerial)
				                                : (!device.strDevPath.empty() && ((*iter).strDevPath == device.strDevPath));
#endif
			if (sameDevice)
			{
				if ((usVid!=0)||(usPid!=0))
				{
					if ( ((*iter).usVid!=usVid)||((*iter).usPid!=usPid) )
						continue;
				}
				if (IsRockusbDevice(devType,(*iter).usVid,(*iter).usPid))
				{
					if ( ((*iter).usbcdUsb&0x0001)==0 )
						curDeviceType = RKUSB_MASKROM;
					else
						curDeviceType = RKUSB_LOADER;
				}
				else if ( FindConfigSetPos(m_deviceMscConfigSet,(*iter).usVid,(*iter).usPid)!=-1 )
				{
					curDeviceType = RKUSB_MSC;
				}
				else
					curDeviceType = RKUSB_NONE;
								
				if ( curDeviceType==usbType )
				{
					if ((device.emUsbType != RKUSB_MASKROM) && (usbType == RKUSB_MASKROM)) {
						if (((*iter).strSerial).size() == 0)
							continue;
					}
					iFoundCount++;
					break;
				}
			}
		}
		if ( iter==deviceList.end() )
		{
			iFoundCount = 0;
		}
		if ( iFoundCount>=8 )
		{
			device.usVid = (*iter).usVid;
			device.usPid = (*iter).usPid;
			device.uiLocationID = (*iter).uiLocationID;
			device.pUsbHandle= (*iter).pUsbHandle;
			device.pOpenedUsbHandle= (*iter).pOpenedUsbHandle;
			device.emUsbType = usbType;
			device.emUsbSpeed = (*iter).emUsbSpeed;
			device.usbcdUsb = (*iter).usbcdUsb;
			device.strDevPath = (*iter).strDevPath;
			device.strSerial = (*iter).strSerial;
			#if !defined(__ANDROID__)
			libusb_ref_device((libusb_device *)device.pUsbHandle);
			#endif
            
			if (usbType==RKUSB_MSC)
			{
				device.emDeviceType = RKNONE_DEVICE;
			}
			else
			{
				if (IsRockusbDevice(devType,device.usVid,device.usPid))
					device.emDeviceType = devType;
			}

			bRet = true;
			break;
		}
		sleep_ms(50);
	}

	FreeDeviceList(deviceList);
	return bRet;
}
bool CRKScan::Wait2(STRUCT_RKDEVICE_DESC &device,ENUM_RKUSB_TYPE usbType,uint_16 usVid,uint_16 usPid)
{
	RKDEVICE_DESC_SET deviceList;
	ENUM_RKUSB_TYPE curDeviceType;
	ENUM_RKDEVICE_TYPE devType;
	device_list_iter iter;
	uint_32 totalDevice;
	int uiWaitSecond;
	int iFoundCount=0;
	bool bRet=false;

	uiWaitSecond = m_waitRKusbSecond;
	time_t tmInit, tmNow;
	time(&tmInit);
	while( difftime(time(&tmNow),tmInit)<=uiWaitSecond )
	{
		FreeDeviceList(deviceList);
		EnumerateUsbDevice(deviceList, totalDevice);
		
		for ( iter=deviceList.begin();iter!=deviceList.end();iter++ )
		{
			bool sameDevice = ((*iter).strSerial==device.strSerial);
#if defined(__ANDROID__)
			sameDevice = !device.strSerial.empty() ? ((*iter).strSerial == device.strSerial)
				                                : (!device.strDevPath.empty() && ((*iter).strDevPath == device.strDevPath));
#endif
			if (sameDevice)
			{
				if ((usVid!=0)||(usPid!=0))
				{
					if ( ((*iter).usVid!=usVid)||((*iter).usPid!=usPid) )
						continue;
				}
				if (IsRockusbDevice(devType,(*iter).usVid,(*iter).usPid))
				{
					if ( ((*iter).usbcdUsb&0x0001)==0 )
						curDeviceType = RKUSB_MASKROM;
					else
						curDeviceType = RKUSB_LOADER;
				}
				else
					curDeviceType = RKUSB_NONE;
								
				if ( curDeviceType==usbType )
				{
					iFoundCount++;
					break;
				}
			}
		}
		if ( iter==deviceList.end() )
		{
			iFoundCount = 0;
		}
		if ( iFoundCount>=8 )
		{
			device.usVid = (*iter).usVid;
			device.usPid = (*iter).usPid;
			device.uiLocationID = (*iter).uiLocationID;
			device.pUsbHandle= (*iter).pUsbHandle;
			device.pOpenedUsbHandle= (*iter).pOpenedUsbHandle;
			device.emUsbType = usbType;
			device.emUsbSpeed = (*iter).emUsbSpeed;
			device.usbcdUsb = (*iter).usbcdUsb;
			device.strDevPath = (*iter).strDevPath;
			device.strSerial = (*iter).strSerial;
			#if !defined(__ANDROID__)
			libusb_ref_device((libusb_device *)device.pUsbHandle);
			#endif
            
			if (IsRockusbDevice(devType,device.usVid,device.usPid))
				device.emDeviceType = devType;

			bRet = true;
			break;
		}
		sleep_ms(50);
	}

	FreeDeviceList(deviceList);
	return bRet;
}
int CRKScan::GetPos(uint_32 locationID)
{
	device_list_iter iter;
	int pos=0;
	bool bFound=false;
	for (iter=m_list.begin();iter!=m_list.end();iter++)
	{
		if (locationID==(*iter).uiLocationID)
		{
			bFound=true;
			break;
		}
		pos++;
	}
	return (bFound?pos:-1);
}
bool CRKScan::GetDevice(STRUCT_RKDEVICE_DESC &device,int pos)
{
	if ( (pos<0)||(static_cast<size_t>(pos)>=m_list.size()) )
	{
		return false;
	}
	device_list_iter iter;
	for (iter=m_list.begin();iter!=m_list.end();iter++)
	{
		if (pos==0)
		{
			break;
		}
		pos--;
	}
	device.usVid = (*iter).usVid;
	device.usPid = (*iter).usPid;
	device.emDeviceType = (*iter).emDeviceType;
	device.emUsbType = (*iter).emUsbType;
	device.emUsbSpeed = (*iter).emUsbSpeed;
	device.uiLocationID = (*iter).uiLocationID;
	device.pUsbHandle= (*iter).pUsbHandle;
	device.pOpenedUsbHandle= (*iter).pOpenedUsbHandle;
	device.usbcdUsb = (*iter).usbcdUsb;
	device.strDevPath = (*iter).strDevPath;
	device.strSerial = (*iter).strSerial;
	return true;
}


bool CRKScan::SetLogObject(CRKLog *pLog)
{
	if (pLog)
	{
		if (m_log)
		{
			delete m_log;
		}
		m_log = pLog;
	}
	else
		return false;
	return true;
}
CRKScan::~CRKScan()
{
	FreeDeviceList(m_list);
	if (m_log)
	{
		delete m_log;
		m_log = NULL;
	}
}
