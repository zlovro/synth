//
// Created by lovro on 22/07/2025.
// Copyright (c) 2025 lovro. All rights reserved.
//

#ifndef SPLAYER_H
#define SPLAYER_H

#include <types.h>
#include <math.h>

#define SPLR_TRACK_COUNT 8
#define SPLR_TRACK_BUFFER_SIZE (BLOCK_SIZE * 2)
#define SPLR_TRACK_BUFFER_SAMPLE_COUNT (SPLR_TRACK_BUFFER_SIZE / 2)
#define SPLR_POLYPHONY_COUNT (SYS_MAX_AVAILABLE_RAM / SPLR_TRACK_BUFFER_SIZE)

#define SPLR_AUDIO_BUFFER_SIZE BLOCK_SIZE
#define SPLR_AUDIO_BUFFER_SAMPLE_COUNT (BLOCK_SIZE / 2)

#define SPLR_DAC_BUFFER_SIZE (SPLR_AUDIO_BUFFER_SIZE * 2)
#define SPLR_DAC_SAMPLE_COUNT (SPLR_DAC_BUFFER_SIZE / sizeof(u16))
#define SPLR_DAC_HALF_BUFFER_SIZE (SPLR_DAC_BUFFER_SIZE / 2)
#define SPLR_DAC_HALF_SAMPLE_COUNT (SPLR_DAC_SAMPLE_COUNT / 2)

#define SPLR_FADE_STOP_AMPLITUDE 300

extern sfsKeyProximityTable gSplrProximityTable;
extern sysTrack *           gSplrTrackCurrent;
extern sysTrack             gSplrTrackInfo[];
extern s16                  gSplrPolyphonyData[SPLR_TRACK_COUNT][SFS_KEY_COUNT][SPLR_AUDIO_BUFFER_SAMPLE_COUNT];
extern sysPolyphony         gSplrPolyphonyInfo[SPLR_TRACK_COUNT][SFS_KEY_COUNT];
extern s16                  gSplrFrontBuf[SPLR_DAC_HALF_SAMPLE_COUNT];
extern s16                  gSplrBackBuf[SPLR_DAC_HALF_SAMPLE_COUNT];
extern f32                  gSplrBackBufF32[SPLR_DAC_HALF_SAMPLE_COUNT];

#ifdef DESKTOP
extern FILE *gSplrWavOutFile;
#endif

#define gSplrMainTrack gSplrTrackInfo

void splrInit();
void splrDeinit();
void splrUpdateTrackData();
void splrSynthesize();

#endif //SPLAYER_H
