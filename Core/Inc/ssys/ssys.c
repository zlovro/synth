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
#include <smidi/smidi.h>

sysSettings gSysSettings         = {2};
bool        gSysIsLoaded         = false;
u32         gSysTickCounter      = 0;
f32         gSysDeltaTime        = 0;
u8          gSysCurrentMenuId    = SYS_MENU_LOADING;
u8          gSysLastMenuId       = SYS_MENU_INVALID;
f32         gSysCurrentMenuTimer = 0;

sfsKeyProximityTable gSysProximityTable;

s16            gSysAudioFrontBuf[SYS_AUDIO_BUFFER_SAMPLE_COUNT];
s16            gSysAudioBackBuf[SYS_AUDIO_BUFFER_SAMPLE_COUNT];
u16 DMA_BUFFER gSysDacBuf[SYS_DAC_SAMPLE_COUNT];
u16 DMA_BUFFER gSysTriBuf[SYS_TRI_BUF_SIZE];
sysTrack       gSysTrackInfo[SYS_TRACK_COUNT];
sysTrack *     gSysTrackCurrent = gSysTrackInfo;
s16            gSysPolyphonyData[SYS_POLYPHONY_COUNT][SYS_AUDIO_BUFFER_SAMPLE_COUNT];
u32            gSysPolyphonyProgress[SYS_POLYPHONY_COUNT] = {};
u8             gSysTmpBlock[BLOCK_SIZE];

u32 gSysDmaProgress     = 0;
u32 gSysDataLoadCounter = 0;

char gSysStrError[128];

sysInputBitmap  gSysBtnKeyStates[SFS_KEYS_WORD_COUNT]     = {{0}};
sysInputBitmap  gSysBtnMtx1State                          = {0};
sysKeyTimestamp gSysKeyTimestamps[SFS_KEY_SEMITONE_RANGE] = {{0}};

ADC_HandleTypeDef *gSysAdc;

synthErrno sysInit(DAC_HandleTypeDef *pDac, ADC_HandleTypeDef *pAdc) {
    glcdSetOrigin(SYS_GUI_ORIGIN, SYS_GUI_ORIGIN);

    memset(gSysTrackInfo, 0, sizeof(sysTrack) * SYS_TRACK_COUNT);
    memset(gSysDacBuf, 0, SYS_DAC_BUFFER_SIZE);
    memset(gSysAudioBackBuf, 0, SYS_AUDIO_BUFFER_SIZE);
    memset(gSysAudioFrontBuf, 0, SYS_AUDIO_BUFFER_SIZE);
    zmem(gSysTmpBlock, BLOCK_SIZE);

    for (int i = 0; i < SYS_TRACK_COUNT; i++)
    {
        (gSysTrackInfo + i)->isActive = false;
    }

    auto ret = HAL_DAC_Start_DMA(pDac, DAC_CHANNEL_1, (u32 *) gSysDacBuf, SYS_DAC_SAMPLE_COUNT, DAC_ALIGN_12B_R);
    if (ret != HAL_OK)
    {
        return SERR_GENERIC_ERROR;
    }

    for (int i = 0; i < SYS_TRI_BUF_HALF_SIZE; i++)
    {
        gSysTriBuf[i] = (f64) i / SYS_TRI_BUF_HALF_SIZE * 0xFFF;
    }
    gSysTriBuf[SYS_TRI_BUF_HALF_SIZE] = 0xFFF;
    for (int i = 0; i < SYS_TRI_BUF_HALF_SIZE; i++)
    {
        gSysTriBuf[SYS_TRI_BUF_HALF_SIZE + i] = (f64) (SYS_TRI_BUF_HALF_SIZE - i) / SYS_TRI_BUF_HALF_SIZE * 0xFFF;
    }

    ret = HAL_DAC_Start_DMA(pDac, DAC_CHANNEL_2, (u32 *) gSysTriBuf, SYS_TRI_BUF_SIZE, DAC_ALIGN_12B_R);
    if (ret != HAL_OK)
    {
        return SERR_GENERIC_ERROR;
    }

    gSysAdc = pAdc;
    ret     = HAL_ADC_Start(pAdc);
    if (ret != HAL_OK)
    {
        return SERR_GENERIC_ERROR;
    }

    return SERR_OK;
}

void sysReadInputs() {
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

void sysSynthesizeAudio() {
    s32 polyphonyCounter = 0;
    for (int i = 0; i < SYS_TRACK_COUNT; ++i)
    {
        sysTrack *track = gSysTrackInfo + i;
        if (!track->isLoaded)
        {
            continue;
        }

        sysSingleInstrumentRuntime *runtimeInstrument = &track->instrument;
        sfsSingleInstrument *       base              = &runtimeInstrument->base;
        UNUSED(base);

        if (runtimeInstrument->instrumentId == SFS_INVALID_INSTRUMENT_ID)
        {
            continue;
        }

        sfsReadBlocks((u8 *) &gSysProximityTable, gSfsHeader->proximityTableBlockStart + SFS_PROXIMITY_TABLE_BLOCK_SIZE * runtimeInstrument->instrumentId, SFS_PROXIMITY_TABLE_BLOCK_SIZE);

        for (int key = 0; key < SFS_KEY_COUNT; ++key)
        {
            sysKeyData *keyData = track->keys + key;

            switch (keyData->state)
            {
                case SYS_BTNSTATE_NULL:
                    break;

                case SYS_BTNSTATE_DOWN:
                case SYS_BTNSTATE_HELD: {
                    sfsKeyProximityTableEntryVelocity *best = NULL;

                    sfsKeyProximityTableEntryVelocity *first  = (gSysProximityTable.masterEntries + key)->byVelocity;
                    sfsKeyProximityTableEntryVelocity *second = first + 1;
                    if (second->velocity == 0)
                    {
                        best = first;
                    }
                    else
                    {
                        for (sfsKeyProximityTableEntryVelocity *entry = second; entry->velocity != SFS_INVALID_VELOCITY; entry++)
                        {
                            sfsKeyProximityTableEntryVelocity *lastEntry = entry - 1;

                            int delta     = abs((int) keyData->velocity - entry->velocity);
                            int lastDelta = abs((int) keyData->velocity - lastEntry->velocity);

                            if (delta > lastDelta)
                            {
                                best = lastEntry;
                                break;
                            }
                        }
                    }

                    if (best == NULL)
                    {
                        continue;
                    }

                    if (polyphonyCounter >= SYS_POLYPHONY_COUNT)
                    {
                        goto loopEnd;
                    }

                    u32 sampleIdx = gSysProximityTable.sampleIdxOrigin + best->sampleIdx;
                    u32 block     = (sampleIdx * sizeof(sfsInstrumentSample)) / BLOCK_SIZE;
                    sfsReadBlocks(gSysTmpBlock, gSfsHeader->sampleInfoBlockStart + block, 1);
                    sfsInstrumentSample *sample = ((sfsInstrumentSample *) gSysTmpBlock) + (sampleIdx % (BLOCK_SIZE / sizeof(sfsInstrumentSample)));

                    UNUSED(sampleIdx);
                    UNUSED(best);

                    u32 current = gSysPolyphonyProgress[polyphonyCounter];

                    if (current >= sample->pcmDataLengthBlocks)
                    {
                        memset(gSysPolyphonyData + polyphonyCounter, 0, SYS_AUDIO_BUFFER_SIZE);
                        goto switchEnd;
                    }

                    sfsReadBlocks((u8 *) (gSysPolyphonyData + polyphonyCounter), sample->pcmDataBlockOffset + current, 1);

                    if (keyData->velocity != best->velocity)
                    {
                        float velocityFix = 1 + (((int) keyData->velocity - best->velocity) / 255.0F);

                        int j = 0;
                        for (s16 *s = (s16 *) (gSysPolyphonyData + polyphonyCounter); j < SYS_AUDIO_BUFFER_SAMPLE_COUNT; ++j, ++s)
                        {
                            *s = (s16) (*s * velocityFix);
                        }
                    }

                    gSysPolyphonyProgress[polyphonyCounter]++;
                    polyphonyCounter++;

                    break;
                }

                case SYS_BTNSTATE_UP:
                    break;
            }
        switchEnd:



        }
    }

loopEnd:

    if (polyphonyCounter == 0)
    {
        memset(gSysAudioBackBuf, 0, SYS_AUDIO_BUFFER_SIZE);
    }
    else
    {
        for (int sampleIdx = 0; sampleIdx < SYS_AUDIO_BUFFER_SAMPLE_COUNT; ++sampleIdx)
        {
            s16 sum = 0;
            for (int i = 0; i < polyphonyCounter; ++i)
            {
                sum += gSysPolyphonyData[i][sampleIdx];
            }

            gSysAudioBackBuf[sampleIdx] = sum;
        }
    }

    for (int i = 0; i < SYS_AUDIO_BUFFER_SAMPLE_COUNT; ++i)
    {
        s16 sample = gSysAudioBackBuf[i];

        s16 s = (0x8000 + sample) / 16;
        UNUSED(s);
        // gSysAudioFrontBuf[i] = i % 2 == 0 ? 0xFFF : 0;
        // gSysAudioFrontBuf[i] = 0xFFF;
        gSysAudioFrontBuf[i] = s;
    }

    // for (int i = 0; i < 4; ++i)
    // {
    //     gSysAudioFrontBuf[SYS_AUDIO_BUFFER_SAMPLE_COUNT - 1 - i] = i % 2 ? 0xFFF : 0;
    // }
}

sysButtonState sysGetButtonState(sysInputBitmap *pMap, u32 pBtn) {
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

int sysPitchBendCalculateArr(float pSysFreq, int pPrescaler, float pOutFreq) {
    return (int) (pSysFreq / (pOutFreq * pPrescaler + pOutFreq) - 1);
}

float bend = 0;

void sysHandlePitchBend() {
    const float sysFreq = 120e6;

    bend          = gSysSettings.pitchBendRangeSemitones * ((HAL_ADC_GetValue(gSysAdc) / 32767.5F) - 1);
    float newFreq = toneAddSemitone(sysFreq, bend);

    TIM_DAC->ARR = sysPitchBendCalculateArr(sysFreq, TIM_DAC->ARR, newFreq);
}

u32 timeDiff;
u32 cnt = 0;

void sysHandleInputs() {
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int i = 0; i < SFS_KEY_SEMITONE_RANGE; ++i)
        {
            bool pressed = false;
            if (pass == 0)
            {
                if (i > TONE_OFFSET_B4 - SFS_FIRST_KEY)
                {
                    int idx = 4 + (i + SFS_FIRST_KEY) - TONE_OFFSET_C5;

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

            sysKeyTimestamp *obj = gSysKeyTimestamps + i;

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

void sysPoll() {
    if (gSysIsLoaded)
    {
        auto track                     = gSysTrackInfo + 0;
        track->isActive                = true;
        track->instrument.instrumentId = 19;

        auto octave = 24;

        track->keys[TONE_OFFSET_B6 - SFS_FIRST_KEY - octave].state    = SYS_BTNSTATE_HELD;
        track->keys[TONE_OFFSET_B6 - SFS_FIRST_KEY - octave].velocity = 255;

        track->keys[TONE_OFFSET_D6 + 1 - SFS_FIRST_KEY - octave].state    = SYS_BTNSTATE_HELD;
        track->keys[TONE_OFFSET_D6 + 1 - SFS_FIRST_KEY - octave].velocity = 255;

        track->keys[TONE_OFFSET_F6 + 1 - SFS_FIRST_KEY - octave].state    = SYS_BTNSTATE_HELD;
        track->keys[TONE_OFFSET_F6 + 1 - SFS_FIRST_KEY - octave].velocity = 255;

        track->keys[TONE_OFFSET_A6 - SFS_FIRST_KEY - octave].state    = SYS_BTNSTATE_HELD;
        track->keys[TONE_OFFSET_A6 - SFS_FIRST_KEY - octave].velocity = 255;

        // sysReadInputs();
        // sysHandleInputs();
        sysUpdateTrackData();
    }
    sysRender();
}

void sysRender() {
    glcdClsSoft();
    glcdDrawRectangle(0, 0, 128, 64, 1);

    if (gSysCurrentMenuId != gSysLastMenuId)
    {
        gSysCurrentMenuTimer = 0;
    }
    gSysLastMenuId = gSysCurrentMenuId;

    // if (gSysDeltaTime > 0)
    // {
    //     glcdSeek(SYS_GUI_ORIGIN, 64 - (SYS_GUI_ORIGIN + 3 * (SYS_FONT_HEIGHT + SYS_FONT_SPACING_Y)));
    //     char fps[64];
    //     sprintf(fps, "TPS: %04ld (%6.2f ms)\nFrame counter: %ld\n", (s32) (1 / gSysDeltaTime), gSysDeltaTime * 1000, gGlcdFrameCounter);
    //     glcdDrawString(fps);
    // }

    sysGlcdSeekOrigin();
    switch (gSysCurrentMenuId)
    {
        case SYS_MENU_ERROR: {
            glcdDrawStringCenteredInRect(gSysStrError, 0, 0, 128, 64, true, true);

            break;
        }
        case SYS_MENU_LOADING: {
            if (gSysCurrentMenuTimer > 0)
            {
                gSysCurrentMenuId = SYS_MENU_LOADING_DONE;
                break;
            }

            glcdDrawStringCenteredInRect("LOADING...", SYS_GUI_ORIGIN, SYS_GUI_ORIGIN, 128 - SYS_GUI_ORIGIN - SYS_GUI_ORIGIN, 64 - SYS_GUI_ORIGIN - SYS_GUI_ORIGIN, true, true);

            break;
        }
        case SYS_MENU_LOADING_DONE: {
            s32 timerY = max(0, (gSysCurrentMenuTimer - 2.0F) * 70);
            if (true || timerY > 0)
            {
                gSysIsLoaded      = true;
                gSysCurrentMenuId = SYS_MENU_DEFAULT;
                break;
            }

            glcdDrawStringCenteredInRect("LOADED.\nL o v r o  S y n t h", SYS_GUI_ORIGIN, timerY + SYS_GUI_ORIGIN, 128 - SYS_GUI_ORIGIN - SYS_GUI_ORIGIN, 64 - SYS_GUI_ORIGIN - SYS_GUI_ORIGIN, true, true);

            break;
        }
        case SYS_MENU_DEFAULT: {
            char buf[32];
            sprintf(buf, "%05d\n", sysGetMainTrack()->instrument.instrumentId);
            glcdDrawString(buf);
            glcdDrawString(sysGetMainTrack()->instrument.cachedName);

            break;
        }

        default: {
            break;
        }
    }

    gSysCurrentMenuTimer += 1.0F / SYS_POLL_RATE;
    glcdFinalize();
}

void sysUpdateTrackData() {
    for (int i = 0; i < SYS_TRACK_COUNT; i++)
    {
        sysTrack *track = gSysTrackInfo + i;
        if (!track->isActive)
        {
            continue;
        }

        sysSingleInstrumentRuntime *instrument = &track->instrument;
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

            u32 stringOffset = *(u32 *) (gSysTmpBlock + (lutOffset % BLOCK_SIZE));
            sfsReadBlocks(gSysTmpBlock, gSfsHeader->stringDataBlockStart + stringOffset / BLOCK_SIZE, 1);

            strcpy(instrument->cachedName, (str) gSysTmpBlock + stringOffset);

            track->isLoaded = true;
        }
    }
}

void sysNoteOn(u8 pTrack, u16 pNoteSemitones, u8 pVelocity) {
    sysTrack *track = gSysTrackInfo + pTrack;
    if (!track->isLoaded)
    {
        return;
    }
}

void sysError(synthErrno pCode) {
    sprintf(gSysStrError, "Error - code %d", pCode);
    gSysCurrentMenuId = SYS_MENU_ERROR;
    sysRender();
}

synthErrno sysDeinit() {
    return SERR_OK;
}
