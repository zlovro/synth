#ifndef MAIN_EX
#define MAIN_EX

#include <support.h>
// #include <cstdio>

enum
{
    ERR_CAT_UART = 1,
    ERR_CAT_SD,
    ERR_CAT_FAT,
    ERR_CAT_SPI,
    ERR_CAT_HAL,
    ERR_CAT_GENERIC,
};

#define printf(fmt, ...) uartPrintf(fmt, ##__VA_ARGS__)
#define printlnf(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

void uartPrintf(const char* pFmt, ...);
void printHex(char* pBuf, u32 pBufLen);
void error(int pCat, int pSub, bool pHalt = false);

#endif //MAIN_EX
