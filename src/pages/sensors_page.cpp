// Page Capteurs : température, humidité, pression et altitude (ENV III, baromètre de l'IMU Pro Mini), et les
// unités détectées avec leur port.
#include <M5Dial.h>
#include "pages.h"
#include "../ble_phone.h"
#include "../display.h"
#include "../imu.h"
#include "../sensors.h"
#include "../settings.h"

static bool draw(float) {
  const int c = 120;
  const SensorData& s = sensors();
  canvas.fillArc(c, c, 119, 111, 0, 360, 0x2104);
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0x8410);
  canvas.drawString("Sensors", c, 34);
  float temp = s.temp;
  bool chip = isnan(temp) && imuTemp(temp);  // seulement la puce de l'IMU : approximative
  if (!s.sht && !s.qmp && !s.bmp && !chip) {
    canvas.setFont(&fonts::efontJA_24);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("No sensor", c, 104);
    canvas.setFont(&fonts::efontJA_16);
    canvas.setTextColor(0xAD55);
    canvas.drawString("ENV III, IMU Pro Mini", c, 136);
    canvas.drawString("on Port A or B", c, 156);
    return false;
  }
  char b[40];
  canvas.setFont(&fonts::efontJA_24);
  canvas.setTextColor(TFT_WHITE);
  snprintf(b, sizeof(b), isnan(temp) ? "-" : "%.1f°C", temp);
  canvas.drawString(b, c, 76);
  canvas.setFont(&fonts::efontJA_16);
  int y = 108;
  if (!isnan(s.hum)) {
    snprintf(b, sizeof(b), "Humidity %.0f%%", s.hum);
    canvas.drawString(b, c, y);
    y += 24;
  }
  if (!isnan(s.press)) {
    snprintf(b, sizeof(b), "%.0f hPa", s.press);
    canvas.drawString(b, c, y);
    snprintf(b, sizeof(b), "Altitude %.0f m", altitudeM(s.press));
    canvas.drawString(b, c, y + 24);
  }
  // Unités détectées, avec leur port
  std::string src;
  auto add = [&](bool on, const char* name, char port) {
    if (!on) return;
    if (!src.empty()) src += "  ";
    src += std::string(name) + " " + port;
  };
  add(s.sht, "SHT30", s.shtPort);
  add(s.qmp, "QMP6988", s.qmpPort);
  add(s.bmp, "BMP280", s.bmpPort);
  if (M5.Imu.isEnabled()) add(true, "IMU", 'A');
  canvas.setTextColor(0x8410);
  canvas.drawString(fit(src, 2 * textSpace(196, 16, 118)).c_str(), c, 196);
  return false;  // une mesure par seconde : 4 images/s suffisent
}

static void click() { sendCmd(CMD_TOGGLE); }
static void swipeX(int) {}

extern const PageOps sensorsPage = {draw, volumeWheel, click, swipeX, nullptr};
