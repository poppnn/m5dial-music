// "Toujours droit" : Unit IMU M5Stack sur le Port A (le M5Dial n'en a pas d'intégrée). La gravité filtrée
// donne l'angle de l'écran ; l'interface est tournée d'autant à l'envoi (display.cpp).
#pragma once

extern float rot;          // rotation affichée (degrés, lissée)
void imuInit();            // l'orientation au démarrage sert de référence "droit"
void imuUpdate(float dt);  // à chaque tour de loop()
bool imuTurning();         // la rotation affichée n'a pas encore rejoint sa cible
void imuFlipped();         // à appeler après avoir changé settings.imuDir
bool imuTemp(float& c);    // température du capteur de l'Unit IMU (°C, lissée) ; false sans IMU
