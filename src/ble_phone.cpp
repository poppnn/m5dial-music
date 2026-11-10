// Pourquoi AMS et pas A2DP/AVRCP : l'ESP32-S3 n'a PAS de Bluetooth Classic (BLE seulement). AMS est le
// service BLE natif d'iOS : commandes ET métadonnées (titre, artiste, durée, temps écoulé), sans app tierce.
// Il ne fournit pas la pochette. Android / Windows n'ont pas d'AMS : touches multimédia HID à la place.
#include <Arduino.h>
#include "ble_phone.h"
#include <NimBLEHIDDevice.h>
#include "ancs.h"
#include "app.h"
#include "clock.h"
#include "config.h"
#include "gb.h"
#include "settings.h"
#include "text.h"

// ---------- AMS (spec Apple "Apple Media Service") ----------
static const char* AMS_SVC = "89D3502B-0F36-433A-8EF4-C502AD55F8DC";
static const char* AMS_RC  = "9B3C81D8-57B1-4A8A-B8DF-0E56F7CA51C2";  // Remote Command
static const char* AMS_EU  = "2F7CABCE-808D-411F-9A0C-BB92BA96C102";  // Entity Update

enum : uint8_t { ENT_PLAYER = 0, ENT_QUEUE = 1, ENT_TRACK = 2 };
enum : uint8_t { QUEUE_SHUFFLE = 2, QUEUE_REPEAT = 3 };
enum : uint8_t { PLAYER_PLAYBACK_INFO = 1, PLAYER_VOLUME = 2 };
enum : uint8_t { TRACK_ARTIST = 0, TRACK_TITLE = 2, TRACK_DURATION = 3 };

static Media media;
static std::mutex mtx;
volatile Link linkMode = LINK_NONE;
volatile bool needSetup = false, titleChanged = false, volPing = false;
volatile uint16_t connHandle = 0;
static NimBLERemoteCharacteristic* rcChar = nullptr;
static NimBLECharacteristic* hidInput = nullptr;
static volatile uint32_t linkGen = 0;  // +1 à chaque déconnexion : une découverte GATT périmée ne se déclare pas connectée

// Rapport HID "Consumer Control" : 1 bit par touche multimédia (bits 0..4), 3 bits de bourrage.
static const uint8_t hidReportMap[] = {
  0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x01,  // Usage Page Consumer, Collection, Report ID 1
  0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x05,  // 5 champs de 1 bit
  0x09, 0xCD, 0x09, 0xB5, 0x09, 0xB6, 0x09, 0xE9, 0x09, 0xEA,  // Play/Pause, Next, Prev, Vol+, Vol-
  0x81, 0x02, 0x95, 0x03, 0x81, 0x01, 0xC0};

Media mediaSnapshot() {
  std::lock_guard<std::mutex> lock(mtx);
  return media;
}

void mediaEdit(const std::function<void(Media&)>& fn) {
  std::lock_guard<std::mutex> lock(mtx);
  fn(media);
  dirty = true;
}

// PlaybackInfo AMS = "state,rate,elapsed" (state: 0 pause, 1 lecture, 2/3 avance/recul)
bool parsePlaybackInfo(const std::string& s, int& state, float& rate, float& elapsed) {
  return sscanf(s.c_str(), "%d,%f,%f", &state, &rate, &elapsed) == 3;
}

// Notification Entity Update : [EntityID][AttributeID][Flags][valeur UTF-8...]
// shortcut: valeurs tronquées à MTU-6 octets (flag 1), lire Entity Attribute si titres très longs.
static void onEntityUpdate(NimBLERemoteCharacteristic*, uint8_t* d, size_t len, bool) {
  if (len < 3) return;
  std::string v = cleanText(std::string((const char*)d + 3, len - 3));  // emojis : carrés avec la police de l'écran
  std::lock_guard<std::mutex> lock(mtx);
  if (d[0] == ENT_TRACK) {
    if (d[1] == TRACK_TITLE) {
      titleChanged = v != media.title;
      media.title = v;
    }
    else if (d[1] == TRACK_ARTIST) media.artist = v;
    else if (d[1] == TRACK_DURATION) media.duration = atof(v.c_str());
  } else if (d[0] == ENT_QUEUE && (d[1] == QUEUE_SHUFFLE || d[1] == QUEUE_REPEAT)) {
    (d[1] == QUEUE_SHUFFLE ? media.shuffle : media.repeat) = atoi(v.c_str());
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

// L'iPhone se connecte à nous (périphérique), puis on est client GATT d'AMS.
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
    linkGen++;
    rcChar = nullptr;
    linkMode = LINK_NONE;
    needSetup = false;
    dirty = true;
    NimBLEDevice::startAdvertising();
  }
};

static void setupAms() {
  needSetup = false;
  uint32_t gen = linkGen;  // téléphone déconnecté pendant la découverte (1-2 s) : on ne passe pas en "connecté"
  // getClient() efface les services GATT déjà découverts : à n'appeler qu'ici, une fois par connexion.
  NimBLEClient* client = NimBLEDevice::getServer()->getClient(connHandle);
  NimBLERemoteService* svc = TARGET_APPLE && client ? client->getService(AMS_SVC) : nullptr;  // Android/Windows : HID direct
  NimBLERemoteCharacteristic* eu = svc ? svc->getCharacteristic(AMS_EU) : nullptr;
  NimBLERemoteCharacteristic* rc = svc ? svc->getCharacteristic(AMS_RC) : nullptr;
  // Nom du téléphone (service GAP 0x1800, caractéristique 0x2A00) : NimBLE ne garde que l'adresse des appairés.
  if (client) bondNameSave(connectedPeer(), client->getValue(NimBLEUUID((uint16_t)0x1800), NimBLEUUID((uint16_t)0x2A00)));
  if (!eu || !rc || !eu->subscribe(true, onEntityUpdate) || !rc->subscribe(true)) {
    if (gen != linkGen) return;
    linkMode = LINK_HID;  // pas un iPhone (Android/Windows) : touches multimédia HID
    dirty = true;
    return;
  }
  // Demande à l'iPhone de nous notifier ces attributs (valeur courante envoyée immédiatement).
  const uint8_t player[] = {ENT_PLAYER, PLAYER_PLAYBACK_INFO, PLAYER_VOLUME};
  const uint8_t track[]  = {ENT_TRACK, TRACK_ARTIST, TRACK_TITLE, TRACK_DURATION};
  const uint8_t queue[]  = {ENT_QUEUE, QUEUE_SHUFFLE, QUEUE_REPEAT};
  eu->writeValue(player, sizeof(player), true);
  eu->writeValue(track, sizeof(track), true);
  eu->writeValue(queue, sizeof(queue), true);
  // Heure de l'iPhone (Current Time Service) : lue maintenant, puis notifiée à chaque changement (heure d'été...).
  NimBLERemoteService* cts = client->getService(NimBLEUUID((uint16_t)0x1805));
  NimBLERemoteCharacteristic* ct = cts ? cts->getCharacteristic(NimBLEUUID((uint16_t)0x2A2B)) : nullptr;
  if (ct) {
    NimBLEAttValue v = ct->readValue();
    clockFromCts(v.data(), v.size());
    ct->subscribe(true, [](NimBLERemoteCharacteristic*, uint8_t* d, size_t len, bool) { clockFromCts(d, len); });
  }
  ancsSetup(client);  // appels et notifications
  if (gen != linkGen) return;
  rcChar = rc;
  linkMode = LINK_AMS;
  dirty = true;
}

// La découverte GATT et les abonnements prennent 1 à 2 s d'allers-retours radio : dans loop(), l'écran gelait à la
// connexion. Une petite tâche s'en charge ; l'animation de l'écran de démarrage continue pendant ce temps.
// shortcut: bondNameSave (NVS) est appelé depuis cette tâche ; sans risque tant que loop() n'écrit pas en même temps
// (seulement en modifiant un réglage, impossible pendant l'écran de démarrage).
static volatile bool setupRunning = false;

static void setupTask(void*) {
  setupAms();
  setupRunning = false;
  vTaskDelete(nullptr);
}

void startSetup() {
  if (!needSetup || setupRunning) return;  // reconnexion pendant une découverte : relancée quand elle a fini
  setupRunning = true;
  if (xTaskCreate(setupTask, "ams_setup", 6144, nullptr, 1, nullptr) != pdPASS) setupRunning = false;
}

// Envoi radio d'une commande : écriture AMS avec réponse (un aller-retour avec l'iPhone, des dizaines de ms) ou
// touche HID appuyée puis relâchée (10 ms). Fait par cmdTask : dans loop(), chaque cran de molette (volume)
// bloquait l'écran et les animations saccadaient en tournant vite.
static void transmit(uint8_t cmd) {
  NimBLERemoteCharacteristic* rc = rcChar;  // copie : la tâche BLE peut le remettre à nullptr entre-temps
  if (linkMode == LINK_AMS && rc) {
    rc->writeValue(&cmd, 1, true);
  } else if (linkMode == LINK_HID) {
    static const uint8_t bit[] = {0, 0, 0x01, 0x02, 0x04, 0x08, 0x10};  // index = CMD_*
    if (cmd >= sizeof(bit)) return;  // aléatoire, répétition : pas d'équivalent HID
    uint8_t v = bit[cmd];
    hidInput->setValue(&v, 1);
    hidInput->notify();
    delay(10);  // touche "appuyée" puis relâchée
    v = 0;
    hidInput->setValue(&v, 1);
    hidInput->notify();
  }
}

static QueueHandle_t cmdQueue = nullptr;

static void cmdTask(void*) {
  uint8_t cmd;
  for (;;) if (xQueueReceive(cmdQueue, &cmd, portMAX_DELAY)) transmit(cmd);
}

void sendCmd(uint8_t cmd) {
  if (!cmdQueue) {
    cmdQueue = xQueueCreate(32, 1);
    xTaskCreate(cmdTask, "media_cmd", 4096, nullptr, 1, nullptr);
  }
  xQueueSend(cmdQueue, &cmd, 0);  // file pleine (32 crans d'avance) : cran ignoré
  // L'iPhone notifie le nouveau mode avec du retard (ou pas du tout) : l'icône montre tout de suite le mode
  // suivant prévu (aléatoire : non <-> tout ; répétition : non -> tout -> un -> non), corrigé dès la notification.
  if (linkMode == LINK_AMS && (cmd == CMD_SHUFFLE || cmd == CMD_REPEAT)) {
    std::lock_guard<std::mutex> lock(mtx);
    if (cmd == CMD_SHUFFLE && media.shuffle >= 0) media.shuffle = media.shuffle ? 0 : 2;
    if (cmd == CMD_REPEAT && media.repeat >= 0) media.repeat = media.repeat == 0 ? 2 : media.repeat == 2 ? 1 : 0;
    dirty = true;
  }
}

std::string connectedPeer() {
  if (!NimBLEDevice::getServer()->getConnectedCount()) return "";  // Govee = connexion client, non comptée
  return NimBLEDevice::getServer()->getPeerInfoByHandle(connHandle).getIdAddress().toString();
}

void forgetBond(const NimBLEAddress& a) {
  if (connectedPeer() == a.toString()) NimBLEDevice::getServer()->disconnect(connHandle);
  NimBLEDevice::deleteBond(a);
}

void forgetAllBonds() {
  if (linkMode != LINK_NONE) NimBLEDevice::getServer()->disconnect(connHandle);
  NimBLEDevice::deleteAllBonds();
}

void startBle() {
  NimBLEDevice::init(BLE_NAME);
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
  if (!TARGET_APPLE) gbInit(server);  // infos de Gadgetbridge / du programme PC
  server->start();

  // "Service Solicitation" AMS (AD type 0x15, UUID 128 bits little-endian) : iOS sait qu'on veut AMS.
  const uint8_t solicit[] = {0x11, 0x15, 0xDC, 0xF8, 0x55, 0xAD, 0x02, 0xC5, 0xF4, 0x8E,
                             0x3A, 0x43, 0x36, 0x0F, 0x2B, 0x50, 0xD3, 0x89};
  NimBLEAdvertisementData adv, scan;
  adv.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  if (TARGET_APPLE) adv.addData(solicit, sizeof(solicit));
  adv.setAppearance(0x0180);                    // télécommande générique
  adv.setCompleteServices16({NimBLEUUID((uint16_t)0x1812)});  // HID : visible sur Android/Windows
  if (!TARGET_APPLE) adv.setCompleteServices(NimBLEUUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E"));  // UART : Gadgetbridge
  scan.setName(BLE_NAME);
  NimBLEAdvertising* a = NimBLEDevice::getAdvertising();
  a->setAdvertisementData(adv);
  a->setScanResponseData(scan);
  a->start();
}
