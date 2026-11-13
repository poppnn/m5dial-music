// Réglages persistants (NVS, via Preferences). Valeurs par défaut = constantes de config.h.
#pragma once
#include <stdint.h>
#include <string>

struct Settings {
  int8_t imuDir;           // +1 / -1 : sens de rotation de l'IMU
  uint8_t encSteps;        // impulsions de l'encodeur par cran (1..4)
  uint16_t dimAfterS;      // atténuation de l'écran sans geste (s, 0 = jamais)
  uint16_t sleepAfterMin;  // extinction sans appareil connecté (min, 0 = jamais)
  uint8_t briDay, briNight;  // luminosité de l'écran (%)
  uint8_t briMode;         // 0 jour, 1 nuit, 2 auto (nuit entre nightFrom et nightTo, heure de l'iPhone)
  uint8_t nightFrom, nightTo;  // heures
  uint8_t notifMode;       // notifications iPhone : 0 aucune, 1 app + expéditeur, 2 app + expéditeur + texte
  uint8_t accent;          // couleur d'accent de l'interface (index de ACCENT_COLORS)
};
extern Settings settings;

enum : uint8_t { NOTIF_OFF, NOTIF_SENDER, NOTIF_FULL };
extern const uint16_t ACCENT_COLORS[];  // RGB565
extern const char* const ACCENT_NAMES[];
extern const int ACCENT_COUNT;
uint16_t accent();  // couleur d'accent choisie (arcs, icônes actives, bordures)

void settingsLoad();
void settingsSave();
void settingsSelfTest();
bool settingsNight();         // luminosité de nuit en ce moment
uint8_t settingsBacklight();  // luminosité de base de l'écran, 0..255 (jour ou nuit)

// Dernier état des LED Govee (préréglage, teinte, luminosité).
struct LedState { uint8_t color; uint16_t hue; bool custom; uint8_t bri; uint8_t fx, fxSpeed; };
LedState ledStateLoad();
void ledStateSave(const LedState& s);

// Nom des appareils appairés (NimBLE ne garde que leur adresse) : mémorisé à la connexion.
void bondNameSave(const std::string& addr, const std::string& name);
std::string bondName(const std::string& addr);  // "" si inconnu
