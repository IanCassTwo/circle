#include "loopback.h"
#include <circle/util.h>
#include <circle/stdarg.h>
#include <assert.h>
#include <string.h>

#define LOGSOURCE "loopbackfile"

CLoopbackFileDevice::CLoopbackFileDevice(const char* pName, FIL* pFile)
{
	snprintf(m_DeviceName, sizeof(m_DeviceName), "loopback-%s", pName);
	m_pFile = pFile;
}

CLoopbackFileDevice::~CLoopbackFileDevice(void)
{
}

int CLoopbackFileDevice::Read(void *pBuffer, size_t nSize)
{
	if (!m_pFile)
		return -1;

	UINT nBytesRead = 0;
	FRESULT result = f_read(m_pFile, pBuffer, nSize, &nBytesRead);
	if (result != FR_OK)
        {
                LogWrite(LogError, "Failed to read %d bytes into memory", nSize);
                return -1;
        }
	return nBytesRead;
}

int CLoopbackFileDevice::Write(const void *pBuffer, size_t nSize)
{
	// Read-only device
	return -1;
}

u64 CLoopbackFileDevice::Seek(u64 nOffset)
{
	if (!m_pFile)
		return -1;

	if (f_lseek(m_pFile, nOffset) != FR_OK)
	{
		return 0;
	}
	return nOffset;
}

u64 CLoopbackFileDevice::GetSize(void) const
{
	if (!m_pFile)
		return 0;

	return f_size(m_pFile);
}

void CLoopbackFileDevice::LogWrite (TLogSeverity Severity, const char *pMessage, ...)
{
        assert (pMessage != 0);

        va_list var;
        va_start (var, pMessage);

        CLogger::Get ()->WriteV ("loopbackfiledevice", Severity, pMessage, var);

        va_end (var);
}

