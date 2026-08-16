#ifndef _PHYSICS_CONFIG
#define _PHYSICS_CONFIG

/**
 * Dual physics profiles for Multi Stunt Car.
 *
 * Classic  — frozen remake feel (springs 320 / damping 200).
 * Amiga+   — exact Amiga/Vesuri constants (276 / 256, Vesuri rate scaling).
 *
 * $0114 in Vesuri applyMomentumAmplification IS decimal 276 (calculate.difference).
 * Remake Amiga+ uses spring/g_physicsStepScale which equals Vesuri's
 * (276 * FRAMERATE_MULTIPLIER) when FRAMERATE_MULTIPLIER = 1/g_physicsStepScale.
 *
 * Default Amiga+ is ON after parity harness + audit sign-off; toggle with U
 * (P is pause). Override via SCR_AMIGA_PHYSICS=0/1 or SetAmigaPhysicsUpgradeEnabled.
 */

#ifndef PHYSICS_UPDATE_HZ
#define PHYSICS_UPDATE_HZ 60
#endif

/** Reference timestep (seconds) the Classic remake integrator was tuned for (~7.14 Hz). */
#define PHYSICS_REFERENCE_STEP_SECONDS 0.14

/* ----- Classic (frozen remake) ----- */
#define CLASSIC_FRONT_SUSPENSION_SPRING 320
#define CLASSIC_FRONT_SUSPENSION_DAMPING 200
#define CLASSIC_REAR_SUSPENSION_SPRING 320
#define CLASSIC_REAR_SUSPENSION_DAMPING 200
#define CLASSIC_IMPACT_SOUND_COOLDOWN 5

/* ----- Amiga+ (calculate.difference / Vesuri applyMomentumAmplification) ----- */
#define AMIGA_PLUS_FRONT_SUSPENSION_SPRING 276 /* $0114 */
#define AMIGA_PLUS_FRONT_SUSPENSION_DAMPING 256
#define AMIGA_PLUS_REAR_SUSPENSION_SPRING 276
#define AMIGA_PLUS_REAR_SUSPENSION_DAMPING 256
#define AMIGA_PLUS_IMPACT_SOUND_COOLDOWN_BASE 5 /* Vesuri: $05 * FRAMERATE_MULTIPLIER */

/* Wall contact (identical both profiles; Amiga uses +/-8) */
#define WALL_CONTACT_IMPULSE 8
#define WALL_CONTACT_DAMPING 4

/* Back-compat macros: resolve to Classic compile-time defaults for any
 * residual direct use; runtime code must call the getters below. */
#define FRONT_SUSPENSION_SPRING CLASSIC_FRONT_SUSPENSION_SPRING
#define FRONT_SUSPENSION_DAMPING CLASSIC_FRONT_SUSPENSION_DAMPING
#define REAR_SUSPENSION_SPRING CLASSIC_REAR_SUSPENSION_SPRING
#define REAR_SUSPENSION_DAMPING CLASSIC_REAR_SUSPENSION_DAMPING

#ifdef __cplusplus
extern "C" {
#endif

bool IsAmigaPhysicsUpgradeEnabled(void);
void SetAmigaPhysicsUpgradeEnabled(bool enabled);
void ToggleAmigaPhysicsUpgrade(void);
const char* GetPhysicsProfileId(void);

long GetActiveFrontSpring(void);
long GetActiveFrontDamping(void);
long GetActiveRearSpring(void);
long GetActiveRearDamping(void);

/** Vesuri-equivalent frame-rate multiplier N = round(PHYSICS_UPDATE_HZ * 0.14). */
long GetAmigaPlusFrameRateMultiplier(void);

/** Impact SFX cooldown ticks for current profile (Amiga+ scales by N). */
long GetActiveImpactSoundCooldown(void);

/** True when Amiga+ should skip remake-only delta slew clamp. */
bool AmigaPlusDisablesDeltaSlewClamp(void);

/** True when Amiga+ should use Vesuri air/ground angular damping split. */
bool AmigaPlusUsesAirGroundAngularDamping(void);

#ifdef __cplusplus
}
#endif

#endif /* _PHYSICS_CONFIG */
