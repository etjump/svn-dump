void Q_strcat( char *dest, int size, const char *src );
char *G_SHA1(char *string);

#define MAX_TOKEN_CHARS 1024
#define HASH_LEN 32

#ifdef WIN32
#include <Windows.h>
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