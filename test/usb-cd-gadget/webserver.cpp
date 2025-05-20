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
#include "util.h"
#include <assert.h>

#define MAX_CONTENT_SIZE        16384
#define MAX_FILES       255
#define MAX_FILENAME    64
#define VERSION         "2.0.0"

// HTML template with CSS styling embedded
static const char HTML_LAYOUT[] = 
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "    <title>USBODE - USB Optical Drive Emulator</title>\n"
    "    <style>\n"
    "        body {background-color: #EAEAEA; color: #333333; font-family: \"Times New Roman\", serif; margin: 0; padding: 0;}\n"
    "        h1, h2, h3 {color: #1E4D8C;}\n"
    "        a {color: #0066CC;}\n"
    "        a:visited {color: #0066CC;}\n"
    "        .container {width: 100%%; margin: 0; padding: 0;}\n"
    "        .header {background-color: #3A7CA5; padding: 10px; text-align: center; color: #FFFFFF;}\n"
    "        .header h1, .header h2 {color: #FFFFFF; margin: 5px 0;}\n"
    "        .content {padding: 10px; background-color: #FFFFFF; min-height: 300px;}\n"
    "        .footer {background-color: #3A7CA5; padding: 10px; text-align: center; color: #FFFFFF;}\n"
    "        .button {background-color: #4CAF50; padding: 7px 15px; text-decoration: none; color: #FFFFFF; margin: 5px; display: inline-block;}\n"
    "        .info-box {background-color: #F5F5F5; padding: 10px; margin: 10px 0;}\n"
    "        .warning {background-color: #FFDDDD; padding: 10px; margin: 10px 0; color: #990000;}\n"
    "        .file-link {padding: 8px; margin: 5px 0; display: block; font-size: 16px;}\n"
    "        .file-link-even {background-color: #E3F2FD;}\n"
    "        .file-link-odd {background-color: #BBDEFB;}\n"
    "        .header-bar {background-color: #2C5F7C; color: #FFFFFF; padding: 5px;}\n"
    "    </style>\n"
    "</head>\n"
    "<body>\n"
    "    <div class=\"container\">\n"
    "        <div class=\"header\">\n"
    "            <h1>USBODE</h1>\n"
    "            <h2>USB Optical Drive Emulator</h2>\n"
    "        </div>\n"
    "        <div class=\"content\">\n"
    "            %s\n"
    "        </div>\n"
    "        <div class=\"footer\">\n"
    "            <p>Version %s</p>\n"
    "        </div>\n"
    "    </div>\n"
    "</body>\n"
    "</html>";

static const u8 s_Index[] =
#include "index.h"
;

LOGMODULE ("kernel");

static const char FromWebServer[] = "webserver";

TShutdownMode CWebServer::s_GlobalShutdownMode = ShutdownNone;

CWebServer::CWebServer (CNetSubSystem *pNetSubSystem, CUSBCDGadget *pCDGadget, CActLED *pActLED, CSocket *pSocket)
:       CHTTPDaemon (pNetSubSystem, pSocket, MAX_CONTENT_SIZE),
        m_pActLED (pActLED),
        m_pCDGadget (pCDGadget),
        m_pContentBuffer(new u8[MAX_CONTENT_SIZE]),
        m_ShutdownMode(ShutdownNone)
{
}

CWebServer::~CWebServer (void)
{
        m_pActLED = 0;
        delete[] m_pContentBuffer;
}

CHTTPDaemon *CWebServer::CreateWorker (CNetSubSystem *pNetSubSystem, CSocket *pSocket)
{
        return new CWebServer (pNetSubSystem, m_pCDGadget, m_pActLED, pSocket);
}

THTTPStatus list_files_as_table(char *output_buffer, size_t max_len) {
    DIR dir;
    FILINFO fno;
    FRESULT fr;
    char content[MAX_CONTENT_SIZE];
    size_t offset = 0;
    char currentImage[MAX_FILENAME];
    if (getCurrentMountedImage(currentImage, sizeof(currentImage))) {
            LOGNOTE("Found image filename %s", currentImage);
    } else {
	    // Defaulting here lets the user get out of a hole
            strcpy(currentImage, "image.iso");
            LOGERR("Could not load image name, using default: %s", currentImage);
    }

    fr = f_opendir(&dir, "/images");
    if (fr != FR_OK) {
        snprintf(output_buffer, max_len, "Error opening directory: %d", fr);
        return HTTPInternalServerError;
    }

    offset += snprintf(content + offset, sizeof(content) - offset,
        "<h3>File Selection</h3>\n"
        "<div class=\"info-box\">\n"
        "    <p>Current File Loaded: <strong>%s</strong></p>\n"
        "    <p>To load a different ISO, select it. No disconnection between the OS and the USBODE will occur.</p>\n"
        "</div>\n"
        "<h4>Available Files:</h4>\n", 
        currentImage);

    int index = 0;
    while (1) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;

        // Skip "." and ".."
        if (fno.fname[0] == '.' && (fno.fname[1] == 0 || (fno.fname[1] == '.' && fno.fname[2] == 0))) {
            continue;
        }

        // Add file with alternating row colors
        const char* rowClass = (index % 2 == 0) ? "file-link-even" : "file-link-odd";
        offset += snprintf(content + offset, sizeof(content) - offset, 
            "<div class=\"file-link %s\"><a href=\"/mount?file=%s\">%s</a></div>\n", 
            rowClass, fno.fname, fno.fname);
        index++;

        if (offset >= sizeof(content) - MAX_FILENAME - 100) {  // prevent overflow
            offset += snprintf(content + offset, sizeof(content) - offset, "<p>Too many files to display completely</p>");
            break;
        }
    }

    offset += snprintf(content + offset, sizeof(content) - offset,
        "<div>\n"
        "    <a class=\"button\" href=\"/\">Return to Homepage</a>\n"
        "</div>");

    f_closedir(&dir);
    
    // Format the complete HTML page using the layout template
    snprintf(output_buffer, max_len, HTML_LAYOUT, content, VERSION);
    return HTTPOK;
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

THTTPStatus generate_index_page(char *output_buffer, size_t max_len) {
    char currentImage[MAX_FILENAME];
    if (getCurrentMountedImage(currentImage, sizeof(currentImage))) {
            LOGNOTE("Found image filename %s", currentImage);
    } else {
            // Defaulting here lets the user get out of a hole
            strcpy(currentImage, "image.iso");
            LOGERR("Could not load image name, using default: %s", currentImage);
    }
    
    char content[MAX_CONTENT_SIZE];
    snprintf(content, sizeof(content),
        "<h3>Welcome to USBODE</h3>\n"
        "<div class=\"info-box\">\n"
        "    <p>Currently Serving: <strong>%s</strong></p>\n"
        "</div>\n"
        "\n"
        "<div>\n"
        "    <a class=\"button\" href=\"/list\">Load Another Image</a>\n"
        "    <a class=\"button\" href=\"/system?action=shutdown\" onclick=\"return confirm('Are you sure you want to shut down the device?');\">Shutdown USBODE</a>\n"
        "</div>",
        currentImage);
    
    // Format the complete HTML page using the layout template
    snprintf(output_buffer, max_len, HTML_LAYOUT, content, VERSION);
    return HTTPOK;
}

THTTPStatus generate_mount_success_page(char *output_buffer, size_t max_len, const char *filename) {
    char content[MAX_CONTENT_SIZE];
    snprintf(content, sizeof(content),
        "<h3>Mounting File</h3>\n"
        "<div class=\"info-box\">\n"
        "    <p>Successfully mounted: <strong>%s</strong></p>\n"
        "</div>\n"
        "\n"
        "<div>\n"
        "    <a class=\"button\" href=\"/\">Return to Homepage</a>\n"
        "    <a class=\"button\" href=\"/list\">Select Another File</a>\n"
        "</div>",
        filename);
    
    // Format the complete HTML page using the layout template
    snprintf(output_buffer, max_len, HTML_LAYOUT, content, VERSION);
    return HTTPOK;
}

THTTPStatus handle_system_operation(char *output_buffer, size_t max_len, const char *action, TShutdownMode *pShutdownMode) {
    char content[MAX_CONTENT_SIZE];
    
    if (strcmp(action, "shutdown") == 0) {
        snprintf(content, sizeof(content),
            "<h3>System Shutdown</h3>\n"
            "<div class=\"info-box\">\n"
            "    <p>The system is shutting down...</p>\n"
            "</div>");
        
        // Set the global shutdown mode instead of the instance variable
        CWebServer::SetGlobalShutdownMode(ShutdownHalt);
        *pShutdownMode = ShutdownHalt;  // Also set the passed pointer for compatibility
    } 
    else if (strcmp(action, "reboot") == 0) {
        snprintf(content, sizeof(content),
            "<h3>System Reboot</h3>\n"
            "<div class=\"info-box\">\n"
            "    <p>The system is rebooting...</p>\n"
            "</div>");
        
        // Set the global shutdown mode instead of the instance variable
        CWebServer::SetGlobalShutdownMode(ShutdownReboot);
        *pShutdownMode = ShutdownReboot;  // Also set the passed pointer for compatibility
    }
    else {
        return HTTPBadRequest;
    }
    
    // Format the complete HTML page using the layout template
    snprintf(output_buffer, max_len, HTML_LAYOUT, content, VERSION);
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

    THTTPStatus resultCode = HTTPOK;
    unsigned nLength = 0;

    LOGNOTE("Path: %s, Params: %s", pPath, pParams ? pParams : "");

    if ((strcmp (pPath, "/") == 0 || strcmp (pPath, "/index.html") == 0))
    {
        // Generate the index page with the HTML template
        resultCode = generate_index_page((char*)m_pContentBuffer, MAX_CONTENT_SIZE);
        nLength = strlen((char*)m_pContentBuffer);
        *ppContentType = "text/html; charset=utf-8";
    } 
    else if (strcmp (pPath, "/list") == 0) 
    { 
        // List images with HTML table formatting
        resultCode = list_files_as_table((char*)m_pContentBuffer, MAX_CONTENT_SIZE);
        nLength = strlen((char*)m_pContentBuffer);
        *ppContentType = "text/html; charset=utf-8";
    }
    else if (strcmp (pPath, "/api/list") == 0) 
    { 
        // List our images in JSON format (keep API endpoint for compatibility)
        resultCode = list_files_as_json((char*)m_pContentBuffer, MAX_CONTENT_SIZE);
        nLength = strlen((char*)m_pContentBuffer);
        *ppContentType = "application/json; charset=utf-8";
    } 
    else if (strcmp (pPath, "/system") == 0 && pParams && strncmp (pParams, "action=", 7) == 0) 
    { 
        // Handle system operation (shutdown/reboot)
        char actionValue[32]; // Buffer to hold either "shutdown" or "reboot"
        const char* equalSign = strchr(pParams, '=');
        if (equalSign && *(equalSign + 1) != '\0') {
            // Extract only the value until next & or end of string
            size_t i = 0;
            const char* p = equalSign + 1;
            while (*p && *p != '&' && i < sizeof(actionValue) - 1) {
                actionValue[i++] = *p++;
            }
            actionValue[i] = '\0';
            
            LOGNOTE("System action requested: %s", actionValue);
            
            resultCode = handle_system_operation((char*)m_pContentBuffer, MAX_CONTENT_SIZE, 
                                              actionValue, &m_ShutdownMode);
            nLength = strlen((char*)m_pContentBuffer);
            *ppContentType = "text/html; charset=utf-8";
        } else {
            LOGERR("system action value is missing");
            strcpy((char*)m_pContentBuffer, "system action value is missing");
            nLength = strlen((char*)m_pContentBuffer);
            return HTTPBadRequest;
        }
    }
    else if (strcmp (pPath, "/mount") == 0 && pParams && strncmp (pParams, "file=", 5) == 0) 
    { 
        // Extract value (after '=')
        char pParamValue[256];
        const char* equalSign = strchr(pParams, '=');
        if (equalSign && *(equalSign + 1) != '\0') {
            strncpy(pParamValue, equalSign + 1, sizeof(pParamValue) - 1);
            pParamValue[sizeof(pParamValue) - 1] = '\0';

	    if (!saveMountedImageName(pParamValue)) {
		    strcpy((char*)m_pContentBuffer, "");
		    nLength = 0;
		    return HTTPInternalServerError;
	    }

            CCueBinFileDevice* cueBinFileDevice = loadCueBinFileDevice(pParamValue);
            if (!cueBinFileDevice) {
                LOGERR("Failed to get cueBinFileDevice");
                return HTTPInternalServerError;
            }

            m_pCDGadget->SetDevice(cueBinFileDevice);
            
            // Generate a success page
            resultCode = generate_mount_success_page((char*)m_pContentBuffer, MAX_CONTENT_SIZE, pParamValue);
            nLength = strlen((char*)m_pContentBuffer);
            *ppContentType = "text/html; charset=utf-8";
        } else {
            LOGERR("mount file value is missing");
            strcpy((char*)m_pContentBuffer, "mount file value is missing");
            nLength = 28;
            return HTTPBadRequest;
        }
    }
    // Keep the API JSON endpoint for compatibility
    else if (strcmp (pPath, "/controller") == 0 && (strncmp (pParams, "mount=", 6) == 0)) 
    { 
        // Extract value (after '=')
        char pParamValue[256];
        const char* equalSign = strchr(pParams, '=');
        if (equalSign && *(equalSign + 1) != '\0') {
            strncpy(pParamValue, equalSign + 1, sizeof(pParamValue) - 1);
            pParamValue[sizeof(pParamValue) - 1] = '\0';

            // Write pParamValue to SD:/image.txt
            FIL txtFile;
            UINT bytesWritten;
            FRESULT Result = f_open(&txtFile, "SD:/image.txt", FA_WRITE | FA_CREATE_ALWAYS);
            if (Result != FR_OK) {
                LOGERR("Cannot open image.txt for writing");
                strcpy((char*)m_pContentBuffer, "");
                nLength = 0;
                return HTTPInternalServerError;
            }

            Result = f_write(&txtFile, pParamValue, strlen(pParamValue), &bytesWritten);
            f_close(&txtFile);

            if (Result != FR_OK || bytesWritten != strlen(pParamValue)) {
                LOGERR("Failed to write to image.txt");
                strcpy((char*)m_pContentBuffer, "");
                nLength = 0;
                return HTTPInternalServerError;
            }
    
            CCueBinFileDevice* cueBinFileDevice = loadCueBinFileDevice(pParamValue);
            if (!cueBinFileDevice) {
                LOGERR("Failed to get cueBinFileDevice");
                return HTTPInternalServerError;
            }

            m_pCDGadget->SetDevice(cueBinFileDevice);
            strcpy((char*)m_pContentBuffer, "{\"status\": \"OK\"}");
            nLength = 16;
            *ppContentType = "application/json; charset=iso-8859-1";
        } else {
            LOGERR("mount value is missing");
            strcpy((char*)m_pContentBuffer, "mount value is missing");
            nLength = 22;
            return HTTPBadRequest;
        }
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
    assert (nLength > 0);
    memcpy (pBuffer, m_pContentBuffer, nLength);

    *pLength = nLength;

    return resultCode;
}

TShutdownMode CWebServer::GetShutdownMode(void) const 
{
    return s_GlobalShutdownMode;
}

void CWebServer::SetGlobalShutdownMode(TShutdownMode mode) 
{
    s_GlobalShutdownMode = mode;
}
