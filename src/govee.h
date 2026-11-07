// LED Govee H7090 : le M5Dial est client BLE de la barre LED (protocole Govee non officiel).
#pragma once
#include <initializer_list>
#include <stdint.h>

struct LedColor { const char* name; uint8_t r, g, b; };
extern const LedColor LED_COLORS[];
extern const int LED_COLOR_COUNT;

extern bool ledOn;    // état supposé : la barre ne renvoie pas son état
extern int ledColor;  // préréglage choisi (utilisé si !ledCustom)
extern int ledBri;    // 5..100 %
extern int ledHue;    // 0..359 : teinte continue
extern bool ledCustom;   // couleur courante = teinte ledHue (sinon le préréglage ledColor)
extern bool ledHueMode;  // la molette règle la teinte (sinon la luminosité)
extern uint32_t goveeRetry;

void goveeInit();                  // callbacks du scan + dernier état mémorisé (après startBle)
void goveeService(bool ledPage);   // connexion, envois limités en fréquence, mémorisation, à chaque tour de loop()
bool goveeConnected();
void goveeSend(std::initializer_list<uint8_t> payload);
void goveeColorChanged();          // couleur modifiée : envoyée au prochain goveeService()
void goveeChanged();               // luminosité / état modifiés : mémorisés après 2 s sans changement
LedColor ledCurrent();             // couleur courante (préréglage ou teinte), avec son nom
LedColor hueColor(int hue);        // teinte HSV à saturation et valeur maximales
