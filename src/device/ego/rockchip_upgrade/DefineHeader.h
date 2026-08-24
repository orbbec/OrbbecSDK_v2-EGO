#ifndef DEFINE_HEADER
#define DEFINE_HEADER
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include "Endian.h"

#ifdef _WIN32
#include "os\win\platform.h"
#elif __APPLE__
#include "os/mac/platform.h"
#else
#include "os/linux/platform.h"
#endif
#include "Property.hpp"


const uint_8 RUN_PARAM_MAGIC[4] = {'R', 'S', 'P', 'M'};
#define MAX_RUN_PARAM 3
typedef enum
{
	IMAGE_TRUST=1<<0,
	IMAGE_UBOOT=1<<1,
	IMAGE_BOOT=1<<2,
	IMAGE_KERNEL=1<<3,
	IMAGE_RESOURCE=1<<4,
	IMAGE_VENDOR=1<<5,
	IMAGE_DATA=1<<6
}ENUM_IMAGE_TYPE;

typedef enum
{
		RKNONE_DEVICE=0,
		RK27_DEVICE=0x10,
		RKCAYMAN_DEVICE,
		RK28_DEVICE=0x20,
		RK281X_DEVICE,
		RKPANDA_DEVICE,
		RKNANO_DEVICE=0x30,
		RKSMART_DEVICE,
		RKCROWN_DEVICE=0x40,
		RK29_DEVICE=0x50,
		RK292X_DEVICE,
		RK30_DEVICE=0x60,
		RK30B_DEVICE,
		RK31_DEVICE=0x70,
		RK32_DEVICE=0x80
}ENUM_RKDEVICE_TYPE;
typedef enum
{
		RK_OS=0,
		ANDROID_OS=0x1
}ENUM_OS_TYPE;

typedef enum
{
		RKUSB_NONE=0x0,
		RKUSB_MASKROM=0x01,
		RKUSB_LOADER=0x02,
		RKUSB_MSC=0x04
}ENUM_RKUSB_TYPE;
typedef enum
{
	LOW_SPEED=0,
	FULL_SPEED,
	HIGH_SPEED,
	SUPER_SPEED,
}ENUM_USB_SPEED;
typedef enum
{
		ENTRY471=1,
		ENTRY472=2,
		ENTRYLOADER=4
}ENUM_RKBOOTENTRY;
#define  MSC_ANDROID_OPER 0xFF
#define  DIRECT_LBA_OPER 0xAA
typedef enum
{
	MSC_NONE_OPER=0x0,
	MSC_FORMAT_OPER=0x1,
	MSC_COPY_OPER=0x2,
	MSC_FORMAT_DATA_OPER=0x4,
	MSC_COPY_DATA_OPER=0x8
}ENUM_MSC_OPER;
typedef enum
{
	MISC_MODIFY_NONE=0,
	MISC_MODIFY_WIPE_ALL,
	MISC_MODIFY_WIPE_DATA,
}ENUM_MISC_MODIFY_FLAG;
typedef enum
{
	WF_UPGRADE=1,
	WF_RESTORE,
	WF_GETOLDDISKSIZE,
	WF_READSN,
	WF_WRITESN,
	WF_ERASEFLASH,
	WF_ERASEIDB,
	WF_GETBLOCKSTATE,
	WF_READMAC,
	WF_WRITEMAC,
	WF_READBT,
	WF_WRITEBT,
	WF_READIMEI,
	WF_WRITEIMEI,
	WF_READUID,
	WF_READCUSTOMDATA,
	WF_WRITECUSTOMDATA,
	WF_READALLINFO,
	WF_WRITEALLINFO,
	WF_DOWNLOADBOOT
}ENUM_WORKFLOW;
#pragma pack(1)
typedef struct sparse_header_t { 
	uint_32	magic;		/* 0xed26ff3a */
	uint_16	major_version;	/* (0x1) - reject images with higher major versions */
	uint_16	minor_version;	/* (0x0) - allow images with higer minor versions */
	uint_16	file_hdr_sz;	/* 28 bytes for first revision of the file format */
	uint_16	chunk_hdr_sz;	/* 12 bytes for first revision of the file format */  
	uint_32	blk_sz;		/* block size in bytes, must be a multiple of 4 (4096) */
	uint_32	total_blks;	/* total blocks in the non-sparse output image */
	uint_32	total_chunks;	/* total chunks in the sparse input image */ 
	uint_32	image_checksum; /* CRC32 checksum of the original data, counting "don't care" */
							/* as 0. Standard 802.3 polynomial, use a Public Domain */
							/* table implementation */
} sparse_header;
#define SPARSE_HEADER_MAGIC	0xed26ff3a
#define UBI_HEADER_MAGIC	0x23494255
#define DHARA_HEADER_MAGIC	0x616844
#define JFFS_HEADER_MAGIC   0xe0011985
#define JFFS_HEADER_MAGIC2  0x20031985
#define CHUNK_TYPE_RAW		0xCAC1
#define CHUNK_TYPE_FILL		0xCAC2
#define CHUNK_TYPE_DONT_CARE	0xCAC3
#define CHUNK_TYPE_CRC32    0xCAC4
typedef struct chunk_header_t {  
	uint_16	chunk_type;	/* 0xCAC1 -> raw; 0xCAC2 -> fill; 0xCAC3 -> don't care */
	uint_16	reserved1;
	uint_32	chunk_sz;	/* in blocks in output image */
	uint_32	total_sz;	/* in bytes of chunk input file including chunk header and data */
} chunk_header;
typedef struct  
{
	uint_16	usYear;
	uint_8	ucMonth;
	uint_8  	ucDay;
	uint_8  	ucHour;
	uint_8  	ucMinute;
	uint_8  	ucSecond;
}STRUCT_RKTIME,*PSTRUCT_RKTIME;
typedef struct
{
	char szItemName[32];
	uint_32 uiItemOffset;
	uint_32 uiItemSize;
}STRUCT_PARAM_ITEM,*PSTRUCT_PARAM_ITEM;
typedef struct
{
	char szItemName[20];
	char szItemValue[256];
}STRUCT_CONFIG_ITEM,*PSTRUCT_CONFIG_ITEM;
// This is an in-memory C++ descriptor, not a wire-format structure. It owns
// std::string fields and must retain their natural alignment on ARM.
#pragma pack()
typedef struct _STRUCT_RKDEVICE_DESC
{
	uint_16 usVid;
	uint_16 usPid;
	uint_16 usbcdUsb;
	uint_32     uiLocationID;
	ENUM_RKUSB_TYPE emUsbType;
	ENUM_RKDEVICE_TYPE emDeviceType;
	ENUM_USB_SPEED emUsbSpeed;
	void*   pUsbHandle;
	// Optional borrowed libusb_device_handle. Android obtains this from the
	// SDK's Java-authorized USB connection; desktop scanners leave it null.
	void*   pOpenedUsbHandle;
	string strSerial;
	string strDevPath;
}STRUCT_RKDEVICE_DESC,*PSTRUCT_RKDEVICE_DESC;
// The following provisioning and firmware structures are binary formats and
// still require byte packing.
#pragma pack(1)
typedef struct _PROVISIONING_DATA 
{
	uint_16 id;
	uint_16 flag;
	uint_16 size;
	uint_16 reserved;
	uint_8   data[1];
}PROVISIONING_DATA,*PPROVISIONING_DATA;
typedef struct {
	uint_32 type;
	uint_32 address;
	uint_32 size;
	uint_32 reserved[1];
}RUN_PARAM, *PRUN_PARAM;
typedef struct {
	uint_32 magic;//"RSPM"
	uint_32 param_count;
	RUN_PARAM param[MAX_RUN_PARAM];
}RUN_PARAM_HEAD, *PRUN_PARAM_HEAD;
#pragma pack()
#pragma pack(1) 
typedef struct
{
	uint_64 offset;
	uint_64 size;
	uint_32 storage;
	uint_32 reserved[3];
	char file[32];
}STORAGE_ENTRY;
typedef struct
{
	uint_32 tag;
	uint_32 head_size;
	uint_32 entry_size;
	uint_32 dwVer;
	uint_32 entry_count;
	uint_8 reserved[44];
}STORAGE_FW_HDR;
#pragma pack()
#define SSFW_TAG                0x57465353
typedef list<STRUCT_RKDEVICE_DESC> RKDEVICE_DESC_SET;
typedef RKDEVICE_DESC_SET::iterator device_list_iter;
typedef vector<string> STRING_VECTOR;
typedef vector<uint_32> UINT_VECTOR;
typedef vector<STRUCT_PARAM_ITEM> PARAM_ITEM_VECTOR;
typedef vector<STRUCT_CONFIG_ITEM> CONFIG_ITEM_VECTOR;
typedef enum
{
		DOWNLOADBOOT_START=1,
		DOWNLOADBOOT_FAIL=2,
		DOWNLOADBOOT_PASS=3,
		DOWNLOADIDBLOCK_START=4,
		DOWNLOADIDBLOCK_FAIL=5,
		DOWNLOADIDBLOCK_PASS=6,
		DOWNLOADIMAGE_START=7,
		DOWNLOADIMAGE_FAIL=8,
		DOWNLOADIMAGE_PASS=9,
		TESTDEVICE_START=10,
		TESTDEVICE_FAIL=11,
		TESTDEVICE_PASS=12,
		RESETDEVICE_START=13,
		RESETDEVICE_FAIL=14,
		RESETDEVICE_PASS=15,
		FORMATDISK_START=16,
		FORMATDISK_FAIL=17,
		FORMATDISK_PASS=18,
		COPYDATA_START=19,
		COPYDATA_FAIL=20,
		COPYDATA_PASS=21,
		WAITMSC_START=22,
		WAITMSC_FAIL=23,
		WAITMSC_PASS=24,
		WAITLOADER_START=25,
		WAITLOADER_FAIL=26,
		WAITLOADER_PASS=27,
		WAITMASKROM_START=28,
		WAITMASKROM_FAIL=29,
		WAITMASKROM_PASS=30,
		ERASEIDB_START=31,
		ERASEIDB_FAIL=32,
		ERASEIDB_PASS=33,
		SWITCHMSC_START=34,
		SWITCHMSC_FAIL=35,
		SWITCHMSC_PASS=36,
		CHECKCHIP_START=37,
		CHECKCHIP_FAIL=38,
		CHECKCHIP_PASS=39,
		PREPAREIDB_START=40,
		PREPAREIDB_FAIL=41,
		PREPAREIDB_PASS=42,
		MUTEXRESETDEVICE_START=43,
		MUTEXRESETDEVICE_FAIL=44,
		MUTEXRESETDEVICE_PASS=45,
		GETOLDDISKSIZE_START=46,
		GETOLDDISKSIZE_FAIL=47,
		GETOLDDISKSIZE_PASS=48,
		READSN_START=49,
		READSN_FAIL=50,
		READSN_PASS=51,
		WRITESN_START=52,
		WRITESN_FAIL=53,
		WRITESN_PASS=54,
		ERASEALLBLOCKS_START=55,
		ERASEALLBLOCKS_FAIL=56,
		ERASEALLBLOCKS_PASS=57,
		GETBLOCKSTATE_START=58,
		GETBLOCKSTATE_FAIL=59,
		GETBLOCKSTATE_PASS=60,
		GETFLASHINFO_START=61,
		GETFLASHINFO_FAIL=62,
		GETFLASHINFO_PASS=63,
		WRITEBACK_START=64,
		WRITEBACK_FAIL=65,
		WRITEBACK_PASS=66,
		FINDUSERDISK_START=67,
		FINDUSERDISK_FAIL=68,
		FINDUSERDISK_PASS=69,
		SHOWUSERDISK_START=70,
		SHOWUSERDISK_FAIL=71,
		SHOWUSERDISK_PASS=72,
		READMAC_START=73,
		READMAC_FAIL=74,
		READMAC_PASS=75,
		WRITEMAC_START=76,
		WRITEMAC_FAIL=77,
		WRITEMAC_PASS=78,
		READBT_START=79,
		READBT_FAIL=80,
		READBT_PASS=81,
		WRITEBT_START=82,
		WRITEBT_FAIL=83,
		WRITEBT_PASS=84,
		LOWERFORMAT_START=85,
		LOWERFORMAT_FAIL=86,
		LOWERFORMAT_PASS=87,
		READIMEI_START=88,
		READIMEI_FAIL=89,
		READIMEI_PASS=90,
		WRITEIMEI_START=91,
		WRITEIMEI_FAIL=92,
		WRITEIMEI_PASS=93,
		SHOWDATADISK_START=94,
		SHOWDATADISK_FAIL=95,
		SHOWDATADISK_PASS=96,
		FINDDATADISK_START=97,
		FINDDATADISK_FAIL=98,
		FINDDATADISK_PASS=99,
		FORMATDATADISK_START=100,
		FORMATDATADISK_FAIL=101,
		FORMATDATADISK_PASS=102,
		COPYDATADISK_START=103,
		COPYDATADISK_FAIL=104,
		COPYDATADISK_PASS=105,
		READUID_START=106,
		READUID_FAIL=107,
		READUID_PASS=108,
		READCUSTOMDATA_START=109,
		READCUSTOMDATA_FAIL=110,
		READCUSTOMDATA_PASS=111,
		WRITECUSTOMDATA_START=112,
		WRITECUSTOMDATA_FAIL=113,
		WRITECUSTOMDATA_PASS=114,
		SETRESETFLAG_START=115,
		SETRESETFLAG_FAIL=116,
		SETRESETFLAG_PASS=117,
		POWEROFF_START=118,
		POWEROFF_FAIL=119,
		POWEROFF_PASS=120,
		READALLINFO_START=121,
		READALLINFO_FAIL=122,
		READALLINFO_PASS=123,
		WRITEALLINFO_START=124,
		WRITEALLINFO_FAIL=125,
		WRITEALLINFO_PASS=126,
		RESETMSC_START=127,
		RESETMSC_FAIL=128,
		RESETMSC_PASS=129,
		SWITCHUSB3_START=142,
		SWITCHUSB3_FAIL=143,
		SWITCHUSB3_PASS=144,
}ENUM_UPGRADE_PROMPT;
typedef enum
{
	TESTDEVICE_PROGRESS,
	DOWNLOADIMAGE_PROGRESS,
	CHECKIMAGE_PROGRESS,
	TAGBADBLOCK_PROGRESS,
	TESTBLOCK_PROGRESS,
	ERASEFLASH_PROGRESS,
	ERASESYSTEM_PROGRESS,
	LOWERFORMAT_PROGRESS,
	ERASEUSERDATA_PROGRESS
}ENUM_PROGRESS_PROMPT;
#define	MSC_SWITCHROCKUSB	0xFFFFFFFE
#define MSC_GETVERSIONINFO	0xFFFFFFFF
#define MSC_RESETDEVICE		0xFFFFFFFD
#define MSC_GETCHIPINFO		0xFFFFFFFC
#define MSC_SHOWUSERDISK	0xFFFFFFFB
#define MSC_GETDEVIVEUID	0xFFFFFFF7
#define MSC_SHOWDATADISK	0xFFFFFFF6
#define MSC_GETPRODUCTMODEL 0xFFFFFFF3
#define MSC_GETPARAMETER	0xFFFFFFF2
#define MSC_GETIDBSECTOR	0xFFFFFFF1
#define MSC_GETPRODUCTSN	0xFFFFFFEF
typedef enum
{
	CALL_FIRST,
	CALL_MIDDLE,
	CALL_LAST
}ENUM_CALL_STEP;
/*----------------------------------------------------------------------
Name    :   UpgradeStepPromptCB
Desc    :   Callback function for prompting upgrade step
Params  :   (IN)deviceLayer:			Device layer ID, used to distinguish devices
            (IN)promptID:				Used to specify state of certain upgrade step
            (IN)oldDeviceLayer			Reserved
Return  :   TRUE:						SUCCESSED
            FALSE:						FAILED
Notes   :   
----------------------------------------------------------------------*/
typedef void (*UpgradeStepPromptCB)(uint_32 deviceLayer,ENUM_UPGRADE_PROMPT promptID,uint_32 oldDeviceLayer);
/*----------------------------------------------------------------------
Name    :   UpgradeStepPromptCB
Desc    :   Callback function for prompting upgrade step which takes long time
Params  :   (IN)deviceLayer:			Device layer ID, used to distinguish devices
            (IN)promptID:				Used to specify progress of certain upgrade step
            (IN)totalValue				Total value, e.g. Flash capacity
			(IN)currentValue			Current value, e.g. Current block being erased
			(IN)emCall					Used for prompt, refer to routine
Return  :   TRUE:						SUCCESSED
            FALSE:						FAILED
Notes   :   
----------------------------------------------------------------------*/
typedef void (*ProgressPromptCB)(uint_32 deviceLayer,ENUM_PROGRESS_PROMPT promptID,long long totalValue,long long currentValue,ENUM_CALL_STEP emCall);

#endif
