#include <M5Dial.h>
#include <sys/time.h>
#include "clock.h"
#include "app.h"

// M5Unified règle déjà l'heure système depuis le RTC au démarrage ; l'heure est locale (CTS iOS = heure locale),
// on la traite comme UTC partout (pas de fuseau) : localtime() rend les mêmes champs.
static tm pending;
static volatile bool pendingSet = false;

bool clockNow(tm& t) {
  time_t now = time(nullptr);
  localtime_r(&now, &t);
  return t.tm_year + 1900 >= 2024;  // RTC jamais réglé : 2000 ou moins
}

bool clockFromCts(const uint8_t* d, size_t len) {
  if (len < 7) return false;
  tm t = {};
  t.tm_year = (d[0] | d[1] << 8) - 1900;
  t.tm_mon = d[2] - 1;
  t.tm_mday = d[3];
  t.tm_hour = d[4];
  t.tm_min = d[5];
  t.tm_sec = d[6];
  if (t.tm_year + 1900 < 2024 || t.tm_mon < 0 || t.tm_mon > 11 || !t.tm_mday || t.tm_hour > 23) return false;
  pending = t;
  pendingSet = true;
  return true;
}

void clockService() {
  if (!pendingSet) return;
  pendingSet = false;
  tm t = pending;
  timeval tv = {mktime(&t), 0};
  settimeofday(&tv, nullptr);
  if (M5.Rtc.isEnabled()) M5.Rtc.setDateTime(&t);
  dirty = true;
}
