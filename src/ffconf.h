#pragma once

// FatFs configuration for Raspberry Pi Pico
// SD card is used for USB MSC only — sprites live in flash.

#define FFCONF_DEF  80286   // must match FF_DEFINED in ff.h (R0.15)

#define FF_FS_READONLY   0    // read+write (needed for USB MSC writes)
#define FF_FS_MINIMIZE   0    // full feature set (needed for f_getfree)
#define FF_USE_FIND      0
#define FF_USE_MKFS      0
#define FF_USE_FASTSEEK  0
#define FF_USE_EXPAND    0
#define FF_USE_CHMOD     0    // not needed — sprites are in flash
#define FF_USE_LABEL     0
#define FF_USE_FORWARD   0
#define FF_USE_STRFUNC   0

#define FF_CODE_PAGE     437  // US English
#define FF_USE_LFN       0    // no long filenames needed
#define FF_FS_RPATH      0
#define FF_VOLUMES       1
#define FF_STR_VOLUME_ID 0
#define FF_MULTI_PARTITION 0
#define FF_MIN_SS        512
#define FF_MAX_SS        512
#define FF_FS_EXFAT      0
#define FF_USE_TRIM      0
#define FF_FS_NOFSINFO   0
#define FF_FS_TINY       0
#define FF_FS_REENTRANT  0

typedef unsigned int   UINT;
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned long  DWORD;
typedef unsigned long long QWORD;
typedef char TCHAR;