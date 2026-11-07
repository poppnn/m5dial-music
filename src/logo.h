// Logo de l'écran de démarrage. LOGO_W = 0 : logo neutre dessiné par le firmware (double croche).
// Pour un logo personnalisé : python tools/png2logo.py mon_logo.png (remplace ce fichier).
#pragma once
#include <stdint.h>
#include <pgmspace.h>

static const uint16_t LOGO_W = 0, LOGO_H = 0;
static const uint16_t logo_data[1] PROGMEM = {0};
