// Page LED. Deux modes de molette, basculés par un appui long court (0,6 s) :
// - luminosité : l'anneau montre la luminosité, en fondu vers la nouvelle couleur ;
// - teinte : l'anneau devient une roue des couleurs, la molette fait défiler la teinte (5° par cran : un tour de
//   molette ~ un tour de roue) ; le repère et la pastille glissent en douceur vers la teinte choisie.
// Glisser = préréglage suivant / précédent : la pastille et le nom sortent en glissant, la nouvelle pastille
// entre avec un rebond. Appui = allumer / éteindre.
#include "pages.h"
#include "../anim.h"
#include "../config.h"
#include "../display.h"
#include "../govee.h"

#define HUE_STEP   5      // degrés par cran
#define HUE_TAU_S  0.07f  // le repère rejoint la teinte choisie en douceur
#define RING_SEG   2      // segments de 2° : dégradé continu (36 segments de 10° faisaient des marches)

static float ledBriShown = 1, hueShown = 0;

static void drawHueRing(int c, float hue, bool on) {
  for (int h = 0; h < 360; h += RING_SEG) {
    LedColor k = hueColor(h + RING_SEG / 2);  // couleur du milieu du segment
    uint16_t col = on ? canvas.color565(k.r, k.g, k.b) : canvas.color565(k.r / 5, k.g / 5, k.b / 5);
    canvas.fillArc(c, c, 119, 111, -90 + h, -90 + h + RING_SEG + 0.5f, col);  // +0,5° : pas de fente entre segments
  }
  canvas.fillArc(c, c, 119, 106, -90 + hue - 2.5f, -90 + hue + 2.5f, TFT_WHITE);  // repère de la teinte choisie
}

static bool draw(float dt) {
  static int shownKey = 0;  // préréglage affiché, ou -1 pour la teinte continue
  static float rgb[3] = {255, 255, 255};
  static uint32_t popStart = 0;
  const int c = 120;
  uint32_t now = millis();
  int key = ledCustom ? -1 : ledColor;
  if (!slideOutPhase && shownKey != key) {
    if (key >= 0) popStart = now;  // rebond seulement pour un préréglage (la teinte défile en continu)
    shownKey = key;
  }
  static bool hueInit = false;
  if (!hueInit) { hueShown = ledHue; hueInit = true; }  // première image : pas de glissement depuis 0°
  float dh = wrap180(ledHue - hueShown);  // par le chemin le plus court (359° -> 0°)
  hueShown = fmodf(hueShown + dh * smoothK(dt, HUE_TAU_S) + 360.f, 360.f);
  LedColor lc = shownKey < 0 ? hueColor((int)lroundf(hueShown)) : LED_COLORS[shownKey];
  LedColor tc = ledCurrent();
  const float trgb[3] = {(float)tc.r, (float)tc.g, (float)tc.b};
  float k = smoothK(dt, COLOR_TAU_S), dist = 0;
  for (int i = 0; i < 3; i++) { rgb[i] += (trgb[i] - rgb[i]) * k; dist += fabsf(trgb[i] - rgb[i]); }

  float target = ledOn ? ledBri / 100.f : 0;
  ledBriShown += (target - ledBriShown) * smoothK(dt, VOL_TAU_S);
  uint32_t tp = now - popStart;
  float radius = 34.f * (tp < POP_MS ? 0.6f + 0.4f * easeOutBack(tp / (float)POP_MS) : 1.f);
  int off = (int)slideOff;

  if (ledHueMode) {
    drawHueRing(c, hueShown, ledOn);
  } else {
    canvas.fillArc(c, c, 119, 111, 0, 360, 0x2104);
    if (ledBriShown > 0.005f)
      canvas.fillArc(c, c, 119, 111, -90, -90 + 360.f * ledBriShown, canvas.color565(rgb[0], rgb[1], rgb[2]));
  }
  canvas.fillCircle(c + off, 92, (int)radius, ledOn ? canvas.color565(lc.r, lc.g, lc.b) : 0x2104);
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_24);
  canvas.setTextColor(TFT_WHITE);
  char name[16];
  if (shownKey < 0) snprintf(name, sizeof(name), "Hue %d°", ledHue);
  else snprintf(name, sizeof(name), "%s", lc.name);
  canvas.drawString(name, c + off, 150);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0xAD55);
  const char* status = !goveeConnected() ? "Searching H7090..." : ledOn ? "LEDs on" : "LEDs off";
  canvas.drawString(status, c, 176);
  canvas.setTextColor(0x5AEB);
  // Alterne toutes les 2,5 s : ce que fait la molette, puis comment changer (128 px max : tient à cette hauteur)
  bool how = now / 2500 % 2;
  canvas.drawString(ledHueMode ? (how ? "Hold: brightness" : "Turn: hue") : (how ? "Hold: hue mode" : "Turn: brightness"), c, 196);
  return fabsf(target - ledBriShown) > 0.004f || dist > 3 || tp < POP_MS || fabsf(dh) > 0.3f;
}

static void wheel(int detents) {
  if (!detents) return;
  if (ledHueMode) {
    ledHue = ((ledHue + detents * HUE_STEP) % 360 + 360) % 360;
    ledCustom = ledOn = true;
    goveeColorChanged();
  } else {
    ledBri = constrain(ledBri + detents * 5, 5, 100);  // 5 % par cran
    goveeChanged();
  }
}

static void click() {
  ledOn = !ledOn;
  goveeSend({0x33, 0x01, (uint8_t)ledOn});
}

static void swipeX(int dir) {
  ledColor = ledCustom ? (dir > 0 ? 0 : LED_COLOR_COUNT - 1) : (ledColor + dir + LED_COLOR_COUNT) % LED_COLOR_COUNT;
  ledCustom = false;
  ledOn = true;
  goveeColorChanged();
  startSlide(dir, false);
}

static void longPress() { ledHueMode = !ledHueMode; }

extern const PageOps ledPage = {draw, wheel, click, swipeX, nullptr, longPress};
