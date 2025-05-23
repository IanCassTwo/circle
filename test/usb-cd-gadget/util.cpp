// util.cpp
#include "util.h"

LOGMODULE("util");
		   //
char tolower(char c) {
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

bool hasCueExtension(const char* imageName) {
    size_t len = strlen(imageName);
    if (len >= 4) {
        const char* ext = imageName + len - 4;
        return tolower(ext[0]) == '.' &&
               tolower(ext[1]) == 'c' &&
               tolower(ext[2]) == 'u' &&
               tolower(ext[3]) == 'e';
    }
    return false;
}

bool hasBinExtension(const char* imageName) {
    size_t len = strlen(imageName);
    if (len >= 4) {
        const char* ext = imageName + len - 4;
        return tolower(ext[0]) == '.' &&
               tolower(ext[1]) == 'b' &&
               tolower(ext[2]) == 'i' &&
               tolower(ext[3]) == 'n';
    }
    return false;
}

bool hasIsoExtension(const char* imageName) {
    size_t len = strlen(imageName);
    if (len >= 4) {
        const char* ext = imageName + len - 4;
        return tolower(ext[0]) == '.' &&
               tolower(ext[1]) == 'i' &&
               tolower(ext[2]) == 's' &&
               tolower(ext[3]) == 'o';
    }
    return false;
}

void change_extension_to_bin(char *fullPath) {
    size_t len = strlen(fullPath);
    //if (len >= 3 && strcmp(fullPath + len - 3, "bin") == 0) {
    if (len >= 3) {
        fullPath[len - 3] = 'b';
        fullPath[len - 2] = 'i';
        fullPath[len - 1] = 'n';
    }
}

void change_extension_to_cue(char *fullPath) {
    size_t len = strlen(fullPath);
    //if (len >= 3 && strcmp(fullPath + len - 3, "iso") == 0) {
    if (len >= 3) {
        fullPath[len - 3] = 'c';
        fullPath[len - 2] = 'u';
        fullPath[len - 1] = 'e';
    }
}

bool ReadFileToString(const char* fullPath, char** out_str) {
    if (!out_str) return false; // safeguard

    FIL* file = new FIL();
    FRESULT result = f_open(file, fullPath, FA_READ);
    if (result != FR_OK) {
        LOGERR("Cannot open file for reading");
        delete file;
        return false;
    }

    DWORD file_size = f_size(file);
    char* buffer = new char[file_size + 1];
    if (!buffer) {
        f_close(file);
        delete file;
        return false;
    }

    UINT bytes_read = 0;
    result = f_read(file, buffer, file_size, &bytes_read);
    f_close(file);
    delete file;

    if (result != FR_OK || bytes_read != file_size) {
        delete[] buffer;
        return false;
    }

    buffer[file_size] = '\0'; // null-terminate
    *out_str = buffer;
    return true;
}

CCueBinFileDevice* loadCueBinFileDevice(const char* imageName) {

    // Construct full path
    char fullPath[255]; //FIXME limits
    snprintf(fullPath, sizeof(fullPath), "SD:/images/%s", imageName);

    FIL* imageFile = new FIL();
    char* cue_str = nullptr;

    // Is this a bin?
    if (hasBinExtension(fullPath)) {
	LOGNOTE("This is a bin file, changing to cue");
	change_extension_to_cue(fullPath);
    }

    // Is this a cue?
    if (hasCueExtension(fullPath)) {
	// Load the cue
	LOGNOTE("This is a cue file, loading cue");
	if (!ReadFileToString(fullPath, &cue_str)) {
	    return nullptr;
	}
	LOGNOTE("Loaded cue %s", cue_str);

	// Load a bin file with the same name
	change_extension_to_bin(fullPath);
	LOGNOTE("Changed to bin %s", fullPath);
    }
   
    // Load the image
    LOGNOTE("Opening image file %s", fullPath);
    FRESULT Result = f_open(imageFile, fullPath, FA_READ);
    if (Result != FR_OK) {
	LOGERR("Cannot open image file for reading");
	delete imageFile;
	return nullptr;
    }

    LOGNOTE("Opened image file %s", fullPath);
        
    // Create our device
    CCueBinFileDevice* ccueBinFileDevice = new CCueBinFileDevice(imageFile, cue_str);

    // Cleanup
    if (cue_str != nullptr)
    	delete[] cue_str;

    return ccueBinFileDevice;
}

bool getCurrentMountedImage(char* outFilename, size_t maxLen) {
    FIL txtFile;
    UINT bytesRead = 0;
    FRESULT result = f_open(&txtFile, "SD:/image.txt", FA_READ | FA_OPEN_EXISTING);

    if (result == FR_NO_FILE) {
        // File doesn't exist: create and populate with default
        return saveDefaultImageName(outFilename, maxLen);
    } else if (result != FR_OK) {
        // Other error opening the file
        return false;
    }

    result = f_read(&txtFile, outFilename, maxLen - 1, &bytesRead);
    f_close(&txtFile);

    if (result != FR_OK) {
        // Read failed
        return false;
    }

    if (bytesRead >= maxLen)
        bytesRead = maxLen - 1;
    outFilename[bytesRead] = '\0';

    // Remove trailing \r and \n
    int i = strlen(outFilename) - 1;
    while (i >= 0 && (outFilename[i] == '\r' || outFilename[i] == '\n')) {
        outFilename[i--] = '\0';
    }

    // If the file was empty or contained only whitespace
    if (outFilename[0] == '\0') {
        return saveDefaultImageName(outFilename, maxLen);
    }

    return true;
}

bool saveDefaultImageName(char* outFilename, size_t maxLen) {
    snprintf(outFilename, maxLen, "image.iso");
    return saveMountedImageName(outFilename);
}

bool saveMountedImageName(const char* imageName) {
    FIL txtFile;
    UINT bytesWritten;
    FRESULT result = f_open(&txtFile, "SD:/image.txt", FA_WRITE | FA_CREATE_ALWAYS);
    if (result != FR_OK) {
        LOGERR("Cannot open image.txt for writing");
        return false;
    }

    size_t len = strlen(imageName);
    result = f_write(&txtFile, imageName, len, &bytesWritten);
    f_close(&txtFile);

    if (result != FR_OK || bytesWritten != len) {
        LOGERR("Failed to write to image.txt");
        return false;
    }

    return true;
}


// Check if a character is a hexadecimal digit (0-9, A-F, a-f)
bool is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || 
           (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
}

// URL decode a string
void urldecode(char* dst, const char* src) {
    char a, b;
    while (*src) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && 
            (is_hex_digit(a) && is_hex_digit(b))) {
            if (a >= 'a') a -= ('a' - 10);
            else if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            
            if (b >= 'a') b -= ('a' - 10);
            else if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            
            *dst++ = 16 * a + b;
            src += 3;
        } 
        else if (*src == '+') {
            *dst++ = ' ';
            src++;
        }
        else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

