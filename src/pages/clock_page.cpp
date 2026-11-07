// Page Horloge : heure de l'iPhone (gardée par le RTC du M5Dial, même déconnecté), anneau des secondes, date.
#include "pages.h"
#include "../ble_phone.h"
#include "../clock.h"
#include "../display.h"
#include "../settings.h"

static const char* DAYS[7] = {"dimanche", "lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi"};
static const char* MONTHS[12] = {"janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août",
                                 "septembre", "octobre", "novembre", "décembre"};

static bool draw(float) {
  const int c = 120;
  tm t;
  bool known = clockNow(t);
  canvas.fillArc(c, c, 119, 111, 0, 360, 0x2104);
  canvas.setTextDatum(middle_center);
  if (!known) {
    canvas.setFont(&fonts::efontJA_24);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("Heure inconnue", c, 108);
    canvas.setFont(&fonts::efontJA_16);
    canvas.setTextColor(0xAD55);
    canvas.drawString("Connecte l'iPhone", c, 140);
    return false;
  }
  if (t.tm_sec) canvas.fillArc(c, c, 119, 111, -90, -90 + 6.f * t.tm_sec, settingsNight() ? 0x8410 : 0xFA20);
  char b[32];
  snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
  canvas.setFont(&fonts::Font7);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(b, c, 108);
  snprintf(b, sizeof(b), "%s %d %s", DAYS[t.tm_wday], t.tm_mday, MONTHS[t.tm_mon]);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0xAD55);
  canvas.drawString(b, c, 160);
  return false;  // 4 images/s au repos suffisent pour l'anneau des secondes
}

static void click() { sendCmd(CMD_TOGGLE); }
static void swipeX(int) {}

extern const PageOps clockPage = {draw, volumeWheel, click, swipeX, nullptr};
