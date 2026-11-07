#include <M5Dial.h>
#include "imu.h"
#include "anim.h"
#include "config.h"
#include "settings.h"

float rot = 0;
static bool imuOk = false;
static float gx = 0, gy = 0, refAngle = 0, targetRot = 0;

static float rawAngle() { return settings.imuDir * atan2f(gx, gy) * RAD_TO_DEG; }

void imuInit() {
  float az;
  imuOk = M5.Imu.isEnabled() && M5.Imu.update() && M5.Imu.getAccel(&gx, &gy, &az);
  if (imuOk) refAngle = rawAngle();
}

void imuUpdate(float dt) {
  float ax, ay, az;
  if (imuOk) {
    M5.Imu.update();
    if (M5.Imu.getAccel(&ax, &ay, &az)) {
      float k = smoothK(dt, IMU_TAU_S);  // passe-bas sur le VECTEUR gravité (pas sur l'angle : pas de saut à ±180°)
      gx += (ax - gx) * k;
      gy += (ay - gy) * k;
      if (gx * gx + gy * gy >= MIN_TILT_G * MIN_TILT_G) {
        float a = wrap180(rawAngle() - refAngle);
        if (fabsf(wrap180(a - targetRot)) > ROT_HYST_DEG) targetRot = a;
      }
    }
  }
  rot = wrap180(rot + wrap180(targetRot - rot) * smoothK(dt, ROT_TAU_S));  // par le chemin le plus court
}

// Sens inversé dans les réglages : les angles changent de signe, la référence "droit" aussi.
void imuFlipped() {
  refAngle = -refAngle;
  targetRot = -targetRot;
  rot = -rot;
}

bool imuTurning() { return fabsf(wrap180(targetRot - rot)) > 0.3f; }
