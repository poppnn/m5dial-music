/*
 * M5Dial -> télécommande Apple Music (iPhone) via BLE "Apple Media Service" (AMS).
 *
 * Pourquoi AMS et pas A2DP/AVRCP : l'ESP32-S3 n'a PAS de Bluetooth Classic (BLE seulement),
 * donc ESP32-A2DP/AVRCP ne peut pas tourner sur le M5Dial. AMS est le service BLE natif
 * d'iOS : commandes (play/pause, piste +/-, volume +/-) ET métadonnées (titre, artiste,
 * durée, temps écoulé), sans app tierce. Il ne fournit pas la pochette -> fallback titre/artiste.
 *
 * Rendu : l'UI est composée dans un sprite 240x240, puis tournée ("toujours droit", IMU EXTERNE
 * sur le Port A, le M5Dial n'en a pas d'intégrée) bande par bande dans 2 petits buffers envoyés
 * en DMA. Tous les buffers sont en SRAM interne (MALLOC_CAP_DMA) : la StampS3 n'a pas de PSRAM.
 *
 * Android / Windows n'ont pas d'AMS : le M5Dial est aussi une télécommande BLE HID (touches
 * multimédia standard), reconnue nativement. Commandes seules, sans métadonnées.
 *
 * 4 pages, swipe vertical pour passer de l'une à l'autre : Musique, LED, Chrono, Micra (3D).
 *   Encodeur  gauche/droite : volume -/+ (LED : luminosité, Micra : tourne la voiture)
 *   Bouton central / tap    : play/pause (LED : on/off, Chrono : départ/pause, Micra : rotation auto)
 *   Bouton central 2 s      : extinction (sur batterie)
 *   Swipe gauche / droite   : piste suivante / précédente (LED : couleur, Chrono : remise à zéro)
 */
#include <M5Dial.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <mutex>
#include <soc/soc_memory_layout.h>  // esp_ptr_dma_capable (IDF 4.4)
#include <algorithm>
#include "logo.h"
#include "model.h"

// ---------- AMS (spec Apple "Apple Media Service") ----------
static const char* AMS_SVC = "89D3502B-0F36-433A-8EF4-C502AD55F8DC";
static const char* AMS_RC  = "9B3C81D8-57B1-4A8A-B8DF-0E56F7CA51C2";  // Remote Command
static const char* AMS_EU  = "2F7CABCE-808D-411F-9A0C-BB92BA96C102";  // Entity Update

enum : uint8_t { ENT_PLAYER = 0, ENT_TRACK = 2 };
enum : uint8_t { PLAYER_PLAYBACK_INFO = 1, PLAYER_VOLUME = 2 };
enum : uint8_t { TRACK_ARTIST = 0, TRACK_TITLE = 2, TRACK_DURATION = 3 };
enum : uint8_t { CMD_TOGGLE = 2, CMD_NEXT = 3, CMD_PREV = 4, CMD_VOL_UP = 5, CMD_VOL_DOWN = 6 };

// Calibration matérielle : impulsions de l'encodeur par cran (4 sur le M5Dial testé par M5Stack).
#define ENC_STEPS_PER_DETENT 4
#define SWIPE_MIN_PX         40

// "Toujours droit" (IMU externe). Calibration : IMU_DIR = -1 si l'écran tourne dans le mauvais sens.
#define IMU_DIR        1
#define IMU_TAU_S      0.40f  // passe-bas de la gravité : les vibrations (> ~1 Hz) sont écrasées
#define MIN_TILT_G     0.35f  // gravité dans le plan de l'écran trop faible (posé à plat) : angle gelé
#define ROT_HYST_DEG   8.0f   // hystérésis : on ne suit un nouvel angle qu'au-delà de 8°
#define ROT_TAU_S      0.12f  // lissage visuel de la rotation affichée

// Animations
#define ARC_TAU_S      0.25f  // l'arc de progression rejoint sa valeur en douceur
#define VOL_TAU_S      0.12f
#define VOL_SHOW_MS    1500
#define SLIDE_OUT_MS   200
#define SLIDE_IN_MS    280
#define COLOR_TAU_S    0.18f  // fondu de la couleur de l'anneau LED
#define POP_MS         380    // rebond de la pastille de couleur
#define PAGE_OUT_MS    170    // glissement vertical Musique <-> LED : sortie puis entrée
#define PAGE_IN_MS     280
#define FADE_MS        300  // fondu écran de démarrage <-> interface (sortie puis entrée)

// Batterie
#define CPU_MHZ        80     // largement assez pour l'UI ; 80 MHz = minimum pour la radio BLE
#define DIM_AFTER_MS   20000  // sans geste : écran atténué...
#define DIM_LEVEL      0.12f  // ... à 12 % de la luminosité normale
#define DIM_TAU_S      0.35f
#define SLEEP_AFTER_MS (10UL * 60 * 1000)  // aucun appareil connecté pendant 10 min : extinction

// ---------- État partagé (écrit par la tâche BLE, lu par loop) ----------
struct Media {
  std::string title, artist;
  float duration = 0, elapsed = 0, rate = 0;
  uint32_t stamp = 0;  // millis() de la dernière mise à jour de elapsed
  float volume = -1;  // 0..1 (AMS), -1 = inconnu
  bool playing = false;
};
static Media media;
static std::mutex mtx;
enum Link : uint8_t { LINK_NONE, LINK_HID, LINK_AMS };  // HID = Android/Windows, AMS = iPhone
static volatile Link linkMode = LINK_NONE;
static volatile bool dirty = true, needSetup = false, titleChanged = false, volPing = false;
static volatile uint16_t connHandle = 0;
static NimBLERemoteCharacteristic* rcChar = nullptr;
static NimBLECharacteristic* hidInput = nullptr;

// Rapport HID "Consumer Control" : 1 bit par touche multimédia (bits 0..4), 3 bits de bourrage.
static const uint8_t hidReportMap[] = {
  0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x01,  // Usage Page Consumer, Collection, Report ID 1
  0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x05,  // 5 champs de 1 bit
  0x09, 0xCD, 0x09, 0xB5, 0x09, 0xB6, 0x09, 0xE9, 0x09, 0xEA,  // Play/Pause, Next, Prev, Vol+, Vol-
  0x81, 0x02, 0x95, 0x03, 0x81, 0x01, 0xC0};

static M5Canvas canvas(&M5Dial.Display);  // UI dans le repère "droit"
// 2 bandes de sortie (ping-pong) : le CPU tourne la bande N pendant que le DMA envoie la bande N-1.
#define BAND_H 40  // 240x40x2 = 19,2 Ko par bande, 6 bandes par image
static M5Canvas band[2] = {M5Canvas(&M5Dial.Display), M5Canvas(&M5Dial.Display)};

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
    if (d[1] == TRACK_TITLE) {
      titleChanged = v != media.title;
      media.title = v;
    }
    else if (d[1] == TRACK_ARTIST) media.artist = v;
    else if (d[1] == TRACK_DURATION) media.duration = atof(v.c_str());
  } else if (d[0] == ENT_PLAYER && d[1] == PLAYER_VOLUME) {
    media.volume = atof(v.c_str());
    volPing = true;
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
  const uint8_t player[] = {ENT_PLAYER, PLAYER_PLAYBACK_INFO, PLAYER_VOLUME};
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

// ---------- LED Govee H7090 : le M5Dial est client BLE de la barre LED ----------
// Protocole Govee non officiel (rétro-ingénierie communautaire) : trames de 20 octets
// 0x33 <cmd> <données...> 0x00... <XOR des 19 premiers octets>. Le H7090 n'a pas de variante connue.
static const char* GOVEE_SVC = "00010203-0405-0607-0809-0a0b0c0d1910";
static const char* GOVEE_WR  = "00010203-0405-0607-0809-0a0b0c0d2b11";

struct LedColor { const char* name; uint8_t r, g, b; };
static const LedColor LED_COLORS[] = {
  {"Blanc", 255, 255, 255}, {"Rouge", 255, 0, 0},   {"Orange", 255, 90, 0},
  {"Jaune", 255, 200, 0},   {"Vert", 0, 255, 40},   {"Cyan", 0, 220, 255},
  {"Bleu", 0, 60, 255},     {"Violet", 150, 0, 255}, {"Rose", 255, 0, 140}};
static const int LED_COLOR_COUNT = sizeof(LED_COLORS) / sizeof(LED_COLORS[0]);

static NimBLEClient* goveeClient = nullptr;
static NimBLERemoteCharacteristic* goveeWr = nullptr;
static NimBLEAddress goveeAddr;
static volatile bool goveeFound = false;
static uint32_t goveeRetry = 0;
static bool ledOn = true;  // état supposé : la barre ne renvoie pas son état
// Luminosité en % : le H7090 attend 0-100 (au-delà il sature : avec 0-255, seul le bas de la course agissait).
#define GOVEE_BRI_MAX 100
static int ledColor = 0, ledBri = 100, ledBriSent = 100;

class GoveeScanCB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    if (d->getName().find("H7090") == std::string::npos) return;  // nom annoncé : "ihoment_H7090_XXXX" / "Govee_H7090_XXXX"
    goveeAddr = d->getAddress();
    goveeFound = true;
    NimBLEDevice::getScan()->stop();
  }
};

static void goveeSend(std::initializer_list<uint8_t> payload) {
  if (!goveeWr) return;
  uint8_t p[20] = {0};
  size_t i = 0;
  for (uint8_t b : payload) p[i++] = b;
  for (i = 0; i < 19; i++) p[19] ^= p[i];
  goveeWr->writeValue(p, sizeof(p), false);
}

static void goveeSendColor() {
  const LedColor& c = LED_COLORS[ledColor];
  goveeSend({0x33, 0x05, 0x02, c.r, c.g, c.b});  // RVB "manuel" standard Govee
  // shortcut: format RGBIC étendu envoyé aussi, ignoré par les modèles qui ne le gèrent pas ; garder le seul qui marche une fois testé sur le H7090.
  goveeSend({0x33, 0x05, 0x15, 0x01, c.r, c.g, c.b, 0, 0, 0, 0, 0, 0xFF, 0x7F});
}

// Connexion / reconnexion. Ne scanne que sur la page LED, pour ne pas vider la batterie.
static void goveeService(bool ledPage) {
  if (goveeWr && goveeClient && goveeClient->isConnected()) {
    if (ledBri != ledBriSent && millis() - goveeRetry > 60) {  // luminosité limitée à ~16 trames/s
      goveeSend({0x33, 0x04, (uint8_t)(ledBri * GOVEE_BRI_MAX / 100)});
      ledBriSent = ledBri;
      goveeRetry = millis();
    }
    return;
  }
  goveeWr = nullptr;
  if (goveeFound) {
    goveeFound = false;
    if (!goveeClient) {
      goveeClient = NimBLEDevice::createClient();
      goveeClient->setConnectTimeout(3000);
    }
    if (goveeClient->connect(goveeAddr)) {
      NimBLERemoteService* s = goveeClient->getService(GOVEE_SVC);
      goveeWr = s ? s->getCharacteristic(GOVEE_WR) : nullptr;
    }
    goveeRetry = millis();
    dirty = true;
  } else if (ledPage && !NimBLEDevice::getScan()->isScanning() && millis() - goveeRetry > 8000) {
    goveeRetry = millis();
    NimBLEDevice::getScan()->start(4000, false);  // asynchrone : l'UI continue de tourner
  }
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

// ---------- Maths : lissage, easing, angles ----------
static float wrap180(float a) { while (a > 180) a -= 360; while (a < -180) a += 360; return a; }
// Lissage exponentiel indépendant du FPS : x rejoint sa cible avec la constante de temps tau.
static float smoothK(float dt, float tau) { return 1.f - expf(-dt / tau); }
static float easeOutCubic(float t) { t = 1.f - t; return 1.f - t * t * t; }
static float easeInCubic(float t) { return t * t * t; }
static float easeOutBack(float t) { t -= 1.f; return 1.f + 2.70158f * t * t * t + 1.70158f * t * t; }  // dépasse puis revient

// ---------- IMU : gravité filtrée -> angle de l'écran ----------
static bool imuOk = false;
static float gx = 0, gy = 0, refAngle = 0, targetRot = 0, rot = 0;

static float rawAngle() { return IMU_DIR * atan2f(gx, gy) * RAD_TO_DEG; }

static void updateImu(float dt) {
  float ax, ay, az;
  if (!imuOk) return;
  M5.Imu.update();
  if (!M5.Imu.getAccel(&ax, &ay, &az)) return;
  float k = smoothK(dt, IMU_TAU_S);  // passe-bas sur le VECTEUR gravité (pas sur l'angle : pas de saut à ±180°)
  gx += (ax - gx) * k;
  gy += (ay - gy) * k;
  if (gx * gx + gy * gy < MIN_TILT_G * MIN_TILT_G) return;
  float a = wrap180(rawAngle() - refAngle);
  if (fabsf(wrap180(a - targetRot)) > ROT_HYST_DEG) targetRot = a;
}

// ---------- Animations ----------
static float arcShown = 0, volShown = 0;
static uint32_t volUntil = 0, slideStart = 0;
static int slideDir = 0;  // +1 le texte part à droite, -1 à gauche, 0 aucun glissement
static std::string shownTitle, shownArtist;

static void startSlide(int dir, bool inOnly) {
  slideDir = dir;
  slideStart = millis() - (inOnly ? SLIDE_OUT_MS : 0);
}

// Décalage horizontal du texte : sortie (easeIn) puis entrée par le côté opposé (easeOut).
static float slideOffset(uint32_t now, bool& outPhase) {
  outPhase = false;
  if (!slideDir) return 0;
  uint32_t t = now - slideStart;
  if (t < SLIDE_OUT_MS) { outPhase = true; return slideDir * 240.f * easeInCubic(t / (float)SLIDE_OUT_MS); }
  if (t < SLIDE_OUT_MS + SLIDE_IN_MS) return -slideDir * 240.f * (1.f - easeOutCubic((t - SLIDE_OUT_MS) / (float)SLIDE_IN_MS));
  slideDir = 0;
  return 0;
}

// ---------- Écran de démarrage : affiché tant qu'aucun appareil n'est connecté ----------
static bool bootShown = true, fadeTo = true;
static uint32_t fadeStart = 0;  // 0 = pas de fondu en cours
static uint8_t baseBri = 128;
static float fadeLevel = 1, dimLevel = 1;  // rétroéclairage = baseBri x fondu x atténuation
static uint32_t lastInput = 0;             // dernier geste (molette, bouton, toucher)

// Fondu au noir par le rétroéclairage : l'écran courant s'éteint, bascule, le nouveau s'allume.
static void updateFade(uint32_t now) {
  bool wantBoot = linkMode == LINK_NONE;
  if (!fadeStart && wantBoot != bootShown) { fadeStart = now | 1; fadeTo = wantBoot; }
  if (!fadeStart) return;
  uint32_t t = now - fadeStart;
  float level;
  if (t < FADE_MS) {
    level = 1.f - easeInCubic(t / (float)FADE_MS);
  } else {
    bootShown = fadeTo;
    level = t < 2 * FADE_MS ? easeOutCubic((t - FADE_MS) / (float)FADE_MS) : 1.f;
    if (t >= 2 * FADE_MS) fadeStart = 0;
  }
  fadeLevel = level;
}

// Atténue l'écran après DIM_AFTER_MS sans geste, le rallume en douceur au premier geste.
// Retourne true pendant la transition (-> FPS élevé).
static bool updateBacklight(uint32_t now, float dt) {
  static uint8_t applied = 0;
  float target = now - lastInput > DIM_AFTER_MS ? DIM_LEVEL : 1.f;
  dimLevel += (target - dimLevel) * smoothK(dt, DIM_TAU_S);
  uint8_t bri = baseBri * fadeLevel * dimLevel;
  if (bri != applied) M5Dial.Display.setBrightness(applied = bri);
  return fabsf(target - dimLevel) > 0.01f;
}

// Logo centré + arc qui tourne sur le bord de l'écran + texte discret.
static void drawBoot(uint32_t now) {
  const int c = 120;
  if (LOGO_W) canvas.pushImage(c - LOGO_W / 2, 100 - LOGO_H / 2, LOGO_W, LOGO_H, logo_data);
  float a = (now % 1200) * (360.f / 1200) - 180;  // 1 tour en 1,2 s
  canvas.fillArc(c, c, 119, 114, 0, 360, 0x2104);
  canvas.fillArc(c, c, 119, 114, a, a + 70, 0xFA20);
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0xAD55);
  canvas.drawString("Connexion Bluetooth...", c, 186);
  canvas.setTextColor(0x5AEB);
  canvas.drawString("M5Dial Music", c, 206);  // nom à choisir dans les réglages Bluetooth
}

enum Page : uint8_t { PAGE_MUSIC, PAGE_LED, PAGE_CHRONO, PAGE_CAR, PAGE_COUNT };
static Page page = PAGE_MUSIC;

// ---------- Page Chrono ----------
static bool chronoRun = false;
static uint32_t chronoStart = 0, chronoAcc = 0;  // temps cumulé des segments terminés (ms)

static uint32_t chronoMs() { return chronoAcc + (chronoRun ? millis() - chronoStart : 0); }

static void chronoToggle() {
  if (chronoRun) chronoAcc += millis() - chronoStart;
  else chronoStart = millis();
  chronoRun = !chronoRun;
}

// Grand afficheur 7 segments, anneau = trotteuse (1 tour par minute), centièmes en dessous.
static bool drawChrono() {
  const int c = 120;
  uint32_t ms = chronoMs(), s = ms / 1000;
  canvas.fillArc(c, c, 119, 111, 0, 360, 0x2104);
  float sweep = (ms % 60000) / 60000.f;
  if (sweep > 0.001f) canvas.fillArc(c, c, 119, 111, -90, -90 + 360.f * sweep, chronoRun ? 0xFA20 : TFT_WHITE);
  char b[16];
  if (s >= 3600) snprintf(b, sizeof(b), "%u:%02u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
  else snprintf(b, sizeof(b), "%02u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::Font7);
  canvas.setTextSize(s >= 3600 ? 0.62f : 1.f);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(b, c, 108);
  canvas.setTextSize(1);
  snprintf(b, sizeof(b), ".%02u", (unsigned)(ms / 10 % 100));
  canvas.setFont(&fonts::efontJA_24);
  canvas.drawString(b, c, 152);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0xAD55);
  canvas.drawString(chronoRun ? "Appui : pause" : ms ? "Glisser : remise à zéro" : "Appui : départ", c, 186);
  return chronoRun;
}

// ---------- Page Micra : maillage 3D (model.h) qui tourne sur lui-même ----------
// Rendu logiciel : projection orthographique, faces arrière éliminées, ombrage plat en niveaux de gris
// (lumière fixe côté caméra), tri du plus loin au plus proche (algorithme du peintre).
#define CAR_SPEED   0.7f    // rad/s
#define CAR_PITCH   0.31f   // caméra un peu au-dessus (18°)
#define CAR_SCALE   0.095f  // px par unité de model.h ([-1000, 1000])
static float carYaw = 0.5f;
static bool carSpin = true;
static float mvx[MODEL_NV], mvy[MODEL_NV], mvz[MODEL_NV];  // sommets dans le repère caméra
struct CarTri { int16_t z; uint16_t i; uint8_t g; };
static CarTri carTris[MODEL_NT];

static bool drawCar(float dt) {
  if (carSpin) carYaw = fmodf(carYaw + dt * CAR_SPEED, 2 * PI);
  const float cy = cosf(carYaw), sy = sinf(carYaw), cp = cosf(CAR_PITCH), sp = sinf(CAR_PITCH);
  for (int i = 0; i < MODEL_NV; i++) {
    float x = model_v[i][0], y = model_v[i][1], z = model_v[i][2];
    float xr = x * cy + z * sy, zr = -x * sy + z * cy;
    mvx[i] = xr;
    mvy[i] = y * cp - zr * sp;
    mvz[i] = y * sp + zr * cp;
  }
  auto sx = [](int i) { return (int)(120 + mvx[i] * CAR_SCALE); };
  auto sy2 = [](int i) { return (int)(128 - mvy[i] * CAR_SCALE); };
  const float L[3] = {-0.42f, 0.63f, 0.65f};  // lumière (normée)
  int n = 0;
  for (int t = 0; t < MODEL_NT; t++) {
    int a = model_t[t][0], b = model_t[t][1], c = model_t[t][2];
    int32_t cross = (sx(b) - sx(a)) * (sy2(c) - sy2(a)) - (sy2(b) - sy2(a)) * (sx(c) - sx(a));
    if (cross >= 0) continue;  // face arrière (y écran vers le bas : sens horaire = face avant)
    float ux = mvx[b] - mvx[a], uy = mvy[b] - mvy[a], uz = mvz[b] - mvz[a];
    float vx = mvx[c] - mvx[a], vy = mvy[c] - mvy[a], vz = mvz[c] - mvz[a];
    float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    float len = sqrtf(nx * nx + ny * ny + nz * nz) + 1e-6f;
    float d = (nx * L[0] + ny * L[1] + nz * L[2]) / len;
    carTris[n++] = {(int16_t)((mvz[a] + mvz[b] + mvz[c]) / 3), (uint16_t)t, (uint8_t)(35 + 220 * (d > 0 ? d : 0))};
  }
  std::sort(carTris, carTris + n, [](const CarTri& p, const CarTri& q) { return p.z < q.z; });
  for (int k = 0; k < n; k++) {
    const uint16_t* f = model_t[carTris[k].i];
    uint8_t g = carTris[k].g;
    canvas.fillTriangle(sx(f[0]), sy2(f[0]), sx(f[1]), sy2(f[1]), sx(f[2]), sy2(f[2]), canvas.color565(g, g, g));
  }
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0xAD55);
  canvas.drawString("Nissan Micra K10 - 1982", 120, 38);
  return carSpin;
}
static float ledBriShown = 1;

// Page LED : anneau = luminosité, en fondu vers la nouvelle couleur ; la pastille et le nom de l'ancienne
// couleur sortent en glissant, la nouvelle pastille entre avec un rebond.
static bool drawLedPage(float dt, float off, bool outPhase) {
  static int shownColor = 0;
  static float rgb[3] = {255, 255, 255};
  static uint32_t popStart = 0;
  const int c = 120;
  uint32_t now = millis();
  if (!outPhase && shownColor != ledColor) { shownColor = ledColor; popStart = now; }
  const LedColor& lc = LED_COLORS[shownColor];
  const LedColor& tc = LED_COLORS[ledColor];
  const float trgb[3] = {(float)tc.r, (float)tc.g, (float)tc.b};
  float k = smoothK(dt, COLOR_TAU_S), dist = 0;
  for (int i = 0; i < 3; i++) { rgb[i] += (trgb[i] - rgb[i]) * k; dist += fabsf(trgb[i] - rgb[i]); }

  float target = ledOn ? ledBri / 100.f : 0;
  ledBriShown += (target - ledBriShown) * smoothK(dt, VOL_TAU_S);
  uint16_t arcCol = canvas.color565(rgb[0], rgb[1], rgb[2]);
  uint32_t tp = now - popStart;
  float radius = 34.f * (tp < POP_MS ? 0.6f + 0.4f * easeOutBack(tp / (float)POP_MS) : 1.f);

  canvas.fillArc(c, c, 119, 111, 0, 360, 0x2104);
  if (ledBriShown > 0.005f) canvas.fillArc(c, c, 119, 111, -90, -90 + 360.f * ledBriShown, arcCol);
  canvas.fillCircle(c + (int)off, 92, (int)radius, ledOn ? canvas.color565(lc.r, lc.g, lc.b) : 0x2104);
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_24);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(lc.name, c + (int)off, 150);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0xAD55);
  const char* status = !goveeWr ? "Recherche H7090..." : ledOn ? "LED allumées" : "LED éteintes";
  canvas.drawString(status, c, 182);
  return fabsf(target - ledBriShown) > 0.004f || dist > 3 || tp < POP_MS;
}

// Changement de page : tout l'écran glisse verticalement (sortie easeIn, puis entrée easeOut), la page
// bascule à mi-parcours. Le décalage est appliqué au moment de l'envoi (pushFrame), sans 2e tampon d'image.
static int pageDir = 0;  // -1 = le contenu part vers le haut, +1 vers le bas, 0 = aucun glissement
static bool pageFlipped = false;
static uint32_t pageStart = 0;
static float frameShift = 0;  // décalage vertical de l'image (px, repère de l'interface)

static void startPageSlide(int dir) {
  if (pageDir) return;
  pageDir = dir;
  pageFlipped = false;
  pageStart = millis();
}

static float pageOffset(uint32_t now) {
  if (!pageDir) return 0;
  uint32_t t = now - pageStart;
  if (t < PAGE_OUT_MS) return pageDir * 240.f * easeInCubic(t / (float)PAGE_OUT_MS);
  if (!pageFlipped) {
    pageFlipped = true;
    page = (Page)((page + (pageDir < 0 ? 1 : PAGE_COUNT - 1)) % PAGE_COUNT);  // vers le haut = page suivante
    goveeRetry = 0;  // cherche les LED tout de suite
  }
  if (t < PAGE_OUT_MS + PAGE_IN_MS)
    return -pageDir * 240.f * (1.f - easeOutCubic((t - PAGE_OUT_MS) / (float)PAGE_IN_MS));
  pageDir = 0;
  return 0;
}

// Envoie la frame : rendu de la bande k dans band[k&1] pendant que le DMA envoie band[(k-1)&1].
// La transaction SPI reste ouverte (startWrite dans setup) : pushImageDMA rend la main tout de suite,
// et le dernier transfert se termine pendant que loop() lit l'encodeur et compose la frame suivante.
static void pushFrame() {
  auto& d = M5Dial.Display;
  const uint16_t* src = (const uint16_t*)canvas.getBuffer();
  int s = (int)lroundf(frameShift);
  float r = rot * DEG_TO_RAD;
  for (int k = 0; k < 240 / BAND_H; k++) {
    M5Canvas& b = band[k & 1];  // ce buffer a fini d'être envoyé : waitDMA() a précédé la bande k-1
    int y = k * BAND_H;
    if (fabsf(rot) < 0.5f) {
      uint16_t* dst = (uint16_t*)b.getBuffer();
      for (int row = 0; row < BAND_H; row++) {  // ligne source décalée de s (glissement de page), noir hors image
        int sr = y + row - s;
        if (sr >= 0 && sr < 240) memcpy(dst + row * 240, src + sr * 240, 240 * 2);
        else memset(dst + row * 240, 0, 240 * 2);
      }
    } else {
      if (s) b.fillScreen(TFT_BLACK);  // image décalée : elle ne recouvre plus tout le disque
      // décalage (0, s) de l'interface tourné de rot (sens horaire à l'écran)
      canvas.pushRotateZoom(&b, 120 - s * sinf(r), 120 - y + s * cosf(r), rot, 1.f, 1.f);  // coins hors du disque : invisibles
    }
    d.waitDMA();
    d.pushImageDMA(0, y, 240, BAND_H, (const lgfx::swap565_t*)b.getBuffer());  // format natif du panneau : 0 conversion
  }
}

// Rend une frame. Retourne true tant qu'une animation est en cours (-> FPS élevé).
static bool draw(float dt) {
  Media m;
  {
    std::lock_guard<std::mutex> lock(mtx);
    m = media;
  }
  uint32_t now = millis();
  float el = m.elapsed + (m.playing ? m.rate * (now - m.stamp) / 1000.f : 0);
  if (m.duration > 0 && el > m.duration) el = m.duration;

  float arcTarget = m.duration > 0 ? el / m.duration : 0;
  arcShown += (arcTarget - arcShown) * smoothK(dt, ARC_TAU_S);
  if (m.volume >= 0) volShown += (m.volume - volShown) * smoothK(dt, VOL_TAU_S);
  bool outPhase;
  float off = slideOffset(now, outPhase);
  if (!outPhase) { shownTitle = m.title; shownArtist = m.artist; }  // ancien texte figé pendant la sortie

  const int c = 120;
  canvas.fillScreen(TFT_BLACK);
  updateFade(now);
  bool dimming = updateBacklight(now, dt);
  frameShift = bootShown ? 0 : pageOffset(now);  // avant de dessiner : la page bascule à mi-glissement
  if (bootShown) {
    drawBoot(now);
    pushFrame();
    return true;  // spinner animé en continu
  }
  // Indicateur de page (un point par page, en bas) : swipe vertical pour changer de page
  for (int i = 0; i < PAGE_COUNT; i++)
    canvas.fillCircle(c + (2 * i - (PAGE_COUNT - 1)) * 6, 212, 2, i == page ? TFT_WHITE : 0x4208);
  if (page == PAGE_CHRONO || page == PAGE_CAR) {
    bool anim = page == PAGE_CHRONO ? drawChrono() : drawCar(dt);
    pushFrame();
    return anim || dimming || fadeStart || pageDir || fabsf(wrap180(targetRot - rot)) > 0.3f;
  }
  if (page == PAGE_LED) {
    bool anim = drawLedPage(dt, off, outPhase);
    pushFrame();
    return anim || dimming || fadeStart || slideDir || pageDir || fabsf(wrap180(targetRot - rot)) > 0.3f;
  }
  canvas.fillArc(c, c, 119, 111, 0, 360, 0x2104);  // piste de fond de l'arc
  if (arcShown > 0.002f) canvas.fillArc(c, c, 119, 111, -90, -90 + 360.f * arcShown, 0xFA20);

  // Anneau de volume intérieur : visible 1,5 s après un changement, fondu au noir sur les 300 dernières ms
  bool volVisible = (int32_t)(volUntil - now) > 0 && m.volume >= 0;
  if (volVisible) {
    float a = std::min(1.f, (volUntil - now) / 300.f);
    canvas.fillArc(c, c, 106, 101, 0, 360, canvas.color565(30 * a, 30 * a, 34 * a));
    if (volShown > 0.005f)
      canvas.fillArc(c, c, 106, 101, -90, -90 + 360.f * volShown, canvas.color565(70 * a, 160 * a, 255 * a));
  }
  canvas.setTextDatum(middle_center);

  if (linkMode == LINK_HID) {
    canvas.setFont(&fonts::efontJA_24);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("Connecté", c, 108);
    canvas.setFont(&fonts::efontJA_16);
    canvas.setTextColor(0xAD55);
    canvas.drawString("Télécommande", c, 140);
  } else if (linkMode == LINK_AMS) {  // LINK_NONE : rien, l'interface est en train de disparaître
    // Icône lecture / pause
    if (m.playing) {
      canvas.fillRect(c - 9, 52, 6, 18, TFT_WHITE);
      canvas.fillRect(c + 3, 52, 6, 18, TFT_WHITE);
    } else {
      canvas.fillTriangle(c - 7, 52, c - 7, 70, c + 9, 61, TFT_WHITE);
    }
    int x = c + (int)off;
    canvas.setFont(&fonts::efontJA_24);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(fit(shownTitle.empty() ? "—" : shownTitle, 190).c_str(), x, 105);
    canvas.setFont(&fonts::efontJA_16);
    canvas.setTextColor(0xAD55);
    canvas.drawString(fit(shownArtist, 170).c_str(), x, 137);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(mmss(el) + " / " + mmss(m.duration), c, 178);
  }

  pushFrame();
  return dimming || fadeStart || slideDir || pageDir || volVisible || fabsf(wrap180(targetRot - rot)) > 0.3f || fabsf(arcTarget - arcShown) > 0.004f;
}

// ---------- Programme ----------
// Message, puis coupure : sur batterie, le maintien d'alimentation (GPIO46) est relâché -> 0 mA, le bouton
// WAKE rallume. Sur USB le courant reste : M5Unified passe en veille profonde (débrancher/rebrancher pour relancer).
// Le bouton du M5Dial (GPIO42) n'est pas une broche RTC : il ne peut pas réveiller une veille profonde.
static void shutdown(const char* msg) {
  canvas.fillScreen(TFT_BLACK);
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_24);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(msg, 120, 120);
  frameShift = 0;
  M5Dial.Display.setBrightness(baseBri);
  pushFrame();
  M5Dial.Display.endWrite();  // attend la fin du DMA et libère le bus
  delay(800);
  M5Dial.Display.sleep();
  M5Dial.Power.powerOff();
}

void setup() {
  setCpuFrequencyMhz(CPU_MHZ);  // ~-20 mA ; le SPI de l'écran (APB 80 MHz) et le BLE ne changent pas
  auto cfg = M5.config();
  cfg.external_imu = true;  // le M5Dial n'a pas d'IMU : cherche une Unit IMU sur le Port A
  M5Dial.begin(cfg, true, false);  // encodeur oui, RFID non
  M5Dial.BtnA.setHoldThresh(2000);
  baseBri = M5Dial.Display.getBrightness();
  Serial.begin(115200);

  int st; float r, e;  // auto-tests : parseur AMS et repliement d'angle
  assert(parsePlaybackInfo("1,1.0,12.5", st, r, e) && st == 1 && e == 12.5f);
  assert(fabsf(wrap180(350.f) + 10.f) < 1e-3f && fabsf(wrap180(-190.f) - 170.f) < 1e-3f);
  assert(fabsf(easeOutBack(0)) < 1e-4f && fabsf(easeOutBack(1) - 1) < 1e-4f && easeOutBack(0.7f) > 1);  // rebond

  // Référence "droit" = orientation au démarrage (volant au centre).
  float az;
  imuOk = M5.Imu.isEnabled() && M5.Imu.update() && M5.Imu.getAccel(&gx, &gy, &az);
  if (imuOk) refAngle = rawAngle();

  // Buffers d'affichage alloués AVANT le BLE (blocs contigus) en SRAM interne : setPsram(false)
  // => M5GFX utilise heap_caps_malloc(MALLOC_CAP_DMA). Total 115 + 2x19 = ~154 Ko.
  canvas.setPsram(false);
  canvas.setColorDepth(16);
  bool ok = canvas.createSprite(240, 240);
  for (auto& b : band) {
    b.setPsram(false);
    b.setColorDepth(16);
    ok = ok && b.createSprite(240, BAND_H);
  }
  assert(ok && esp_ptr_dma_capable(canvas.getBuffer()) && esp_ptr_dma_capable(band[1].getBuffer()));
  Serial.printf("Affichage : SRAM DMA libre %u o (plus grand bloc %u o)\n",
                heap_caps_get_free_size(MALLOC_CAP_DMA), heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
  M5Dial.Display.startWrite();  // bus SPI gardé : les transferts DMA ne bloquent plus loop()
  startBle();
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(new GoveeScanCB());
  scan->setActiveScan(true);  // le nom "..._H7090_..." est souvent dans la réponse au scan
  lastInput = millis();
}

void loop() {
  static uint32_t lastUs = micros();
  uint32_t nowUs = micros();
  float dt = (nowUs - lastUs) / 1e6f;
  lastUs = nowUs;

  M5Dial.update();
  if (needSetup) setupAms();
  updateImu(dt);
  rot = wrap180(rot + wrap180(targetRot - rot) * smoothK(dt, ROT_TAU_S));  // par le chemin le plus court

  goveeService(page == PAGE_LED);

  // Encodeur (accumule les impulsions jusqu'à un cran complet) : volume, ou luminosité sur la page LED
  static long encLast = M5Dial.Encoder.read();
  long enc = M5Dial.Encoder.read();
  int detents = 0;
  while (enc - encLast >= ENC_STEPS_PER_DETENT) { detents++; encLast += ENC_STEPS_PER_DETENT; }
  while (encLast - enc >= ENC_STEPS_PER_DETENT) { detents--; encLast -= ENC_STEPS_PER_DETENT; }
  if (bootShown) {
    // écran de démarrage : commandes ignorées (seul l'appui long d'extinction reste actif)
  } else if (page == PAGE_LED) {
    if (detents) ledBri = constrain(ledBri + detents * 5, 5, 100);  // 5 % par cran
  } else if (page == PAGE_CAR) {
    if (detents) { carYaw += detents * 15 * DEG_TO_RAD; dirty = true; }  // tourner la voiture à la main
  } else {  // Musique et Chrono : volume
    for (int i = 0; i < abs(detents); i++) sendCmd(detents > 0 ? CMD_VOL_UP : CMD_VOL_DOWN);
    if (detents) volPing = true;
  }
  if (volPing) { volPing = false; volUntil = millis() + VOL_SHOW_MS; }

  auto t = M5Dial.Touch.getDetail();
  uint32_t now = millis();
  if (detents || M5Dial.BtnA.isPressed() || t.isPressed()) lastInput = now;  // réveille l'écran atténué

  // Aucun appareil connecté (ni geste) pendant 10 min : extinction pour ne pas vider la batterie
  static uint32_t lastActive = 0;
  if (linkMode != LINK_NONE || lastInput == now) lastActive = now;
  if (now - lastActive > SLEEP_AFTER_MS) shutdown("Veille...");

  if (!bootShown && (M5Dial.BtnA.wasClicked() || t.wasClicked())) {  // au relâcher, pour distinguer l'appui long
    if (page == PAGE_LED) {
      ledOn = !ledOn;
      goveeSend({0x33, 0x01, (uint8_t)ledOn});
    } else if (page == PAGE_CHRONO) {
      chronoToggle();
    } else if (page == PAGE_CAR) {
      carSpin = !carSpin;
    } else {
      sendCmd(CMD_TOGGLE);
    }
    dirty = true;
  }

  if (M5Dial.BtnA.wasHold()) shutdown("Arrêt...");  // appui long 2 s
  if (!bootShown && (t.wasFlicked() || t.wasDragged())) {
    // Geste ramené dans le repère de l'interface, qui peut être tournée de rot degrés.
    float r = rot * DEG_TO_RAD;
    float dx = cosf(r) * t.distanceX() + sinf(r) * t.distanceY();
    float dy = -sinf(r) * t.distanceX() + cosf(r) * t.distanceY();
    if (fabsf(dy) > fabsf(dx) && fabsf(dy) > SWIPE_MIN_PX) {  // vertical : change de page, l'écran suit le doigt
      startPageSlide(dy < 0 ? -1 : +1);
    } else if (fabsf(dx) > SWIPE_MIN_PX) {
      int dir = dx > 0 ? +1 : -1;
      if (page == PAGE_LED) {
        ledColor = (ledColor + dir + LED_COLOR_COUNT) % LED_COLOR_COUNT;
        ledOn = true;
        goveeSendColor();
        startSlide(dir, false);
      } else if (page == PAGE_CHRONO) {
        chronoRun = false;  // remise à zéro
        chronoAcc = 0;
      } else if (page == PAGE_MUSIC) {
        sendCmd(dir < 0 ? CMD_NEXT : CMD_PREV);  // vers la gauche = suivante (comme un carrousel)
        startSlide(dir, false);
      }
    }
    dirty = true;
  }
  if (titleChanged) {  // piste changée depuis l'iPhone : le nouveau titre entre par la droite
    titleChanged = false;
    if (!slideDir) startSlide(-1, true);
  }

  // ~60 FPS max pendant une animation, 4 Hz au repos (économie de batterie)
  static uint32_t lastDraw = 0, lastFrameUs = micros();
  static bool animating = true;
  if (dirty || millis() - lastDraw >= (animating ? 16u : 250u)) {
    float fdt = (micros() - lastFrameUs) / 1e6f;
    lastFrameUs = micros();
    dirty = false;
    lastDraw = millis();
    animating = draw(fdt);
  }
  // Rend la main à FreeRTOS : le cœur s'arrête (tâche idle) au lieu de tourner à vide. 10 ms au repos suffisent :
  // l'encodeur compte en matériel (PCNT) et le BLE vit dans sa propre tâche.
  delay(animating || dirty ? 1 : 10);
}
