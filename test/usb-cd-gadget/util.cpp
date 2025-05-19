// util.cpp
#include "util.h"

LOGMODULE("util");
		   //
char tolower(char c) {
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
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

void change_extension_to_cue(char *fullPath) {
    size_t len = strlen(fullPath);
    if (len >= 3 && strcmp(fullPath + len - 3, "bin") == 0) {
        fullPath[len - 3] = 'c';
        fullPath[len - 2] = 'u';
        fullPath[len - 1] = 'e';
    }
}

CCueBinFileDevice* loadCueBinFileDevice(char* imageName) {

    // Construct full path
    char fullPath[160];
    snprintf(fullPath, sizeof(fullPath), "SD:/images/%s", imageName);

    // Load our image
    FIL* pFile = new FIL();
    FRESULT Result = f_open(pFile, fullPath, FA_READ);
    if (Result != FR_OK) {
        LOGERR("Cannot open iso file for reading");
        delete pFile;
        return nullptr;
    }

    // Optional CUE content
    char* cue_str = nullptr;

    if (hasBinExtension(imageName)) {
        // Change .bin to .cue
        change_extension_to_cue(fullPath);
        FIL* cFile = new FIL();
        Result = f_open(cFile, fullPath, FA_READ);
        if (Result != FR_OK) {
            LOGERR("Cannot open cue file for reading");
            f_close(cFile);
            f_close(pFile);
            delete cFile;
            delete pFile;
            return nullptr;
        }

        DWORD file_size = f_size(cFile);
        cue_str = new char[file_size + 1];
        if (!cue_str) {
            f_close(cFile);
            f_close(pFile);
            delete cFile;
            delete pFile;
            return nullptr;
        }

        UINT bytes_read = 0;
        FRESULT res = f_read(cFile, cue_str, file_size, &bytes_read);
        f_close(cFile);
	delete cFile;

        if (res != FR_OK || bytes_read != file_size) {
            delete[] cue_str;
            f_close(pFile);
            delete pFile;
            return nullptr;
        }

        cue_str[file_size] = '\0'; // null-terminate
    }

    CCueBinFileDevice* ccueBinFileDevice = new CCueBinFileDevice(pFile, cue_str);

    // CCueBinFileDevice now owns this, so clean up
    if (cue_str != nullptr)
    	delete[] cue_str;

    return ccueBinFileDevice;
}
