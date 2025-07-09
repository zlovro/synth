//
// Created by Made on 13/06/2025.
//

#ifndef SMIDI_H
#define SMIDI_H

#include <types.h>

#define SMIDI_TIMEDIV_MASK_FRAMES 0x8000

typedef pstruct {
    u32 magic;
    u32 chunkSize;
    u16 format;
    u16 trackCount;
    u16 timeDivision;
} smidiHeader;

typedef pstruct {
    u32 magic;
    u32 chunkSize;
} smidiTrackHeader;

typedef enum {
    SMIDI_EVENT_CHANNEL = 1 << 8,
    SMIDI_EVENT_META    = 2 << 8,
    SMIDI_EVENT_SYSEX   = 3 << 8,

    SMIDI_CHANNEL_EVENT_NOTE_OFF = SMIDI_EVENT_CHANNEL | 8,
    SMIDI_CHANNEL_EVENT_NOTE_ON,
    SMIDI_CHANNEL_EVENT_NOTE_AFTERTOUCH,
    SMIDI_CHANNEL_EVENT_CONTOROLLER,
    SMIDI_CHANNEL_EVENT_PROGRAM_CHANGE,
    SMIDI_CHANNEL_EVENT_CHANNEL_AFTERTOUCH,
    SMIDI_CHANNEL_EVENT_PITCH_BEND,

    SMIDI_META_EVENT_END_OF_TRACK       = SMIDI_EVENT_META | 47,
    SMIDI_META_EVENT_SET_TEMPO          = SMIDI_EVENT_META | 81,
    SMIDI_META_EVENT_SET_TIME_SIGNATURE = SMIDI_EVENT_META | 88,
} smidiEventType;

typedef struct {
    u16 ticksPerBeat;
    u32 usPerTick;
    u32 deltaTime;

    smidiEventType event;
    u8             channel;

    union {
        u8 param1, note;
    };

    union {
        u8 param2, velocity;
    };

    u32 skip;
} smidiChannelEvent;

extern smidiChannelEvent gSmidiCurrentEvent;
extern const u8          gSmidiTestBuffer[];

#define SMIDI_TEST_BUFFER_SIZE 1131

void smidiReadHeader(u8 *pData, smidiHeader *pHdr);
void smidiReadTrackHeader(u8 *pData, smidiTrackHeader *pHdr);
void smidiParseEvent(u8 *pData, smidiChannelEvent *pOutEvent);
u32  smidiVarRead(u8 *pDat, u8 *pOutLength);
void smidiTestReadEvent(u32 pIdx);

void smidiParseData(u8 *pData, u32 pLen, void (*pCallback)(int));

#endif //SMIDI_H
