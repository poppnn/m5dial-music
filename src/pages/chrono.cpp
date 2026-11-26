// Page Chrono (design M5Dial Fluid) : minutes et secondes en grands chiffres, centièmes dessous, arc d'une minute
// (accent pendant la course, gris à l'arrêt), puce d'état. Il continue de tourner quand on change de page.
#include "pages.h"
#include "../display.h"
#include "../settings.h"
#include "../ui.h"

static bool chronoRun = false;
static uint32_t chronoStart = 0, chronoAcc = 0;  // temps cumulé des segments terminés (ms)

static uint32_t chronoMs() { return chronoAcc + (chronoRun ? millis() - chronoStart : 0); }

static bool draw(float) {
  const int c = 120;
  uint32_t ms = chronoMs(), s = ms / 1000;
  uiValueArc((ms % 60000) / 60000.f, chronoRun ? GRAD : solid(UI_INK2), 6, ms > 0, chronoRun ? 0 : UI_INK3);
  uiText("STOPWATCH", c, 64, F_CAPTION, UI_INK3);
  char b[16];
  if (s >= 3600) snprintf(b, sizeof(b), "%u:%02u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
  else snprintf(b, sizeof(b), "%02u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
  if (s >= 3600) {  // H:MM:SS : réduit pour tenir entre les bords
    uiFont(F_DISPLAY, b);
    canvas.setTextSize(0.82f);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(uiCol(UI_INK));
    canvas.drawString(b, c, 110);
    canvas.setTextSize(1);
  } else {
    uiText(b, c, 110, F_DISPLAY, UI_INK);
  }
  snprintf(b, sizeof(b), ".%02u", (unsigned)(ms / 10 % 100));
  uiText(b, c, 148, F_FIGURE, UI_INK2);
  if (chronoRun) uiChip(188, "Running", rgb565(ACCENT_RGB[settings.accent][0]), rgb565(ACCENT_RGB[settings.accent][0]));
  else uiChip(188, ms ? "Swipe to reset" : "Press to start");
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
