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
#include "filetype.h"

class CCueBinFileDevice : public CDevice
{
public:
	CCueBinFileDevice(FIL* pFile, const char* cue_str = nullptr);
	~CCueBinFileDevice(void);

	int Read (void *pBuffer, size_t nCount);
        int Write (const void *pBuffer, size_t nCount);
        u64 Seek (u64 ullOffset);
        u64 GetSize (void) const;
	int IOCtl(unsigned long ulCmd, void *pData);
	const char* GetCueSheet() const;
	static void LogWrite (TLogSeverity Severity, const char *pMessage, ...);

private:
	FIL* m_pFile;
	FileType m_FileType = FileType::ISO;
	const char* m_cue_str = nullptr;
	static constexpr const char* default_cue_sheet =
		"FILE \"image.iso\" BINARY\n"
		"  TRACK 01 MODE1/2048\n"
		"    INDEX 01 00:00:00\n";
};

#endif

