#include "cg_local.h"

#ifdef WIN32
#define Rectangle LCC_Rectangle
#include <Windows.h>
#undef Rectangle
// Maybe not the best hardware ID but should do the trick, I hope.
char *getHardwareInfo() 
{
    int systemInfoSum = 0;
    char hwId[MAX_TOKEN_CHARS] = "\0";
    char rootdrive[MAX_PATH] = "\0";
    char vsnc[MAX_PATH] = "\0";
    DWORD vsn;
   
    SYSTEM_INFO systemInfo;
    GetSystemInfo( &systemInfo );

    // Random data from processor
    systemInfoSum = systemInfo.dwProcessorType + systemInfo.wProcessorLevel + systemInfo.wProcessorArchitecture;

    itoa(systemInfoSum, hwId, 10);
	// HDD data
    GetEnvironmentVariable("HOMEDRIVE", rootdrive, sizeof(rootdrive));
    Q_strcat(rootdrive, sizeof(rootdrive), "\\");

    if(GetVolumeInformation(rootdrive, 0, 0, &vsn, 0, 0, 0, 0) == 0)
    {
        // Failed to get volume info
        Q_strcat(rootdrive, sizeof(rootdrive), "failed");
    }

    itoa(vsn, vsnc, 10);

    Q_strcat(hwId, sizeof(hwId), vsnc);
    
    return G_SHA1(hwId);
}

#endif