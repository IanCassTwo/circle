#ifndef _ISOFILEDEVICE_H
#define _ISOFILEDEVICE_H

#include <circle/device.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/fs/partitionmanager.h>
#include <circle/types.h>
#include <circle/sysconfig.h>
#include <fatfs/ff.h>
#include <circle/logger.h>
#include <linux/kernel.h>
#include "filetype.h"

class CISOFileDevice : public CDevice
{
public:
	CISOFileDevice(FIL* pFile);
	~CISOFileDevice(void);

	int Read (void *pBuffer, size_t nCount);
        int Write (const void *pBuffer, size_t nCount);
        u64 Seek (u64 ullOffset);
        u64 GetSize (void) const;
	int IOCtl(unsigned long ulCmd, void *pData);
	static void LogWrite (TLogSeverity Severity, const char *pMessage, ...);

private:
	FIL* m_pFile;
	FileType m_FileType;
};

#endif

