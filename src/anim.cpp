#include <Arduino.h>
#include "anim.h"
#include "config.h"

int slideDir = 0;
float slideOff = 0;
bool slideOutPhase = false;
static uint32_t slideStart = 0;

void startSlide(int dir, bool inOnly) {
  slideDir = dir;
  slideStart = millis() - (inOnly ? SLIDE_OUT_MS : 0);
}

void updateSlide(uint32_t now) {
  slideOutPhase = false;
  slideOff = 0;
  if (!slideDir) return;
  uint32_t t = now - slideStart;
  if (t < SLIDE_OUT_MS) {
    slideOutPhase = true;
    slideOff = slideDir * 240.f * easeInCubic(t / (float)SLIDE_OUT_MS);
  } else if (t < SLIDE_OUT_MS + SLIDE_IN_MS) {
    slideOff = -slideDir * 240.f * (1.f - easeOutCubic((t - SLIDE_OUT_MS) / (float)SLIDE_IN_MS));
  } else {
    slideDir = 0;
  }
}
