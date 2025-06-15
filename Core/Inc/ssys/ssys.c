//
// Created by Made on 19/05/2025.
//

#include "ssys.h"

#include <main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glcd/glcd.h>
#include <stm32h7xx_hal.h>
#include <sfs/sfs.h>

sysSettings gSysSettings         = {2};
bool        gSysIsLoaded         = false;
u32         gSysDeltaTimeMs      = 0;
u8          gSysCurrentMenuId    = SYS_MENU_LOADING;
u8          gSysLastMenuId       = SYS_MENU_INVALID;
f32         gSysCurrentMenuTimer = 0;

u16      gSysAudioFrontBuf[SYS_AUDIO_BUFFER_SAMPLE_COUNT];
u16      gSysAudioBackBuf[SYS_AUDIO_BUFFER_SAMPLE_COUNT];
u16      gSysDacBuf[SYS_DAC_SAMPLE_COUNT];
sysTrack gSysTracks[SYS_POLYPHONY_COUNT];
u16      gSysTrackData[SYS_TRACK_COUNT][SYS_TRACK_BUFFER_SAMPLE_COUNT];
u8       gSysTmpBlock[BLOCK_SIZE];

u32 gSysDmaProgress     = 0;
u32 gSysDataLoadCounter = 0;

sysInputBitmap  gSysBtnKeyStates[SYS_KEYS_WORD_COUNT]     = {{0}};
sysInputBitmap  gSysBtnMtx1State                          = {0};
sysKeyTimestamp gSysKeyTimestamps[SYS_KEY_SEMITONE_RANGE] = {{0}};

ADC_HandleTypeDef* gSysAdc;

synthErrno sysInit(DAC_HandleTypeDef* pDac, ADC_HandleTypeDef* pAdc)
{
    glcdSetOrigin(SYS_GUI_ORIGIN, SYS_GUI_ORIGIN);

    memset(gSysTracks, 0, sizeof(sysTrack) * SYS_POLYPHONY_COUNT);
    memset(gSysDacBuf, 0, SYS_DAC_BUFFER_SIZE);
    memset(gSysTmpBlock, 0, BLOCK_SIZE);

    for (int i = 0; i < SYS_POLYPHONY_COUNT; i++)
    {
        (gSysTracks + i)->instrument.instrumentId = SFS_INVALID_INSTRUMENT_ID;
    }

    sysGetMainTrack()->instrument.instrumentId = 0;

    HAL_DAC_Start_DMA(pDac, DAC_CHANNEL_1, (u32*)gSysDacBuf, SYS_DAC_BUFFER_SIZE, DAC_ALIGN_12B_R);

    gSysAdc = pAdc;
    HAL_ADC_Start(pAdc);

    return SERR_OK;
}

void sysReadInputs()
{
    // gSysBtnMtx1State.lastState = gSysBtnMtx1State.currentState;
    // // button matrix 1
    // for (int row = 0; row < BTNMTX1_Size; ++row)
    // {
    //     gpioSet(gBtnMtx1RowPins + row, true);
    //
    //     for (int col = 0; col < BTNMTX1_Size; ++col)
    //     {
    //         auto pos                      = row * BTNMTX1_Size + col;
    //         bool set                      = gpioGet(gBtnMtx1ColPins + col);
    //         gSysBtnMtx1State.currentState = (gSysBtnMtx1State.currentState & ~(1 << pos)) | (set << pos);
    //     }
    //
    //     gpioSet(gBtnMtx1RowPins + row, false);
    // }
}

sysButtonState sysGetButtonState(sysInputBitmap* pMap, u32 pBtn)
{
    u32 current = pMap->currentState;
    u32 last    = pMap->lastState;

    if ((current & pBtn) && (last & pBtn))
    {
        return SYS_BTNSTATE_HELD;
    }
    if ((current & pBtn) && !(last & pBtn))
    {
        return SYS_BTNSTATE_DOWN;
    }
    if (!(current & pBtn) && (last & current))
    {
        return SYS_BTNSTATE_UP;
    }

    return SYS_BTNSTATE_NULL;
}

int sysPitchBendCalculateArr(float pSysFreq, int pPrescaler, float pOutFreq)
{
    return (int)(pSysFreq / (pOutFreq * pPrescaler + pOutFreq) - 1);
}

float bend = 0;

void sysHandlePitchBend()
{
    const float sysFreq = 120e6;

    bend          = gSysSettings.pitchBendRangeSemitones * ((HAL_ADC_GetValue(gSysAdc) / 32767.5F) - 1);
    float newFreq = toneAddSemitone(sysFreq, bend);

    TIM_DAC->ARR = sysPitchBendCalculateArr(sysFreq, TIM_DAC->ARR, newFreq);
}

u32 timeDiff;
u32 cnt = 0;

void sysHandleInputs()
{
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int i = 0; i < SYS_KEY_SEMITONE_RANGE; ++i)
        {
            bool pressed = false;
            if (pass == 0)
            {
                if (i > TONE_OFFSET_B4 - SYS_FIRST_KEY)
                {
                    int idx = 4 + (i + SYS_FIRST_KEY) - TONE_OFFSET_C5;

                    for (int col = 5; col < 5 + 4; col++)
                    {
                        muxWrite(&gMuxOgKeyCol, col, false);
                    }

                    muxWrite(&gMuxOgKeyCol, 5 + idx / 8, true);
                    pressed = muxRead(&gMuxOgKeyRow, 8 + idx % 8);
                }
                else
                {
                    for (int col = 0; col < 5; ++col)
                    {
                        muxWrite(&gMuxOgKeyCol, col, false);
                    }

                    muxWrite(&gMuxOgKeyCol, i / 8, true);
                    pressed = muxRead(&gMuxOgKeyRow, i % 8);
                }
            }
            else
            {
                for (int col = 0; col < 8; col++)
                {
                    muxWrite(&gMuxKeyCol, col, false);
                }

                muxWrite(&gMuxKeyCol, i / 8, true);
                pressed = muxRead(&gMuxKeyRow, i % 8);
            }

            if (pressed)
            {
                cnt++;
            }

            sysKeyTimestamp* obj = gSysKeyTimestamps + i;

            u32 tim5      = TIM_US->CNT;
            u32 timeStamp = tim5;
            if (timeStamp == 0)
            {
                timeStamp = 1;
            }

            if (pass == 0)
            {
                obj->timeStampPressedFirst = pressed ? timeStamp : 0;
            }
            else
            {
                obj->timeStampPressedSecond = pressed ? timeStamp : 0;
            }
        }
    }

    sysKeyTimestamp obj = gSysKeyTimestamps[TONE_OFFSET_C6 - TONE_OFFSET_C2];
    if (obj.timeStampPressedFirst != 0 && obj.timeStampPressedSecond != 0)
    {
        timeDiff = abs(obj.timeStampPressedSecond - obj.timeStampPressedFirst);
    }
    else
    {
        timeDiff = 0xDEADBEEF;
    }
}

void sysPoll()
{
    if (gSysIsLoaded)
    {
        sysReadInputs();
        sysHandleInputs();
        sysUpdateTrackData();
    }
    sysRender();
}

void sysRender()
{
    glcdClsSoft();
    glcdDrawRectangle(0, 0, 128, 64, 1);

    if (gSysCurrentMenuId != gSysLastMenuId)
    {
        sysGlcdSeekOrigin();
        gSysCurrentMenuTimer = 0;
    }
    gSysLastMenuId = gSysCurrentMenuId;

    switch (gSysCurrentMenuId)
    {
        case SYS_MENU_LOADING:
            {
                sysGlcdSeekOrigin();

                if (gSysCurrentMenuTimer > 2)
                {
                    gSysCurrentMenuId = SYS_MENU_LOADING_DONE;
                    break;
                }

                glcdDrawStringCenteredInRect("LOADING...", SYS_GUI_ORIGIN, SYS_GUI_ORIGIN, 128 - SYS_GUI_ORIGIN - SYS_GUI_ORIGIN, 64 - SYS_GUI_ORIGIN - SYS_GUI_ORIGIN, true, true);

                break;
            }
        case SYS_MENU_LOADING_DONE:
            {
                s32 timerY = max(0, (gSysCurrentMenuTimer - 2.0F) * 70);
                if (timerY > 64 - SYS_GUI_ORIGIN)
                {
                    gSysIsLoaded      = true;
                    gSysCurrentMenuId = SYS_MENU_DEFAULT;
                    break;
                }

                glcdDrawStringCenteredInRect("LOADED.\nL o v r o  S y n t h", SYS_GUI_ORIGIN, timerY + SYS_GUI_ORIGIN, 128 - SYS_GUI_ORIGIN - SYS_GUI_ORIGIN, 64 - SYS_GUI_ORIGIN - SYS_GUI_ORIGIN, true, true);

                break;
            }
        case SYS_MENU_DEFAULT:
            {
                sysGlcdSeekOrigin();

                char buf[64];
                sprintf(buf, "timeDiff: %05lu\npitchBend: %.2f (%.2f kHz)\ncnt: %lu\n", timeDiff == 0xdeadbeef ? 0 : timeDiff, bend, 120.0e6 / (TIM_DAC->ARR + 1), cnt);
                glcdDrawString(buf);
                // glcdDrawString(sysGetMainTrack()->instrument.cachedName);

                // char buf[16];
                // sprintf(buf, "%05d\n", sysGetMainTrack()->instrument.instrumentId);
                // glcdDrawString(buf);
                // glcdDrawString(sysGetMainTrack()->instrument.cachedName);

                // if (!gSysCurrentInstrumentIsMulti)
                // {
                //     glcdDrawString(gSysCurrentInstrumentSingle->cachedName);
                // }
                // else
                // {
                //     glcdDrawString("Multis not implemented.");
                // }
                break;
            }

        default:
            {
                break;
            }
    }

    gSysCurrentMenuTimer += 1.0F / SYS_POLL_RATE;
    glcdCopyBackBufferToFront();
}

void sysUpdateTrackData()
{
    for (int i = 0; i < SYS_POLYPHONY_COUNT; i++)
    {
        sysTrack*                   track      = gSysTracks + i;
        sysSingleInstrumentRuntime* instrument = &track->instrument;

        if (instrument->instrumentId == SFS_INVALID_INSTRUMENT_ID)
        {
            continue;
        }

        if (instrument->instrumentId >= gSfsHeader->instrumentCount)
        {
            instrument->instrumentId = 0;
        }

        if (!track->isLoaded)
        {
            u32 off = instrument->instrumentId * sizeof(sfsSingleInstrument);

            sfsReadBlocks(gSysTmpBlock, gSfsHeader->instrumentInfoDataBlockStart + off / BLOCK_SIZE, 1);
            memcpy(instrument, gSysTmpBlock + off % BLOCK_SIZE, sizeof(sfsSingleInstrument)); // NOT a typo. instrument.base is always at offset 0x0

            u32 lutOffset = instrument->base.nameStrIndex * sizeof(u32);
            sfsReadBlocks(gSysTmpBlock, gSfsHeader->stringLutBlockStart + lutOffset / BLOCK_SIZE, 1);

            u32 stringOffset = *(u32*)(gSysTmpBlock + (lutOffset % BLOCK_SIZE));
            sfsReadBlocks(gSysTmpBlock, gSfsHeader->stringDataBlockStart + stringOffset / BLOCK_SIZE, 1);

            strcpy(instrument->cachedName, (str)gSysTmpBlock + stringOffset);

            sfsReadBlocks(gSysTrackData[i], instrument->base.firstSampleBlock, SYS_TRACK_BUFFER_BLOCK_COUNT);
            track->currentSample = 1;

            track->isLoaded = true;
        }
    }
}

void sysNoteOn(u8 pTrack, u16 pNoteSemitones, u8 pVelocity)
{
    sysTrack* track = gSysTracks + pTrack;
    if (!track->isLoaded)
    {
        return;
    }

    if (track->currentSample % SYS_TRACK_BUFFER_SIZE == SYS_TRACK_BUFFER_SIZE / 2)
    {
        sfsReadBlocks(gSysTrackData[pTrack], track->instrument.base.firstSampleBlock + track->currentSample / BLOCK_SIZE, SYS_TRACK_BUFFER_BLOCK_COUNT);
    }
    if (track->currentSample % SYS_TRACK_BUFFER_SIZE == 0)
    {
        sfsReadBlocks((u8*)(gSysTrackData[pTrack] + SYS_TRACK_SAMPLE_COUNT / 2), track->instrument.base.firstSampleBlock + track->currentSample / BLOCK_SIZE, SYS_TRACK_BUFFER_BLOCK_COUNT);
    }

    track->currentSample++;
}

synthErrno sysDeinit()
{
    return SERR_OK;
}
