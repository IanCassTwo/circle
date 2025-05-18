// util.h
#ifndef UTIL_H
#define UTIL_H
#include <circle/util.h>
#include <discimage/cuebinfile.h>

char tolower(char c);
bool hasBinExtension(const char* imageName);
void change_extension_to_cue(char* fullPath);
CCueBinFileDevice* loadCueBinFileDevice(char* imageName);

#endif // UTIL_H
