//
// webserver.cpp
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
#include "webserver.h"
#include <circle/logger.h>
#include <circle/string.h>
#include <circle/util.h>
#include <assert.h>

#define MAX_CONTENT_SIZE        8192
#define MAX_FILES       255
#define MAX_FILENAME    64

static const u8 s_Index[] =
#include "index.h"
;

LOGMODULE ("kernel");

static const char FromWebServer[] = "webserver";

CWebServer::CWebServer (CNetSubSystem *pNetSubSystem, CActLED *pActLED, CSocket *pSocket)
:       CHTTPDaemon (pNetSubSystem, pSocket, MAX_CONTENT_SIZE),
        m_pActLED (pActLED)
{
}

CWebServer::~CWebServer (void)
{
        m_pActLED = 0;
}

CHTTPDaemon *CWebServer::CreateWorker (CNetSubSystem *pNetSubSystem, CSocket *pSocket)
{
        return new CWebServer (pNetSubSystem, m_pActLED, pSocket);
}

THTTPStatus list_files_as_json(char *json_output, size_t max_len) {
    DIR dir;
    FILINFO fno;
    FRESULT fr;
    size_t offset = 0;

    fr = f_opendir(&dir, "/images");
    if (fr != FR_OK) {
        snprintf(json_output, max_len, "{\"error\": %d}", fr);
        return HTTPInternalServerError;
    }

    offset += snprintf(json_output + offset, max_len - offset, "[");

    int first = 1;
    while (1) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;

        // Skip "." and ".."
        if (fno.fname[0] == '.' && (fno.fname[1] == 0 || (fno.fname[1] == '.' && fno.fname[2] == 0))) {
            continue;
        }

        if (!first) {
            offset += snprintf(json_output + offset, max_len - offset, ",");
        }
        first = 0;

        // Add filename to JSON array
        offset += snprintf(json_output + offset, max_len - offset, "\"%s\"", fno.fname);

        if (offset >= max_len - MAX_FILENAME - 4) {  // prevent overflow
            break;
        }
    }

    snprintf(json_output + offset, max_len - offset, "]");
    f_closedir(&dir);
    return HTTPOK;
}

THTTPStatus CWebServer::GetContent (const char  *pPath,
                                    const char  *pParams,
                                    const char  *pFormData,
                                    u8          *pBuffer,
                                    unsigned    *pLength,
                                    const char **ppContentType)
{
        assert (pPath != 0);
        assert (ppContentType != 0);
        assert (m_pActLED != 0);

        CString String;
        const u8 *pContent = 0;
        unsigned nLength = 0;
	THTTPStatus resultCode = HTTPOK;

	LOGNOTE("Params is %s", pParams);

	if ((strcmp (pPath, "/") == 0 || strcmp (pPath, "/index.html") == 0))
        {
		// GET the index page
                pContent = s_Index;
                nLength = sizeof s_Index-1;
                *ppContentType = "text/html; charset=iso-8859-1";
        } 
	else if (strcmp (pPath, "/controller") == 0 && (strcmp (pParams, "list") == 0)) 
	{ 
		// List our images
		char json_output[MAX_CONTENT_SIZE];
		resultCode = list_files_as_json(json_output, MAX_CONTENT_SIZE);
                pContent = (const u8*)json_output;
                nLength = strlen((char*)json_output);
                *ppContentType = "application/json; charset=iso-8859-1";
	} 
	else if (strcmp (pPath, "/controller") == 0 && (strcmp (pParams, "mount") == 0)) 
	{ 
	}
	else
        {
                return HTTPNotFound;
        }

        assert (pLength != 0);
        if (*pLength < nLength)
        {
                LOGERR("Increase MAX_CONTENT_SIZE to at least %u", nLength);
                return HTTPInternalServerError;
        }

        assert (pBuffer != 0);
        assert (pContent != 0);
        assert (nLength > 0);
        memcpy (pBuffer, pContent, nLength);

        *pLength = nLength;

        return resultCode;
}
