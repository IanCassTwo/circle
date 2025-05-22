//
// kernel.cpp
//
// Test for USB Mass Storage Gadget by Mike Messinides
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2023-2024  R. Stange <rsta2@o2online.de>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "kernel.h"
#include "webserver.h"
#include "ftpdaemon.h"
#include "util.h"

#define DRIVE "SD:"
#define FIRMWARE_PATH   DRIVE "/firmware/"
#define CONFIG_FILE     DRIVE "/wpa_supplicant.conf"
#define HOSTNAME "CDROM"
#define LOG_FILE        DRIVE "/system.log"

LOGMODULE ("kernel");

CKernel* CKernel::s_pInstance = nullptr;

CKernel::CKernel (void)
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
    m_Timer (&m_Interrupt),
    m_Logger (m_Options.GetLogLevel (), &m_Timer, LOG_FILE),
    m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
    m_WLAN (FIRMWARE_PATH),
        m_Net (0, 0, 0, 0, HOSTNAME, NetDeviceTypeWLAN),
        m_WPASupplicant (CONFIG_FILE),
    m_CDGadget (&m_Interrupt)
{
	//m_ActLED.Blink (5);	// show we are alive
    s_pInstance = this; // Set the static instance pointer
}

CKernel::~CKernel (void)
{
}

boolean CKernel::Initialize (void)
{
	boolean bOK = TRUE;

	if (bOK)
	{
		bOK = m_Screen.Initialize ();
		LOGNOTE("Initialized screen");
	}

	if (bOK)
	{
		bOK = m_Serial.Initialize (115200);
		LOGNOTE("Initialized serial");
	}

	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Screen;
		}

		bOK = m_Logger.Initialize (pTarget);
		LOGNOTE("Initialized logger");
	}

	if (bOK)
	{
		bOK = m_Interrupt.Initialize ();
		LOGNOTE("Initialized interrupts");
	}

	if (bOK)
	{
		bOK = m_Timer.Initialize ();
		LOGNOTE("Initialized timer");
	}

	if (bOK)
	{
		bOK = m_EMMC.Initialize ();
		LOGNOTE("Initialized eMMC");
	}

	if (bOK)
        {
                if (f_mount (&m_FileSystem, DRIVE, 1) != FR_OK)
                {
                        LOGERR("Cannot mount drive: %s", DRIVE);

                        bOK = FALSE;
                }
                else
                {
                        LOGNOTE("Initialized filesystem");
                        
                        // Write directly to the log file
                        WriteToLogFileSimple("Filesystem initialized successfully");
                }
        }

	// Re-initialize the logger after the filesystem is mounted
	// to ensure file logging works properly
	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Screen;
		}

		// Re-initialize the logger to enable file logging now that the filesystem is mounted
		if (!m_Logger.Initialize (pTarget))
		{
			LOGERR("Failed to re-initialize logger for file logging");
		}
		else
		{
			LOGNOTE("File logging enabled");
			// Write a test message to verify logging works
			LOGNOTE("This message should appear in the log file");
		}
	}

	if (bOK)
	{
		// After file system is mounted and logger is re-initialized
		// Add a specific test to verify log file writing
		if (WriteToLogFileSimple("DIRECT TEST: Log file write test after initialization"))
		{
			LOGNOTE("Direct log write test succeeded");
		}
		else
		{
			LOGERR("Direct log write test failed");
		}
		
		// Try multiple LOGNOTE calls to see if they appear in the log
		LOGNOTE("TEST LOG 1: This should appear in the log file");
		LOGNOTE("TEST LOG 2: This should also appear in the log file");
		LOGNOTE("TEST LOG 3: And this should appear in the log file too");
	}

	/*
	if(bOK)
	{
		bOK = m_CDGadget.Initialize ();
		LOGNOTE("Initialized cdrom");
	}
	*/

	if (bOK)
        {
                bOK = m_WLAN.Initialize ();
		LOGNOTE("Initialized WLAN");
        }

        if (bOK)
        {
                bOK = m_Net.Initialize (FALSE);
		LOGNOTE("Initialized network");
        }

        if (bOK)
        {
                bOK = m_WPASupplicant.Initialize ();
		LOGNOTE("Initialized WAP supplicant");
        }

	return bOK;
}

TShutdownMode CKernel::Run (void)
{
    LOGNOTE ("=====================================");
    LOGNOTE ("Welcome to USBODE"); 
    LOGNOTE ("Compile time: " __DATE__ " " __TIME__);
    LOGNOTE ("=====================================");
    
    // Log directly to file
    WriteToLogFileSimple("=====================================");
    WriteToLogFileSimple("Welcome to USBODE");
    char CompileMsg[100];
    snprintf(CompileMsg, sizeof(CompileMsg), "Compile time: %s %s", __DATE__, __TIME__);
    WriteToLogFileSimple(CompileMsg);
    WriteToLogFileSimple("=====================================");

	char imageName[MAX_FILENAME];
	if (getCurrentMountedImage(imageName, sizeof(imageName))) {
	    LOGNOTE("Found image filename %s", imageName);
	} else {
	    strcpy(imageName, "image.iso");
	    LOGERR("Could not load image name, using default: %s", imageName);
		WriteToLogFileFmt("Could not load image name %s, falling back to default which is image.iso", imageName);
	}

	CCueBinFileDevice* cueBinFileDevice = loadCueBinFileDevice(imageName);
	if (!cueBinFileDevice) {
		LOGERR("Failed to get cueBinFileDevice");
		return ShutdownHalt;
	}

	m_CDGadget.SetDevice (cueBinFileDevice);
	m_CDGadget.Initialize ();

	bool showIP = true;
	static const char ServiceName[] = HOSTNAME;
	static const char *ppText[] = {"path=/index.html", nullptr};
	CmDNSPublisher *pmDNSPublisher = nullptr;
	CWebServer *pCWebServer = nullptr;
	CFTPDaemon *m_pFTPDaemon = nullptr;

	for (unsigned nCount = 0; 1; nCount++)
	{
		// must be called from TASK_LEVEL to allow I/O operations
		m_CDGadget.UpdatePlugAndPlay ();
		m_CDGadget.Update ();

		// Show details of the network connection
		if (m_Net.IsRunning() && showIP) {
                        showIP = false;
                        LOGNOTE("==========================================");
                        m_WLAN.DumpStatus ();
                        CString IPString;
                        m_Net.GetConfig ()->GetIPAddress ()->Format (&IPString);
                        LOGNOTE("Our IP address is %s", (const char *) IPString);
						WriteToLogFileFmt("Our IP address is %s", (const char *) IPString);
                        LOGNOTE("==========================================");
		}

		// Publish mDNS
		if (m_Net.IsRunning() && pmDNSPublisher == nullptr) {
			pmDNSPublisher = new CmDNSPublisher (&m_Net);
			if (!pmDNSPublisher->PublishService (ServiceName, "_http._tcp", 5004, ppText))
			{
				LOGNOTE ("Cannot publish service");
			}
			LOGNOTE("Published mDNS");
		}

		// Start the Web Server
		if (m_Net.IsRunning() && pCWebServer == nullptr) {
			pCWebServer = new CWebServer (&m_Net, &m_CDGadget, &m_ActLED) ;
			LOGNOTE("Started Webserver");
                }

		// Start the FTP Server
		if (m_Net.IsRunning() && !m_pFTPDaemon)
		{
			m_pFTPDaemon = new CFTPDaemon("cdrom", "cdrom");
			if (!m_pFTPDaemon->Initialize())
			{
				LOGERR("Failed to init FTP daemon");
				delete m_pFTPDaemon;
				m_pFTPDaemon = nullptr;
			}
			else
				LOGNOTE("FTP daemon initialized");
		 }

		// Check for shutdown/reboot request from the web interface
		if (pCWebServer != nullptr) {
			TShutdownMode mode = pCWebServer->GetShutdownMode();
			if (mode != ShutdownNone) {
				LOGNOTE("Shutdown requested via web interface: %s", 
					   (mode == ShutdownReboot) ? "Reboot" : "Halt");
				
				// Clean up resources
				delete pmDNSPublisher;
				delete pCWebServer;
				if (m_pFTPDaemon) {
					delete m_pFTPDaemon;
				}
				
				return mode;
			}
		}

                m_Scheduler.Yield();

		// Stop spinning
                //m_Scheduler.MsSleep(10);
	}

	LOGNOTE("ShutdownHalt");
	return ShutdownHalt;
}

boolean CKernel::WriteToLogFileSimple(const char *pMessage)
{
    if (!m_FileSystem.fs_type) {
        // File system not mounted yet
        return FALSE;
    }

    FIL logFile;
    FRESULT Result = f_open(&logFile, LOG_FILE, FA_WRITE | FA_OPEN_ALWAYS);
    
    if (Result != FR_OK) {
        return FALSE;
    }
    
    // Seek to end of file to append
    f_lseek(&logFile, f_size(&logFile));
    
    // Add timestamp
    char LogEntry[512];
    unsigned long ticks = m_Timer.GetTicks();
    snprintf(LogEntry, sizeof(LogEntry), "[%lu] %s\n", ticks, pMessage);
    
    // Write to file
    UINT BytesWritten;
    Result = f_write(&logFile, LogEntry, strlen(LogEntry), &BytesWritten);
    
    // Ensure log is written immediately
    f_sync(&logFile);
    f_close(&logFile);
    
    return (Result == FR_OK);
}

boolean CKernel::WriteToLogFile(const char *pSource, TLogSeverity Severity, const char *pMessage)
{
    if (!m_FileSystem.fs_type) {
        // File system not mounted yet
        return FALSE;
    }

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

    // Format the complete message
    char LogMessage[512];
    snprintf(LogMessage, sizeof(LogMessage), "[%s] %s: %s", 
             pSource, pSeverityName, pMessage);
    
    // Use the existing WriteToLogFile method to do the actual writing
    return WriteToLogFileSimple(LogMessage);
}

boolean CKernel::WriteToLogFileFmt(const char *pFormat, ...)
{
    if (!m_FileSystem.fs_type) {
        // File system not mounted yet
        return FALSE;
    }

    FIL logFile;
    FRESULT Result = f_open(&logFile, LOG_FILE, FA_WRITE | FA_OPEN_ALWAYS);
    
    if (Result != FR_OK) {
        return FALSE;
    }
    
    // Seek to end of file to append
    f_lseek(&logFile, f_size(&logFile));
    
    // Format timestamp
    char LogPrefix[32];
    unsigned long ticks = m_Timer.GetTicks();
    
    // Manually format the prefix to avoid snprintf
    LogPrefix[0] = '[';
    
    // Convert ticks to string
    char tickStr[20];
    unsigned long val = ticks;
    int pos = 0;
    
    // Handle the case where ticks is 0
    if (val == 0) {
        tickStr[pos++] = '0';
    } else {
        // Convert to digits in reverse order
        int temp[20];
        int tpos = 0;
        while (val > 0) {
            temp[tpos++] = val % 10;
            val /= 10;
        }
        
        // Reverse the digits
        while (tpos > 0) {
            tickStr[pos++] = '0' + temp[--tpos];
        }
    }
    
    tickStr[pos] = '\0';
    
    // Copy to prefix buffer
    int prefixPos = 1;
    for (int i = 0; tickStr[i]; i++) {
        LogPrefix[prefixPos++] = tickStr[i];
    }
    
    LogPrefix[prefixPos++] = ']';
    LogPrefix[prefixPos++] = ' ';
    LogPrefix[prefixPos] = '\0';
    
    // Write prefix to file
    UINT BytesWritten;
    Result = f_write(&logFile, LogPrefix, prefixPos, &BytesWritten);
    
    if (Result != FR_OK) {
        f_close(&logFile);
        return FALSE;
    }
    
    // Format and write the message with va_list
    va_list args;
    va_start(args, pFormat);
    
    // We'll buffer the output in small chunks to avoid stack issues
    char buffer[128];
    int totalWritten = 0;
    int i = 0;
    
    // Process format string character by character
    while (pFormat[i]) {
        int bufPos = 0;
        
        // Copy until next % or end of string, up to buffer size
        // Fix the signed/unsigned comparison warning
        while (pFormat[i] && pFormat[i] != '%' && bufPos < (int)(sizeof(buffer) - 1)) {
            buffer[bufPos++] = pFormat[i++];
        }
        
        // Handle % format specifiers
        if (pFormat[i] == '%') {
            i++; // Skip the %
            
            // Handle the format specifier
            switch (pFormat[i]) {
                case 's': {
                    // String specifier
                    const char* str = va_arg(args, const char*);
                    
                    // Write what we have in the buffer so far
                    if (bufPos > 0) {
                        f_write(&logFile, buffer, bufPos, &BytesWritten);
                        totalWritten += BytesWritten;
                    }
                    
                    // Write the string argument directly
                    if (str) {
                        size_t len = strlen(str);
                        f_write(&logFile, str, len, &BytesWritten);
                        totalWritten += BytesWritten;
                    } else {
                        const char* nullStr = "(null)";
                        f_write(&logFile, nullStr, 6, &BytesWritten);
                        totalWritten += BytesWritten;
                    }
                    
                    bufPos = 0; // Reset buffer
                    i++; // Move past the 's'
                    break;
                }
                
                case 'd':
                case 'i':
                case 'u': {
                    // Integer specifier
                    int val = va_arg(args, int);
                    
                    // Write what we have in the buffer so far
                    if (bufPos > 0) {
                        f_write(&logFile, buffer, bufPos, &BytesWritten);
                        totalWritten += BytesWritten;
                    }
                    
                    // Remove unused variables warning - no need to declare these anymore
                    // char numStr[20];
                    // int numPos = 0;
                    
                    // Handle negative numbers
                    if (pFormat[i] != 'u' && val < 0) {
                        f_write(&logFile, "-", 1, &BytesWritten);
                        totalWritten += BytesWritten;
                        val = -val;
                    }
                    
                    // Handle zero
                    if (val == 0) {
                        f_write(&logFile, "0", 1, &BytesWritten);
                        totalWritten += 1;
                    } else {
                        // Convert to digits in reverse order
                        int temp[20];
                        int tpos = 0;
                        while (val > 0) {
                            temp[tpos++] = val % 10;
                            val /= 10;
                        }
                        
                        // Write in correct order
                        while (tpos > 0) {
                            char digit = '0' + temp[--tpos];
                            f_write(&logFile, &digit, 1, &BytesWritten);
                            totalWritten += 1;
                        }
                    }
                    
                    bufPos = 0; // Reset buffer
                    i++; // Move past the format specifier
                    break;
                }
                
                default:
                    // Unknown format specifier, just copy it
                    buffer[bufPos++] = '%';
                    buffer[bufPos++] = pFormat[i++];
                    break;
            }
        }
        
        // Write current buffer if it has content
        if (bufPos > 0) {
            f_write(&logFile, buffer, bufPos, &BytesWritten);
            totalWritten += BytesWritten;
        }
    }
    
    va_end(args);
    
    // Write newline
    f_write(&logFile, "\n", 1, &BytesWritten);
    
    // Ensure log is written immediately
    f_sync(&logFile);
    f_close(&logFile);
    
    return TRUE;
}
