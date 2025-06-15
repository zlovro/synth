//
// Created by Made on 29/01/2025.
//

#ifndef SOLFEGE_H
#define SOLFEGE_H

#include <serrno.h>
#include <string.h>
#include <math.h>
#include <types.h>

typedef pstruct
{
    f32 frequency;
    u16 semitoneOffset;
    u8  padding[2];
} tone;

#define TONE_BASE_FREQ 16.3515978313F // C0

#define toneAddSemitone(freq, offset) (freq * pow(2, offset / 12.0))
#define toneOffset(t, semitones) (tone) {.frequency = (f32)toneAddSemitone(t.frequency, semitones), .semitoneOffset = (u16)(t.semitoneOffset + semitones)}
#define toneFromAbsoluteSemitoneOffset(offset) (tone) {.frequency = (f32)toneAddSemitone(TONE_BASE_FREQ, (offset)), .semitoneOffset = (u16)(offset)}

static_assert(sizeof(tone) == 8);

#define charIsNote(c) ((c == 'h' || c == 'H') || (c >= 'a' && c <= 'g') || (c >= 'A' && c <= 'G'))
#define charIsSharp(c) (c == '#')
#define charIsFlat(c) (c == 'b')

#define TUNING 440
#define TONE_OFFSET_C0 (12 * 0)
#define TONE_OFFSET_C1 (12 * 1)
#define TONE_OFFSET_C2 (12 * 2)
#define TONE_OFFSET_C3 (12 * 3)
#define TONE_OFFSET_C4 (12 * 4)
#define TONE_OFFSET_C5 (12 * 5)
#define TONE_OFFSET_C6 (12 * 6)
#define TONE_OFFSET_C7 (12 * 7)

#define TONE_OFFSET_B4 (TONE_OFFSET_C5 - 1)

extern tone TONE_A3;
extern tone TONE_A4;
extern tone TONE_A5;
extern tone TONE_A6;
extern tone TONE_A7;
extern tone TONE_A8;

extern u8   gNoteNameToOffsetTable[];
extern char gOffsetToNoteNameTable[];

void       solfegeInit();
synthErrno solfegeParseNote(str pString, tone* pOutTone);
synthErrno solfegeToneToStr(str pString, tone* pInTone, bool pPreferFlats);
synthErrno solfegeToneWithVelocityToStr(str pString, tone* pInTone, u8 pVelocity, bool pPreferFlats);
bool         solfegeToneIsNatural(u8 pSemitoneOffset);

#endif //SOLFEGE_H
