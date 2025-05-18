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

LOGMODULE ("kernel");

CKernel::CKernel (void)
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
	m_WLAN (FIRMWARE_PATH),
        m_Net (0, 0, 0, 0, HOSTNAME, NetDeviceTypeWLAN),
        m_WPASupplicant (CONFIG_FILE),
	m_CDGadget (&m_Interrupt)
{
	//m_ActLED.Blink (5);	// show we are alive
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
		LOGNOTE("Initialized filesystem");
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
	LOGNOTE ("Compile time: " __DATE__ " " __TIME__);

	// Load image name from image.txt
	FIL txtFile;
	char imageName[128];  // Make sure this is large enough for the filename
	FRESULT Result = f_open(&txtFile, "SD:/image.txt", FA_READ);
	if (Result != FR_OK)
	{
	    LOGERR("Cannot open image.txt for reading");
	    return ShutdownHalt;
	}

	// Read the filename
	UINT bytesRead;
	Result = f_read(&txtFile, imageName, sizeof(imageName) - 1, &bytesRead);
	f_close(&txtFile);

	if (Result != FR_OK || bytesRead == 0)
	{
	    LOGERR("Failed to read filename from image.txt");
	    return ShutdownHalt;
	}
	imageName[bytesRead] = '\0';


	// I wish we had strpbrk
	for (int i = 0; imageName[i] != '\0'; ++i) {
	    if (imageName[i] == '\r' || imageName[i] == '\n') {
		imageName[i] = '\0';
		break;
	    }
	}

	CCueBinFileDevice* cueBinFileDevice = loadCueBinFileDevice(imageName);
	if (!cueBinFileDevice) {
		LOGERR("Failed to get cueBinFileDevice");
		return ShutdownHalt;
	}

	m_CDGadget.SetDevice (cueBinFileDevice);

	/*
	// Construct full path
	char fullPath[160];
	snprintf(fullPath, sizeof(fullPath), "SD:/images/%s", imageName);

	// Load our image
        FIL pFile;
        Result = f_open (&pFile, fullPath, FA_READ);
        if (Result != FR_OK)
        {
                LOGERR("Cannot open iso file for reading");
                return ShutdownHalt;
        }

	// Is this a bin file?
	char *cue_str = nullptr;
	if (hasBinExtension(imageName)) {
		// Load our cue
		change_extension_to_cue(fullPath);
		FIL cFile;
		Result = f_open (&cFile, fullPath, FA_READ);
		if (Result != FR_OK)
		{
			LOGERR("Cannot open iso file for reading");
			return ShutdownHalt;
		}

		// Get file size
		DWORD file_size = f_size(&cFile);

		// Allocate buffer (+1 for null-terminator)
		char *buffer = new char[file_size + 1];
		if (!buffer) {
			f_close(&cFile);
			return ShutdownHalt;
		}

		UINT bytes_read = 0;
		FRESULT res = f_read(&cFile, buffer, file_size, &bytes_read);
		f_close(&cFile);

		if (res != FR_OK || bytes_read != file_size) {
			delete[] buffer;
			return ShutdownHalt;
		}
		buffer[file_size] = '\0'; // null-terminate
		cue_str = buffer;
	}

	// Start the CDROM
	LOGNOTE("Loaded Image");
	m_CDGadget.SetDevice (new CCueBinFileDevice(&pFile, cue_str));
	*/

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

                m_Scheduler.Yield();

		// Stop spinning
                //m_Scheduler.MsSleep(10);
	}

	LOGNOTE("ShutdownHalt");
	return ShutdownHalt;
}
