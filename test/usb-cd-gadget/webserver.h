//
// webserver.h
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2015  R. Stange <rsta2@o2online.de>
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
#ifndef _webserver_h
#define _webserver_h

#include <circle/usb/gadget/usbcdgadget.h>
#include <circle/net/httpdaemon.h>
#include <circle/actled.h>
#include <fatfs/ff.h>
#include <linux/kernel.h>
#include <discimage/cuebinfile.h>
#include "kernel.h" // Add this to have access to TShutdownMode

// Forward declaration of TShutdownMode enum to avoid circular dependencies
enum TShutdownMode
{
    ShutdownNone,
    ShutdownHalt,
    ShutdownReboot
};

class CWebServer : public CHTTPDaemon
{
public:
    CWebServer (CNetSubSystem *pNetSubSystem, CUSBCDGadget *pCDGadget, CActLED *pActLED, CSocket *pSocket = 0);
    ~CWebServer (void);

    // from CHTTPDaemon
    CHTTPDaemon *CreateWorker (CNetSubSystem *pNetSubSystem, CSocket *pSocket);
    
    // Get the shutdown mode (if any)
    TShutdownMode GetShutdownMode(void) const { return m_ShutdownMode; }

private:
    // from CHTTPDaemon
    THTTPStatus GetContent (const char  *pPath,
                          const char  *pParams,
                          const char  *pFormData,
                          u8          *pBuffer,
                          unsigned    *pLength,
                          const char **ppContentType);
    
private:
    CActLED *m_pActLED;
    CUSBCDGadget *m_pCDGadget;
    u8 *m_pContentBuffer;    // Added content buffer as class member
    TShutdownMode m_ShutdownMode;
};

#endif
