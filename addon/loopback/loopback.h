#ifndef _LOOPBACKFILEDEVICE_H
#define _LOOPBACKFILEDEVICE_H

#include <circle/device.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/fs/partitionmanager.h>
#include <circle/types.h>
#include <circle/sysconfig.h>
#include <fatfs/ff.h>
#include <circle/logger.h>
#include <linux/kernel.h>

class CLoopbackFileDevice : public CDevice
{
public:
	CLoopbackFileDevice(const char* pName, FIL* pFile);
	~CLoopbackFileDevice(void);

	int Read (void *pBuffer, size_t nCount);
        int Write (const void *pBuffer, size_t nCount);
        u64 Seek (u64 ullOffset);
        u64 GetSize (void) const;
	static void LogWrite (TLogSeverity Severity, const char *pMessage, ...);

private:
	FIL* m_pFile;
	char m_DeviceName[16];

};

#endif

