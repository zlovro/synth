#define DEBUG

#include "stm32f4xx.h"

#include <cstdarg>
#include <cstdio>
#include <string>

#include <fat/fat_filelib.h>
#include <ulibSD/sd_io.h>
#include <main_ex.h>
#include <stm32f4xx_it.h>
#include <fasthal/fh_common.h>
#include <fasthal/fh_gpio.h>
#include <fasthal/fh_uart.h>

extern "C" {
    #include "main.h"
}

#define PIN_SHIFT_RESET GPIO_PIN_04
#define PIN_SHIFT_CLK GPIO_PIN_03
#define PIN_SHIFT_DATA GPIO_PIN_02

void gpioWrite8Bit(u8 pVal)
{
    fhGpioPinWriteA(GPIO_PIN_00, pVal >> 0 & 1);
    fhGpioPinWriteA(GPIO_PIN_01, pVal >> 1 & 1);
    fhGpioPinWriteA(GPIO_PIN_02, pVal >> 2 & 1);
    fhGpioPinWriteA(GPIO_PIN_03, pVal >> 3 & 1);
    fhGpioPinWriteB(GPIO_PIN_12, pVal >> 4 & 1);
    fhGpioPinWriteB(GPIO_PIN_13, pVal >> 5 & 1);
    fhGpioPinWriteB(GPIO_PIN_14, pVal >> 6 & 1);
    fhGpioPinWriteB(GPIO_PIN_15, pVal >> 7 & 1);
}

void error(int pCat, int pSub, bool pHalt)
{
    auto data = pCat << 5 | pSub;
    gpioWrite8Bit(data);
    halt(pHalt)
}

template <typename... Args>
std::string format(const std::string& pFmt, Args... pArgs)
{
    auto tmp = new char[0x1000];
    sprintf(tmp, pFmt.c_str(), pArgs...);
    auto dst = std::string(tmp);
    delete[] tmp;

    return dst;
}

void uartPrintf(const char* pFmt, ...)
{
    va_list args;
    va_start(args, pFmt);

    auto tmp = new char[0x1000];

    vsprintf(tmp, pFmt, args);
    va_end(args);

    auto dst = std::string(tmp);
    fhUartWriteBulk1((uint8_t*)dst.data(), dst.length());

    delete[] tmp;

    // if (ret != 0)
    // {
    //     error(ERR_CAT_UART, ret, true);
    // }
}

void printHex(char* pBuf, u32 pBufLen)
{
    for (int i = 0; i < pBufLen; ++i)
    {
        printf("%02x ", pBuf[i]);
    }

    printf("\n");
}

void gpioInit()
{
    fhGpioInitSystemA();
    fhGpioInitSystemB();

    fhGpioPinSetupA(GPIO_PIN_03 | GPIO_PIN_02 | GPIO_PIN_01 | GPIO_PIN_00, GPIO_MODE_OUTPUT, GPIO_OTYPE_PUSH_PULL, GPIO_SPEED_HIGH, GPIO_PULL_NONE);
    fhGpioPinSetupB(GPIO_PIN_15 | GPIO_PIN_14 | GPIO_PIN_13 | GPIO_PIN_12, GPIO_MODE_OUTPUT, GPIO_OTYPE_PUSH_PULL, GPIO_SPEED_HIGH, GPIO_PULL_NONE);
}

void uartInit()
{
    fhUartInit1(115200);
}

SD_DEV gSdDev = {};

int fatReadSectors(uint32 pSector, uint8* pBuffer, uint32 pSectorCount)
{
    if (pSectorCount == 0)
    {
        return 0;
    }

    // printf("Read sectors 0x%x --> 0x%x\n", pSector, pSector + pSectorCount);

    u32 t0 = gSysTick;

    int ret = 1;
    for (u32 i = 0, sector = pSector; i < pSectorCount; ++i, sector++)
    {
        auto sdRet = SD_Read(&gSdDev, pBuffer + i * 0x200, sector, 0, 0x200);
        if (sdRet != SD_OK)
        {
            printf("\tSD_Read returned %d, sector: 0x%x\n", sdRet);

            printf("\tlast sector: %x\n", gSdDev.last_sector);
            printf("\tcard type: %x\n", gSdDev.cardtype);

            error(ERR_CAT_SD, sdRet, true);
            ret = 0;
            break;
        }
    }

    auto ms = gSysTick - t0;
    printf("\tTook %d ms to read %d sectors (%d KiB/s)\n", ms, pSectorCount, (int)(pSectorCount * 0.5 / (ms / 1000.0)));

    return ret;
}

int fatWriteSectors(uint32 pSector, uint8* pBuffer, uint32 pSectorCount)
{
    printf("Writing to sd card is forbidden!\n");
    error(ERR_CAT_SD, 0xF, true);

    return 0;
}

void fatInit()
{
    int ret = sdInit(&gSdDev);
    if (ret != SD_OK)
    {
        printf("sdInit returned %d\n", ret);
        error(ERR_CAT_SD, ret, true);
    }

    // printf("SD Card info:\n");
    // printf("\tSectors: 0x%x\n", gSdDev.last_sector);
    // printf("\tType:    0x%x\n", gSdDev.cardtype);
    // printf("\tMounted: 0x%x\n", gSdDev.mount);

    // for (int i = 0; i < 0x800; ++i)
    // {
    //     u8 buf[0x200];
    //     fatReadSectors(i, buf, 1);
    // }

    fl_init();
    ret = -fl_attach_media(fatReadSectors, nullptr);
    if (ret != FAT_INIT_OK)
    {
        printf("fl_attach_media returned %d\n", ret);
        error(ERR_CAT_FAT, ret, true);
    }
}

int main()
{
    fhInit();
    gpioWrite8Bit(4);
    gpioInit();
    gpioWrite8Bit(15);
    // uartInit();
    gpioWrite8Bit(31);

    // auto i = 0;
    // while (1)
    // {
    //     gpioWrite8Bit(SystemCoreClock / 1'000'000);
    //     // printf("bascanska ploca\n");
    //     fhDelayMs(1000 / 6);
    // }

    // auto gpio = GPIO_InitTypeDef{GPIO_PIN_9 | GPIO_PIN_8, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_LOW, 0};
    // HAL_GPIO_Init(GPIOA, &gpio);
    //
    // HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9 | GPIO_PIN_8, GPIO_PIN_SET);

    // fatInit();

    // auto instruments = 2;
    // for (int i = 0; i < instruments; ++i)
    // {
    //     auto path = format("/instruments/%03d/default.wav", i);
    //     auto fp   = fl_fopen(path.c_str(), "r");
    //     if (!fp)
    //     {
    //         printf("failed to open file '%s'\n", path.c_str());
    //         continue;
    //     }
    //
    //     printf("file '%s':\n", path.c_str());
    //     printf("\tlength: %d B\n", flFileSize(fp));
    //     printf("\n");
    //
    //     fl_fclose(fp);
    // }

    // if (!fp)
    // {
    //     printf("Could not open wave file\n");
    //     error(ERR_CAT_FAT, 15, true);
    // }

    // auto i  = 0;
    // auto t0 = gSysTick;
    //
    // u32 size;
    //
    // while (1)
    // {
    //     if (gSysTick - t0 >= 5000)
    //     {
    //         break;
    //     }
    //
    //     auto k0 = gSysTick;
    //
    //     auto fp = (FL_FILE*)fl_fopen("/instruments/000/default.wav", "r");
    //
    //     size = fp->filelength;
    //
    //     auto buffer = new u8[size];
    //     fl_fread(buffer, 1, size, fp);
    //     printf("\tK0: %d ms\n", gSysTick - k0);
    //
    //     delete[] buffer;
    //
    //     auto k1 = gSysTick;
    //     fl_fclose(fp);
    //     printf("\tK1: %d ms\n", gSysTick - k1);
    //
    //     i++;
    // }
    //
    //
    // uint32_t deltaMs = gSysTick - t0;
    // auto     tDelta  = deltaMs / 1000.0F;
    // auto     speed   = i * size / tDelta;
    //
    // printf("SD Card speed = %d KiB/s (%d KiB file was loaded %d times in %d ms)\n", (int)(speed / 1024), (int)size / 1024, i, (int)deltaMs);


    // fl_listdirectory("/");

    // __HAL_RCC_GPIOA_CLK_ENABLE();
    // __HAL_RCC_GPIOB_CLK_ENABLE();
    //
    // /*Configure GPIO pin Output Level */
    // HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
    // HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    //
    // GPIO_InitTypeDef gpio = {};
    //
    // gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    // gpio.Pull  = GPIO_NOPULL;
    // gpio.Pin   = GPIO_PIN_10;
    // gpio.Speed = GPIO_SPEED_FREQ_LOW;
    //
    // HAL_GPIO_Init(GPIOA, &gpio);
    //
    // gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    // gpio.Pull  = GPIO_NOPULL;
    // gpio.Pin   = GPIO_PIN_8;
    // gpio.Speed = GPIO_SPEED_FREQ_LOW;
    //
    // HAL_GPIO_Init(GPIOA, &gpio);
    //
    // auto time = gSysTick;
    // while (1)
    // {
    //     if (gSysTick - time >= 3000)
    //     {
    //         break;
    //     }
    //
    //     HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_10);
    //     HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_8);
    //     HAL_Delay(50);
    // }
}

void Error_Handler(void)
{
    printf("Error handler reached!\n");
    halt(1)
}
