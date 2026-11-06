/*
 * M5Dial -> télécommande Apple Music (iPhone) via BLE "Apple Media Service" (AMS).
 *
 * Pourquoi AMS et pas A2DP/AVRCP : l'ESP32-S3 n'a PAS de Bluetooth Classic (BLE seulement),
 * donc ESP32-A2DP/AVRCP ne peut pas tourner sur le M5Dial. AMS est le service BLE natif
 * d'iOS : commandes (play/pause, piste +/-, volume +/-) ET métadonnées (titre, artiste,
 * durée, temps écoulé), sans app tierce. Il ne fournit pas la pochette -> fallback titre/artiste.
 *
 * Android / Windows n'ont pas d'AMS : le M5Dial est aussi une télécommande BLE HID (touches
 * multimédia standard), reconnue nativement. Commandes seules, sans métadonnées.
 *
 *   Encodeur  gauche/droite : volume -/+
 *   Bouton central / tap    : play/pause
 *   Bouton central 2 s      : extinction (sur batterie)
 *   Swipe droite / gauche   : piste suivante / précédente
 */
#include <M5Dial.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <mutex>

// ---------- AMS (spec Apple "Apple Media Service") ----------
static const char* AMS_SVC = "89D3502B-0F36-433A-8EF4-C502AD55F8DC";
static const char* AMS_RC  = "9B3C81D8-57B1-4A8A-B8DF-0E56F7CA51C2";  // Remote Command
static const char* AMS_EU  = "2F7CABCE-808D-411F-9A0C-BB92BA96C102";  // Entity Update

enum : uint8_t { ENT_PLAYER = 0, ENT_TRACK = 2 };
enum : uint8_t { PLAYER_PLAYBACK_INFO = 1 };
enum : uint8_t { TRACK_ARTIST = 0, TRACK_TITLE = 2, TRACK_DURATION = 3 };
enum : uint8_t { CMD_TOGGLE = 2, CMD_NEXT = 3, CMD_PREV = 4, CMD_VOL_UP = 5, CMD_VOL_DOWN = 6 };

// Calibration matérielle : impulsions de l'encodeur par cran (4 sur le M5Dial testé par M5Stack).
#define ENC_STEPS_PER_DETENT 4
#define SWIPE_MIN_PX         40

// ---------- État partagé (écrit par la tâche BLE, lu par loop) ----------
struct Media {
  std::string title, artist;
  float duration = 0, elapsed = 0, rate = 0;
  uint32_t stamp = 0;  // millis() de la dernière mise à jour de elapsed
  bool playing = false;
};
static Media media;
static std::mutex mtx;
enum Link : uint8_t { LINK_NONE, LINK_HID, LINK_AMS };  // HID = Android/Windows, AMS = iPhone
static volatile Link linkMode = LINK_NONE;
static volatile bool dirty = true, needSetup = false;
static volatile uint16_t connHandle = 0;
static NimBLERemoteCharacteristic* rcChar = nullptr;
static NimBLECharacteristic* hidInput = nullptr;

// Rapport HID "Consumer Control" : 1 bit par touche multimédia (bits 0..4), 3 bits de bourrage.
static const uint8_t hidReportMap[] = {
  0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x01,  // Usage Page Consumer, Collection, Report ID 1
  0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x05,  // 5 champs de 1 bit
  0x09, 0xCD, 0x09, 0xB5, 0x09, 0xB6, 0x09, 0xE9, 0x09, 0xEA,  // Play/Pause, Next, Prev, Vol+, Vol-
  0x81, 0x02, 0x95, 0x03, 0x81, 0x01, 0xC0};

static M5Canvas canvas(&M5Dial.Display);

// PlaybackInfo AMS = "state,rate,elapsed" (state: 0 pause, 1 lecture, 2/3 avance/recul)
static bool parsePlaybackInfo(const std::string& s, int& state, float& rate, float& elapsed) {
  return sscanf(s.c_str(), "%d,%f,%f", &state, &rate, &elapsed) == 3;
}

// Notification Entity Update : [EntityID][AttributeID][Flags][valeur UTF-8...]
// shortcut: valeurs tronquées à MTU-6 octets (flag 1), lire Entity Attribute si titres très longs.
static void onEntityUpdate(NimBLERemoteCharacteristic*, uint8_t* d, size_t len, bool) {
  if (len < 3) return;
  std::string v((const char*)d + 3, len - 3);
  std::lock_guard<std::mutex> lock(mtx);
  if (d[0] == ENT_TRACK) {
    if (d[1] == TRACK_TITLE) media.title = v;
    else if (d[1] == TRACK_ARTIST) media.artist = v;
    else if (d[1] == TRACK_DURATION) media.duration = atof(v.c_str());
  } else if (d[0] == ENT_PLAYER && d[1] == PLAYER_PLAYBACK_INFO) {
    int state; float rate, el;
    if (parsePlaybackInfo(v, state, rate, el)) {
      media.playing = state == 1;
      media.rate = rate;
      media.elapsed = el;
      media.stamp = millis();
    }
  }
  dirty = true;
}

// ---------- Connexion : l'iPhone se connecte à nous (périphérique), puis on est client GATT d'AMS ----------
class ServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo& info) override {
    connHandle = info.getConnHandle();
    NimBLEDevice::startSecurity(connHandle);  // AMS exige un lien chiffré (appairage)
  }
  void onAuthenticationComplete(NimBLEConnInfo& info) override {
    if (info.isEncrypted()) needSetup = true;  // découverte GATT faite dans loop(), pas dans la tâche BLE
    else NimBLEDevice::getServer()->disconnect(info.getConnHandle());
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    rcChar = nullptr;
    linkMode = LINK_NONE;
    needSetup = false;
    dirty = true;
    NimBLEDevice::startAdvertising();
  }
};

static void setupAms() {
  needSetup = false;
  NimBLEClient* client = NimBLEDevice::getServer()->getClient(connHandle);
  NimBLERemoteService* svc = client ? client->getService(AMS_SVC) : nullptr;
  NimBLERemoteCharacteristic* eu = svc ? svc->getCharacteristic(AMS_EU) : nullptr;
  NimBLERemoteCharacteristic* rc = svc ? svc->getCharacteristic(AMS_RC) : nullptr;
  if (!eu || !rc || !eu->subscribe(true, onEntityUpdate) || !rc->subscribe(true)) {
    linkMode = LINK_HID;  // pas un iPhone (Android/Windows) : touches multimédia HID
    dirty = true;
    return;
  }
  // Demande à l'iPhone de nous notifier ces attributs (valeur courante envoyée immédiatement).
  const uint8_t player[] = {ENT_PLAYER, PLAYER_PLAYBACK_INFO};
  const uint8_t track[]  = {ENT_TRACK, TRACK_ARTIST, TRACK_TITLE, TRACK_DURATION};
  eu->writeValue(player, sizeof(player), true);
  eu->writeValue(track, sizeof(track), true);
  rcChar = rc;
  linkMode = LINK_AMS;
  dirty = true;
}

static void sendCmd(uint8_t cmd) {
  if (linkMode == LINK_AMS && rcChar) {
    rcChar->writeValue(&cmd, 1, true);
  } else if (linkMode == LINK_HID) {
    static const uint8_t bit[] = {0, 0, 0x01, 0x02, 0x04, 0x08, 0x10};  // index = CMD_*
    uint8_t v = bit[cmd];
    hidInput->setValue(&v, 1);
    hidInput->notify();
    delay(10);  // touche "appuyée" puis relâchée
    v = 0;
    hidInput->setValue(&v, 1);
    hidInput->notify();
  }
}

static void startBle() {
  NimBLEDevice::init("M5Dial Music");
  NimBLEDevice::setMTU(185);
  NimBLEDevice::setSecurityAuth(true, false, true);  // bonding, sans MITM, Secure Connections
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCB());

  NimBLEHIDDevice* hid = new NimBLEHIDDevice(server);
  hid->setManufacturer("M5Stack");
  hid->setPnp(0x02, 0xE502, 0xA111, 0x0100);
  hid->setHidInfo(0x00, 0x01);
  hid->setReportMap((uint8_t*)hidReportMap, sizeof(hidReportMap));
  hid->setBatteryLevel(100);
  hidInput = hid->getInputReport(1);
  server->start();

  // "Service Solicitation" AMS (AD type 0x15, UUID 128 bits little-endian) : iOS sait qu'on veut AMS.
  const uint8_t solicit[] = {0x11, 0x15, 0xDC, 0xF8, 0x55, 0xAD, 0x02, 0xC5, 0xF4, 0x8E,
                             0x3A, 0x43, 0x36, 0x0F, 0x2B, 0x50, 0xD3, 0x89};
  NimBLEAdvertisementData adv, scan;
  adv.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  adv.addData(solicit, sizeof(solicit));
  adv.setAppearance(0x0180);                    // télécommande générique
  adv.setCompleteServices16({NimBLEUUID((uint16_t)0x1812)});  // HID : visible sur Android/Windows
  scan.setName("M5Dial Music");
  NimBLEAdvertising* a = NimBLEDevice::getAdvertising();
  a->setAdvertisementData(adv);
  a->setScanResponseData(scan);
  a->start();
}

// ---------- Affichage ----------
static std::string fit(std::string s, int maxW) {  // tronque proprement en UTF-8 + "…"
  if (canvas.textWidth(s.c_str()) <= maxW) return s;
  while (!s.empty() && canvas.textWidth((s + "...").c_str()) > maxW) {
    do s.pop_back(); while (!s.empty() && (s.back() & 0xC0) == 0x80);
    if (!s.empty() && (uint8_t)s.back() >= 0xC0) s.pop_back();
  }
  return s + "...";
}

static String mmss(float sec) {
  int s = sec < 0 ? 0 : (int)sec;
  char b[12];
  snprintf(b, sizeof(b), "%d:%02d", s / 60, s % 60);
  return b;
}

static void draw() {
  Media m;
  {
    std::lock_guard<std::mutex> lock(mtx);
    m = media;
  }
  float el = m.elapsed + (m.playing ? m.rate * (millis() - m.stamp) / 1000.f : 0);
  if (m.duration > 0 && el > m.duration) el = m.duration;

  const int c = 120;
  canvas.fillScreen(TFT_BLACK);
  canvas.fillArc(c, c, 119, 111, 0, 360, 0x2104);  // piste de fond de l'arc
  if (m.duration > 0) canvas.fillArc(c, c, 119, 111, -90, -90 + 360.f * el / m.duration, 0xFA20);
  canvas.setTextDatum(middle_center);

  if (linkMode == LINK_NONE) {
    canvas.setFont(&fonts::efontJA_16);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("Bluetooth :", c, 105);
    canvas.drawString("M5Dial Music", c, 125);
    canvas.setTextColor(0xAD55);
    canvas.drawString("iPhone / Android / PC", c, 150);
  } else if (linkMode == LINK_HID) {
    canvas.setFont(&fonts::efontJA_24);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("Connecté", c, 108);
    canvas.setFont(&fonts::efontJA_16);
    canvas.setTextColor(0xAD55);
    canvas.drawString("Télécommande", c, 140);
  } else {
    // Icône lecture / pause
    if (m.playing) {
      canvas.fillRect(c - 9, 52, 6, 18, TFT_WHITE);
      canvas.fillRect(c + 3, 52, 6, 18, TFT_WHITE);
    } else {
      canvas.fillTriangle(c - 7, 52, c - 7, 70, c + 9, 61, TFT_WHITE);
    }
    canvas.setFont(&fonts::efontJA_24);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(fit(m.title.empty() ? "—" : m.title, 190).c_str(), c, 105);
    canvas.setFont(&fonts::efontJA_16);
    canvas.setTextColor(0xAD55);
    canvas.drawString(fit(m.artist, 170).c_str(), c, 137);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(mmss(el) + " / " + mmss(m.duration), c, 178);
  }
  canvas.pushSprite(0, 0);
}

// ---------- Programme ----------
void setup() {
  auto cfg = M5.config();
  M5Dial.begin(cfg, true, false);  // encodeur oui, RFID non
  M5Dial.BtnA.setHoldThresh(2000);
  Serial.begin(115200);

  int st; float r, e;  // auto-test du parseur AMS
  assert(parsePlaybackInfo("1,1.0,12.5", st, r, e) && st == 1 && e == 12.5f);

  canvas.setColorDepth(16);
  canvas.createSprite(240, 240);  // ~115 Ko en RAM interne, double buffer sans scintillement
  startBle();
}

void loop() {
  M5Dial.update();
  if (needSetup) setupAms();

  // Encodeur -> volume (accumule les impulsions jusqu'à un cran complet)
  static long encLast = M5Dial.Encoder.read();
  long enc = M5Dial.Encoder.read();
  while (enc - encLast >= ENC_STEPS_PER_DETENT) { sendCmd(CMD_VOL_UP);   encLast += ENC_STEPS_PER_DETENT; }
  while (encLast - enc >= ENC_STEPS_PER_DETENT) { sendCmd(CMD_VOL_DOWN); encLast -= ENC_STEPS_PER_DETENT; }

  auto t = M5Dial.Touch.getDetail();
  if (M5Dial.BtnA.wasClicked() || t.wasClicked()) sendCmd(CMD_TOGGLE);  // au relâcher, pour distinguer l'appui long

  // Appui long : coupe l'alimentation batterie (relâche le maintien GPIO46).
  // Sur USB, le courant reste : le M5Dial passe en veille profonde, débrancher/rebrancher pour relancer.
  if (M5Dial.BtnA.wasHold()) {
    canvas.fillScreen(TFT_BLACK);
    canvas.setFont(&fonts::efontJA_24);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("Arrêt...", 120, 120);
    canvas.pushSprite(0, 0);
    delay(800);
    M5Dial.Display.sleep();
    M5Dial.Power.powerOff();
  }
  if (t.wasFlicked() || t.wasDragged()) {
    int dx = t.distanceX();
    if (dx > SWIPE_MIN_PX) sendCmd(CMD_NEXT);
    else if (dx < -SWIPE_MIN_PX) sendCmd(CMD_PREV);
  }

  // Redessin : immédiat sur changement, sinon 4 Hz pour faire avancer l'arc
  static uint32_t lastDraw = 0;
  if (dirty || millis() - lastDraw > 250) {
    dirty = false;
    lastDraw = millis();
    draw();
  }
}
