// Heure : RTC du M5Dial (garde l'heure éteint), mise à l'heure par l'iPhone (Current Time Service).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <time.h>

bool clockNow(tm& t);  // false tant que l'heure n'a jamais été réglée
// Trame CTS "Current Time" (0x2A2B) : année LE16, mois, jour, h, min, s, jour de semaine, 1/256 s, raison.
// Appelable depuis la tâche BLE : la mise à l'heure (I2C du RTC) est faite plus tard par clockService().
bool clockFromCts(const uint8_t* d, size_t len);
bool clockFromLocalEpoch(long long t);  // secondes depuis 1970, heure locale (Gadgetbridge, programme PC)
void clockService();  // dans loop()
