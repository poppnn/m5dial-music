#include <Preferences.h>
#include "settings.h"
#include "config.h"

Settings settings = {IMU_DIR, ENC_STEPS_PER_DETENT, DIM_AFTER_MS / 1000, SLEEP_AFTER_MS / 60000, 50, 15, false};
static Preferences prefs;

void settingsLoad() {
  prefs.begin("m5dial", false);  // reste ouvert : écritures rares (validation d'un réglage, connexion)
  settings.imuDir = prefs.getChar("imuDir", settings.imuDir) < 0 ? -1 : 1;
  settings.encSteps = constrain(prefs.getUChar("encSteps", settings.encSteps), 1, 4);
  settings.dimAfterS = prefs.getUShort("dimS", settings.dimAfterS);
  settings.sleepAfterMin = prefs.getUShort("sleepM", settings.sleepAfterMin);
  settings.briDay = constrain(prefs.getUChar("briD", settings.briDay), 5, 100);
  settings.briNight = constrain(prefs.getUChar("briN", settings.briNight), 5, 100);
  settings.night = prefs.getBool("night", settings.night);
}

void settingsSave() {
  prefs.putChar("imuDir", settings.imuDir);
  prefs.putUChar("encSteps", settings.encSteps);
  prefs.putUShort("dimS", settings.dimAfterS);
  prefs.putUShort("sleepM", settings.sleepAfterMin);
  prefs.putUChar("briD", settings.briDay);
  prefs.putUChar("briN", settings.briNight);
  prefs.putBool("night", settings.night);
}

uint8_t settingsBacklight() { return (settings.night ? settings.briNight : settings.briDay) * 255 / 100; }

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
