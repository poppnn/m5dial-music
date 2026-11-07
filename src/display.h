// Rendu : l'UI est composée dans un sprite 240x240 (repère "droit"), puis tournée (IMU) bande par bande
// dans 2 petits buffers envoyés en DMA. Tous les buffers sont en SRAM interne : la StampS3 n'a pas de PSRAM.
#pragma once
#include <M5Dial.h>
#include <string>

extern M5Canvas canvas;
extern float frameShift;  // décalage vertical de l'image (glissement de page), appliqué à l'envoi

void displayInit();  // à appeler AVANT le BLE (blocs contigus en SRAM DMA)
void pushFrame();
std::string fit(std::string s, int maxW);  // tronque proprement en UTF-8 + "..."
String mmss(float sec);
