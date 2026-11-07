// Page Système : diagnostic (démarrage, Bluetooth, LED, processeur, images/s, RAM, durée).
#include "pages.h"
#include "../ble_phone.h"
#include "../config.h"
#include "../display.h"
#include "../govee.h"

uint16_t fpsShown = 0;   // images réellement envoyées à l'écran pendant la dernière seconde
float frameMsShown = 0;  // temps de rendu + envoi d'une image (ms, lissé)

static bool draw(float) {
  const int c = 120;
  char v[8][32];
  esp_reset_reason_t rr = esp_reset_reason();  // cause du dernier démarrage : repère les plantages
  snprintf(v[0], 32, "%s", rr == ESP_RST_PANIC ? "plantage" : rr == ESP_RST_BROWNOUT ? "tension basse"
           : (rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT) ? "blocage" : "normal");
  snprintf(v[1], 32, "%s", linkMode == LINK_AMS ? "iPhone" : linkMode == LINK_HID ? "Android / PC" : "aucun");
  // Pas de getServer()->getClient() ici : il efface les services GATT du lien (AMS libéré -> plantage).
  int8_t rssi = 0;
  if (linkMode == LINK_NONE || ble_gap_conn_rssi(connHandle, &rssi) != 0) rssi = 0;
  if (rssi) snprintf(v[2], 32, "%d dBm", rssi); else snprintf(v[2], 32, "-");
  snprintf(v[3], 32, "%s", goveeConnected() ? "connectées" : "non connectées");
  snprintf(v[4], 32, "%u MHz  %.0f °C", (unsigned)getCpuFrequencyMhz(), temperatureRead());
  snprintf(v[5], 32, "%u  (%.1f ms)", fpsShown, frameMsShown);
  snprintf(v[6], 32, "%u Ko", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
  uint32_t s = millis() / 1000;
  snprintf(v[7], 32, "%u:%02u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
  static const char* labels[8] = {"Démarrage", "Bluetooth", "Signal", "LED", "Processeur", "Images/s", "RAM libre", "Allumé"};

  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("Système  v" FW_VERSION, c, 28);
  for (int i = 0; i < 8; i++) {
    int y = 52 + i * 20;
    canvas.setTextDatum(middle_right);
    canvas.setTextColor(0x8410);
    canvas.drawString(labels[i], c - 4, y);
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(v[i], c + 4, y);
  }
  return false;  // au repos : 4 images/s suffisent pour l'horloge
}

static void click() { sendCmd(CMD_TOGGLE); }
static void swipeX(int) {}

extern const PageOps systemPage = {draw, volumeWheel, click, swipeX};
