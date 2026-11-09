// Style "K10" (Réglages > Style) : bandes de la Micra K10 en fond des pages et sur l'écran de démarrage.
#pragma once
#include <stdint.h>

// Dessine le décor (repère 1000 x 300) en (ox, oy), à l'échelle k, avec les couleurs données (RGB565).
void drawK10(int ox, int oy, float k, uint16_t black, uint16_t grey);

// Fond discret des pages : bandes en gris sombre sur le noir, dans le bas de l'écran, sous le contenu.
inline void drawK10Background() { drawK10(-10, 150, 0.26f, 0x4A49, 0x2945); }
