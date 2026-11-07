// État partagé entre modules.
#pragma once
#include <stdint.h>

extern volatile bool dirty;  // une nouvelle image est demandée tout de suite (écrit aussi par la tâche BLE)
extern uint32_t lastInput;   // millis() du dernier geste (molette, bouton, toucher)
