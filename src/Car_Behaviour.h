
#ifndef _CAR_BEHAVIOUR
#define _CAR_BEHAVIOUR

/*    ========= */
/*    Constants */
/*    ========= */
#define CAR_WIDTH 64
#define CAR_LENGTH 128

// Wheel rotation speed constants
#define WHEEL_SPEED_LOW_THRESHOLD 0x800 // Threshold for low speed wheel calculation
#define WHEEL_SPEED_HIGH_OFFSET 0x3000  // Added to high speed wheel calculation
#define WHEEL_SPEED_MAX 0xffff          // Maximum wheel rotation speed
#define WHEEL_SPEED_MAX_CLAMPED 0xff00  // Clamped maximum wheel speed
#define WHEEL_ANGLE_MASK 0x5ffff        // 6-frame cycle (6 * 65536) for cockpit wheel animation

// new new controls for Car Behaviour, Player 1
// must not clash with other KEY definitions
#define KEY_P1_LEFT 0x00000001l
#define KEY_P1_RIGHT 0x00000002l
#define KEY_P1_ACCEL 0x00000004l
#define KEY_P1_BRAKE 0x00000008l
#define KEY_P1_BOOST 0x00000010l

#define AMIGA_PAL_HZ (3546895)

#define REDUCTION 238 // (238/256) — Amiga $EE
/* INCREASE was Amiga calculate.difference spring ($0114=276). Opponent now uses GetActive*Spring(). */
#define INCREASE 276

#include "PhysicsConfig.h"

typedef enum { OPPONENT = 0, PLAYER, NUM_CARS } CarType;

typedef struct {
    long piece;
    long distanceIntoSection;
    long roadXPosition;
    long rearWheelSurfaceXPosition;
    long playerY;
    long playerZSpeed;
    long touchingRoad;
    bool dropStartDone;
    long offMapStatus;
} CarRoadCollisionState;

/*    ===================== */
/*    Structure definitions */
/*    ===================== */

/*    ============================== */
/*    External function declarations */
/*    ============================== */
extern void ResetPlayer(void);

extern void CarBehaviourForInstance(long instanceIndex, DWORD input, long* x, long* y, long* z, long* x_angle,
                                    long* y_angle, long* z_angle, float stepSeconds);
extern long PushCarBehaviourInstance(long instanceIndex);
extern void PopCarBehaviourInstance(long previousInstance);
extern long GetActiveCarBehaviourInstance(void);
extern void SetCarRoadStateForInstance(long instanceIndex, long piece, long distanceIntoSection);
extern bool GetCarRoadCollisionStateForInstance(long instanceIndex, CarRoadCollisionState* state_out);
extern bool IsCarWreckedForInstance(long instanceIndex);
extern void ForceCarWreckForInstance(long instanceIndex);

extern void CarBehaviour(DWORD input, long* x, long* y, long* z, long* x_angle, long* y_angle, long* z_angle,
                        float stepSeconds);

extern void LimitViewpointY(long* y);
extern void LimitViewpointYForInstance(long instanceIndex, long* y);
extern void ProjectCarRenderPositionToRoadNormalForInstance(long instanceIndex, long* x, long* y, long* z);

extern long AmigaVolumeToMixerGain(long amiga_volume);

extern long CalculateDisplaySpeed(void);

extern void FramesWheelsEngine(IDirectSoundBuffer8* engineSoundBuffers[]);
extern void FramesWheelsEngineSubstep(IDirectSoundBuffer8* engineSoundBuffers[], int substeps_per_logic,
                                      float pitchScale = 1.0f);
extern void StepEngineAudioStateSubstep(int substeps_per_logic);
extern void EngineSoundStopped(void);
extern void SilenceEnginePlaybackKeepRevs(void);
extern void ResetEngineAudioState(void);
extern void PrimeEngineAudioForGameplayStart(void);

extern void CalculatePlayersRoadPosition(void);

extern void DrawOtherGraphics(void);
extern void UpdateDamage(void);
extern void ResetFourteenFrameTiming(void);
extern void AdvanceFourteenFrameTiming(void);
/** Amiga get.players.input keyboard coupling (fire→accel, brake→boost). */
extern DWORD ApplyAmigaKeyboardInputCoupling(DWORD input);
/** Advance fourteen-frame / boost-drain cadence at Amiga ~50 Hz (after car.control). */
extern void AccumulateAmigaFrameTiming(double stepSeconds);
/** Amiga init: triangular BCD sum over the track boost byte (reference/StuntCarRacer.s standard.league3). */
extern long ComputeAmigaInitialBoostReserve(long trackBoostByte);
/** Decode packed BCD boost.reserve for HUD (tens + ones nibbles). */
extern long FormatBoostReserveForHud(long bcdReserve);
/** Call once per game-logic tick to allow damage to be applied again (rate-limit damage like boost). */
extern void BeginLogicTickDamagePeriod(void);

extern void ResetLapData(long car);
extern void UpdateLapData(void);
/** Amiga-style current / best lap times for the cockpit HUD (M:SS.HH). */
extern bool FormatCurrentLapTimeForHud(wchar_t* out, size_t outChars);
extern bool FormatBestLapTimeForHud(wchar_t* out, size_t outChars);
extern bool IsBestLapTimePlayers(void);

#ifdef USE_AMIGA_RECORDING
// Following only used for testing against Amiga
extern bool GetRecordedAmigaWord(long* value_out);
extern bool GetRecordedAmigaLong(long* value_out);
extern void CompareAmigaWord(char* name, long amiga_value, long* value);
extern void CompareRecordedAmigaWord(char* name, long* value);
extern void CloseAmigaRecording(void);
#endif

#endif /* _CAR_BEHAVIOUR */
