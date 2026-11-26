// Mise à jour OTA : Bluetooth arrêté (sa mémoire sert au Wi-Fi et au TLS), réseau choisi à la molette parmi les
// réseaux visibles (mot de passe tapé à la molette, ou celui déjà enregistré), version
// en ligne (ota/version.txt), puis firmware de cette plateforme (ota/<plateforme>.bin) écrit dans l'autre
// emplacement d'application ; on démarre dessus au redémarrage. HTTPS vérifié avec les racines de ota_roots.h.
#include <M5Dial.h>
#include <assert.h>
#include <algorithm>
#include <vector>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <NimBLEDevice.h>
#include "ota.h"
#include "ota_roots.h"
#include "app.h"
#include "ble_phone.h"
#include "govee.h"
#include "config.h"
#include "display.h"
#include "encoder.h"
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

void show(const char* title, const std::string& line, float progress, const std::string& line2) {
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

// ---------- molette et bouton, lus ici : loop() ne tourne plus pendant la mise à jour ----------
static long encLast = 0;

static int readDetents() {
  long e = encoderRead(), st = settings.encSteps;
  int d = 0;
  while (e - encLast >= st) { d++; encLast += st; }
  while (encLast - e >= st) { d--; encLast -= st; }
  if (d) haptic(d);
  return d;
}

enum Key { K_NONE, K_CLICK, K_LONG };

// Appui court (au relâcher) ou appui long (dès LONG_PRESS_MS, sans clic au relâcher).
static Key readKey() {
  static bool held = false;
  M5Dial.update();
  Key k = K_NONE;
  if (M5Dial.BtnA.isPressed() && !held && M5Dial.BtnA.pressedFor(LONG_PRESS_MS)) { held = true; k = K_LONG; }
  else if (M5Dial.BtnA.wasReleased()) { k = held ? K_NONE : K_CLICK; held = false; }
  if (k != K_NONE) haptic(0);
  return k;
}

// ---------- choix d'un réseau ----------
// Liste à la molette (5 lignes autour de la sélection, comme les Réglages) ; appui = choisir.
static int choose(const char* title, const std::vector<std::string>& items, int sel) {
  for (;;) {
    // Crans lus AVANT constrain : c'est une macro qui évalue son premier argument jusqu'à 3 fois (le 2e appel à
    // readDetents() renvoyait 0, la sélection ne bougeait jamais).
    int d = readDetents();
    sel = constrain(sel + d, 0, (int)items.size() - 1);
    if (readKey() == K_CLICK) return sel;
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::efontJA_16);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(title, 120, 30);
    for (int i = sel - 2; i <= sel + 2; i++) {
      if (i < 0 || i >= (int)items.size()) continue;
      int y = 120 + (i - sel) * 30;
      if (i == sel) canvas.fillRoundRect(20, y - 13, 200, 26, 8, 0x2104);
      canvas.setTextColor(i == sel ? TFT_WHITE : 0x8410);
      canvas.drawString(fit(items[i], 2 * textSpace(y, 16, 112) - 12).c_str(), 120, y);
    }
    canvas.setTextColor(0x5AEB);
    canvas.drawString("Press: choose", 120, 212);
    frameShift = 0;
    pushFrame();
    delay(15);
  }
}

// ---------- mot de passe à la molette ----------
// Anneau de saisie : OK, effacer, puis les caractères (minuscules d'abord : les mots de passe de partage de
// connexion en sont souvent faits). Molette = caractère, appui = l'ajouter, appui long = terminé.
static const char PW_CHARS[] = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ -_.!@#$%&*?+=/:;,'\"()[]{}<>^~|\\`";
enum { PW_OK, PW_DEL, PW_FIRST };
static const int PW_COUNT = PW_FIRST + sizeof(PW_CHARS) - 1;

static std::string pwLabel(int e) {
  e = (e % PW_COUNT + PW_COUNT) % PW_COUNT;
  return e == PW_OK ? "OK" : e == PW_DEL ? "DEL" : PW_CHARS[e - PW_FIRST] == ' ' ? "space" : std::string(1, PW_CHARS[e - PW_FIRST]);
}

// Applique l'entrée e de l'anneau au mot de passe. Renvoie 1 = terminé, -1 = retour (effacer sur un mot de passe
// vide), 0 = on continue.
static int pwApply(std::string& pw, int e) {
  if (e == PW_OK) return 1;
  if (e == PW_DEL) {
    if (pw.empty()) return -1;
    pw.pop_back();
  } else if (pw.size() < 63) {
    pw += PW_CHARS[e - PW_FIRST];
  }
  return 0;
}

static bool enterPassword(const std::string& ssid, std::string& pw) {
  int sel = PW_FIRST;
  for (;;) {
    sel = ((sel + readDetents()) % PW_COUNT + PW_COUNT) % PW_COUNT;
    Key k = readKey();
    if (k == K_LONG) return true;
    if (k == K_CLICK) {
      int r = pwApply(pw, sel);
      if (r) return r > 0;
    }
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::efontJA_16);
    canvas.setTextColor(0x8410);
    canvas.drawString(fit(ssid, 150).c_str(), 120, 30);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("Password", 120, 52);
    canvas.setFont(&fonts::efontJA_24);
    std::string shown = pw + "_";  // fin du mot de passe si trop long pour la ligne
    while (canvas.textWidth(shown.c_str()) > 190) shown.erase(0, 1);
    canvas.drawString(shown.c_str(), 120, 88);
    for (int d = -2; d <= 2; d++) {  // caractère choisi en gros, ses voisins de part et d'autre
      canvas.setTextSize(d ? 1 : 2);
      canvas.setTextColor(d ? 0x5AEB : (sel < PW_FIRST ? accent() : TFT_WHITE));
      canvas.drawString(pwLabel(sel + d).c_str(), 120 + d * 46 + (d > 0 ? 8 : d < 0 ? -8 : 0), 146);
    }
    canvas.setTextSize(1);
    canvas.setFont(&fonts::efontJA_16);
    canvas.setTextColor(0x5AEB);
    canvas.drawString("Press: add   Hold: done", 120, 200);
    frameShift = 0;
    pushFrame();
    delay(15);
  }
}

// ---------- connexion ----------
// Partage de connexion en 2,4 GHz (iPhone : écran Partage de connexion ouvert, « Maximiser la compatibilité »).
// L'iPhone n'apparaît parfois qu'au bout de quelques secondes : nouvel essai toutes les 8 s. "" = connecté, sinon la
// raison de l'échec.
static std::string connect(const std::string& ssid, const std::string& pass) {
  wifiReason = 0;
  WiFi.begin(ssid.c_str(), pass.c_str());
  for (uint32_t t0 = millis(), tTry = t0; WiFi.status() != WL_CONNECTED;) {
    uint32_t now = millis();
    if (now - t0 > WIFI_TIMEOUT_MS || readKey() == K_CLICK) {
      WiFi.disconnect();
      bool pw = wifiReason == 15 || wifiReason == 202 || wifiReason == 204 || wifiReason == 2;
      return now - t0 <= WIFI_TIMEOUT_MS ? "Cancelled" : pw ? "Wrong password?" : "Not reached (" + std::to_string(wifiReason) + ")";
    }
    if (now - tTry > 8000) {
      tTry = now;
      WiFi.disconnect();
      WiFi.begin(ssid.c_str(), pass.c_str());
    }
    show("Connecting", ssid, -1, std::to_string((now - t0) / 1000) + "s   press = back");
    delay(100);
  }
  return "";
}

// Réseaux visibles (sans doublons, le plus fort d'abord, le réseau enregistré en tête), choix, mot de passe,
// connexion ; recommence jusqu'à ce qu'une connexion réussisse. Le réseau qui marche est enregistré.
static void joinWifi() {
  std::string saved, savedPass;
  bool hasSaved = wifiLoad(saved, savedPass);
  for (;;) {
    show("Wi-Fi", "Scanning...", -1, "Turn the hotspot on");
    int n = WiFi.scanNetworks();
    std::vector<std::string> nets;
    std::vector<bool> open;
    for (int i = 0; i < n; i++) {
      std::string s = WiFi.SSID(i).c_str();
      if (s.empty() || std::find(nets.begin(), nets.end(), s) != nets.end()) continue;
      bool first = hasSaved && s == saved;
      nets.insert(first ? nets.begin() : nets.end(), s);
      open.insert(first ? open.begin() : open.end(), WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    }
    WiFi.scanDelete();
    std::vector<std::string> items = nets;
    if (hasSaved && !nets.empty() && nets[0] == saved) items[0] += "  (saved)";
    items.push_back("Scan again");
    items.push_back("Cancel");
    int c = choose(nets.empty() ? "No network: hotspot on?" : "Choose the Wi-Fi", items, 0);
    if (c == (int)items.size() - 1) finish("Cancelled", "Restarting...");
    if (c >= (int)nets.size()) continue;
    std::string pass = hasSaved && nets[c] == saved ? savedPass : "";
    if (!open[c] && pass.empty() && !enterPassword(nets[c], pass)) continue;
    std::string err = connect(nets[c], pass);
    if (err.empty()) {
      if (!hasSaved || nets[c] != saved || pass != savedPass) wifiSave(nets[c].c_str(), pass.c_str());
      return;
    }
    if (nets[c] == saved) hasSaved = false;  // mot de passe enregistré refusé : on le redemandera
    show("No Wi-Fi", nets[c], -1, err);
    delay(2500);
  }
}

void otaSelfTest() {
  std::string pw;
  assert(pwApply(pw, PW_FIRST) == 0 && pw == "a");                       // premier caractère : 'a'
  assert(pwApply(pw, PW_FIRST + 36) == 0 && pw == "aA");                 // après les 26 minuscules et 10 chiffres
  assert(pwApply(pw, PW_DEL) == 0 && pw == "a" && pwApply(pw, PW_DEL) == 0 && pw.empty());
  assert(pwApply(pw, PW_DEL) == -1 && pwApply(pw, PW_OK) == 1);          // effacer sur vide = retour ; OK = terminé
  assert(pwLabel(-1) == "`" && pwLabel(PW_COUNT) == "OK" && pwLabel(PW_FIRST + 62) == "space");  // l'anneau boucle
}

void wifiSession(const char* title) {
  M5Dial.Display.setBrightness(settingsBacklight());
  show(title, "Stopping Bluetooth...");
  // Les tâches de fond (connexion à la barre LED, découverte du téléphone, commandes en file) finissent d'abord :
  // arrêté sous elles, le Bluetooth libéré les faisait planter. loop() ne tourne plus, rien de nouveau n'arrive.
  for (uint32_t t0 = millis(); (goveeBusy() || bleBusy()) && millis() - t0 < 6000;) delay(20);
  NimBLEDevice::deinit(true);
  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) { wifiReason = info.wifi_sta_disconnected.reason; },
               ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.persistent(false);  // rien d'écrit dans la NVS du Wi-Fi : le réseau est gardé dans nos réglages
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  encLast = encoderRead();
  joinWifi();
}

void otaRun() {
  wifiSession("Update");

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
