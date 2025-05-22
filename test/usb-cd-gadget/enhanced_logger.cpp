#include "enhanced_logger.h"
#include <circle/util.h>
#include "util.h"

CEnhancedLogger::CEnhancedLogger(unsigned nLogLevel, CTimer *pTimer, const char *pLogFilePath)
    : CLogger(nLogLevel, pTimer),
      m_LogFilePath(pLogFilePath),
      m_bFileInitialized(FALSE),
      m_pTimer(pTimer)  // Store the timer pointer
{
}

CEnhancedLogger::~CEnhancedLogger(void)
{
    if (m_bFileInitialized)
    {
        f_close(&m_LogFile);
    }
}

boolean CEnhancedLogger::Initialize(CDevice *pTarget)
{
    // Initialize the base logger
    boolean bOK = CLogger::Initialize(pTarget);
    if (!bOK)
    {
        return FALSE;
    }

    // Open log file for writing (append mode)
    FRESULT Result = f_open(&m_LogFile, m_LogFilePath, FA_WRITE | FA_OPEN_ALWAYS);
    
    if (Result != FR_OK)
    {
        // File open failed, but base logger is still working
        // so return TRUE but remember file isn't initialized
        m_bFileInitialized = FALSE;
        CLogger::Write("enhlogger", LogError, "Failed to open log file");
        return TRUE;
    }
    
    // Seek to end of file to append
    f_lseek(&m_LogFile, f_size(&m_LogFile));
    
    m_bFileInitialized = TRUE;
    
    // Write header to the log file
    const char* Header = "\n--- New Session Started ---\n";
    UINT BytesWritten;
    Result = f_write(&m_LogFile, Header, strlen(Header), &BytesWritten);
    if (Result != FR_OK || BytesWritten != strlen(Header))
    {
        CLogger::Write("enhlogger", LogError, "Failed to write header to log file");
    }
    
    f_sync(&m_LogFile);
    
    // Test write to confirm file is working
    const char* TestMsg = "Logger initialized and ready\n";
    Result = f_write(&m_LogFile, TestMsg, strlen(TestMsg), &BytesWritten);
    if (Result != FR_OK || BytesWritten != strlen(TestMsg))
    {
        CLogger::Write("enhlogger", LogError, "Failed to write test message to log file");
    }
    f_sync(&m_LogFile);

    CLogger::Write("enhlogger", LogNotice, "Enhanced logger initialized successfully");
    return TRUE;
}

void CEnhancedLogger::Write(const char *pSource, TLogSeverity Severity, const char *pMessage)
{
    // First, perform the normal logging operation
    CLogger::Write(pSource, Severity, pMessage);
    
    // Additionally write to the file
    if (m_bFileInitialized)
    {
        WriteToFile(pSource, Severity, pMessage);
    }
}

void CEnhancedLogger::WriteToFile(const char *pSource, TLogSeverity Severity, const char *pMessage)
{
    if (!m_bFileInitialized)
    {
        return;
    }

    // Format the log entry similar to base logger but tailored for file
    const char *pSeverityName = "???";
    switch (Severity)
    {
    case LogPanic:   pSeverityName = "PANIC";   break;
    case LogError:   pSeverityName = "ERROR";   break;
    case LogWarning: pSeverityName = "WARNING"; break;
    case LogNotice:  pSeverityName = "NOTICE";  break;
    case LogDebug:   pSeverityName = "DEBUG";   break;
    default:         pSeverityName = "UNKNOWN"; break;
    }

    // Create the log entry with a timestamp - prepare it before file operations
    char LogEntry[512];
    
    // Use timer to get timestamp
    unsigned long ticks = 0;
    if (m_pTimer != nullptr)
    {
        ticks = m_pTimer->GetTicks();
    }
    
    snprintf(LogEntry, sizeof(LogEntry), "[%lu] [%s] %s: %s\n", 
             ticks, pSource, pSeverityName, pMessage);
    
    // Open a fresh file handle for each write
    FIL tempFile;
    FRESULT Result = f_open(&tempFile, m_LogFilePath, FA_WRITE | FA_OPEN_ALWAYS);
    if (Result != FR_OK)
    {
        return;
    }
    
    // Seek to end of file to append
    f_lseek(&tempFile, f_size(&tempFile));
    
    // Write to file
    UINT BytesWritten;
    Result = f_write(&tempFile, LogEntry, strlen(LogEntry), &BytesWritten);
    
    // Ensure log is written immediately
    f_sync(&tempFile);
    
    // Close the file immediately after writing
    f_close(&tempFile);
    
    // Yield to allow other processes to run
    if (CScheduler::Get() != nullptr)
    {
        CScheduler::Get()->Yield();
    }
}