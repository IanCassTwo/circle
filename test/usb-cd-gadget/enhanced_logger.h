#ifndef _enhanced_logger_h
#define _enhanced_logger_h

#include <circle/logger.h>
#include <fatfs/ff.h>
#include <circle/string.h>
#include <circle/sched/scheduler.h>
#include <circle/timer.h>

class CEnhancedLogger : public CLogger
{
public:
    CEnhancedLogger(unsigned nLogLevel, CTimer *pTimer, const char *pLogFilePath);
    virtual ~CEnhancedLogger(void);

    boolean Initialize(CDevice *pTarget);
    
    // Override the Write method to also write to file
    virtual void Write(const char *pSource, TLogSeverity Severity, const char *pMessage);

private:
    void WriteToFile(const char *pSource, TLogSeverity Severity, const char *pMessage);

    FIL m_LogFile;
    CString m_LogFilePath;
    boolean m_bFileInitialized;
    CTimer *m_pTimer;  // Store the timer pointer directly
};

#endif