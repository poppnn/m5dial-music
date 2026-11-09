#include <M5Dial.h>
#include "power.h"
#include "anim.h"
#include "app.h"
#include "ble_phone.h"
#include "config.h"
#include "display.h"
#include "settings.h"

bool bootShown = true;
uint32_t fadeStart = 0;
uint32_t linkedAt = 0;
static bool fadeTo = true;
static float fadeLevel = 1, dimLevel = 1;  // rétroéclairage = réglage jour/nuit x fondu x atténuation

// Fondu au noir par le rétroéclairage : l'écran courant s'éteint, bascule, le nouveau s'allume.
void updateFade(uint32_t now) {
  bool wantBoot = linkMode == LINK_NONE;
  if (wantBoot) linkedAt = 0;
  else if (bootShown && !linkedAt) linkedAt = now | 1;
  // Connexion : l'animation de validation se joue en entier sur l'écran de démarrage, puis le fondu
  bool animDone = wantBoot || !bootShown || now - linkedAt >= LINK_ANIM_MS;
  if (!fadeStart && wantBoot != bootShown && animDone) { fadeStart = now | 1; fadeTo = wantBoot; }
  if (!fadeStart) return;
  uint32_t t = now - fadeStart;
  float level;
  if (t < FADE_MS) {
    level = 1.f - easeInCubic(t / (float)FADE_MS);
  } else {
    bootShown = fadeTo;
    level = t < 2 * FADE_MS ? easeOutCubic((t - FADE_MS) / (float)FADE_MS) : 1.f;
    if (t >= 2 * FADE_MS) fadeStart = 0;
  }
  fadeLevel = level;
}

// Atténue l'écran après settings.dimAfterS sans geste (0 = jamais), le rallume en douceur au premier geste.
bool updateBacklight(uint32_t now, float dt) {
  static uint8_t applied = 0;
  bool idle = settings.dimAfterS && now - lastInput > settings.dimAfterS * 1000UL;
  float target = idle ? DIM_LEVEL : 1.f;
  dimLevel += (target - dimLevel) * smoothK(dt, DIM_TAU_S);
  uint8_t bri = settingsBacklight() * fadeLevel * dimLevel;
  if (bri != applied) M5Dial.Display.setBrightness(applied = bri);
  return fabsf(target - dimLevel) > 0.01f;
}

// Message, puis coupure : sur batterie, le maintien d'alimentation (GPIO46) est relâché -> 0 mA, le bouton
// WAKE rallume. Sur USB le courant reste : M5Unified passe en veille profonde (débrancher/rebrancher pour relancer).
// Le bouton du M5Dial (GPIO42) n'est pas une broche RTC : il ne peut pas réveiller une veille profonde.
void shutdown(const char* msg) {
  canvas.fillScreen(TFT_BLACK);
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_24);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(msg, 120, 120);
  frameShift = 0;
  M5Dial.Display.setBrightness(settingsBacklight());
  pushFrame();
  M5Dial.Display.endWrite();  // attend la fin du DMA et libère le bus
  delay(800);
  M5Dial.Display.sleep();
  M5Dial.Power.powerOff();
}
