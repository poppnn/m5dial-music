// Rétroéclairage (fondu démarrage <-> interface, atténuation sans geste) et extinction.
#pragma once
#include <stdint.h>

extern bool bootShown;      // écran de démarrage affiché (aucun appareil connecté)
extern uint32_t fadeStart;  // 0 = pas de fondu en cours
extern uint32_t linkedAt;   // instant de la connexion pendant l'écran de démarrage (animation de validation), 0 sinon

void updateFade(uint32_t now);
bool updateBacklight(uint32_t now, float dt);  // true pendant une transition (-> FPS élevé)
void shutdown(const char* msg);
