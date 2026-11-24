// Web UI : Réglages > Web UI. Le M5Dial rejoint le partage de connexion du téléphone (Bluetooth arrêté pendant la
// session) et sert une page à ouvrir dans le navigateur du téléphone : réglages, LED, Wi-Fi, export / import de la
// configuration (fichier JSON), appareils appairés (oubliés au redémarrage), image de démarrage.
#pragma once
#include <string>

void webuiRun();  // ne revient pas : redémarre en sortant (bouton, page "Done", ou 15 min sans requête)

// Configuration en JSON (export) et application d'un JSON (import / enregistrement depuis la page).
std::string configJson();
std::string configApply(const std::string& js);  // "" = appliqué, sinon le premier champ refusé
std::string jsonStr(const std::string& s);       // chaîne JSON entre guillemets, caractères spéciaux échappés
void webuiSelfTest();
