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

#include "sser/sser.h"

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
sysTrack       gSysTrackInfo[SYS_TRACK_COUNT] = {};
sysTrack *     gSysTrackCurrent               = gSysTrackInfo;
s16            gSysPolyphonyData[SYS_TRACK_COUNT][SFS_KEY_COUNT][SYS_AUDIO_BUFFER_SAMPLE_COUNT];
sysPolyphony   gSysPolyphonyInfo[SYS_TRACK_COUNT][SFS_KEY_COUNT] = {};
u8             gSysTmpBlock[BLOCK_SIZE];

u32 gSysDmaProgress     = 0;
u32 gSysDataLoadCounter = 0;

char gSysStrError[128];

sysInputBitmap  gSysBtnKeyStates[SFS_KEYS_WORD_COUNT]     = {{0}};
sysInputBitmap  gSysBtnMtx1State                          = {0};
sysKeyTimestamp gSysKeyTimestamps[SFS_KEY_SEMITONE_RANGE] = {{0}};

ADC_HandleTypeDef *gSysAdc;

u32 gMidiEvent = 0;

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

    gSysAdc = pAdc;
    ret     = HAL_ADC_Start(pAdc);
    if (ret != HAL_OK)
    {
        return SERR_GENERIC_ERROR;
    }

    for (;; gMidiEvent++)
    {
        smidiTestReadEvent(gMidiEvent);
        if (gSmidiCurrentEvent.event == SMIDI_CHANNEL_EVENT_NOTE_ON)
        {
            break;
        }
    }

    sysGetMainTrack()->isActive                = true;
    sysGetMainTrack()->instrument.instrumentId = 16; // jv

    return SERR_OK;
}

void sysReadInputs() {
    if (TIM_US->CNT - sysGetMainTrack()->midiUs > gSmidiCurrentEvent.deltaTime * gSmidiCurrentEvent.usPerTick)
    {
        sysGetMainTrack()->midiUs = TIM_US->CNT;
        if (gSmidiCurrentEvent.event == SMIDI_CHANNEL_EVENT_NOTE_ON)
        {
            int  note = gSmidiCurrentEvent.note - 12 - SFS_FIRST_KEY;
            u8   idx  = min(max(0, note), 60);
            auto key  = sysGetMainTrack()->keys + idx;

            key->state    = SYS_BTNSTATE_HELD;
            key->velocity = 255;
        }
        else if (gSmidiCurrentEvent.event == SMIDI_CHANNEL_EVENT_NOTE_OFF)
        {
            int  note = gSmidiCurrentEvent.note - 12 - SFS_FIRST_KEY;
            u8   idx  = min(max(0, note), 60);
            auto key  = (sysGetMainTrack()->keys) + idx;

            key->state = SYS_BTNSTATE_UP;
        }

        smidiTestReadEvent(++gMidiEvent);
    }

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

        sfsReadBlocksFull((u8 *) &gSysProximityTable, gSfsHeader->proximityTableBlockStart + SFS_PROXIMITY_TABLE_BLOCK_SIZE * runtimeInstrument->instrumentId, SFS_PROXIMITY_TABLE_BLOCK_SIZE);

        for (int key = 0; key < SFS_KEY_COUNT; ++key)
        {
            sysPolyphony *polyphony = &(gSysPolyphonyInfo[i][key]);
            sysKeyData *  keyData   = track->keys + key;

            switch (keyData->state)
            {
                case SYS_BTNSTATE_NULL: {
                    polyphony->play = false;
                    break;
                }

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

                    sfsReadBlockFull(gSysTmpBlock, gSfsHeader->sampleInfoBlockStart + block);
                    sfsInstrumentSample *sample = ((sfsInstrumentSample *) gSysTmpBlock) + (sampleIdx % (BLOCK_SIZE / sizeof(sfsInstrumentSample)));

                    u32  current = polyphony->blockProgress;
                    bool inLoop  = polyphony->inLoop;

                    if (sample != polyphony->sample)
                    {
                        *polyphony        = (sysPolyphony){};
                        polyphony->sample = sample;
                    }

                    u8 *polyData = (u8 *) gSysPolyphonyData[i][key];

                    if (base->soundType & SFS_SOUND_TYPE_LOOP)
                    {
                        u32 loopStartSamples    = sample->loopStart;
                        u32 loopStartBytes      = loopStartSamples * SAMPLE_SIZE;
                        u32 loopStartBlock      = sample->pcmDataBlockOffset + loopStartBytes / BLOCK_SIZE;
                        u32 loopStartOffset     = loopStartBytes % BLOCK_SIZE;
                        u16 loopDurationSamples = sample->loopDuration - 1;
                        u32 loopDurationBytes   = loopDurationSamples * SAMPLE_SIZE;

                        u32 sampleOff      = SAMPLE_SIZE * (loopStartSamples + polyphony->loopProgressSamples);
                        u32 sampleOffBlock = sampleOff / BLOCK_SIZE;
                        u32 loopBlock      = sample->pcmDataBlockOffset + sampleOffBlock;
                        u32 loopBlockOff   = sampleOff % BLOCK_SIZE;

                        if (inLoop)
                        {
                            if (loopDurationBytes > BLOCK_SIZE)
                            {
                                u32 remainingBytes = SAMPLE_SIZE * (loopDurationSamples - polyphony->loopProgressSamples);
                                if (remainingBytes > BLOCK_SIZE)
                                {
                                    sfsReadBlocksFromOffsetPartial(polyData, loopBlock, loopBlockOff, BLOCK_SIZE);
                                    polyphony->loopProgressSamples += SAMPLES_PER_BLOCK;
                                }
                                else
                                {
                                    u32 left = BLOCK_SIZE - remainingBytes;
                                    u8 *ptr  = polyData;
                                    sfsReadBlocksFromOffsetPartial(ptr, loopBlock, loopBlockOff, remainingBytes);
                                    ptr += remainingBytes;

                                    if (left > 0)
                                    {
                                        sfsReadBlocksFromOffsetPartial(ptr, loopStartBlock, loopStartOffset, left);
                                        polyphony->loopProgressSamples = left / SAMPLE_SIZE;
                                    }
                                    else
                                    {
                                        polyphony->loopProgressSamples = 0;
                                    }
                                }
                            }
                            else
                            {
                                u32 remaining = BLOCK_SIZE;
                                u8 *ptr       = polyData;

                                if (polyphony->loopProgressSamples > 0)
                                {
                                    u32 toRead = loopDurationBytes - polyphony->loopProgressSamples * SAMPLE_SIZE;
                                    sfsReadBlocksFromOffsetPartial(ptr, loopBlock, loopBlockOff, toRead);
                                    remaining -= toRead;
                                    ptr += toRead;
                                }

                                while (remaining >= loopDurationBytes)
                                {
                                    sfsReadBlocksFromOffsetPartial(ptr, loopStartBlock, loopStartOffset, loopDurationBytes);
                                    remaining -= loopDurationBytes;
                                    ptr += loopDurationBytes;
                                }

                                if (remaining > 0)
                                {
                                    sfsReadBlocksFromOffsetPartial(ptr, loopStartBlock, loopStartOffset, remaining);
                                }

                                polyphony->loopProgressSamples = remaining / SAMPLE_SIZE;
                            }
                        }

                        else
                        {
                            if (current * SAMPLES_PER_BLOCK <= loopStartSamples)
                            {
                                sfsReadBlockFull(polyData, sample->pcmDataBlockOffset + polyphony->blockProgress);
                                polyphony->blockProgress++;
                            }
                            else
                            {
                                u32 remaining = BLOCK_SIZE;
                                u8 *ptr       = polyData;

                                polyphony->loopProgressSamples = sample->loopStart % SAMPLES_PER_BLOCK;

                                sampleOff      = SAMPLE_SIZE * (loopStartSamples + polyphony->loopProgressSamples);
                                sampleOffBlock = sampleOff / BLOCK_SIZE;
                                loopBlock      = sample->pcmDataBlockOffset + sampleOffBlock;
                                loopBlockOff   = sampleOff % BLOCK_SIZE;

                                if (polyphony->loopProgressSamples > 0)
                                {
                                    u32 toRead = loopDurationBytes - polyphony->loopProgressSamples * SAMPLE_SIZE;
                                    sfsReadBlocksFromOffsetPartial(ptr, loopBlock, loopBlockOff, toRead);
                                    remaining -= toRead;
                                    ptr += toRead;
                                }

                                while (remaining > loopDurationBytes)
                                {
                                    sfsReadBlocksFromOffsetPartial(ptr, loopStartBlock, loopStartOffset, loopDurationBytes);
                                    remaining -= loopDurationBytes;
                                    ptr += loopDurationBytes;
                                }

                                if (remaining > 0)
                                {
                                    sfsReadBlocksFromOffsetPartial(ptr, loopStartBlock, loopStartOffset, remaining);
                                }

                                polyphony->loopProgressSamples = remaining / SAMPLE_SIZE;
                                polyphony->inLoop              = true;
                            }
                        }
                    }
                    else
                    {
                        if (current >= sample->pcmDataBlockOffset + sample->pcmDataLengthBlocks)
                        {
                            memset(polyData, 0, SYS_AUDIO_BUFFER_SIZE);
                            *polyphony     = (sysPolyphony){};
                            // keyData->state = SYS_BTNSTATE_NULL;
                            goto switchEnd;
                        }

                        sfsReadBlockFull(polyData, sample->pcmDataBlockOffset + current);

                        polyphony->blockProgress++;
                    }

                    if (keyData->velocity != best->velocity)
                    {
                        float velocityFix = 1 + (((int) keyData->velocity - best->velocity) / 255.0F);

                        int j = 0;
                        for (s16 *s = (s16 *) (polyData); j < SYS_AUDIO_BUFFER_SAMPLE_COUNT; ++j, ++s)
                        {
                            *s = (s16) (*s * velocityFix);
                        }
                    }

                    polyphony->play = true;
                    polyphonyCounter++;

                    break;
                }

                case SYS_BTNSTATE_UP: {
                    keyData->state = SYS_BTNSTATE_NULL;
                    break;
                }
            }
        switchEnd:



        }
    }

loopEnd:

    memset(gSysAudioBackBuf, 0, SYS_AUDIO_BUFFER_SIZE);

    if (polyphonyCounter != 0)
    {
        for (int i = 0; i < SYS_TRACK_COUNT; ++i)
        {
            sysTrack *track = gSysTrackInfo + i;
            if (!track->isActive)
            {
                continue;
            }

            sysPolyphony *arr  = gSysPolyphonyInfo[i];
            auto          poly = gSysPolyphonyData[i];
            for (int j = 0; j < SFS_KEY_COUNT; ++j)
            {
                if (!arr[j].play)
                {
                    continue;
                }

                s16 *data = poly[j];
                for (int k = 0; k < SYS_AUDIO_BUFFER_SAMPLE_COUNT; ++k)
                {
                    gSysAudioBackBuf[k] += data[k];
                }
            }
        }
        // for (int sampleIdx = 0; sampleIdx < SYS_AUDIO_BUFFER_SAMPLE_COUNT; ++sampleIdx)
        // {
        //     s16 sum = 0;
        //     for (int i = 0; i < polyphonyCounter; ++i)
        //     {
        //         sum += gSysPolyphonyData[i][sampleIdx];
        //     }
        //
        //     gSysAudioBackBuf[sampleIdx] = sum;
        // }
    }

    for (int i = 0; i < SYS_AUDIO_BUFFER_SAMPLE_COUNT; ++i)
    {
        gSysAudioFrontBuf[i] = (0x8000 + gSysAudioBackBuf[i]) / 16;
    }

    sserSendAudio((u8 *) gSysAudioFrontBuf, SYS_AUDIO_BUFFER_SIZE);
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
        sysReadInputs();
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
            sfsReadBlockFull(gSysTmpBlock, gSfsHeader->instrumentInfoDataBlockStart + off / BLOCK_SIZE);

            memcpy(instrument, gSysTmpBlock + off % BLOCK_SIZE, sizeof(sfsSingleInstrument)); // NOT a typo. instrument.base is always at offset 0x0

            u32 lutOffset = instrument->base.nameStrIndex * sizeof(u32);
            sfsReadBlockFull(gSysTmpBlock, gSfsHeader->stringLutBlockStart + lutOffset / BLOCK_SIZE);

            u32 stringOffset = *(u32 *) (gSysTmpBlock + (lutOffset % BLOCK_SIZE));
            sfsReadBlockFull(gSysTmpBlock, gSfsHeader->stringDataBlockStart + stringOffset / BLOCK_SIZE);

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
    sysPrintf("error %d\n", (int) pCode);
    sprintf(gSysStrError, "Error - code %d", pCode);
    gSysCurrentMenuId = SYS_MENU_ERROR;
    sysRender();
}

synthErrno sysDeinit() {
    return SERR_OK;
}
