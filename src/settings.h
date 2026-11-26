// Réglages persistants (NVS, via Preferences). Valeurs par défaut = constantes de config.h.
#pragma once
#include <stdint.h>
#include <string>
#include "pages/pages.h"

struct Settings {
  uint8_t encSteps;        // impulsions de l'encodeur par cran (1..4)
  uint16_t dimAfterS;      // atténuation de l'écran sans geste (s, 0 = jamais)
  uint16_t sleepAfterMin;  // extinction sans appareil connecté (min, 0 = jamais)
  uint8_t briDay, briNight;  // luminosité de l'écran (%)
  uint8_t briMode;         // 0 jour, 1 nuit, 2 auto (nuit entre nightFrom et nightTo, heure de l'iPhone)
  uint8_t nightFrom, nightTo;  // heures
  uint8_t notifMode;       // notifications iPhone : 0 aucune, 1 app + expéditeur, 2 app + expéditeur + texte
  uint8_t accent;          // accent de l'interface (index de ACCENT_RGB)
  uint8_t haptic;          // clic du buzzer à chaque cran et appui (0 = non)
  uint8_t pageOrder[PAGE_COUNT];  // ordre des pages au glissement vertical (Web UI) : chaque Page une fois
};
extern Settings settings;

enum : uint8_t { NOTIF_OFF, NOTIF_SENDER, NOTIF_FULL };
extern const uint32_t ACCENT_RGB[][2];  // préréglages d'accent : début et fin du dégradé (RGB 8 bits)
extern const char* const ACCENT_NAMES[];
extern const int ACCENT_COUNT;

void settingsLoad();
void settingsSave();
void settingsSelfTest();
// Ordre des pages : "024135" (un chiffre par Page, chacune une fois). false si la chaîne n'en est pas un.
bool pageOrderParse(const std::string& s, uint8_t* order);
std::string pageOrderText(const uint8_t* order);
bool settingsNight();         // luminosité de nuit en ce moment
uint8_t settingsBacklight();  // luminosité de base de l'écran, 0..255 (jour ou nuit)

// Dernier état des LED Govee (préréglage, teinte, luminosité).
struct LedState { uint8_t color; uint16_t hue; bool custom; uint8_t bri; };
LedState ledStateLoad();
void ledStateSave(const LedState& s);

// Partage de connexion pour la mise à jour OTA (envoyé depuis le web flasher). false si aucun.
bool wifiLoad(std::string& ssid, std::string& pass);
void wifiSave(const char* ssid, const char* pass);

// Appareils à oublier, choisis dans la Web UI (le Bluetooth y est arrêté) : oubliés au démarrage suivant.
void bondForgetLater(const std::string& id);  // id = "adresse/type"
std::string bondForgetTake();                 // liste "id,id," puis effacée

// Nom des appareils appairés (NimBLE ne garde que leur adresse) : mémorisé à la connexion.
void bondNameSave(const std::string& addr, const std::string& name);
std::string bondName(const std::string& addr);  // "" si inconnu
