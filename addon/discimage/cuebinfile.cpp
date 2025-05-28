#include "cuebinfile.h"

#include <assert.h>
#include <circle/stdarg.h>
#include <circle/util.h>
#include <stdlib.h>
#include <string.h>

#define LOGSOURCE "CCueBinFileDevice"

CCueBinFileDevice::CCueBinFileDevice(FIL *pFile, char *cue_str) {
    m_pFile = pFile;
    if (cue_str != nullptr) {
        size_t len = strlen(cue_str);
        m_cue_str = new char[len + 1];
        strcpy(m_cue_str, cue_str);
        m_FileType = FileType::CUEBIN;
    } else {
        size_t len = strlen(default_cue_sheet);
        m_cue_str = new char[len + 1];
        strcpy(m_cue_str, default_cue_sheet);
        m_FileType = FileType::ISO;
    }
}

CCueBinFileDevice::~CCueBinFileDevice(void) {
    f_close(m_pFile);
    if (m_cue_str != nullptr)
        delete[] m_cue_str;
}

int CCueBinFileDevice::Read(void *pBuffer, size_t nSize) {
    if (!m_pFile) {
        LogWrite(LogError, "Read !m_pFile");
        return -1;
    }

    UINT nBytesRead = 0;
    FRESULT result = f_read(m_pFile, pBuffer, nSize, &nBytesRead);
    if (result != FR_OK) {
        LogWrite(LogError, "Failed to read %d bytes into memory, err %d", nSize, result);
        return -1;
    }
    return nBytesRead;
}

int CCueBinFileDevice::Write(const void *pBuffer, size_t nSize) {
    // Read-only device
    return -1;
}

u64 CCueBinFileDevice::Seek(u64 nOffset) {
    if (!m_pFile) {
        LogWrite(LogError, "Seek !m_pFile");
        return -1;
    }

    FRESULT result = f_lseek(m_pFile, nOffset);
    if (result != FR_OK) {
        LogWrite(LogError, "Seek to offset %llu is not ok, err %d", nOffset, result);
        return 0;
    }
    return nOffset;
}

u64 CCueBinFileDevice::GetSize(void) const {
    if (!m_pFile) {
        LogWrite(LogError, "GetSize !m_pFile");
        return 0;
    }

    u64 size = f_size(m_pFile);
    if (size < 0) {
        LogWrite(LogError, "GetSize f_size < 0");
        return 0;
    }

    return size;
}

char *readCueFromFIL(FIL *file) {
    // Get file size
    DWORD file_size = f_size(file);
    char *buffer = (char *)malloc(file_size + 1);  // +1 for null terminator
    if (!buffer) return nullptr;

    UINT bytes_read;
    FRESULT res = f_read(file, buffer, file_size, &bytes_read);
    if (res != FR_OK || bytes_read != file_size) {
        free(buffer);
        return nullptr;
    }
    f_close(file);
    buffer[file_size] = '\0';  // null terminate
    return buffer;
}

const char *CCueBinFileDevice::GetCueSheet() const {
    return m_cue_str;
}

void CCueBinFileDevice::LogWrite(TLogSeverity Severity, const char *pMessage, ...) {
    assert(pMessage != 0);

    va_list var;
    va_start(var, pMessage);

    CLogger::Get()->WriteV("cuebinfiledevice", Severity, pMessage, var);

    va_end(var);
}
