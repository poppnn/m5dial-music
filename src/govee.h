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
enum LedWheel { WHEEL_BRI, WHEEL_HUE, WHEEL_FX, WHEEL_COUNT };
extern int ledWheel;     // ce que règle la molette de la page LED (appui long = suivant)
// Effets joués par le M5Dial lui-même : il envoie la couleur du moment ~10 fois/s à la barre.
enum LedFx { FX_FIXED, FX_BLINK, FX_BREATHE, FX_RAINBOW, FX_COUNT };
extern int ledFx;
extern const char* const LED_FX_NAMES[FX_COUNT];
#define FX_SPEED_COUNT   9
#define FX_SPEED_DEFAULT 4  // x1
extern const float FX_SPEEDS[FX_SPEED_COUNT];
extern int ledFxSpeed;      // index dans FX_SPEEDS (la molette, en mode effet)
extern uint32_t goveeRetry;

void goveeInit();                  // callbacks du scan + dernier état mémorisé (après startBle)
void goveeSelfTest();              // effets (avant goveeInit)
void goveeService(bool ledPage);   // connexion, envois limités en fréquence, mémorisation, à chaque tour de loop()
bool goveeConnected();
void goveeSend(std::initializer_list<uint8_t> payload);
void goveeColorChanged();          // couleur modifiée : envoyée au prochain goveeService()
void goveeChanged();               // luminosité / état modifiés : mémorisés après 2 s sans changement
LedColor ledCurrent();             // couleur courante (préréglage ou teinte), avec son nom
LedColor ledShown(uint32_t now);   // couleur envoyée à l'instant now : ledCurrent() modulée par l'effet
LedColor hueColor(int hue);        // teinte HSV à saturation et valeur maximales
