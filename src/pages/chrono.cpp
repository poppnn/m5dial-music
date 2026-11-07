// Page Chrono : grand afficheur 7 segments, anneau = trotteuse (1 tour par minute), centièmes en dessous.
// Il continue de tourner quand on change de page.
#include "pages.h"
#include "../display.h"

static bool chronoRun = false;
static uint32_t chronoStart = 0, chronoAcc = 0;  // temps cumulé des segments terminés (ms)

static uint32_t chronoMs() { return chronoAcc + (chronoRun ? millis() - chronoStart : 0); }

static bool draw(float) {
  const int c = 120;
  uint32_t ms = chronoMs(), s = ms / 1000;
  canvas.fillArc(c, c, 119, 111, 0, 360, 0x2104);
  float sweep = (ms % 60000) / 60000.f;
  if (sweep > 0.001f) canvas.fillArc(c, c, 119, 111, -90, -90 + 360.f * sweep, chronoRun ? 0xFA20 : TFT_WHITE);
  char b[16];
  if (s >= 3600) snprintf(b, sizeof(b), "%u:%02u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
  else snprintf(b, sizeof(b), "%02u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::Font7);
  canvas.setTextSize(s >= 3600 ? 0.62f : 1.f);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(b, c, 108);
  canvas.setTextSize(1);
  snprintf(b, sizeof(b), ".%02u", (unsigned)(ms / 10 % 100));
  canvas.setFont(&fonts::efontJA_24);
  canvas.drawString(b, c, 152);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0xAD55);
  canvas.drawString(chronoRun ? "Appui : pause" : ms ? "Glisser : remise à zéro" : "Appui : départ", c, 186);
  return chronoRun;
}

static void click() {
  if (chronoRun) chronoAcc += millis() - chronoStart;
  else chronoStart = millis();
  chronoRun = !chronoRun;
}

static void swipeX(int) {  // remise à zéro
  chronoRun = false;
  chronoAcc = 0;
}

extern const PageOps chronoPage = {draw, volumeWheel, click, swipeX};
