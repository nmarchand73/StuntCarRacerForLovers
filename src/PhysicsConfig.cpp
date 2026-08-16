#include "PhysicsConfig.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#ifndef SCR_AMIGA_PHYSICS_DEFAULT_ON
/* Parity harness PASS + audit complete — default Amiga+ ON; Classic via P or SCR_AMIGA_PHYSICS=0. */
#define SCR_AMIGA_PHYSICS_DEFAULT_ON 1
#endif

static bool g_amigaPhysicsUpgradeEnabled = (SCR_AMIGA_PHYSICS_DEFAULT_ON != 0);

static void InitDefaultFromEnv(void) {
    static bool done = false;
    if (done)
        return;
    done = true;
    const char* env = std::getenv("SCR_AMIGA_PHYSICS");
    if (!env)
        return;
    if (env[0] == '1' && env[1] == '\0')
        g_amigaPhysicsUpgradeEnabled = true;
    else if (env[0] == '0' && env[1] == '\0')
        g_amigaPhysicsUpgradeEnabled = false;
}

bool IsAmigaPhysicsUpgradeEnabled(void) {
    InitDefaultFromEnv();
    return g_amigaPhysicsUpgradeEnabled;
}

void SetAmigaPhysicsUpgradeEnabled(bool enabled) {
    InitDefaultFromEnv();
    g_amigaPhysicsUpgradeEnabled = enabled;
    std::printf("Physics profile: %s\n", GetPhysicsProfileId());
}

void ToggleAmigaPhysicsUpgrade(void) {
    SetAmigaPhysicsUpgradeEnabled(!IsAmigaPhysicsUpgradeEnabled());
}

const char* GetPhysicsProfileId(void) {
    return IsAmigaPhysicsUpgradeEnabled() ? "amiga-plus-v1" : "classic";
}

long GetActiveFrontSpring(void) {
    return IsAmigaPhysicsUpgradeEnabled() ? AMIGA_PLUS_FRONT_SUSPENSION_SPRING
                                          : CLASSIC_FRONT_SUSPENSION_SPRING;
}

long GetActiveFrontDamping(void) {
    return IsAmigaPhysicsUpgradeEnabled() ? AMIGA_PLUS_FRONT_SUSPENSION_DAMPING
                                          : CLASSIC_FRONT_SUSPENSION_DAMPING;
}

long GetActiveRearSpring(void) {
    return IsAmigaPhysicsUpgradeEnabled() ? AMIGA_PLUS_REAR_SUSPENSION_SPRING
                                         : CLASSIC_REAR_SUSPENSION_SPRING;
}

long GetActiveRearDamping(void) {
    return IsAmigaPhysicsUpgradeEnabled() ? AMIGA_PLUS_REAR_SUSPENSION_DAMPING
                                          : CLASSIC_REAR_SUSPENSION_DAMPING;
}

long GetAmigaPlusFrameRateMultiplier(void) {
    const double n = static_cast<double>(PHYSICS_UPDATE_HZ) * PHYSICS_REFERENCE_STEP_SECONDS;
    long rounded = static_cast<long>(std::floor(n + 0.5));
    if (rounded < 1)
        rounded = 1;
    return rounded;
}

long GetActiveImpactSoundCooldown(void) {
    if (IsAmigaPhysicsUpgradeEnabled())
        return AMIGA_PLUS_IMPACT_SOUND_COOLDOWN_BASE * GetAmigaPlusFrameRateMultiplier();
    return CLASSIC_IMPACT_SOUND_COOLDOWN;
}

bool AmigaPlusDisablesDeltaSlewClamp(void) {
    return IsAmigaPhysicsUpgradeEnabled();
}

bool AmigaPlusUsesAirGroundAngularDamping(void) {
    return IsAmigaPhysicsUpgradeEnabled();
}
