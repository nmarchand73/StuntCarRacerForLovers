#include "PhysicsConfig.h"

#include <cmath>
#include <cstdio>

bool IsAmigaPhysicsUpgradeEnabled(void) {
    return true;
}

void SetAmigaPhysicsUpgradeEnabled(bool /*enabled*/) {
    /* Amiga settings are locked on; Classic remake springs retired. */
    std::printf("Physics profile: %s\n", GetPhysicsProfileId());
}

void ToggleAmigaPhysicsUpgrade(void) {
    SetAmigaPhysicsUpgradeEnabled(true);
}

const char* GetPhysicsProfileId(void) {
    return "amiga";
}

long GetActiveFrontSpring(void) {
    return AMIGA_PLUS_FRONT_SUSPENSION_SPRING;
}

long GetActiveFrontDamping(void) {
    return AMIGA_PLUS_FRONT_SUSPENSION_DAMPING;
}

long GetActiveRearSpring(void) {
    return AMIGA_PLUS_REAR_SUSPENSION_SPRING;
}

long GetActiveRearDamping(void) {
    return AMIGA_PLUS_REAR_SUSPENSION_DAMPING;
}

long GetAmigaPlusFrameRateMultiplier(void) {
    const double n = static_cast<double>(PHYSICS_UPDATE_HZ) * PHYSICS_REFERENCE_STEP_SECONDS;
    long rounded = static_cast<long>(std::floor(n + 0.5));
    if (rounded < 1)
        rounded = 1;
    return rounded;
}

long GetActiveImpactSoundCooldown(void) {
    return AMIGA_PLUS_IMPACT_SOUND_COOLDOWN_BASE * GetAmigaPlusFrameRateMultiplier();
}

bool AmigaPlusDisablesDeltaSlewClamp(void) {
    return true;
}

bool AmigaPlusUsesAirGroundAngularDamping(void) {
    return true;
}
