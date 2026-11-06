// Logo de l'écran de démarrage, RGB565 en flash (const => .rodata, ne prend pas de RAM).
// Généré par : python tools/png2logo.py mon_logo.png
// Vide (LOGO_W = 0) : l'écran de démarrage s'affiche sans logo.
#pragma once
#include <stdint.h>
#include <pgmspace.h>

static const uint16_t LOGO_W = 0, LOGO_H = 0;
static const uint16_t logo_data[] PROGMEM = {0};
