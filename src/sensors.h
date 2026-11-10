// Unités M5Stack sur les ports Grove : ENV III (SHT30 température / humidité + QMP6988 pression) et baromètre
// BMP280 de l'IMU Pro Mini (son BMI270 + BMM150 est lu par M5Unified, voir imu.cpp). Chaque capteur est cherché
// sur le Port A puis sur le Port B ; tout est optionnel.
#pragma once
#include <math.h>

struct SensorData {
  float temp = NAN, hum = NAN, press = NAN;  // °C, % HR, hPa (NAN = pas de capteur ou mesure invalide)
  const char* tempSrc = "";                  // "SHT30", "BMP280", "QMP6988"
  bool sht = false, qmp = false, bmp = false;
  char shtPort = 0, qmpPort = 0, bmpPort = 0;  // 'A' ou 'B'
};

void sensorsInit();    // après M5Dial.begin() (Port A ouvert par M5Unified pour l'IMU)
void sensorsUpdate();  // dans loop() : une mesure par seconde
const SensorData& sensors();
float altitudeM(float hPa);  // altitude barométrique (atmosphère standard, 1013,25 hPa au niveau de la mer)
void sensorsSelfTest();
