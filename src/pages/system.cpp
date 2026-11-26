// Page Système (design M5Dial Fluid) : version, puis diagnostic (démarrage, Bluetooth, LED, processeur, images/s,
// RAM, durée) sur deux colonnes de part et d'autre du centre.
#include "pages.h"
#include "../ble_phone.h"
#include "../config.h"
#include "../display.h"
#include "../govee.h"
#include "../ui.h"

uint16_t fpsShown = 0;   // images réellement envoyées à l'écran pendant la dernière seconde
float frameMsShown = 0;  // temps de rendu + envoi d'une image (ms, lissé)

static bool draw(float) {
  const int c = 120;
  char v[8][32];
  uint16_t vc[8];
  for (auto& k : vc) k = UI_INK;
  esp_reset_reason_t rr = esp_reset_reason();  // cause du dernier démarrage : repère les plantages
  bool bad = rr == ESP_RST_PANIC || rr == ESP_RST_BROWNOUT || rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT;
  snprintf(v[0], 32, "%s", rr == ESP_RST_PANIC ? "crash" : rr == ESP_RST_BROWNOUT ? "brownout" : bad ? "watchdog" : "normal");
  if (bad) vc[0] = UI_NEGATIVE;
  snprintf(v[1], 32, "%s", linkMode == LINK_AMS ? "iPhone" : linkMode == LINK_HID ? (TARGET_APPLE ? "Android/PC" : FW_VARIANT) : "none");
  // Pas de getServer()->getClient() ici : il efface les services GATT du lien (AMS libéré -> plantage).
  int8_t rssi = 0;
  if (linkMode == LINK_NONE || ble_gap_conn_rssi(connHandle, &rssi) != 0) rssi = 0;
  if (rssi) snprintf(v[2], 32, "%d dBm", rssi); else snprintf(v[2], 32, "\xE2\x80\x94");
  bool leds = goveeConnected();
  snprintf(v[3], 32, "%s", leds ? "connected" : "not found");
  if (leds) vc[3] = UI_POSITIVE;
  snprintf(v[4], 32, "%u MHz \xC2\xB7 %.0f\xC2\xB0", (unsigned)getCpuFrequencyMhz(), temperatureRead());
  snprintf(v[5], 32, "%u \xC2\xB7 %.1f ms", fpsShown, frameMsShown);
  snprintf(v[6], 32, "%u KB", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
  uint32_t s = millis() / 1000;
  snprintf(v[7], 32, "%u:%02u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
  static const char* labels[8] = {"Last boot", "Phone", "Signal", "LEDs", "CPU", "FPS", "Free RAM", "Uptime"};

  uiText("SYSTEM", c, 34, F_CAPTION, UI_INK3);
  uiText(uiFit("v" FW_VERSION " \xC2\xB7 " FW_VARIANT, 150, F_MICRO), c, 50, F_MICRO, UI_INK2);  // version git longue : tronquée
  for (int i = 0; i < 8; i++) {
    int y = 74 + i * 19;
    int room = textSpace(y, 14, 110) - 8;  // largeur de chaque colonne à cette hauteur (écran rond)
    uiText(uiFit(labels[i], room, F_SMALL), c - 6, y, F_SMALL, UI_INK3, middle_right);
    uiText(uiFit(v[i], room, F_SMALL), c + 6, y, F_SMALL, vc[i], middle_left);
  }
  return false;  // au repos : 4 images/s suffisent pour l'horloge
}

static void click() { sendCmd(CMD_TOGGLE); }
static void swipeX(int) {}

extern const PageOps systemPage = {draw, volumeWheel, click, swipeX};
