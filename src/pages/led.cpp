// Page LED : anneau = luminosité, en fondu vers la nouvelle couleur ; la pastille et le nom de l'ancienne
// couleur sortent en glissant, la nouvelle pastille entre avec un rebond.
#include "pages.h"
#include "../anim.h"
#include "../config.h"
#include "../display.h"
#include "../govee.h"

static float ledBriShown = 1;

static bool draw(float dt) {
  static int shownColor = 0;
  static float rgb[3] = {255, 255, 255};
  static uint32_t popStart = 0;
  const int c = 120;
  uint32_t now = millis();
  if (!slideOutPhase && shownColor != ledColor) { shownColor = ledColor; popStart = now; }
  const LedColor& lc = LED_COLORS[shownColor];
  const LedColor& tc = LED_COLORS[ledColor];
  const float trgb[3] = {(float)tc.r, (float)tc.g, (float)tc.b};
  float k = smoothK(dt, COLOR_TAU_S), dist = 0;
  for (int i = 0; i < 3; i++) { rgb[i] += (trgb[i] - rgb[i]) * k; dist += fabsf(trgb[i] - rgb[i]); }

  float target = ledOn ? ledBri / 100.f : 0;
  ledBriShown += (target - ledBriShown) * smoothK(dt, VOL_TAU_S);
  uint16_t arcCol = canvas.color565(rgb[0], rgb[1], rgb[2]);
  uint32_t tp = now - popStart;
  float radius = 34.f * (tp < POP_MS ? 0.6f + 0.4f * easeOutBack(tp / (float)POP_MS) : 1.f);
  int off = (int)slideOff;

  canvas.fillArc(c, c, 119, 111, 0, 360, 0x2104);
  if (ledBriShown > 0.005f) canvas.fillArc(c, c, 119, 111, -90, -90 + 360.f * ledBriShown, arcCol);
  canvas.fillCircle(c + off, 92, (int)radius, ledOn ? canvas.color565(lc.r, lc.g, lc.b) : 0x2104);
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_24);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(lc.name, c + off, 150);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0xAD55);
  const char* status = !goveeConnected() ? "Recherche H7090..." : ledOn ? "LED allumées" : "LED éteintes";
  canvas.drawString(status, c, 182);
  return fabsf(target - ledBriShown) > 0.004f || dist > 3 || tp < POP_MS;
}

static void wheel(int detents) {
  if (detents) ledBri = constrain(ledBri + detents * 5, 5, 100);  // 5 % par cran
}

static void click() {
  ledOn = !ledOn;
  goveeSend({0x33, 0x01, (uint8_t)ledOn});
}

static void swipeX(int dir) {
  ledColor = (ledColor + dir + LED_COLOR_COUNT) % LED_COLOR_COUNT;
  ledOn = true;
  goveeSendColor();
  startSlide(dir, false);
}

extern const PageOps ledPage = {draw, wheel, click, swipeX};
