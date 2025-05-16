#include "cuebinfile.h"
#include "ioctl.h"
#include <circle/util.h>
#include <circle/stdarg.h>
#include <assert.h>
#include <string.h>

#define LOGSOURCE "CCueBinFileDevice"

CCueBinFileDevice::CCueBinFileDevice(FIL* pFile, FIL* cFile)
{
	m_pFile = pFile;
	m_cFile = cFile;
	m_FileType = FileType::TypeCUEBIN;
}

CCueBinFileDevice::~CCueBinFileDevice(void)
{
	f_close(m_pFile);
	f_close(m_cFile);
}

int CCueBinFileDevice::Read(void *pBuffer, size_t nSize)
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

int CCueBinFileDevice::Write(const void *pBuffer, size_t nSize)
{
	// Read-only device
	return -1;
}

u64 CCueBinFileDevice::Seek(u64 nOffset)
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

u64 CCueBinFileDevice::GetSize(void) const
{
	if (!m_pFile) {
                LogWrite(LogError, "GetSize !m_pFile");
		return -1;
	}

	return f_size(m_pFile);
}

int CCueBinFileDevice::IOCtl(unsigned long ulCmd, void *pData)
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

FIL* CCueBinFileDevice::GetCueFileHandle() const {
    return m_cFile;
}

void CCueBinFileDevice::LogWrite (TLogSeverity Severity, const char *pMessage, ...)
{
        assert (pMessage != 0);

        va_list var;
        va_start (var, pMessage);

        CLogger::Get ()->WriteV ("loopbackfiledevice", Severity, pMessage, var);

        va_end (var);
}
