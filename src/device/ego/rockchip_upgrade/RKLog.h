#ifndef RKLOG_HEADER
#define RKLOG_HEADER
#include "DefineHeader.h"

#define GET_FMT_STRING(fmt, buf) \
	{ \
	va_list args; \
	va_start(args, fmt); \
	vsnprintf(buf, sizeof(buf)-1, fmt, args); \
	va_end(args); \
	buf[sizeof(buf)-1] = 0x00; \
	};



class CRKLog
{
public:
	string GetLogSavePath();
 	bool GetEnableLog();
 	void SetEnableLog(bool bEnable);
 	property<CRKLog,string,READ_ONLY> LogSavePath;
 	property<CRKLog,bool,READ_WRITE> EnableLog;
	CRKLog(string logFilePath,string logFileName,bool enable=true);
	~CRKLog();
	bool SaveBuffer(string fileName,uint_8* lpBuffer,uint_32 dwSize);
	void PrintBuffer(string &strOutput,uint_8* lpBuffer,uint_32 dwSize,uint_32 uiLineCount=16);
	void Record(const char* lpFmt,...);
	
protected:
private:
	string m_path;
	string  m_name;
	bool    m_enable;
	bool Write(string text);
};

#endif