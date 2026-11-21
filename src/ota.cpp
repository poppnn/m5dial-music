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
#define WIFI_TIMEOUT_MS 30000
#define READ_TIMEOUT_MS 15000

static void show(const char* title, const std::string& line, float progress = -1, const std::string& line2 = "") {
  canvas.fillScreen(TFT_BLACK);
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_24);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(title, 120, 92);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0x8410);
  canvas.drawString(fit(line, 200).c_str(), 120, 128);
  if (!line2.empty()) canvas.drawString(fit(line2, 190).c_str(), 120, 152);
  if (progress >= 0) {
    canvas.fillArc(120, 120, 119, 111, 0, 360, 0x2104);
    canvas.fillArc(120, 120, 119, 111, -90, -90 + 360.f * progress, accent());
  }
  frameShift = 0;
  pushFrame();
}

[[noreturn]] static void finish(const char* title, const std::string& line, const std::string& line2 = "",
                                uint32_t ms = 3000) {
  Serial.printf("OTA: %s | %s | %s\n", title, line.c_str(), line2.c_str());
  show(title, line, -1, line2);
  delay(ms);
  ESP.restart();
  for (;;) {}
}

static volatile int wifiReason = 0;  // dernière raison de déconnexion (esp_wifi : 201 réseau absent, 15 / 204 mot de passe...)

// Échec de connexion : dit pourquoi, d'après la dernière raison et un scan (réseau vu ou non, canal, signal).
[[noreturn]] static void wifiFailed(const std::string& ssid) {
  show("No Wi-Fi", "Scanning...");
  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  int seen = -1;
  for (int i = 0; i < n; i++) if (WiFi.SSID(i) == ssid.c_str()) seen = i;
  char why[48], how[48];
  if (n <= 0) {
    snprintf(why, sizeof(why), "No network seen at all");
    snprintf(how, sizeof(how), "Wi-Fi radio off? (code %d)", wifiReason);
  } else if (seen < 0) {
    snprintf(why, sizeof(why), "Hotspot not seen (%d others)", n);
    snprintf(how, sizeof(how), "Keep its screen open, 2.4 GHz");
  } else {
    snprintf(why, sizeof(why), "Seen: ch %d, %d dBm", WiFi.channel(seen), WiFi.RSSI(seen));
    bool pw = wifiReason == 15 || wifiReason == 202 || wifiReason == 204 || wifiReason == 2;
    snprintf(how, sizeof(how), pw ? "Wrong password? (code %d)" : "Refused (code %d)", wifiReason);
  }
  finish("No Wi-Fi", why, how, 10000);
}

void otaRun() {
  std::string ssid, pass;
  if (!wifiLoad(ssid, pass)) return;
  M5Dial.Display.setBrightness(settingsBacklight());
  show("Update", "Stopping Bluetooth...");
  NimBLEDevice::deinit(true);
  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) { wifiReason = info.wifi_sta_disconnected.reason; },
               ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.persistent(false);  // rien d'écrit dans la NVS du Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid.c_str(), pass.c_str());
  // Partage de connexion en 2,4 GHz (iPhone : écran Partage de connexion ouvert, « Maximiser la compatibilité »).
  // L'iPhone n'apparaît parfois qu'au bout de quelques secondes : nouvel essai toutes les 8 s.
  for (uint32_t t0 = millis(), tTry = t0; WiFi.status() != WL_CONNECTED;) {
    uint32_t now = millis();
    if (now - t0 > WIFI_TIMEOUT_MS) wifiFailed(ssid);
    if (now - tTry > 8000) {
      tTry = now;
      WiFi.disconnect();
      WiFi.begin(ssid.c_str(), pass.c_str());
    }
    M5Dial.update();
    if (M5Dial.BtnA.wasPressed()) finish("Cancelled", "Restarting...");
    char sub[40];
    snprintf(sub, sizeof(sub), "%lus  press = cancel", (unsigned long)((now - t0) / 1000));
    show("Connecting", ssid, -1, wifiReason ? std::string(sub) + "  (" + std::to_string(wifiReason) + ")" : sub);
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
