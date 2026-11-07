// Apple Notification Center Service (iPhone) : appels entrants et notifications.
// Sécurité en conduite : par défaut seuls le nom de l'app et le compteur s'affichent (texte : option des Réglages).
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
