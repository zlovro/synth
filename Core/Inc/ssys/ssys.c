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

#include "splayer/splayer.h"
#include "sser/sser.h"

bool gSysIsLoaded         = false;
bool gSysIsBusy           = false;
u32  gSysSkippedPasses    = 0;
u8   gSysCurrentMenuId    = SYS_MENU_LOADING;
u8   gSysLastMenuId       = SYS_MENU_INVALID;
f32  gSysCurrentMenuTimer = 0;

u16 DMA_BUFFER gSysDacBuf[SPLR_DAC_SAMPLE_COUNT] = {};

u32 gSysDmaProgress     = 0;
u32 gSysDataLoadCounter = 0;
u32 gMidiEvent          = 0;

char gSysStrError[128];

sfsKeyProximityTable gSysProximityTable;
sysSettings          gSysSettings                              = {2};
sysInputBitmap       gSysBtnKeyStates[SFS_KEYS_WORD_COUNT]     = {{0}};
sysInputBitmap       gSysBtnMtx1State                          = {0};
sysKeyTimestamp      gSysKeyTimestamps[SFS_KEY_SEMITONE_RANGE] = {{0}};
sysRotaryEncoder     gSysRotaryEncoder                         = {};
sysDebugData         gSysDebugData                             = {};

ADC_HandleTypeDef *gSysAdc;

const bool DEBUG_MENU = true;

synthErrno sysInit(DAC_HandleTypeDef *pDac, ADC_HandleTypeDef *pAdc) {
    glcdSetOrigin(SYS_GUI_ORIGIN, SYS_GUI_ORIGIN);

    zmem(gSfsTmpBlock, BLOCK_SIZE);

    for (int i = 0; i < SPLR_TRACK_COUNT; i++)
    {
        (gSplrTrackInfo + i)->isActive = false;
    }

    auto ret = HAL_DAC_Start_DMA(pDac, DAC_CHANNEL_1, (u32 *) gSysDacBuf, SPLR_DAC_SAMPLE_COUNT, DAC_ALIGN_12B_R);
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

    gSplrMainTrack->isActive                = true;
    gSplrMainTrack->instrument.instrumentId = 20;
    gSysDebugData.submenu                   = SYS_DEBUG_SUBMENU_INSTRUMENT_SELECT;
    gSysDebugData.reloadSample              = true;

    return SERR_OK;
}

void sysReadInputs() {
    gSysRotaryEncoder.outA              = HAL_GPIO_ReadPin(ROT_A_GPIO_Port, ROT_A_Pin);
    gSysRotaryEncoder.outB              = HAL_GPIO_ReadPin(ROT_B_GPIO_Port, ROT_B_Pin);
    gSysRotaryEncoder.sw                = !HAL_GPIO_ReadPin(ROT_SW_GPIO_Port, ROT_SW_Pin);
    gSysDebugData.changeMenuButtonState = HAL_GPIO_ReadPin(BTN_DBG_GPIO_Port, BTN_DBG_Pin);

    sysTrack *track = gSplrMainTrack;
    if (!track->isLoaded)
    {
        return;
    }
    sysSingleInstrumentRuntime *instrument = &(track->instrument);

    sfsInstrumentSample *sample = &gSysDebugData.sample;
    if (gSysDebugData.reloadSample)
    {
        sfsReadBlocksFull((u8 *) &gSysProximityTable, gSfsHeader->proximityTableBlockStart + SFS_PROXIMITY_TABLE_BLOCK_SIZE * instrument->instrumentId, SFS_PROXIMITY_TABLE_BLOCK_SIZE);

        sfsKeyProximityTableEntryVelocity *entry = gSysProximityTable.masterEntries[gSysDebugData.noteSelected].byVelocity;

        for (int i = 0; i < SFS_MAX_VELOCITY_COUNT; ++i, entry++)
        {
            if (entry->velocity == SFS_INVALID_VELOCITY)
            {
                break;
            }

            if (entry->velocity == gSysDebugData.velocity)
            {
                break;
            }

            gSysDebugData.velocity = entry->velocity;
        }

        u32 idx   = gSysProximityTable.sampleIdxOrigin + entry->sampleIdx;
        u32 block = idx / SAMPLE_INFOS_PER_BLOCK;
        sfsReadBlockFromOffsetPartial((u8*)sample, gSfsHeader->sampleInfoBlockStart + block, (idx % SAMPLE_INFOS_PER_BLOCK) * sizeof(sfsInstrumentSample), sizeof(sfsInstrumentSample));

        gSysDebugData.reloadSample = false;
    }

    // button up
    if (!gSysRotaryEncoder.sw && gSysRotaryEncoder.lastSw)
    {
        gSysDebugData.submenu++;
        if (gSysDebugData.submenu == SYS_DEBUG_SUBMENU_MAX)
        {
            gSysDebugData.submenu = 0;
        }

        if (gSysDebugData.submenu == SYS_DEBUG_SUBMENU_LOOP_CHANGE && sample->loopDuration == 0)
        {
            gSysDebugData.submenu--;
        }
    }

    if (!gSysDebugData.changeMenuButtonState && gSysDebugData.lastChangeMenuButtonState)
    {
        gSysDebugData.loopButton++;
        if (gSysDebugData.loopButton == SYS_DEBUG_LOOP_BUTTON_MAX)
        {
            gSysDebugData.loopButton = 0;
        }
    }

    bool cw      = !gSysRotaryEncoder.outA && gSysRotaryEncoder.lastOutA && gSysRotaryEncoder.outB;
    bool ccw     = !gSysRotaryEncoder.outB && gSysRotaryEncoder.lastOutB && gSysRotaryEncoder.outA;
    bool rotated = cw || ccw;

    if (rotated)
    {
        if (gSysDebugData.submenu == SYS_DEBUG_SUBMENU_INSTRUMENT_SELECT)
        {
            u16 lastId = gSfsHeader->instrumentCount - 1;
            if (cw)
            {
                if (instrument->instrumentId == lastId)
                {
                    instrument->instrumentId = 0;
                }
                else
                {
                    instrument->instrumentId++;
                }
            }
            else
            {
                if (instrument->instrumentId == 0)
                {
                    instrument->instrumentId = lastId;
                }
                else
                {
                    instrument->instrumentId--;
                }
            }

            track->isLoaded            = false;
            gSysDebugData.reloadSample = true;
        }
        if (gSysDebugData.submenu == SYS_DEBUG_SUBMENU_LOOP_CHANGE)
        {
            switch (gSysDebugData.loopButton)
            {
                case SYS_DEBUG_LOOP_BUTTON_START: {
                    if (cw)
                    {
                        if (sample->loopStart + sample->loopDuration < sample->pcmDataLengthSamples)
                        {
                            sample->loopStart++;
                        }
                    }
                    else
                    {
                        if (sample->loopStart > 0)
                        {
                            sample->loopStart--;
                        }
                    }
                    break;
                }

                case SYS_DEBUG_LOOP_BUTTON_DURATION: {
                    if (cw)
                    {
                        if (sample->loopStart + sample->loopDuration < sample->pcmDataLengthSamples)
                        {
                            sample->loopDuration++;
                        }
                    }
                    else
                    {
                        if (sample->loopDuration > 0)
                        {
                            sample->loopDuration--;
                        }
                    }
                    break;
                }

                case SYS_DEBUG_LOOP_BUTTON_NOTE_CHANGE: {
                    if (cw)
                    {
                        if (gSysDebugData.noteSelected < SFS_KEY_COUNT - 1)
                        {
                            gSysDebugData.noteSelected++;
                        }
                    }
                    else
                    {
                        if (gSysDebugData.noteSelected > 0)
                        {
                            gSysDebugData.noteSelected--;
                        }
                    }
                    gSysDebugData.reloadSample = true;

                    break;
                }

                case SYS_DEBUG_LOOP_BUTTON_VELOCITY_CHANGE: {
                    u8 idx   = 0;
                    u8 count = 0;

                    sfsKeyProximityTableEntryVelocity *entry = gSysProximityTable.masterEntries[gSysDebugData.noteSelected].byVelocity;
                    for (int i = 0; i < SFS_MAX_VELOCITY_COUNT; ++i, entry++)
                    {
                        if (entry->velocity == gSysDebugData.velocity)
                        {
                            idx = i;
                        }

                        if (entry->velocity == SFS_INVALID_VELOCITY)
                        {
                            count = i;
                        }
                    }

                    idx++;
                    if (idx == count)
                    {
                        idx = 0;
                    }

                    gSysDebugData.velocity     = (entry + idx)->velocity;
                    gSysDebugData.reloadSample = true;

                    break;
                }

                case SYS_DEBUG_LOOP_BUTTON_WRITE: {
                    sfsKeyProximityTableEntryVelocity *entry = gSysProximityTable.masterEntries[gSysDebugData.noteSelected].byVelocity;

                    for (int i = 0; i < SFS_MAX_VELOCITY_COUNT; ++i, entry++)
                    {
                        if (entry->velocity == SFS_INVALID_VELOCITY)
                        {
                            break;
                        }

                        if (entry->velocity == gSysDebugData.velocity)
                        {
                            break;
                        }

                        gSysDebugData.velocity = entry->velocity;
                    }

                    u32 idx   = gSysProximityTable.sampleIdxOrigin + entry->sampleIdx;
                    u32 block = gSfsHeader->sampleInfoBlockStart + idx / SAMPLE_INFOS_PER_BLOCK;
                    sfsReadBlockFull(gSfsTmpBlock, block);

                    memcpy((sfsInstrumentSample *) gSfsTmpBlock + idx % SAMPLE_INFOS_PER_BLOCK, sample, sizeof(sfsInstrumentSample));
                    sfsWriteBlockFull(gSfsTmpBlock, block);

                    break;
                }

                default:
                    break;
            }
        }
    }

    for (int i = 0; i < SFS_KEY_COUNT; ++i)
    {
        sysKeyData *key = track->keys + i;
        key->velocity   = 0;
        key->state      = SYS_KEYSTATE_NULL;
    }

    track->keys[gSysDebugData.noteSelected].state    = SYS_KEYSTATE_HELD;
    track->keys[gSysDebugData.noteSelected].velocity = gSysDebugData.velocity;


    // if (TIM_US->CNT - gSplrMainTrack->midiUs > gSmidiCurrentEvent.deltaTime * gSmidiCurrentEvent.usPerTick)
    // {
    //     gSplrMainTrack->midiUs = TIM_US->CNT;
    //     if (gSmidiCurrentEvent.event == SMIDI_CHANNEL_EVENT_NOTE_ON)
    //     {
    //         int  note = gSmidiCurrentEvent.note - 12 - SFS_FIRST_KEY;
    //         u8   idx  = min(max(0, note), 60);
    //         auto key  = gSplrMainTrack->keys + idx;
    //
    //         key->state    = SYS_KEYSTATE_HELD;
    //         key->velocity = 255;
    //     }
    //     else if (gSmidiCurrentEvent.event == SMIDI_CHANNEL_EVENT_NOTE_OFF)
    //     {
    //         int  note = gSmidiCurrentEvent.note - 12 - SFS_FIRST_KEY;
    //         u8   idx  = min(max(0, note), 60);
    //         auto key  = (gSplrMainTrack->keys) + idx;
    //
    //         key->state = SYS_KEYSTATE_NULL;
    //     }
    //
    //     smidiTestReadEvent(++gMidiEvent);
    // }

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

sysKeyState sysGetButtonState(sysInputBitmap *pMap, u32 pBtn) {
    u32 current = pMap->currentState;
    u32 last    = pMap->lastState;

    if ((current & pBtn) && (last & pBtn))
    {
        return SYS_KEYSTATE_HELD;
    }
    if ((current & pBtn) && !(last & pBtn))
    {
        return SYS_KEYSTATE_DOWN;
    }
    if (!(current & pBtn) && (last & current))
    {
        return SYS_KEYSTATE_UP;
    }

    return SYS_KEYSTATE_NULL;
}

int sysPitchBendCalculateArr(float pSysFreq, int pPrescaler, float pOutFreq) {
    return (int) (pSysFreq / (pOutFreq * pPrescaler + pOutFreq) - 1);
}

void sysHandlePitchBend() {
    const float sysFreq = 120e6;

    // 16 bit ADC. aj dobro
    float bend    = gSysSettings.pitchBendRangeSemitones * ((HAL_ADC_GetValue(gSysAdc) / 32767.5F) - 1);
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
        splrUpdateTrackData();
    }
    sysRender();

    gSysRotaryEncoder.lastOutA              = gSysRotaryEncoder.outA;
    gSysRotaryEncoder.lastOutB              = gSysRotaryEncoder.outB;
    gSysRotaryEncoder.lastSw                = gSysRotaryEncoder.sw;
    gSysDebugData.lastChangeMenuButtonState = gSysDebugData.changeMenuButtonState;
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
    //     glcdSeek(SYS_GUI_ORIGIN, 64 - (SYS_GUI_ORIGIN + 3 * (SYS_LINE_HEIGHT)));
    //     char fps[64];
    //     sprintf(fps, "TPS: %04ld (%6.2f ms)\nFrame counter: %ld\n", (s32) (1 / gSysDeltaTime), gSysDeltaTime * 1000, gGlcdFrameCounter);
    //     glcdDrawString(fps);
    // }

    sysGlcdSeekOrigin();
    gGlcdWrapX = 128;

    sysSingleInstrumentRuntime *mainTrackInstrument = &gSplrMainTrack->instrument;

    switch (gSysCurrentMenuId)
    {
        case SYS_MENU_ERROR: {
            glcdDrawStringCenteredInRect(gSysStrError, 0, 0, 128, 64, true, true);

            break;
        }
        case SYS_MENU_LOADING: {
            if (gSysCurrentMenuTimer > 2)
            {
                gSysCurrentMenuId = SYS_MENU_LOADING_DONE;
                break;
            }

            glcdDrawStringCenteredInRect("LOADING...", SYS_GUI_ORIGIN, SYS_GUI_ORIGIN, 128 - SYS_GUI_ORIGIN - SYS_GUI_ORIGIN, 64 - SYS_GUI_ORIGIN - SYS_GUI_ORIGIN, true, true);

            break;
        }
        case SYS_MENU_LOADING_DONE: {
            s32 timerY = max(0, (gSysCurrentMenuTimer - 2.0F) * 70);
            if (timerY > 64)
            {
                gSysIsLoaded      = true;
                gSysCurrentMenuId = DEBUG_MENU ? SYS_MENU_DEBUG : SYS_MENU_DEFAULT;
                break;
            }

            glcdDrawStringCenteredInRect("LOADED.\nL o v r o  S y n t h", SYS_GUI_ORIGIN, timerY + SYS_GUI_ORIGIN, 128 - SYS_GUI_ORIGIN - SYS_GUI_ORIGIN, 64 - SYS_GUI_ORIGIN - SYS_GUI_ORIGIN, true, true);

            break;
        }
        case SYS_MENU_DEFAULT: {
            glcdPrintf("%05d\n", mainTrackInstrument->instrumentId);
            glcdDrawString(mainTrackInstrument->cachedName);

            break;
        }

        case SYS_MENU_DEBUG: {
            glcdSeek(SYS_GUI_ORIGIN + SYS_GUI_DOT_STRIDE, SYS_GUI_ORIGIN);
            glcdSetOrigin(SYS_GUI_ORIGIN + SYS_GUI_DOT_STRIDE, SYS_GUI_ORIGIN);

            gGlcdWrapX = SYS_GUI_DEBUG_LOOP_PANE_X - 2;

            glcdPrintf("%05d\n", mainTrackInstrument->instrumentId);
            glcdDrawString(mainTrackInstrument->cachedName);
            glcdDrawString("\n");

            gGlcdWrapX = 128;

            glcdDrawLineVertical(SYS_GUI_DEBUG_LOOP_PANE_X - 2, 0, 64);

            u8 writeBtnWidth = 0;
            u8 writeBtnY     = 0;
            u8 writeBtnX     = 0;
            if (gSysDebugData.sample.loopDuration > 0)
            {
                glcdSeek(SYS_GUI_ORIGIN + SYS_GUI_DEBUG_LOOP_PANE_X, SYS_GUI_ORIGIN);
                glcdSetOrigin(SYS_GUI_ORIGIN + SYS_GUI_DEBUG_LOOP_PANE_X, SYS_GUI_ORIGIN);

                char note[4];
                solfegeSemitoneToStr(note, gSysDebugData.noteSelected + SFS_FIRST_KEY, false);
                glcdPrintf("note: %s\nvel: %03d\nlpst: %d\nlpdur: %d\n", note, gSysDebugData.velocity, gSysDebugData.sample.loopStart, gSysDebugData.sample.loopDuration);

                gGlcdCursorY = 50;
                writeBtnY    = gGlcdCursorY - 2;
                writeBtnX    = gGlcdCursorX - 2;
                u8 begin     = gGlcdCursorX;
                glcdPrintf("WRITE");
                writeBtnWidth = 4 + gGlcdCursorX - begin;
            }

            switch (gSysDebugData.submenu)
            {
                case SYS_DEBUG_SUBMENU_INSTRUMENT_SELECT: {
                    glcdSeek(SYS_GUI_ORIGIN, SYS_GUI_ORIGIN);
                    glcdSetOrigin(SYS_GUI_ORIGIN, SYS_GUI_ORIGIN);

                    glcdDrawRectangle(SYS_GUI_ORIGIN, SYS_GUI_ORIGIN, SYS_GUI_DOT_SIZE, SYS_GUI_DOT_SIZE, 1);

                    break;
                }

                case SYS_DEBUG_SUBMENU_LOOP_CHANGE: {
                    glcdSeek(SYS_GUI_ORIGIN + SYS_GUI_DEBUG_LOOP_PANE_X, SYS_GUI_ORIGIN);
                    glcdSetOrigin(SYS_GUI_ORIGIN + SYS_GUI_DEBUG_LOOP_PANE_X, SYS_GUI_ORIGIN);

                    u8 dotX = 1 + SYS_GUI_DEBUG_LOOP_PANE_X;
                    if (gSysDebugData.loopButton != SYS_DEBUG_LOOP_BUTTON_WRITE)
                    {
                        glcdDrawRectangle(dotX, SYS_GUI_ORIGIN + gSysDebugData.loopButton * SYS_LINE_HEIGHT + SYS_LINE_HEIGHT / 2, SYS_GUI_DOT_SIZE, SYS_GUI_DOT_SIZE, 1);
                    }
                    else
                    {
                        glcdDrawRectangle(writeBtnX, writeBtnY, writeBtnWidth, SYS_LINE_HEIGHT + 3, 1);
                    }

                    break;
                }

                default: {
                    break;
                }
            }

            break;
        }

        default: {
            break;
        }
    }

    gSysCurrentMenuTimer += SYS_DELTA_TIME;
    glcdFinalize();
}

void sysUpdateTrackData() {
    for (int i = 0; i < SPLR_TRACK_COUNT; i++)
    {
        sysTrack *track = gSplrTrackInfo + i;
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
            sfsReadBlockFull(gSfsTmpBlock, gSfsHeader->instrumentInfoDataBlockStart + off / BLOCK_SIZE);

            memcpy(instrument, gSfsTmpBlock + off % BLOCK_SIZE, sizeof(sfsSingleInstrument)); // NOT a typo. instrument.base is always at offset 0x0

            u32 lutOffset = instrument->base.nameStrIndex * sizeof(u32);
            sfsReadBlockFull(gSfsTmpBlock, gSfsHeader->stringLutBlockStart + lutOffset / BLOCK_SIZE);

            u32 stringOffset = *(u32 *) (gSfsTmpBlock + (lutOffset % BLOCK_SIZE));
            sfsReadBlockFull(gSfsTmpBlock, gSfsHeader->stringDataBlockStart + stringOffset / BLOCK_SIZE);

            strcpy(instrument->cachedName, (str) gSfsTmpBlock + stringOffset);

            track->isLoaded = true;
        }
    }
}

void sysNoteOn(u8 pTrack, u16 pNoteSemitones, u8 pVelocity) {
    sysTrack *track = gSplrTrackInfo + pTrack;
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
