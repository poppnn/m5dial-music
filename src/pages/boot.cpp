// Écran de démarrage (aucun appareil connecté) et table des pages.
#include "pages.h"
#include "../display.h"
#include "../logo.h"

extern const PageOps musicPage, ledPage, chronoPage, clockPage, systemPage, settingsPage;
const PageOps* const PAGES[PAGE_COUNT] = {&musicPage, &ledPage, &chronoPage, &clockPage, &systemPage, &settingsPage};

// Logo centré + arc qui tourne sur le bord de l'écran + texte discret.
void drawBoot(uint32_t now) {
  const int c = 120;
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
  canvas.fillArc(c, c, 119, 114, a, a + 70, 0xFA20);
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0xAD55);
  canvas.drawString("Connexion Bluetooth...", c, 186);
  canvas.setTextColor(0x5AEB);
  canvas.drawString("M5Dial Music", c, 206);  // nom à choisir dans les réglages Bluetooth
}
