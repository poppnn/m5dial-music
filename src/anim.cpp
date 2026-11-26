#include <Arduino.h>
#include "anim.h"
#include "config.h"

int slideDir = 0;
float slideOff = 0;
float slideAlpha = 1;
bool slideOutPhase = false;
static uint32_t slideStart = 0;

void startSlide(int dir, bool inOnly) {
  slideDir = dir;
  slideStart = millis() - (inOnly ? SLIDE_OUT_MS : 0);
}

void updateSlide(uint32_t now) {
  slideOutPhase = false;
  slideOff = 0;
  slideAlpha = 1;
  if (!slideDir) return;
  uint32_t t = now - slideStart;
  if (t < SLIDE_OUT_MS) {  // sortie de 90 px en s'effaçant, puis retour par l'autre côté avec un léger rebond
    slideOutPhase = true;
    slideOff = slideDir * SLIDE_PX * easeInOutSine(t / (float)SLIDE_OUT_MS);
  } else if (t < SLIDE_OUT_MS + SLIDE_IN_MS) {
    slideOff = -slideDir * SLIDE_PX * (1.f - easeOutBackK((t - SLIDE_OUT_MS) / (float)SLIDE_IN_MS, 1.2f));
  } else {
    slideDir = 0;
  }
  slideAlpha = 1.f - fminf(1.f, fabsf(slideOff) / 120.f);
}
