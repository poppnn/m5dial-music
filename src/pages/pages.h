// Pages de l'interface : chacune dessine dans `canvas` et réagit à la molette, à l'appui et au glissement
// horizontal. Le glissement vertical (changement de page) est géré par main.cpp.
#pragma once
#include <stdint.h>
#include <string>

enum Page : uint8_t { PAGE_MUSIC, PAGE_LED, PAGE_CHRONO, PAGE_CLOCK, PAGE_SYS, PAGE_SET, PAGE_COUNT };

struct PageOps {
  bool (*draw)(float dt);      // true tant qu'une animation propre à la page est en cours
  void (*wheel)(int detents);  // crans de molette (signés)
  void (*click)();             // appui molette ou tap
  void (*swipeX)(int dir);     // +1 vers la droite, -1 vers la gauche
  // Tap écran à la position (x, y) de l'interface, n-ième tap rapproché. nullptr : comme click().
  void (*tap)(int x, int y, uint8_t clicks);
};
extern const PageOps* const PAGES[PAGE_COUNT];

void drawBoot(uint32_t now);                // écran de démarrage (aucun appareil connecté)
void musicTick(float dt, uint32_t now);     // à chaque image, quelle que soit la page (arc, titres)
void musicVolumePing();                     // le volume vient de changer : anneau de volume visible
void volumeWheel(int detents);              // molette = volume (pages Musique, Chrono, Système)

// Écrans iPhone (notify.cpp) : appel entrant plein écran, notification en surimpression.
bool drawCall(uint32_t now, const std::string& caller);
bool drawToast(uint32_t now);
bool toastShown(uint32_t now);

extern uint16_t fpsShown;  // page Système : images/s et temps de rendu, mesurés par main.cpp
extern float frameMsShown;
