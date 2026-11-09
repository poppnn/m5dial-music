// Apple Notification Center Service (iPhone) : appels entrants et notifications.
// Sécurité en conduite : par défaut le nom de l'app et l'expéditeur (titre) ; texte ou aucune notification : Réglages.
#pragma once
#include <NimBLEDevice.h>
#include <string>

void ancsSetup(NimBLEClient* client);  // iPhone connecté et chiffré (depuis setupAms)
bool ancsService();                    // dans loop() : true si un appel ou une notification vient d'arriver
void ancsReset();                      // déconnexion

bool ancsCall(std::string& caller);    // appel entrant en cours
void ancsAnswer(bool accept);          // décrocher / refuser
int ancsCount();                       // notifications présentes sur l'iPhone

// since = 0 : rien à afficher ; closing = instant où la fermeture a commencé (0 = ouverte)
struct NotifToast { std::string app, title, message; uint32_t since = 0, closing = 0; };
const NotifToast& ancsToast();
void ancsToastDismiss();  // lance l'animation de fermeture
void ancsSelfTest();

// Firmwares Android / Windows (gb.cpp) : mêmes toasts et écran d'appel que l'ANCS, depuis loop().
void notifPush(const std::string& app, const std::string& title, const std::string& msg);
void callPush(bool incoming, const std::string& name);
void notifCount(int n);  // notifications présentes sur le téléphone
