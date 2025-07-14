//
// Created by Made on 19/05/2025.
//

#ifndef SSYS_H
#define SSYS_H

#include <types.h>
#include <serrno.h>
#include <sfs.h>
#include <stm32h7xx_hal.h>
#include <solfege/solfege.h>
#include <sser/sser.h>

#define SYS_GUI_OUTLINE_THICKNESS 1
#define SYS_GUI_OUTLINE_MARGIN 4
#define SYS_GUI_ORIGIN (SYS_GUI_OUTLINE_THICKNESS + SYS_GUI_OUTLINE_MARGIN)
#define SYS_GUI_DOT_SIZE 3
#define SYS_GUI_DOT_PADDING 1
#define SYS_GUI_DOT_STRIDE SYS_GUI_DOT_SIZE + SYS_GUI_DOT_PADDING
#define SYS_GUI_DEBUG_LOOP_PANE_X 68

#define sysGlcdSeekOrigin() glcdSeek(SYS_GUI_ORIGIN, SYS_GUI_ORIGIN); glcdSetOrigin(SYS_GUI_ORIGIN, SYS_GUI_ORIGIN)

#define SYS_FONT_HEIGHT 8
#define SYS_FONT_SPACING_X 1
#define SYS_FONT_SPACING_Y 1
#define SYS_LINE_HEIGHT (SYS_FONT_HEIGHT + SYS_FONT_SPACING_Y)

#define SYS_MAX_AVAILABLE_RAM (512 * 1024)
#define SYS_TRACK_COUNT 8
#define SYS_TRACK_BUFFER_SIZE (BLOCK_SIZE * 2)
#define SYS_TRACK_BUFFER_SAMPLE_COUNT (SYS_TRACK_BUFFER_SIZE / 2)
#define SYS_POLYPHONY_COUNT (SYS_MAX_AVAILABLE_RAM / SYS_TRACK_BUFFER_SIZE)

#define SYS_AUDIO_BUFFER_SIZE BLOCK_SIZE
#define SYS_AUDIO_BUFFER_SAMPLE_COUNT (BLOCK_SIZE / 2)

#define SYS_DAC_BUFFER_SIZE (SYS_AUDIO_BUFFER_SIZE * 2)
#define SYS_DAC_SAMPLE_COUNT (SYS_DAC_BUFFER_SIZE / sizeof(u16))
#define SYS_DAC_HALF_BUFFER_SIZE (SYS_DAC_BUFFER_SIZE / 2)
#define SYS_DAC_HALF_SAMPLE_COUNT (SYS_DAC_SAMPLE_COUNT / 2)

#define SYS_POLL_RATE 187
#define SYS_POLL_PERIOD_TICKS (1000 / SYS_POLL_RATE)
#define SYS_POLL_PERIOD_US (1e6 / SYS_POLL_RATE)

enum {
    SYS_BTNMTX1_BUTTON_0_MASK = 1 << 0,
    SYS_BTNMTX1_BUTTON_1_MASK = 1 << 1,
    SYS_BTNMTX1_BUTTON_2_MASK = 1 << 2,
    SYS_BTNMTX1_BUTTON_3_MASK = 1 << 3,
    SYS_BTNMTX1_BUTTON_4_MASK = 1 << 4,
    SYS_BTNMTX1_BUTTON_5_MASK = 1 << 5,
    SYS_BTNMTX1_BUTTON_6_MASK = 1 << 6,
    SYS_BTNMTX1_BUTTON_7_MASK = 1 << 7,
    SYS_BTNMTX1_BUTTON_8_MASK = 1 << 8,
    SYS_BTNMTX1_BUTTON_9_MASK = 1 << 9,
    SYS_BTNMTX1_BUTTON_A_MASK = 1 << 10,
    SYS_BTNMTX1_BUTTON_B_MASK = 1 << 11,
    SYS_BTNMTX1_BUTTON_C_MASK = 1 << 12,
    SYS_BTNMTX1_BUTTON_D_MASK = 1 << 13,
    SYS_BTNMTX1_BUTTON_E_MASK = 1 << 14,
    SYS_BTNMTX1_BUTTON_F_MASK = 1 << 15,
};

typedef enum : u8 {
    SYS_BTNSTATE_NULL,
    SYS_BTNSTATE_DOWN, // just pressed
    SYS_BTNSTATE_HELD,
    SYS_BTNSTATE_UP, // just released
} sysButtonState;

typedef enum : u8 {
    SYS_MENU_ERROR,
    SYS_MENU_LOADING,
    SYS_MENU_LOADING_DONE,
    SYS_MENU_DEFAULT,
    SYS_MENU_DEBUG,
    SYS_MENU_INVALID = 0xFF,
} sysMenuId;

typedef struct {
    sfsSingleInstrument base;
    u32                 loadedBlock;
    u16                 instrumentId;
    char                cachedName[64];
} sysSingleInstrumentRuntime;

typedef struct {
    sysButtonState state;
    u8             velocity;
} sysKeyData;

typedef struct {
    sysKeyData                 keys[SFS_KEY_COUNT];
    sysSingleInstrumentRuntime instrument;
    u32                        midiUs;
    u16                        lastInstrumentId;
    u32                        currentBlock;
    bool                       isLoaded;
    bool                       isActive;
} sysTrack;

typedef struct {
    sfsInstrumentSample *sample;
    u32                  blockProgress;
    u16                  loopProgressSamples;

    bool play    : 1;
    bool inLoop  : 1;
    bool loopOver: 1;
} sysPolyphony;

typedef struct {
    u32 currentState;
    u32 lastState;
} sysInputBitmap;

typedef struct {
    s32 timeStampPressedFirst, timeStampPressedSecond;
} sysKeyTimestamp;

typedef struct {
    u8 pitchBendRangeSemitones;
} sysSettings;

typedef struct {
    u32  fallingEdgeTimestampA;
    u32  fallingEdgeTimestampB;
    bool outA    : 1;
    bool lastOutA: 1;
    bool outB    : 1;
    bool lastOutB: 1;
    bool sw      : 1;
    bool lastSw  : 1;
} sysRotaryEncoder;

typedef enum : u8 {
    SYS_DEBUG_SUBMENU_INSTRUMENT_SELECT,
    SYS_DEBUG_SUBMENU_LOOP_CHANGE,
    SYS_DEBUG_SUBMENU_MAX,
} sysDebugSubmenu;

typedef enum : u8 {
    SYS_DEBUG_LOOP_BUTTON_NOTE_CHANGE,
    SYS_DEBUG_LOOP_BUTTON_VELOCITY_CHANGE,
    SYS_DEBUG_LOOP_BUTTON_START,
    SYS_DEBUG_LOOP_BUTTON_DURATION,
    SYS_DEBUG_LOOP_BUTTON_WRITE,
    SYS_DEBUG_LOOP_BUTTON_MAX,
} sysDebugLoopButton;

typedef struct {
    sysDebugSubmenu     submenu;
    sysDebugLoopButton  loopButton;
    u8                  noteSelected;
    u8                  velocity;
    sfsInstrumentSample sample;
    bool                reloadSample;
    bool                lastChangeMenuButtonState;
    bool                changeMenuButtonState;
} sysDebugData;

extern sysSettings          gSysSettings;
extern bool                 gSysIsLoaded;
extern u32                  gSysTickCounter;
extern f32                  gSysDeltaTime;
extern u8                   gSysCurrentMenuId;
extern u8                   gSysLastMenuId;
extern f32                  gSysCurrentMenuTimer;
extern DMA_BUFFER u16       gSysDacBuf[];
extern s16                  gSysAudioFrontBuf[];
extern s16                  gSysAudioBackBuf[];
extern sfsKeyProximityTable gSysProximityTable;
extern sysTrack *           gSysTrackCurrent;
extern sysTrack             gSysTrackInfo[];
extern s16                  gSysPolyphonyData[SYS_TRACK_COUNT][SFS_KEY_COUNT][SYS_AUDIO_BUFFER_SAMPLE_COUNT];
extern sysPolyphony         gSysPolyphonyInfo[SYS_TRACK_COUNT][SFS_KEY_COUNT];
extern u32                  gSysDmaProgress;
extern u32                  gSysDataLoadCounter;
extern char                 gSysStrError[];
extern sysRotaryEncoder     gSysRotaryEncoder;
extern sysDebugData         gSysDebugData;

extern sysInputBitmap  gSysBtnMtx1State;
extern sysInputBitmap  gSysBtnKeyStates[SFS_KEYS_WORD_COUNT];
extern sysKeyTimestamp gSysKeyTimestamps[SFS_KEY_SEMITONE_RANGE];

extern ADC_HandleTypeDef *gSysAdc;

#define sysBtnMtx1GetState(b) sysGetButtonState(&gSysBtnMtx1State, b)

#define sysGetTrackData(i) ((u16*)(gSysTrackData + i))

synthErrno     sysInit(DAC_HandleTypeDef *pDac, ADC_HandleTypeDef *pAdc);
void           sysPoll();
void           sysUpdateTrackData();
void           sysRender();
sysButtonState sysGetButtonState(sysInputBitmap *pMap, u32 pBtn);
void           sysHandleInputs();
void           sysReadInputs();
void           sysHandlePitchBend();
void           sysSynthesizeAudio();
synthErrno     sysDeinit();
void           sysNoteOn(u8 pTrack, u16 pNoteSemitones, u8 pVelocity);
void           sysNoteOff(u8 pTrack, u16 pNoteSemitones, u8 pVelocity);
void           sysError(synthErrno pCode);

#define sysGetMainTrack() gSysTrackInfo
#define sysPrintf(fmt, ...) printf("ssys: " fmt, ##__VA_ARGS__)


#endif //SSYS_H
