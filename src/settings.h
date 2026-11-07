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
};
extern Settings settings;

void settingsLoad();
void settingsSave();
void settingsSelfTest();
bool settingsNight();         // luminosité de nuit en ce moment
uint8_t settingsBacklight();  // luminosité de base de l'écran, 0..255 (jour ou nuit)

// Nom des appareils appairés (NimBLE ne garde que leur adresse) : mémorisé à la connexion.
void bondNameSave(const std::string& addr, const std::string& name);
std::string bondName(const std::string& addr);  // "" si inconnu
