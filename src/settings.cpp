#include <Preferences.h>
#include "settings.h"
#include "config.h"
#include "clock.h"

Settings settings = {ENC_STEPS_PER_DETENT, DIM_AFTER_MS / 1000, SLEEP_AFTER_MS / 60000, 50, 15, 0, 21, 7,
                     NOTIF_SENDER, 0, 1, {PAGE_MUSIC, PAGE_LED, PAGE_CHRONO, PAGE_CLOCK, PAGE_SYS, PAGE_SET}};
const uint16_t ACCENT_COLORS[] = {0xFA20, 0xF8C3, 0xF951, 0x901F, 0x02DF, 0x06FF, 0x072B, 0xFE80, 0xFFFF};
const char* const ACCENT_NAMES[] = {"orange", "red", "pink", "purple", "blue", "cyan", "green", "yellow", "white"};
const int ACCENT_COUNT = sizeof(ACCENT_COLORS) / sizeof(ACCENT_COLORS[0]);

uint16_t accent() { return ACCENT_COLORS[settings.accent]; }
static Preferences prefs;

void settingsLoad() {
  prefs.begin("m5dial", false);  // reste ouvert : écritures rares (validation d'un réglage, connexion)
  settings.encSteps = constrain(prefs.getUChar("encSteps", settings.encSteps), 1, 4);
  settings.dimAfterS = prefs.getUShort("dimS", settings.dimAfterS);
  settings.sleepAfterMin = prefs.getUShort("sleepM", settings.sleepAfterMin);
  settings.briDay = constrain(prefs.getUChar("briD", settings.briDay), 5, 100);
  settings.briNight = constrain(prefs.getUChar("briN", settings.briNight), 5, 100);
  settings.briMode = min<uint8_t>(prefs.getUChar("briMode", settings.briMode), 2);
  settings.nightFrom = prefs.getUChar("nightFrom", settings.nightFrom) % 24;
  settings.nightTo = prefs.getUChar("nightTo", settings.nightTo) % 24;
  // Ancien réglage booléen "notifTxt" (texte affiché ou non) : repris comme app + expéditeur / tout
  settings.notifMode = prefs.isKey("notifMode") ? min<uint8_t>(prefs.getUChar("notifMode"), NOTIF_FULL)
                       : prefs.getBool("notifTxt", false) ? NOTIF_FULL : NOTIF_SENDER;
  settings.accent = prefs.getUChar("accent", 0) % ACCENT_COUNT;
  settings.haptic = prefs.getUChar("haptic", settings.haptic) ? 1 : 0;
  uint8_t order[PAGE_COUNT];
  // Absent, ou d'un firmware qui avait d'autres pages (taille ou contenu différents) : ordre par défaut
  if (prefs.getBytesLength("pages") == PAGE_COUNT && prefs.getBytes("pages", order, PAGE_COUNT) == PAGE_COUNT &&
      pageOrderParse(pageOrderText(order), order))
    memcpy(settings.pageOrder, order, PAGE_COUNT);
}

bool pageOrderParse(const std::string& s, uint8_t* order) {
  if (s.size() != PAGE_COUNT) return false;
  uint8_t seen = 0, tmp[PAGE_COUNT];
  for (int i = 0; i < PAGE_COUNT; i++) {
    int p = s[i] - '0';
    if (p < 0 || p >= PAGE_COUNT || seen & 1 << p) return false;
    seen |= 1 << p;
    tmp[i] = p;
  }
  memcpy(order, tmp, PAGE_COUNT);
  return true;
}

std::string pageOrderText(const uint8_t* order) {
  std::string s;
  for (int i = 0; i < PAGE_COUNT; i++) s += (char)('0' + (order[i] < 10 ? order[i] : 9));
  return s;
}

void settingsSave() {
  prefs.putUChar("encSteps", settings.encSteps);
  prefs.putUShort("dimS", settings.dimAfterS);
  prefs.putUShort("sleepM", settings.sleepAfterMin);
  prefs.putUChar("briD", settings.briDay);
  prefs.putUChar("briN", settings.briNight);
  prefs.putUChar("briMode", settings.briMode);
  prefs.putUChar("nightFrom", settings.nightFrom);
  prefs.putUChar("nightTo", settings.nightTo);
  prefs.putUChar("notifMode", settings.notifMode);
  prefs.putUChar("accent", settings.accent);
  prefs.putUChar("haptic", settings.haptic);
  uint8_t old[PAGE_COUNT];
  if (prefs.getBytesLength("pages") != PAGE_COUNT || prefs.getBytes("pages", old, PAGE_COUNT) != PAGE_COUNT ||
      memcmp(old, settings.pageOrder, PAGE_COUNT))  // putBytes réécrit même sans changement
    prefs.putBytes("pages", settings.pageOrder, PAGE_COUNT);
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
  uint8_t o[PAGE_COUNT] = {};
  assert(pageOrderParse("530124", o) && o[0] == 5 && o[5] == 4 && pageOrderText(o) == "530124");
  assert(!pageOrderParse("530125", o) && !pageOrderParse("53012", o) && !pageOrderParse("53012a", o));
  assert(!pageOrderParse("012346", o) && o[0] == 5);  // refusé : rien de modifié
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

bool wifiLoad(std::string& ssid, std::string& pass) {
  ssid = prefs.getString("wifiS", "").c_str();
  pass = prefs.getString("wifiP", "").c_str();
  return !ssid.empty();
}

void wifiSave(const char* ssid, const char* pass) {
  prefs.putString("wifiS", ssid);
  prefs.putString("wifiP", pass);
}

void bondForgetLater(const std::string& id) {
  String s = prefs.getString("bondDel", "");
  if (s.length() + id.size() + 1 <= 200) prefs.putString("bondDel", s + id.c_str() + ",");
}

std::string bondForgetTake() {
  if (!prefs.isKey("bondDel")) return "";
  std::string s = prefs.getString("bondDel").c_str();
  prefs.remove("bondDel");
  return s;
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
