// Protocole Govee non officiel (rétro-ingénierie communautaire) : trames de 20 octets
// 0x33 <cmd> <données...> 0x00... <XOR des 19 premiers octets>. Le H7090 n'a pas de variante connue.
#include <Arduino.h>
#include "govee.h"
#include <NimBLEDevice.h>
#include "app.h"

static const char* GOVEE_SVC = "00010203-0405-0607-0809-0a0b0c0d1910";
static const char* GOVEE_WR  = "00010203-0405-0607-0809-0a0b0c0d2b11";

const LedColor LED_COLORS[] = {
  {"Blanc", 255, 255, 255}, {"Rouge", 255, 0, 0},   {"Orange", 255, 90, 0},
  {"Jaune", 255, 200, 0},   {"Vert", 0, 255, 40},   {"Cyan", 0, 220, 255},
  {"Bleu", 0, 60, 255},     {"Violet", 150, 0, 255}, {"Rose", 255, 0, 140}};
const int LED_COLOR_COUNT = sizeof(LED_COLORS) / sizeof(LED_COLORS[0]);

static NimBLEClient* goveeClient = nullptr;
static NimBLERemoteCharacteristic* goveeWr = nullptr;
static NimBLEAddress goveeAddr;
static volatile bool goveeFound = false;
uint32_t goveeRetry = 0;
bool ledOn = true;
// Luminosité en % : le H7090 attend 0-100 (au-delà il sature : avec 0-255, seul le bas de la course agissait).
#define GOVEE_BRI_MAX 100
int ledColor = 0, ledBri = 100;
static int ledBriSent = 100;

class GoveeScanCB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    if (d->getName().find("H7090") == std::string::npos) return;  // nom annoncé : "ihoment_H7090_XXXX" / "Govee_H7090_XXXX"
    goveeAddr = d->getAddress();
    goveeFound = true;
    NimBLEDevice::getScan()->stop();
  }
};

void goveeInit() {
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(new GoveeScanCB());
  scan->setActiveScan(true);  // le nom "..._H7090_..." est souvent dans la réponse au scan
}

bool goveeConnected() { return goveeWr != nullptr; }

void goveeSend(std::initializer_list<uint8_t> payload) {
  if (!goveeWr) return;
  uint8_t p[20] = {0};
  size_t i = 0;
  for (uint8_t b : payload) p[i++] = b;
  for (i = 0; i < 19; i++) p[19] ^= p[i];
  goveeWr->writeValue(p, sizeof(p), false);
}

void goveeSendColor() {
  const LedColor& c = LED_COLORS[ledColor];
  goveeSend({0x33, 0x05, 0x02, c.r, c.g, c.b});  // RVB "manuel" standard Govee
  // shortcut: format RGBIC étendu envoyé aussi, ignoré par les modèles qui ne le gèrent pas ; garder le seul qui marche une fois testé sur le H7090.
  goveeSend({0x33, 0x05, 0x15, 0x01, c.r, c.g, c.b, 0, 0, 0, 0, 0, 0xFF, 0x7F});
}

// Connexion / reconnexion. Ne scanne que sur la page LED, pour ne pas vider la batterie.
void goveeService(bool ledPage) {
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
