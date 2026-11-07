// Lien avec le téléphone : le M5Dial est périphérique BLE. iPhone -> client GATT d'AMS (titre, commandes) ;
// Android / Windows -> télécommande HID (touches multimédia).
#pragma once
#include <NimBLEDevice.h>
#include <mutex>
#include <string>

enum : uint8_t { CMD_TOGGLE = 2, CMD_NEXT = 3, CMD_PREV = 4, CMD_VOL_UP = 5, CMD_VOL_DOWN = 6 };

struct Media {
  std::string title, artist;
  float duration = 0, elapsed = 0, rate = 0;
  uint32_t stamp = 0;  // millis() de la dernière mise à jour de elapsed
  float volume = -1;   // 0..1 (AMS), -1 = inconnu
  bool playing = false;
};

enum Link : uint8_t { LINK_NONE, LINK_HID, LINK_AMS };  // HID = Android/Windows, AMS = iPhone
extern volatile Link linkMode;
extern volatile bool needSetup, titleChanged, volPing;
extern volatile uint16_t connHandle;

Media mediaSnapshot();  // copie sous verrou (écrit par la tâche BLE)
bool parsePlaybackInfo(const std::string& s, int& state, float& rate, float& elapsed);
void startBle();
void setupAms();  // à appeler depuis loop() quand needSetup : découverte GATT hors de la tâche BLE
void sendCmd(uint8_t cmd);
std::string connectedPeer();               // adresse d'identité du téléphone connecté ("" sinon)
void forgetBond(const NimBLEAddress& a);   // oublie un appareil (le déconnecte s'il est connecté)
void forgetAllBonds();
