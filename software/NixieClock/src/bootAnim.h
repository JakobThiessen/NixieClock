#ifndef BOOT_ANIM_H
#define BOOT_ANIM_H

#include <stdint.h>

#define ANIM_NONE       0   // kein Effekt
#define ANIM_MATRIX     1   // grüner Zeichen-Regen
#define ANIM_SLOT       2   // Nixie-Slot-Machine + Boot-Status
#define ANIM_COLORWAVE  3   // Regenbogen-Farbwellen

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Führt die gewählte Boot-Animation aus.
 * @param mode       ANIM_* Konstante
 * @param durationMs Laufzeit in Millisekunden
 */
void runBootAnimation(uint8_t mode, uint32_t durationMs);

#ifdef __cplusplus
}
#endif

#endif // BOOT_ANIM_H
