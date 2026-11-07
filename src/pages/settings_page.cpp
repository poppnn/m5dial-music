// Page Réglages : molette = choisir une ligne ; appui = modifier (molette = valeur, enregistrée aussitôt),
// appui = terminer. Lignes "Oublier" / "Tout effacer" : premier appui = confirmation, second = action.
#include "pages.h"
#include "../ble_phone.h"
#include "../display.h"
#include "../imu.h"
#include "../settings.h"

enum { IT_IMU, IT_ENC, IT_DIM, IT_SLEEP, IT_BRI_DAY, IT_BRI_NIGHT, IT_MODE, IT_NIGHT_FROM, IT_NIGHT_TO, IT_NOTIF_TEXT, IT_VALUES };  // puis les appareils

static const uint16_t DIM_OPTS[] = {0, 10, 20, 30, 60, 120};   // s
static const uint16_t SLEEP_OPTS[] = {0, 5, 10, 20, 30, 60};   // min
static const uint8_t BRI_DAY_OPTS[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
static const uint8_t BRI_NIGHT_OPTS[] = {5, 10, 15, 20, 30, 40, 50};
#define COUNT(a) (int)(sizeof(a) / sizeof(a[0]))

static int sel = 0;
static bool editing = false, confirming = false;

template <typename T>
static int nearest(const T* opts, int n, int v) {  // index de l'option la plus proche (valeur NVS hors table)
  int best = 0;
  for (int i = 1; i < n; i++) if (abs(opts[i] - v) < abs(opts[best] - v)) best = i;
  return best;
}

static int optCount(int it) {
  switch (it) {
    case IT_IMU: case IT_NOTIF_TEXT: return 2;
    case IT_MODE: return 3;
    case IT_NIGHT_FROM: case IT_NIGHT_TO: return 24;
    case IT_ENC: return 4;
    case IT_DIM: return COUNT(DIM_OPTS);
    case IT_SLEEP: return COUNT(SLEEP_OPTS);
    case IT_BRI_DAY: return COUNT(BRI_DAY_OPTS);
    default: return COUNT(BRI_NIGHT_OPTS);
  }
}

static int optIndex(int it) {
  switch (it) {
    case IT_IMU: return settings.imuDir < 0;
    case IT_NOTIF_TEXT: return settings.notifText;
    case IT_MODE: return settings.briMode;
    case IT_NIGHT_FROM: return settings.nightFrom;
    case IT_NIGHT_TO: return settings.nightTo;
    case IT_ENC: return settings.encSteps - 1;
    case IT_DIM: return nearest(DIM_OPTS, COUNT(DIM_OPTS), settings.dimAfterS);
    case IT_SLEEP: return nearest(SLEEP_OPTS, COUNT(SLEEP_OPTS), settings.sleepAfterMin);
    case IT_BRI_DAY: return nearest(BRI_DAY_OPTS, COUNT(BRI_DAY_OPTS), settings.briDay);
    default: return nearest(BRI_NIGHT_OPTS, COUNT(BRI_NIGHT_OPTS), settings.briNight);
  }
}

static void setOpt(int it, int i) {
  switch (it) {
    case IT_IMU: {
      int8_t dir = i ? -1 : 1;
      if (dir != settings.imuDir) { settings.imuDir = dir; imuFlipped(); }
      break;
    }
    case IT_MODE: settings.briMode = i; break;
    case IT_NOTIF_TEXT: settings.notifText = i; break;
    case IT_NIGHT_FROM: settings.nightFrom = i; break;
    case IT_NIGHT_TO: settings.nightTo = i; break;
    case IT_ENC: settings.encSteps = i + 1; break;
    case IT_DIM: settings.dimAfterS = DIM_OPTS[i]; break;
    case IT_SLEEP: settings.sleepAfterMin = SLEEP_OPTS[i]; break;
    case IT_BRI_DAY: settings.briDay = BRI_DAY_OPTS[i]; break;
    default: settings.briNight = BRI_NIGHT_OPTS[i]; break;
  }
  settingsSave();  // NVS n'écrit que les valeurs qui ont changé
}

static void valueText(int it, char* b, size_t n) {
  int i = optIndex(it);
  switch (it) {
    case IT_IMU: snprintf(b, n, "%s", i ? "reversed" : "normal"); break;
    case IT_NOTIF_TEXT: snprintf(b, n, "%s", i ? "shown" : "hidden"); break;
    case IT_MODE: snprintf(b, n, "%s", i == 2 ? (settingsNight() ? "auto (night)" : "auto (day)") : i ? "night" : "day"); break;
    case IT_NIGHT_FROM: case IT_NIGHT_TO: snprintf(b, n, "%d:00", i); break;
    case IT_ENC: snprintf(b, n, "%d pulse%s", i + 1, i ? "s" : ""); break;
    case IT_DIM: if (DIM_OPTS[i]) snprintf(b, n, "%u s", DIM_OPTS[i]); else snprintf(b, n, "never"); break;
    case IT_SLEEP: if (SLEEP_OPTS[i]) snprintf(b, n, "%u min", SLEEP_OPTS[i]); else snprintf(b, n, "never"); break;
    case IT_BRI_DAY: snprintf(b, n, "%u %%", BRI_DAY_OPTS[i]); break;
    default: snprintf(b, n, "%u %%", BRI_NIGHT_OPTS[i]); break;
  }
}

static const char* LABELS[IT_VALUES] = {"IMU dir", "Wheel", "Dim after", "Power off", "Day level",
                                        "Night level", "Brightness", "Night from", "Night until", "Notif text"};

// Lignes : réglages, puis un "Oublier" par appareil appairé, puis "Tout effacer" (s'il y en a).
static int itemCount() {
  int bonds = NimBLEDevice::getNumBonds();
  return IT_VALUES + (bonds ? bonds + 1 : 1);
}

static bool draw(float) {
  const int c = 120;
  int n = itemCount(), bonds = NimBLEDevice::getNumBonds();
  if (sel >= n) sel = n - 1;
  std::string peer = connectedPeer();
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("Settings", c, 26);
  for (int i = sel - 2; i <= sel + 2; i++) {  // 5 lignes : au-delà, l'écran rond est trop étroit
    if (i < 0 || i >= n) continue;
    int y = 118 + (i - sel) * 28;
    int room = textSpace(y, 16, 114) - 6;  // largeur de chaque colonne à cette hauteur
    bool cur = i == sel;
    char val[40];
    std::string label;
    bool live = false;
    if (i < IT_VALUES) {
      label = LABELS[i];
      valueText(i, val, sizeof(val));
    } else if (!bonds) {
      label = "Devices";
      snprintf(val, sizeof(val), "none");
    } else if (i - IT_VALUES < bonds) {  // un appareil appairé
      std::string a = NimBLEDevice::getBondedAddress(i - IT_VALUES).toString(), name = bondName(a);
      label = "Forget";
      live = a == peer;  // appareil connecté : nom en vert
      snprintf(val, sizeof(val), "%s", (name.empty() ? a.substr(9) : name).c_str());
    } else {
      label = "Erase all";
      snprintf(val, sizeof(val), "%d device%s", bonds, bonds > 1 ? "s" : "");
    }
    if (cur && confirming) snprintf(val, sizeof(val), "confirm?");
    if (cur) canvas.fillRoundRect(16, y - 13, 208, 26, 8, 0x2104);
    canvas.setTextDatum(middle_right);
    canvas.setTextColor(cur ? TFT_WHITE : 0x8410);
    canvas.drawString(fit(label, room).c_str(), c - 4, y);
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(cur && (editing || confirming) ? 0xFA20 : live ? 0x07E8 : cur ? TFT_WHITE : 0x8410);
    canvas.drawString(fit(val, room).c_str(), c + 4, y);
  }
  return false;
}

static void wheel(int detents) {
  confirming = false;
  if (editing) {
    setOpt(sel, constrain(optIndex(sel) + detents, 0, optCount(sel) - 1));
  } else {
    sel = constrain(sel + detents, 0, itemCount() - 1);
  }
}

static void click() {
  int bonds = NimBLEDevice::getNumBonds();
  if (sel < IT_VALUES) { editing = !editing; return; }
  if (!bonds) return;
  if (!confirming) { confirming = true; return; }
  confirming = false;
  if (sel - IT_VALUES < bonds) forgetBond(NimBLEDevice::getBondedAddress(sel - IT_VALUES));
  else forgetAllBonds();
}

static void swipeX(int) { editing = confirming = false; }

extern const PageOps settingsPage = {draw, wheel, click, swipeX};
