// État partagé entre modules.
#pragma once
#include <stdint.h>

extern volatile bool dirty;  // une nouvelle image est demandée tout de suite (écrit aussi par la tâche BLE)
extern uint32_t lastInput;   // millis() du dernier geste (molette, bouton, toucher)
void haptic(int dir);        // bip du buzzer (Réglages > Click) : dir < 0 / > 0 = cran de molette, 0 = appui
