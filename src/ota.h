// Mise à jour sans fil (OTA) : par le partage de connexion du téléphone, choisi et son mot de passe tapé à la molette
// (ou envoyés depuis le web flasher), télécharge le firmware publié sur GitHub Pages pour cette plateforme s'il change.
#pragma once

#include <string>

// Session Wi-Fi (Update, Web UI) : Bluetooth arrêté (sa mémoire sert au Wi-Fi), partage de connexion choisi à la
// molette et rejoint. On en sort toujours par un redémarrage.
void wifiSession(const char* title);
// Écran d'état plein écran : titre, ligne, anneau de progression (0..1, < 0 = aucun), seconde ligne.
void show(const char* title, const std::string& line, float progress = -1, const std::string& line2 = "");
void otaSelfTest();  // saisie du mot de passe à la molette
void otaRun();  // Réglages > Update : prend tout l'écran, ne revient pas (redémarre à la fin, même en cas d'échec)
