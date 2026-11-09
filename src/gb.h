// Firmwares Android et Windows : infos (morceau, notifications, appels, heure) reçues en lignes de texte sur le
// Nordic UART Service, au format de la montre Bangle.js que parle Gadgetbridge (Android). Le programme PC
// (tools/windows) envoie les mêmes lignes. Les commandes multimédia restent en HID.
#pragma once
#include <NimBLEDevice.h>
#include <string>

void gbInit(NimBLEServer* server);  // avant server->start()
void gbService();                   // dans loop() : traite les lignes reçues
void gbAnswer(bool accept);         // appel entrant : décrocher / refuser (renvoyé à Gadgetbridge)
std::string gbDecode(const std::string& s);  // chaîne JSON façon Espruino (\xHH, \uXXXX, Latin-1) -> UTF-8
bool gbField(const std::string& js, const char* key, std::string& out);  // valeur d'un champ (chaîne décodée ou nombre brut)
void gbSelfTest();
