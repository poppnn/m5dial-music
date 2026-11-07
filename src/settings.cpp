#include <Preferences.h>
#include "settings.h"
#include "config.h"
#include "clock.h"

Settings settings = {IMU_DIR, ENC_STEPS_PER_DETENT, DIM_AFTER_MS / 1000, SLEEP_AFTER_MS / 60000, 50, 15, 0, 21, 7, false};
static Preferences prefs;

void settingsLoad() {
  prefs.begin("m5dial", false);  // reste ouvert : écritures rares (validation d'un réglage, connexion)
  settings.imuDir = prefs.getChar("imuDir", settings.imuDir) < 0 ? -1 : 1;
  settings.encSteps = constrain(prefs.getUChar("encSteps", settings.encSteps), 1, 4);
  settings.dimAfterS = prefs.getUShort("dimS", settings.dimAfterS);
  settings.sleepAfterMin = prefs.getUShort("sleepM", settings.sleepAfterMin);
  settings.briDay = constrain(prefs.getUChar("briD", settings.briDay), 5, 100);
  settings.briNight = constrain(prefs.getUChar("briN", settings.briNight), 5, 100);
  settings.briMode = min<uint8_t>(prefs.getUChar("briMode", settings.briMode), 2);
  settings.nightFrom = prefs.getUChar("nightFrom", settings.nightFrom) % 24;
  settings.nightTo = prefs.getUChar("nightTo", settings.nightTo) % 24;
  settings.notifText = prefs.getBool("notifTxt", settings.notifText);
}

void settingsSave() {
  prefs.putChar("imuDir", settings.imuDir);
  prefs.putUChar("encSteps", settings.encSteps);
  prefs.putUShort("dimS", settings.dimAfterS);
  prefs.putUShort("sleepM", settings.sleepAfterMin);
  prefs.putUChar("briD", settings.briDay);
  prefs.putUChar("briN", settings.briNight);
  prefs.putUChar("briMode", settings.briMode);
  prefs.putUChar("nightFrom", settings.nightFrom);
  prefs.putUChar("nightTo", settings.nightTo);
  prefs.putBool("notifTxt", settings.notifText);
}

// Plage de nuit qui peut passer minuit (21 h -> 7 h) ou non (1 h -> 5 h).
static bool inNight(int h, int from, int to) { return from > to ? h >= from || h < to : h >= from && h < to; }

bool settingsNight() {
  if (settings.briMode != 2) return settings.briMode == 1;
  tm t;
  return clockNow(t) && inNight(t.tm_hour, settings.nightFrom, settings.nightTo);  // heure inconnue : jour
}

uint8_t settingsBacklight() { return (settingsNight() ? settings.briNight : settings.briDay) * 255 / 100; }

void settingsSelfTest() {
  assert(inNight(23, 21, 7) && inNight(3, 21, 7) && !inNight(12, 21, 7) && !inNight(7, 21, 7));
  assert(inNight(2, 1, 5) && !inNight(6, 1, 5));
}

LedState ledStateLoad() {
  return {prefs.getUChar("ledCol", 0), prefs.getUShort("ledHue", 0), prefs.getBool("ledCus", false),
          prefs.getUChar("ledBri", 100)};
}

void ledStateSave(const LedState& s) {
  prefs.putUChar("ledCol", s.color);
  prefs.putUShort("ledHue", s.hue);
  prefs.putBool("ledCus", s.custom);
  prefs.putUChar("ledBri", s.bri);
}

// Clé NVS (15 caractères max) : "n" + adresse sans les ":" = 13 caractères.
static String bondKey(const std::string& addr) {
  String k = "n";
  for (char c : addr) if (c != ':') k += c;
  return k;
}

void bondNameSave(const std::string& addr, const std::string& name) {
  if (!name.empty() && bondName(addr) != name) prefs.putString(bondKey(addr).c_str(), name.c_str());
}

std::string bondName(const std::string& addr) {
  String k = bondKey(addr);
  return prefs.isKey(k.c_str()) ? prefs.getString(k.c_str()).c_str() : "";
}
