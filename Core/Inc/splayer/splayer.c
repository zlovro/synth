//
// Created by lovro on 22/07/2025.
// Copyright (c) 2025 lovro. All rights reserved.
//

#include <stdlib.h>
#include <ssys/ssys.h>
#include <splayer/splayer.h>

#include "freeverb/freeverb.h"
#include "wav/wav.h"

sysTrack *           gSplrTrackCurrent;
sfsKeyProximityTable gSplrProximityTable                                                                 = {};
sysTrack             gSplrTrackInfo[SPLR_TRACK_COUNT]                                                    = {};
s16                  gSplrPolyphonyData[SPLR_TRACK_COUNT][SFS_KEY_COUNT][SPLR_AUDIO_BUFFER_SAMPLE_COUNT] = {};
sysPolyphony         gSplrPolyphonyInfo[SPLR_TRACK_COUNT][SFS_KEY_COUNT]                                 = {};
s16                  gSplrFrontBuf[SPLR_DAC_HALF_SAMPLE_COUNT]                                           = {};
s16                  gSplrBackBuf[SPLR_DAC_HALF_SAMPLE_COUNT]                                            = {};
f32                  gSplrBackBufF32[SPLR_DAC_HALF_SAMPLE_COUNT]                                         = {};

#ifdef DESKTOP
FILE *gSplrWavOutFile;
#endif

void splrInit() {
    #ifdef DESKTOP
    gSplrWavOutFile = fopen("splr-out.wav", "wb+");
    if (!gSplrWavOutFile)
    {
        return;
    }
    #endif

    gSplrMainTrack->isActive                = true;
    gSplrMainTrack->isLoaded                = false;
    gSplrMainTrack->instrument.instrumentId = 20;
}

void splrDeinit() {
    #ifdef DESKTOP
    fseek(gSplrWavOutFile, 0, SEEK_END);
    u32 size = ftell(gSplrWavOutFile);
    fseek(gSplrWavOutFile, 0, SEEK_SET);

    void *fileData = malloc(size);
    u32   read     = fread(fileData, 1, size, gSplrWavOutFile);
    if (read != size)
    {
        return;
    }

    void *buf = malloc(size + sizeof(wavHeader));
    wavWrite(buf, 16, 1, SFS_SAMPLERATE, fileData, size);

    fseek(gSplrWavOutFile, 0, SEEK_SET);
    fwrite(buf, 1, size, gSplrWavOutFile);

    free(fileData);
    free(buf);

    fclose(gSplrWavOutFile);
    #endif
}

void splrUpdateTrackData() {
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

            #ifdef DEBUG
            printf("Loaded instrument '%s':\n", instrument->cachedName);
            printf("\tid:   %d\n", instrument->instrumentId);
            printf("\tloop: %d\n", instrument->base.soundType & SFS_SOUND_TYPE_LOOP);
            fflush(stdout);
            #endif
        }
    }
}

void splrSynthesize() {
    s32 polyphonyCounter = 0;
    for (int i = 0; i < SPLR_TRACK_COUNT; ++i)
    {
        sysTrack *track = gSplrTrackInfo + i;
        if (!track->isLoaded)
        {
            continue;
        }

        sysSingleInstrumentRuntime *runtimeInstrument = &track->instrument;
        sfsSingleInstrument *       instrumentBase    = &runtimeInstrument->base;
        UNUSED(instrumentBase);

        if (runtimeInstrument->instrumentId == SFS_INVALID_INSTRUMENT_ID)
        {
            continue;
        }

        sfsReadBlocksFull((u8 *) &gSplrProximityTable, gSfsHeader->proximityTableBlockStart + SFS_PROXIMITY_TABLE_BLOCK_SIZE * runtimeInstrument->instrumentId, SFS_PROXIMITY_TABLE_BLOCK_SIZE);

        for (int key = 0; key < SFS_KEY_COUNT; ++key)
        {
            sysPolyphony *polyphony   = &(gSplrPolyphonyInfo[i][key]);
            sysKeyData *  keyData     = track->keys + key;
            u8 *          polyData    = (u8 *) gSplrPolyphonyData[i][key];
            s16 *         polyDataS16 = (s16 *) polyData;

            if (!polyphony->fading && keyData->state == SYS_KEYSTATE_NULL)
            {
                *polyphony = (sysPolyphony){};
            }
            else if ((keyData->state == SYS_KEYSTATE_DOWN || keyData->state == SYS_KEYSTATE_HELD) || polyphony->fading)
            {
                if (keyData->state == SYS_KEYSTATE_DOWN && polyphony->fading)
                {
                    *polyphony = (sysPolyphony){};
                }

                sfsKeyProximityTableEntryVelocity *first  = (gSplrProximityTable.masterEntries + key)->byVelocity;
                sfsKeyProximityTableEntryVelocity *second = first + 1;
                sfsKeyProximityTableEntryVelocity *best   = first;

                if (second->velocity == 0)
                {
                    best = first;
                }
                else
                {
                    int lowestDelta = INT_MAX;
                    int lastDelta   = INT_MAX;
                    for (int j = 0; j < SFS_MAX_VELOCITY_COUNT; ++j)
                    {
                        sfsKeyProximityTableEntryVelocity *x = first + j;
                        if (x->velocity == SFS_INVALID_VELOCITY)
                        {
                            break;
                        }

                        int delta = abs((int) x->velocity - keyData->velocity);
                        if (delta > lastDelta) // velocities are sorted in ascending order, if current delta is higher than last delta, it will only keep increasing, quit the loop
                        {
                            break;
                        }

                        lastDelta = delta;

                        if (delta < lowestDelta)
                        {
                            lowestDelta = delta;
                            best        = x;
                        }
                    }
                }

                if (best == NULL)
                {
                    continue;
                }

                if (polyphonyCounter >= SPLR_POLYPHONY_COUNT)
                {
                    goto loopEnd;
                }

                u32 sampleIdx = gSplrProximityTable.sampleIdxOrigin + best->sampleIdx;
                u32 block     = (sampleIdx * sizeof(sfsInstrumentSample)) / BLOCK_SIZE;

                sfsReadBlockFull(gSfsTmpBlock, gSfsHeader->sampleInfoBlockStart + block);
                sfsInstrumentSample *sample = ((sfsInstrumentSample *) gSfsTmpBlock) + (sampleIdx % (BLOCK_SIZE / sizeof(sfsInstrumentSample)));

                u32  blockProgress = polyphony->blockProgress;
                bool inLoop        = polyphony->inLoop;

                if (sample->pcmDataBlockOffset != polyphony->sample.pcmDataBlockOffset)
                {
                    *polyphony        = (sysPolyphony){};
                    polyphony->sample = *sample;
                }

                u32 sampleLengthSamples = sample->pcmDataLengthSamples;

                // bool held = keyData->state == SPLR_KEYSTATE_HELD;
                if (instrumentBase->soundType & SFS_SOUND_TYPE_LOOP)
                {
                    u32 loopStartSamples    = sample->loopStart;
                    u32 loopStartBytes      = loopStartSamples * SAMPLE_SIZE;
                    u32 loopStartBlock      = sample->pcmDataBlockOffset + loopStartBytes / BLOCK_SIZE;
                    u32 loopStartOffset     = loopStartBytes % BLOCK_SIZE;
                    u16 loopDurationSamples = sample->loopDuration;
                    u32 loopDurationBytes   = loopDurationSamples * SAMPLE_SIZE;
                    u32 loopEndSample       = loopStartSamples + loopDurationSamples;
                    UNUSED(loopEndSample);

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
                        u32 blockProgressSamples = blockProgress * SAMPLES_PER_BLOCK;
                        u32 blockProgressBytes   = SAMPLE_SIZE * blockProgressSamples;
                        if (blockProgressSamples < loopStartSamples)
                        {
                            sfsReadBlockFull(polyData, sample->pcmDataBlockOffset + polyphony->blockProgress);
                            polyphony->blockProgress++;
                        }
                        else
                        {
                            u32 remaining = BLOCK_SIZE;
                            u8 *ptr       = polyData;
                            u32 loopProgress_Stack;

                            u32 readLoopSamples = blockProgressSamples - loopStartSamples;
                            if (readLoopSamples > loopDurationSamples)
                            {
                                // make sure to overwrite
                                u32 a = loopStartBytes - (blockProgressBytes - BLOCK_SIZE);
                                ptr += a + loopDurationBytes;
                                remaining          = BLOCK_SIZE - (ptr - polyData);
                                loopProgress_Stack = 0;
                            }
                            else
                            {
                                loopProgress_Stack = readLoopSamples;
                            }

                            sampleOff      = SAMPLE_SIZE * (loopStartSamples + loopProgress_Stack);
                            sampleOffBlock = sampleOff / BLOCK_SIZE;
                            loopBlock      = sample->pcmDataBlockOffset + sampleOffBlock;
                            loopBlockOff   = sampleOff % BLOCK_SIZE;

                            if (loopProgress_Stack > 0)
                            {
                                u32 toRead = loopDurationBytes - loopProgress_Stack * SAMPLE_SIZE;
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
                    if (blockProgress >= (SAMPLE_SIZE * sampleLengthSamples) / BLOCK_SIZE)
                    {
                        goto switchEnd;
                    }

                    // printf("Reading block 0x%X\n", sample->pcmDataBlockOffset + blockProgress);
                    sfsReadBlockFull(polyData, sample->pcmDataBlockOffset + blockProgress);

                    polyphony->blockProgress++;
                }

                if (keyData->velocity != best->velocity)
                {
                    f32 velocityFix = 1.0F + ((f32) ((int) keyData->velocity - (int) best->velocity) / 255.0F);

                    int j = 0;
                    for (s16 *s = polyDataS16; j < SPLR_AUDIO_BUFFER_SAMPLE_COUNT; ++j, ++s)
                    {
                        *s = (s16) ((f32) (*s) * velocityFix);
                    }
                }

                polyphony->play = true;
                polyphonyCounter++;

                sserSendAudio(polyData, SPLR_AUDIO_BUFFER_SIZE);
            }
            else if (keyData->state == SYS_KEYSTATE_UP)
            {
                polyphony->fading              = true;
                polyphony->fadeDurationSamples = (u32) (instrumentBase->fadeTimeForced * SFS_SAMPLERATE);
                polyphony->fadeProgressSamples = 0;

                keyData->state = SYS_KEYSTATE_NULL;
            }
            else
            {
            }

        switchEnd:

            if (polyphony->fading)
            {
                for (int j = 0; j < SAMPLES_PER_BLOCK; ++j)
                {
                    float t        = max(0, 1.0F - (f32) polyphony->fadeProgressSamples / (f32) polyphony->fadeDurationSamples);
                    polyDataS16[j] = (s16) ((f32) polyDataS16[j] * t);

                    polyphony->fadeProgressSamples++;
                    if (polyphony->fadeProgressSamples >= polyphony->fadeDurationSamples)
                    {
                        // keystate is already null, by setting fading to false we pass the check to reset the polyphony
                        polyphony->fading = false;
                        break;
                    }
                }
            }
        }
    }

loopEnd:
    memset(gSplrBackBuf, 0, SPLR_AUDIO_BUFFER_SIZE);

    if (polyphonyCounter != 0)
    {
        // mix
        for (int i = 0; i < SPLR_TRACK_COUNT; ++i)
        {
            sysTrack *track = gSplrTrackInfo + i;
            if (!track->isActive)
            {
                continue;
            }

            sysPolyphony *arr                                   = gSplrPolyphonyInfo[i];
            s16 (*        poly)[SPLR_AUDIO_BUFFER_SAMPLE_COUNT] = gSplrPolyphonyData[i];

            for (int j = 0; j < SFS_KEY_COUNT; ++j)
            {
                if (!arr[j].play)
                {
                    continue;
                }

                s16 *data = poly[j];
                for (int k = 0; k < SPLR_AUDIO_BUFFER_SAMPLE_COUNT; ++k)
                {
                    gSplrBackBuf[k] = (s16) ((int) gSplrBackBuf[k] + (int) data[k]);
                }
            }
        }

        #ifdef DESKTOP
        // fwrite(gSplrBackBuf, 1, SPLR_DAC_HALF_BUFFER_SIZE, gSplrWavOutFile);
        #endif

        if (gSysSettings.reverbEnabled)
        {
            for (int i = 0; i < SPLR_AUDIO_BUFFER_SAMPLE_COUNT; ++i)
            {
                gSplrBackBufF32[i] = (f32) gSplrBackBuf[i];
            }
            fvProcess(gSplrBackBufF32, SPLR_AUDIO_BUFFER_SAMPLE_COUNT);
            for (int i = 0; i < SPLR_AUDIO_BUFFER_SAMPLE_COUNT; ++i)
            {
                gSplrBackBuf[i] = (s16) (gSplrBackBufF32[i]);
            }
        }
    }

    #ifdef DESKTOP
    memcpy(gSplrFrontBuf, gSplrBackBuf, SPLR_DAC_HALF_BUFFER_SIZE);
    #else
    for (int i = 0; i < SPLR_AUDIO_BUFFER_SAMPLE_COUNT; ++i)
    {
        gSplrFrontBuf[i] = (0x8000 + gSplrBackBuf[i]) / 16;
    }
    #endif

    sserSendAudio((u8 *) gSplrFrontBuf, SPLR_AUDIO_BUFFER_SIZE);
}
