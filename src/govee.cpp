// Protocole Govee non officiel (rétro-ingénierie communautaire) : trames de 20 octets
// 0x33 <cmd> <données...> 0x00... <XOR des 19 premiers octets>. Le H7090 n'a pas de variante connue.
#include <Arduino.h>
#include "govee.h"
#include <NimBLEDevice.h>
#include "app.h"
#include "settings.h"

static const char* GOVEE_SVC = "00010203-0405-0607-0809-0a0b0c0d1910";
static const char* GOVEE_WR  = "00010203-0405-0607-0809-0a0b0c0d2b11";

const LedColor LED_COLORS[] = {
  {"White", 255, 255, 255}, {"Red", 255, 0, 0},     {"Orange", 255, 90, 0},
  {"Yellow", 255, 200, 0},  {"Green", 0, 255, 40},  {"Cyan", 0, 220, 255},
  {"Blue", 0, 60, 255},     {"Purple", 150, 0, 255}, {"Pink", 255, 0, 140}};
const int LED_COLOR_COUNT = sizeof(LED_COLORS) / sizeof(LED_COLORS[0]);

static NimBLEClient* goveeClient = nullptr;
static NimBLERemoteCharacteristic* goveeWr = nullptr;
static NimBLEAddress goveeAddr;
static volatile bool goveeFound = false;
uint32_t goveeRetry = 0;
bool ledOn = true, ledCustom = false, ledHueMode = false;
// Luminosité en % : le H7090 attend 0-100 (au-delà il sature : avec 0-255, seul le bas de la course agissait).
#define GOVEE_BRI_MAX 100
#define GOVEE_MIN_GAP_MS 60   // ~16 trames/s au plus quand on tourne la molette
#define SAVE_AFTER_MS    2000 // mémorisation NVS 2 s après le dernier changement (pas une écriture par cran)
int ledColor = 0, ledBri = 100, ledHue = 0;
static int ledBriSent = -1;
static bool colorPending = false;
static uint32_t lastSend = 0, changedAt = 0;

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
  LedState s = ledStateLoad();
  ledColor = s.color % LED_COLOR_COUNT;
  ledHue = s.hue % 360;
  ledCustom = s.custom;
  ledBri = constrain(s.bri, 5, 100);
}

bool goveeConnected() { return goveeWr != nullptr; }

LedColor hueColor(int hue) {
  float h = (hue % 360) / 60.f, x = 1 - fabsf(fmodf(h, 2) - 1);
  float r = 0, g = 0, b = 0;
  switch ((int)h) {
    case 0: r = 1; g = x; break;
    case 1: r = x; g = 1; break;
    case 2: g = 1; b = x; break;
    case 3: g = x; b = 1; break;
    case 4: r = x; b = 1; break;
    default: r = 1; b = x; break;
  }
  return {"Hue", (uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255)};
}

LedColor ledCurrent() { return ledCustom ? hueColor(ledHue) : LED_COLORS[ledColor]; }

void goveeSend(std::initializer_list<uint8_t> payload) {
  if (!goveeWr) return;
  uint8_t p[20] = {0};
  size_t i = 0;
  for (uint8_t b : payload) p[i++] = b;
  for (i = 0; i < 19; i++) p[19] ^= p[i];
  goveeWr->writeValue(p, sizeof(p), false);
}

static void sendColor() {
  LedColor c = ledCurrent();
  goveeSend({0x33, 0x05, 0x02, c.r, c.g, c.b});  // RVB "manuel" standard Govee
  // shortcut: format RGBIC étendu envoyé aussi, ignoré par les modèles qui ne le gèrent pas ; garder le seul qui marche une fois testé sur le H7090.
  goveeSend({0x33, 0x05, 0x15, 0x01, c.r, c.g, c.b, 0, 0, 0, 0, 0, 0xFF, 0x7F});
}

void goveeChanged() { changedAt = millis() | 1; }
void goveeColorChanged() { colorPending = true; goveeChanged(); }

// Connexion / reconnexion. Ne scanne que sur la page LED, pour ne pas vider la batterie.
void goveeService(bool ledPage) {
  uint32_t now = millis();
  if (changedAt && now - changedAt > SAVE_AFTER_MS) {
    changedAt = 0;
    ledStateSave({(uint8_t)ledColor, (uint16_t)ledHue, ledCustom, (uint8_t)ledBri});
  }
  if (goveeWr && goveeClient && goveeClient->isConnected()) {
    if (now - lastSend < GOVEE_MIN_GAP_MS) return;
    if (colorPending) {
      colorPending = false;
      sendColor();
      lastSend = now;
    } else if (ledBri != ledBriSent) {
      goveeSend({0x33, 0x04, (uint8_t)(ledBri * GOVEE_BRI_MAX / 100)});
      ledBriSent = ledBri;
      lastSend = now;
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
      if (goveeWr && ledOn) {  // connectée : la barre reprend la couleur et la luminosité mémorisées
        colorPending = true;
        ledBriSent = -1;
      } else {
        ledBriSent = ledBri;  // éteinte : rien à envoyer (une luminosité pourrait la rallumer)
      }
    }
    goveeRetry = millis();
    dirty = true;
  } else if (ledPage && !NimBLEDevice::getScan()->isScanning() && millis() - goveeRetry > 8000) {
    goveeRetry = millis();
    NimBLEDevice::getScan()->start(4000, false);  // asynchrone : l'UI continue de tourner
  }
}
