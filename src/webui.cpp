// Web UI servie par le M5Dial sur le partage de connexion (voir webui.h). Serveur HTTP simple (port 80, sans mot de
// passe : seuls les appareils déjà sur TON partage de connexion la voient), une requête à la fois.
#include <M5Dial.h>
#include <WebServer.h>
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <assert.h>
#include <vector>
#include "webui.h"
#include "webui_page.h"
#include "bootanim.h"
#include "config.h"
#include "gb.h"
#include "govee.h"
#include "ota.h"
#include "settings.h"

#define IDLE_MS (15 * 60 * 1000UL)  // sans requête : fin de session

struct Dev { std::string id, name; };  // id = "adresse/type" (type d'adresse NimBLE)
static std::vector<Dev> devs;           // relevés avant l'arrêt du Bluetooth

std::string jsonStr(const std::string& s) {
  std::string o = "\"";
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
    else if (c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
    else o += (char)c;
  }
  return o + "\"";
}

std::string configJson() {
  const Settings& s = settings;
  char b[512];
  snprintf(b, sizeof(b),
           "{\"firmware\":\"%s %s\",\"imuDir\":%d,\"encSteps\":%u,\"dimAfterS\":%u,\"sleepAfterMin\":%u,\"briDay\":%u,"
           "\"briNight\":%u,\"briMode\":%u,\"nightFrom\":%u,\"nightTo\":%u,\"notifMode\":%u,\"accent\":%u,\"haptic\":%u,"
           "\"ledColor\":%d,\"ledHue\":%d,\"ledCustom\":%d,\"ledBri\":%d,",
           FW_VERSION, FW_VARIANT, s.imuDir, s.encSteps, s.dimAfterS, s.sleepAfterMin, s.briDay, s.briNight, s.briMode,
           s.nightFrom, s.nightTo, s.notifMode, s.accent, s.haptic, ledColor, ledHue, ledCustom ? 1 : 0, ledBri);
  std::string o = b, ssid, pass;
  o += "\"wifiSsid\":" + jsonStr(wifiLoad(ssid, pass) ? ssid : "") + ",\"accents\":[";
  for (int i = 0; i < ACCENT_COUNT; i++) o += (i ? "," : "") + jsonStr(ACCENT_NAMES[i]);
  o += "],\"ledColors\":[";
  for (int i = 0; i < LED_COLOR_COUNT; i++) o += (i ? "," : "") + jsonStr(LED_COLORS[i].name);
  o += "],\"devices\":[";
  for (size_t i = 0; i < devs.size(); i++)
    o += std::string(i ? "," : "") + "{\"id\":" + jsonStr(devs[i].id) + ",\"name\":" + jsonStr(devs[i].name) + "}";
  return o + "],\"bootCapacity\":" + std::to_string(bootAnimCapacity()) + "}";
}

// Champ numérique entier : absent = on garde la valeur ; présent mais pas un nombre dans [lo, hi] = refusé.
static bool num(const std::string& js, const char* key, long lo, long hi, long& v, std::string& err) {
  std::string t;
  if (!gbField(js, key, t)) return false;
  char* end;
  long x = strtol(t.c_str(), &end, 10);
  if (t.empty() || *end || x < lo || x > hi) { if (err.empty()) err = key; return false; }
  v = x;
  return true;
}

// Tout est vérifié avant d'appliquer quoi que ce soit : un fichier à moitié faux ne change rien.
std::string configApply(const std::string& js) {
  Settings s = settings;
  long v, color = ledColor, hue = ledHue, custom = ledCustom, bri = ledBri;
  std::string err;
  if (num(js, "imuDir", -1, 1, v, err)) { if (v) s.imuDir = v; else err = "imuDir"; }
  if (num(js, "encSteps", 1, 4, v, err)) s.encSteps = v;
  if (num(js, "dimAfterS", 0, 3600, v, err)) s.dimAfterS = v;
  if (num(js, "sleepAfterMin", 0, 600, v, err)) s.sleepAfterMin = v;
  if (num(js, "briDay", 5, 100, v, err)) s.briDay = v;
  if (num(js, "briNight", 5, 100, v, err)) s.briNight = v;
  if (num(js, "briMode", 0, 2, v, err)) s.briMode = v;
  if (num(js, "nightFrom", 0, 23, v, err)) s.nightFrom = v;
  if (num(js, "nightTo", 0, 23, v, err)) s.nightTo = v;
  if (num(js, "notifMode", 0, NOTIF_FULL, v, err)) s.notifMode = v;
  if (num(js, "accent", 0, ACCENT_COUNT - 1, v, err)) s.accent = v;
  if (num(js, "haptic", 0, 1, v, err)) s.haptic = v;
  num(js, "ledColor", 0, LED_COLOR_COUNT - 1, color, err);
  num(js, "ledHue", 0, 359, hue, err);
  num(js, "ledCustom", 0, 1, custom, err);
  num(js, "ledBri", 5, 100, bri, err);
  std::string ssid, pass, oldSsid, oldPass, t;
  bool hasOld = wifiLoad(oldSsid, oldPass), setWifi = gbField(js, "wifiSsid", ssid) && !ssid.empty();
  if (setWifi) {  // mot de passe absent : celui déjà enregistré s'il s'agit du même réseau
    pass = gbField(js, "wifiPass", t) ? t : (hasOld && ssid == oldSsid ? oldPass : "");
    if (ssid.size() > 32 || pass.size() > 63) err = err.empty() ? "wifi" : err;
  }
  if (!err.empty()) return err;
  settings = s;
  settingsSave();
  ledColor = color;
  ledHue = hue;
  ledCustom = custom;
  ledBri = bri;
  ledStateSave({(uint8_t)ledColor, (uint16_t)ledHue, ledCustom, (uint8_t)ledBri});
  if (setWifi && (!hasOld || ssid != oldSsid || pass != oldPass)) wifiSave(ssid.c_str(), pass.c_str());
  return "";
}

static WebServer* server = nullptr;
static uint32_t lastReq = 0;
static bool bootOk = false;

static void sendJson(int code, const std::string& body) {
  lastReq = millis();
  server->send(code, "application/json", body.c_str());
}

static void onUpload() {  // image de démarrage : écrite dans la partition au fil de la réception
  HTTPUpload& u = server->upload();
  lastReq = millis();
  if (u.status == UPLOAD_FILE_START) bootOk = bootAnimBegin(server->arg("n").toInt());
  else if (u.status == UPLOAD_FILE_WRITE) bootOk = bootOk && bootAnimWrite(u.buf, u.currentSize);
  else if (u.status == UPLOAD_FILE_END) { bool e = bootAnimEnd(); bootOk = bootOk && e; }
  else if (u.status == UPLOAD_FILE_ABORTED) { bootAnimEnd(); bootOk = false; }
}

void webuiRun() {
  devs.clear();
  for (int i = 0; i < NimBLEDevice::getNumBonds(); i++) {
    NimBLEAddress a = NimBLEDevice::getBondedAddress(i);
    devs.push_back({a.toString() + "/" + std::to_string(a.getType()), bondName(a.toString())});
  }
  wifiSession("Web UI");
  WebServer web(80);
  server = &web;
  web.on("/", HTTP_GET, [] { lastReq = millis(); server->send_P(200, "text/html", WEBUI_PAGE); });
  web.on("/config", HTTP_GET, [] { sendJson(200, configJson()); });
  web.on("/config", HTTP_POST, [] {
    std::string err = configApply(server->arg("plain").c_str());
    sendJson(err.empty() ? 200 : 400, err.empty() ? configJson() : "{\"error\":" + jsonStr(err) + "}");
  });
  web.on("/forget", HTTP_POST, [] {
    std::string id = server->arg("id").c_str();
    for (size_t i = 0; i < devs.size(); i++)
      if (devs[i].id == id) { bondForgetLater(id); devs.erase(devs.begin() + i); return sendJson(200, configJson()); }
    sendJson(404, "{\"error\":\"device\"}");
  });
  web.on("/boot", HTTP_POST, [] { sendJson(bootOk ? 200 : 400, bootOk ? "{}" : "{\"error\":\"image\"}"); }, onUpload);
  web.on("/boot/clear", HTTP_POST, [] { bootAnimClear(); sendJson(200, "{}"); });
  web.on("/exit", HTTP_POST, [] {
    sendJson(200, "{}");
    delay(300);
    ESP.restart();
  });
  web.begin();
  std::string url = "http://" + std::string(WiFi.localIP().toString().c_str());
  lastReq = millis();
  for (uint32_t shown = 0;;) {
    web.handleClient();
    M5Dial.update();
    if (M5Dial.BtnA.wasReleased() || millis() - lastReq > IDLE_MS || WiFi.status() != WL_CONNECTED) {
      show("Web UI", "Restarting...");
      delay(500);
      ESP.restart();
    }
    if (millis() - shown > 1000) {  // adresse à taper dans le navigateur du téléphone
      show("Open on the phone", url, -1, "Press = done");
      shown = millis();
    }
    delay(2);
  }
}

void webuiSelfTest() {
  assert(jsonStr("a\"b\\c\n") == "\"a\\\"b\\\\c\\u000a\"");
  Settings keep = settings;
  std::string err;
  long v = 0;
  assert(!num("{\"briDay\":150}", "briDay", 5, 100, v, err) && err == "briDay");  // hors limites : refusé
  err.clear();
  assert(!num("{\"x\":1}", "briDay", 5, 100, v, err) && err.empty());           // absent : rien
  assert(num("{\"briDay\":40,\"y\":1}", "briDay", 5, 100, v, err) && v == 40);
  assert(configApply("{\"briDay\":40,\"accent\":99}") == "accent" && settings.briDay == keep.briDay);  // rien appliqué
}
