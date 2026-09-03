#ifndef _PHYSICS_CONFIG
#define _PHYSICS_CONFIG

/**
 * Amiga physics settings (calculate.difference / Vesuri).
 *
 * Springs 276 ($0114), damping 256, Vesuri rate scaling via g_physicsStepScale.
 * Classic remake springs (320/200) are retired — getters always return Amiga values.
 */

#ifndef PHYSICS_UPDATE_HZ
#define PHYSICS_UPDATE_HZ 60
#endif

/** Reference timestep (seconds) the Classic remake integrator was tuned for (~7.14 Hz). */
#define PHYSICS_REFERENCE_STEP_SECONDS 0.14

/* Amiga calculate.difference / Vesuri applyMomentumAmplification */
#define AMIGA_PLUS_FRONT_SUSPENSION_SPRING 276 /* $0114 */
#define AMIGA_PLUS_FRONT_SUSPENSION_DAMPING 256
#define AMIGA_PLUS_REAR_SUSPENSION_SPRING 276
#define AMIGA_PLUS_REAR_SUSPENSION_DAMPING 256
#define AMIGA_PLUS_IMPACT_SOUND_COOLDOWN_BASE 5 /* Vesuri: $05 * FRAMERATE_MULTIPLIER */

/* Legacy aliases — same Amiga values (Classic 320/200 retired). */
#define CLASSIC_FRONT_SUSPENSION_SPRING AMIGA_PLUS_FRONT_SUSPENSION_SPRING
#define CLASSIC_FRONT_SUSPENSION_DAMPING AMIGA_PLUS_FRONT_SUSPENSION_DAMPING
#define CLASSIC_REAR_SUSPENSION_SPRING AMIGA_PLUS_REAR_SUSPENSION_SPRING
#define CLASSIC_REAR_SUSPENSION_DAMPING AMIGA_PLUS_REAR_SUSPENSION_DAMPING
#define CLASSIC_IMPACT_SOUND_COOLDOWN AMIGA_PLUS_IMPACT_SOUND_COOLDOWN_BASE

/* Wall contact (Amiga uses +/-8) */
#define WALL_CONTACT_IMPULSE 8
#define WALL_CONTACT_DAMPING 4

#define FRONT_SUSPENSION_SPRING AMIGA_PLUS_FRONT_SUSPENSION_SPRING
#define FRONT_SUSPENSION_DAMPING AMIGA_PLUS_FRONT_SUSPENSION_DAMPING
#define REAR_SUSPENSION_SPRING AMIGA_PLUS_REAR_SUSPENSION_SPRING
#define REAR_SUSPENSION_DAMPING AMIGA_PLUS_REAR_SUSPENSION_DAMPING

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
