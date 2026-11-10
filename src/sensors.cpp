// Capteurs Grove. L'ESP32-S3 n'a que 2 contrôleurs I2C, déjà pris (bus interne : écran tactile, RTC ; Port A :
// IMU) : le Port B est lu en basculant les broches du contrôleur du Port A le temps d'une mesure, une fois par
// seconde, puis remises (tout se passe dans loop(), comme la lecture de l'IMU : pas d'accès simultané).
#include <M5Dial.h>
#include <assert.h>
#include "sensors.h"

#define F_I2C 400000
#define PORT_B_PIN1 1  // Port B du M5Dial : G1 / G2 (ordre SDA / SCL essayé dans les deux sens)
#define PORT_B_PIN2 2

static SensorData d;
static int portNum = -1, aSda = -1, aScl = -1, bSda = PORT_B_PIN2, bScl = PORT_B_PIN1;
static bool onB = false;
static uint8_t bmpAddr = 0x76;
static uint16_t T1, P1;   // étalonnage BMP280
static int16_t T2, T3, P2, P3, P4, P5, P6, P7, P8, P9;
static double qa0, qa1, qa2, qb00, qbt1, qbt2, qbp1, qb11, qbp2, qb12, qb21, qbp3;  // étalonnage QMP6988

const SensorData& sensors() { return d; }

static bool usePort(char p) {
  bool wantB = p == 'B';
  if (portNum < 0) return false;
  if (wantB == onB) return true;
  M5.Ex_I2C.release();
  onB = wantB;
  return M5.Ex_I2C.begin((i2c_port_t)portNum, wantB ? bSda : aSda, wantB ? bScl : aScl);
}

static bool cmd2(uint8_t addr, uint8_t a, uint8_t b) {
  const uint8_t c[2] = {a, b};
  bool ok = M5.Ex_I2C.start(addr, false, F_I2C) && M5.Ex_I2C.write(c, 2);
  M5.Ex_I2C.stop();
  return ok;
}

static bool readRaw(uint8_t addr, uint8_t* buf, size_t n) {
  bool ok = M5.Ex_I2C.start(addr, true, F_I2C) && M5.Ex_I2C.read(buf, n, true);
  M5.Ex_I2C.stop();
  return ok;
}

static bool readReg(uint8_t addr, uint8_t reg, uint8_t* buf, size_t n) {
  return M5.Ex_I2C.readRegister(addr, reg, buf, n, F_I2C);
}

static bool writeReg(uint8_t addr, uint8_t reg, uint8_t v) { return M5.Ex_I2C.writeRegister8(addr, reg, v, F_I2C); }

// ---------- SHT30 (ENV III, 0x44) : mesure périodique 1/s, lue sans attendre ----------
static uint8_t crc8(const uint8_t* p, int n) {  // polynôme 0x31, départ 0xFF (datasheet Sensirion)
  uint8_t c = 0xFF;
  for (int i = 0; i < n; i++) {
    c ^= p[i];
    for (int b = 0; b < 8; b++) c = c & 0x80 ? (c << 1) ^ 0x31 : c << 1;
  }
  return c;
}

static void shtRead() {
  uint8_t b[6];
  if (!cmd2(0x44, 0xE0, 0x00) || !readRaw(0x44, b, 6) || crc8(b, 2) != b[2] || crc8(b + 3, 2) != b[5]) return;
  float t = -45 + 175.f * (b[0] << 8 | b[1]) / 65535.f, h = 100.f * (b[3] << 8 | b[4]) / 65535.f;
  if (t > -40 && t < 85) d.temp = t, d.tempSrc = "SHT30";
  if (h >= 0 && h <= 100) d.hum = h;
}

// ---------- BMP280 (IMU Pro Mini, 0x76) : formules de compensation de la datasheet Bosch ----------
static void bmpCompensate(int32_t adcT, int32_t adcP, double& t, double& p) {
  double v1 = (adcT / 16384.0 - T1 / 1024.0) * T2;
  double v2 = (adcT / 131072.0 - T1 / 8192.0) * (adcT / 131072.0 - T1 / 8192.0) * T3;
  double fine = v1 + v2;
  t = fine / 5120.0;
  v1 = fine / 2.0 - 64000.0;
  v2 = v1 * v1 * P6 / 32768.0 + v1 * P5 * 2.0;
  v2 = v2 / 4.0 + P4 * 65536.0;
  v1 = (P3 * v1 * v1 / 524288.0 + P2 * v1) / 524288.0;
  v1 = (1.0 + v1 / 32768.0) * P1;
  if (v1 == 0) { p = NAN; return; }
  p = (1048576.0 - adcP - v2 / 4096.0) * 6250.0 / v1;
  p += (P9 * p * p / 2147483648.0 + p * P8 / 32768.0 + P7) / 16.0;  // Pa
}

static bool bmpInit() {
  uint8_t id = 0, c[24];
  if (!readReg(bmpAddr, 0xD0, &id, 1) || id != 0x58 || !readReg(bmpAddr, 0x88, c, 24)) return false;
  auto u = [&](int i) { return (uint16_t)(c[i] | c[i + 1] << 8); };
  T1 = u(0); T2 = u(2); T3 = u(4);
  P1 = u(6); P2 = u(8); P3 = u(10); P4 = u(12); P5 = u(14); P6 = u(16); P7 = u(18); P8 = u(20); P9 = u(22);
  return writeReg(bmpAddr, 0xF5, 0x08) && writeReg(bmpAddr, 0xF4, 0x2F);  // filtre x4 ; T x1, P x4, continu
}

static void bmpRead(bool useTemp, bool usePress) {
  uint8_t b[6];
  if (!readReg(bmpAddr, 0xF7, b, 6)) return;
  double t, p;
  bmpCompensate(b[3] << 12 | b[4] << 4 | b[5] >> 4, b[0] << 12 | b[1] << 4 | b[2] >> 4, t, p);
  if (useTemp && t > -40 && t < 85) d.temp = t, d.tempSrc = "BMP280";
  if (usePress && p > 30000 && p < 110000) d.press = p / 100;
}

// ---------- QMP6988 (ENV III, 0x70) : coefficients OTP et formules de la datasheet QST ----------
static bool qmpInit() {
  uint8_t id = 0, a[25];
  if (!readReg(0x70, 0xD1, &id, 1) || id != 0x5C) return false;
  writeReg(0x70, 0xE0, 0xE6);  // reset logiciel
  delay(10);
  if (!readReg(0x70, 0xA0, a, 25)) return false;
  auto s16 = [&](int i) { return (double)(int16_t)(a[i] << 8 | a[i + 1]); };
  auto s20 = [](uint32_t v) { return (double)((int32_t)(v << 12) >> 12); };
  qb00 = s20(a[0] << 12 | a[1] << 4 | a[24] >> 4) / 16.0;
  qa0 = s20(a[18] << 12 | a[19] << 4 | (a[24] & 0x0F)) / 16.0;
  qbt1 = 1.0e-01 + 9.1e-02 * s16(2) / 32767.0;
  qbt2 = 1.2e-08 + 1.2e-06 * s16(4) / 32767.0;
  qbp1 = 3.3e-02 + 1.9e-02 * s16(6) / 32767.0;
  qb11 = 2.1e-07 + 1.4e-07 * s16(8) / 32767.0;
  qbp2 = -6.3e-10 + 3.5e-10 * s16(10) / 32767.0;
  qb12 = 2.9e-13 + 7.6e-13 * s16(12) / 32767.0;
  qb21 = 2.1e-15 + 1.2e-14 * s16(14) / 32767.0;
  qbp3 = 1.3e-16 + 7.9e-17 * s16(16) / 32767.0;
  qa1 = -6.3e-03 + 4.3e-04 * s16(20) / 32767.0;
  qa2 = -1.9e-11 + 1.2e-10 * s16(22) / 32767.0;
  return writeReg(0x70, 0xF1, 0x02) && writeReg(0x70, 0xF4, 0x2F);  // filtre IIR x4 ; T x1, P x4, continu
}

static void qmpRead(bool useTemp) {
  uint8_t b[6];
  if (!readReg(0x70, 0xF7, b, 6)) return;
  double dp = (double)(b[0] << 16 | b[1] << 8 | b[2]) - 8388608.0, dt = (double)(b[3] << 16 | b[4] << 8 | b[5]) - 8388608.0;
  double tr = qa0 + qa1 * dt + qa2 * dt * dt;  // 1/256 °C
  double pr = qb00 + qbt1 * tr + qbp1 * dp + qb11 * dp * tr + qbt2 * tr * tr + qbp2 * dp * dp + qb12 * dp * tr * tr
              + qb21 * dp * dp * tr + qbp3 * dp * dp * dp;  // Pa
  if (useTemp && tr / 256 > -40 && tr / 256 < 85) d.temp = tr / 256, d.tempSrc = "QMP6988";
  if (pr > 30000 && pr < 110000) d.press = pr / 100;
}

// ---------- détection et lecture ----------
static char find(uint8_t addr) {
  for (char p : {'A', 'B'})
    if (usePort(p) && M5.Ex_I2C.scanID(addr, 100000)) return p;
  return 0;
}

void sensorsInit() {
  if (!M5.Ex_I2C.isEnabled()) M5.Ex_I2C.begin();  // Port A, broches réglées par M5Unified pour le M5Dial
  portNum = M5.Ex_I2C.getPort();
  aSda = M5.Ex_I2C.getSDA();
  aScl = M5.Ex_I2C.getSCL();
  if (portNum < 0) return;
  for (int order = 0; order < 2; order++) {  // Port B : SDA/SCL dans un sens puis dans l'autre
    d.shtPort = find(0x44);
    d.qmpPort = find(0x70);
    bmpAddr = 0x76;
    d.bmpPort = find(0x76);
    if (!d.bmpPort) bmpAddr = 0x77, d.bmpPort = find(0x77);
    if (d.shtPort == 'B' || d.qmpPort == 'B' || d.bmpPort == 'B') break;
    std::swap(bSda, bScl);
  }
  d.sht = d.shtPort && usePort(d.shtPort) && cmd2(0x44, 0x21, 0x30);  // mesure périodique, 1/s, haute répétabilité
  d.qmp = d.qmpPort && usePort(d.qmpPort) && qmpInit();
  d.bmp = d.bmpPort && usePort(d.bmpPort) && bmpInit();
  usePort('A');
}

void sensorsUpdate() {
  static uint32_t last = 0;
  if (portNum < 0 || millis() - last < 1000) return;
  last = millis();
  // Température : SHT30 (ENV III) > BMP280 > QMP6988 ; pression : QMP6988 > BMP280
  if (d.qmp && usePort(d.qmpPort)) qmpRead(!d.sht && !d.bmp);
  if (d.bmp && usePort(d.bmpPort)) bmpRead(!d.sht, !d.qmp);
  if (d.sht && usePort(d.shtPort)) shtRead();
  usePort('A');  // l'IMU est lue sur le Port A à chaque image
}

float altitudeM(float hPa) { return 44330.f * (1.f - powf(hPa / 1013.25f, 0.1903f)); }

void sensorsSelfTest() {
  const uint8_t v[2] = {0xBE, 0xEF};
  assert(crc8(v, 2) == 0x92);  // exemple de la datasheet SHT3x
  // Exemple de la datasheet BMP280 (§ 3.12) : 25,08 °C et 100 653 Pa (avant sensorsInit, qui lit le vrai étalonnage)
  T1 = 27504; T2 = 26435; T3 = -1000; P1 = 36477; P2 = -10685; P3 = 3024; P4 = 2855; P5 = 140; P6 = -7;
  P7 = 15500; P8 = -14600; P9 = 6000;
  double t, p;
  bmpCompensate(519888, 415148, t, p);
  assert(fabs(t - 25.08) < 0.01 && fabs(p - 100653.27) < 1.0);
  assert(fabsf(altitudeM(1013.25f)) < 0.1f && fabsf(altitudeM(899.f) - 1000.f) < 15.f);
}
