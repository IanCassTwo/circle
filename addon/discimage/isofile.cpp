#include "isofile.h"
#include "ioctl.h"
#include <circle/util.h>
#include <circle/stdarg.h>
#include <assert.h>
#include <string.h>

#define LOGSOURCE "CISOFileDevice"

CISOFileDevice::CISOFileDevice(FIL* pFile)
{
	m_pFile = pFile;
	m_FileType = FileType::TypeISO;
}

CISOFileDevice::~CISOFileDevice(void)
{
	f_close(m_pFile);
}

int CISOFileDevice::Read(void *pBuffer, size_t nSize)
{
	if (!m_pFile) {
                LogWrite(LogError, "Read !m_pFile");
		return -1;
	}

	UINT nBytesRead = 0;
	FRESULT result = f_read(m_pFile, pBuffer, nSize, &nBytesRead);
	if (result != FR_OK)
        {
                LogWrite(LogError, "Failed to read %d bytes into memory", nSize);
                return -1;
        }
	return nBytesRead;
}

int CISOFileDevice::Write(const void *pBuffer, size_t nSize)
{
	// Read-only device
	return -1;
}

u64 CISOFileDevice::Seek(u64 nOffset)
{
	if (!m_pFile) {
                LogWrite(LogError, "Seek !m_pFile");
		return -1;
	}

	if (f_lseek(m_pFile, nOffset) != FR_OK)
	{
                LogWrite(LogError, "Seek Not Ok");
		return 0;
	}
	return nOffset;
}

u64 CISOFileDevice::GetSize(void) const
{
	if (!m_pFile) {
                LogWrite(LogError, "GetSize !m_pFile");
		return -1;
	}

	return f_size(m_pFile);
}

int CISOFileDevice::IOCtl(unsigned long ulCmd, void *pData)
{
	switch (ulCmd)
	{
		// All because C++ doesn't have a convenient "instanceof" 
		// function and I can't use dynamic_cast
		case IOC_FILE_TYPE:
			if (pData != nullptr)
			{
				FileType* pFileType = static_cast<FileType*>(pData);
				*pFileType = m_FileType;
				return 0;
			}
			return -1;
	}
	return -1;
}

void CISOFileDevice::LogWrite (TLogSeverity Severity, const char *pMessage, ...)
{
        assert (pMessage != 0);

        va_list var;
        va_start (var, pMessage);

        CLogger::Get ()->WriteV ("loopbackfiledevice", Severity, pMessage, var);

        va_end (var);
}
