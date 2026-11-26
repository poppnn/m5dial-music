// Page Horloge (design M5Dial Fluid) : heure du téléphone (gardée par le RTC du M5Dial, même déconnecté) en grands
// chiffres, date en capitales espacées, 60 points autour du cadran qui s'allument au fil de la minute (accent le jour,
// gris la nuit), bouton blanc sur la seconde courante.
#include "pages.h"
#include "../ble_phone.h"
#include "../clock.h"
#include "../display.h"
#include "../settings.h"
#include "../ui.h"

static const char* DAYS[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
static const char* MONTHS[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

static bool draw(float) {
  const int c = 120;
  tm t;
  if (!clockNow(t)) {
    uiRingStroke(120, 120, 112, 6, UI_SURFACE2);
    uiText("CLOCK", c, 96, F_CAPTION, UI_INK3);
    uiText("Time unknown", c, 122, F_TITLE, UI_INK);
    uiText("Connect your phone", c, 146, F_BODY, UI_INK3);
    return false;
  }
  bool night = settingsNight();
  for (int i = 0; i < 60; i++) {
    float a = (-90 + i * 6) * DEG_TO_RAD, x = 120 + 112 * cosf(a), y = 120 + 112 * sinf(a);
    bool lit = i <= t.tm_sec;
    Paint p = !lit ? solid(UI_INK4) : night ? solid(UI_INK3) : GRAD;
    uiDisc(x, y, i % 5 ? 1.6f : 2.4f, p);
  }
  float a = (-90 + t.tm_sec * 6) * DEG_TO_RAD, kx = 120 + 112 * cosf(a), ky = 120 + 112 * sinf(a);
  if (!night) uiDisc(kx, ky, 10, solid(rgb565(ACCENT_RGB[settings.accent][1])), 0.35f);
  uiDisc(kx, ky, 4.5f, solid(UI_INK));
  char b[32];
  snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
  uiText(b, c, 112, F_DISPLAY_XL, UI_INK);
  snprintf(b, sizeof(b), "%s  %d  %s", DAYS[t.tm_wday], t.tm_mday, MONTHS[t.tm_mon]);
  uiText(b, c, 158, F_CAPTION, UI_INK2);
  return false;  // 4 images/s au repos suffisent pour les secondes
}

static void click() { sendCmd(CMD_TOGGLE); }
static void swipeX(int) {}

extern const PageOps clockPage = {draw, volumeWheel, click, swipeX, nullptr};
