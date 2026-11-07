// LED Govee H7090 : le M5Dial est client BLE de la barre LED (protocole Govee non officiel).
#pragma once
#include <initializer_list>
#include <stdint.h>

struct LedColor { const char* name; uint8_t r, g, b; };
extern const LedColor LED_COLORS[];
extern const int LED_COLOR_COUNT;

extern bool ledOn;  // état supposé : la barre ne renvoie pas son état
extern int ledColor, ledBri;
extern uint32_t goveeRetry;

void goveeInit();                  // callbacks du scan (après startBle)
void goveeService(bool ledPage);   // connexion / reconnexion / envoi de la luminosité, à chaque tour de loop()
bool goveeConnected();
void goveeSend(std::initializer_list<uint8_t> payload);
void goveeSendColor();
