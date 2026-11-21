// Mise à jour OTA : Bluetooth arrêté (sa mémoire sert au Wi-Fi et au TLS), Wi-Fi du partage de connexion, version
// en ligne (ota/version.txt), puis firmware de cette plateforme (ota/<plateforme>.bin) écrit dans l'autre
// emplacement d'application ; on démarre dessus au redémarrage. HTTPS vérifié avec les racines de ota_roots.h.
#include <M5Dial.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <NimBLEDevice.h>
#include "ota.h"
#include "ota_roots.h"
#include "config.h"
#include "display.h"
#include "settings.h"

#define OTA_BASE "https://poppnn.github.io/m5dial-music/ota/"
#if defined(TARGET_ANDROID)
#define OTA_FILE "android.bin"
#elif defined(TARGET_WINDOWS)
#define OTA_FILE "windows.bin"
#else
#define OTA_FILE "m5dial.bin"
#endif
#define WIFI_TIMEOUT_MS 20000
#define READ_TIMEOUT_MS 15000

static void show(const char* title, const std::string& line, float progress = -1) {
  canvas.fillScreen(TFT_BLACK);
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_24);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(title, 120, 100);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0x8410);
  canvas.drawString(fit(line, 190).c_str(), 120, 136);
  if (progress >= 0) {
    canvas.fillArc(120, 120, 119, 111, 0, 360, 0x2104);
    canvas.fillArc(120, 120, 119, 111, -90, -90 + 360.f * progress, accent());
  }
  frameShift = 0;
  pushFrame();
}

[[noreturn]] static void finish(const char* title, const std::string& line) {
  show(title, line);
  delay(3000);
  ESP.restart();
  for (;;) {}
}

void otaRun() {
  std::string ssid, pass;
  if (!wifiLoad(ssid, pass)) return;
  M5Dial.Display.setBrightness(settingsBacklight());
  show("Update", "Stopping Bluetooth...");
  NimBLEDevice::deinit(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  for (uint32_t t0 = millis(); WiFi.status() != WL_CONNECTED;) {
    // Partage de connexion en 2,4 GHz (iPhone : « Maximiser la compatibilité »), sinon le M5Dial ne le voit pas
    if (millis() - t0 > WIFI_TIMEOUT_MS) finish("No Wi-Fi", "Hotspot on, 2.4 GHz?");
    M5Dial.update();
    if (M5Dial.BtnA.wasPressed()) finish("Cancelled", "Restarting...");
    show("Connecting", ssid + " (press = cancel)");
    delay(200);
  }

  WiFiClientSecure client;
  client.setCACert(OTA_ROOTS);
  HTTPClient http;
  http.setTimeout(READ_TIMEOUT_MS);
  show("Checking", "Latest version...");
  if (!http.begin(client, OTA_BASE "version.txt")) finish("Error", "Bad address");
  int code = http.GET();
  if (code != HTTP_CODE_OK) finish("Error", "Version: " + std::string(code < 0 ? "no connection" : "HTTP " + std::to_string(code)));
  String v = http.getString();
  http.end();
  v.trim();
  if (v.isEmpty() || v.length() > 40) finish("Error", "Bad version file");
  std::string ver = "v" + std::string(v.c_str());
  if (v == FW_VERSION) finish("Up to date", ver);

  show("Downloading", ver, 0);
  if (!http.begin(client, OTA_BASE OTA_FILE)) finish("Error", "Bad address");
  code = http.GET();
  if (code != HTTP_CODE_OK) finish("Error", "Firmware: HTTP " + std::to_string(code));
  int len = http.getSize();
  if (len <= 0 || !Update.begin(len)) finish("Error", "No room for the firmware");
  WiFiClient* s = http.getStreamPtr();
  static uint8_t buf[4096];
  int got = 0;
  uint32_t last = millis(), shown = 0;
  while (got < len) {
    size_t avail = s->available();
    if (!avail) {
      if (millis() - last > READ_TIMEOUT_MS) { Update.abort(); finish("Error", "Download cut"); }
      delay(2);
      continue;
    }
    int n = s->readBytes(buf, min<size_t>(avail, sizeof(buf)));
    if (n <= 0 || Update.write(buf, n) != (size_t)n) { Update.abort(); finish("Error", "Flash write"); }
    got += n;
    last = millis();
    if (last - shown > 150) { show("Downloading", ver, got / (float)len); shown = last; }
  }
  http.end();
  // Vérifie l'image (en-tête, empreinte SHA-256) avant de la rendre démarrable : une image abîmée n'est jamais lancée
  if (!Update.end(true)) finish("Error", Update.errorString());
  finish("Updated", ver + ", restarting");
}
