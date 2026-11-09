// Écran de démarrage (aucun appareil connecté) et table des pages.
#include <algorithm>
#include "pages.h"
#include "../anim.h"
#include "../config.h"
#include "../display.h"
#include "../power.h"
#include "../settings.h"
#include "../logo.h"

extern const PageOps musicPage, ledPage, chronoPage, clockPage, systemPage, settingsPage;
const PageOps* const PAGES[PAGE_COUNT] = {&musicPage, &ledPage, &chronoPage, &clockPage, &systemPage, &settingsPage};

// Téléphone connecté : l'arc qui tourne se resserre vers le centre en se refermant en cercle, puis éclate en une
// pastille de validation (coche) avec une onde qui part vers le bord. Ensuite, fondu vers l'interface.
static void drawLinked(uint32_t now, uint32_t t) {
  const int c = 120;
  float a = (now % 1200) * (360.f / 1200) - 180;  // même rotation que l'arc d'attente : pas de saut
  if (t < LINK_IN_MS) {
    float e = easeInCubic(t / (float)LINK_IN_MS);
    float r = 116 - 84 * e, w = 3 + 2 * e;  // rayon 116 -> 32, arc 70° -> cercle complet
    canvas.fillArc(c, c, r + w, r - w, a, a + 70 + 290 * e, accent());
    return;
  }
  float q = std::min(1.f, (t - LINK_IN_MS) / (float)(LINK_ANIM_MS - LINK_IN_MS));
  float wave = easeOutCubic(q);  // onde blanche qui part vers le bord en s'éteignant
  if (q < 1) {
    uint8_t g = 255 * (1 - q);
    canvas.fillArc(c, c, 36 + 84 * wave, 33 + 84 * wave - 2 * (1 - q), 0, 360, canvas.color565(g, g, g));
  }
  float s = easeOutBack(std::min(1.f, q / 0.6f));  // pastille qui gonfle avec un léger rebond
  canvas.fillCircle(c, c, 34 * s, accent());
  if (q > 0.2f) {  // coche blanche
    float k = std::min(1.f, (q - 0.2f) / 0.3f) * s;
    canvas.drawWideLine(c - 14 * k, c + 1 * k, c - 4 * k, c + 11 * k, 3.5f * k, TFT_WHITE);
    canvas.drawWideLine(c - 4 * k, c + 11 * k, c + 15 * k, c - 10 * k, 3.5f * k, TFT_WHITE);
  }
}

// Logo centré + arc qui tourne sur le bord de l'écran + texte discret.
void drawBoot(uint32_t now) {
  const int c = 120;
  if (linkedAt) { drawLinked(now, now - linkedAt); return; }
  if (LOGO_W) {
    canvas.pushImage(c - LOGO_W / 2, 100 - LOGO_H / 2, LOGO_W, LOGO_H, logo_data);
  } else {  // logo neutre par défaut (aucun logo.h personnalisé) : double croche, sans bitmap en flash
    canvas.fillEllipse(97, 128, 13, 10, TFT_WHITE);
    canvas.fillEllipse(147, 118, 13, 10, TFT_WHITE);
    canvas.fillRect(104, 68, 6, 60, TFT_WHITE);
    canvas.fillRect(154, 58, 6, 60, TFT_WHITE);
    canvas.fillTriangle(104, 68, 159, 58, 159, 72, TFT_WHITE);  // barre de liaison inclinée
    canvas.fillTriangle(104, 68, 159, 72, 104, 82, TFT_WHITE);
  }
  float a = (now % 1200) * (360.f / 1200) - 180;  // 1 tour en 1,2 s
  canvas.fillArc(c, c, 119, 114, 0, 360, 0x2104);
  canvas.fillArc(c, c, 119, 114, a, a + 70, accent());
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0xAD55);
  canvas.drawString("Waiting for phone", c, 186);
  canvas.setTextColor(0x5AEB);
  canvas.drawString("M5Dial Music", c, 206);  // nom à choisir dans les réglages Bluetooth
}
