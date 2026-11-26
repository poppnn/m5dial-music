// Maths d'animation (lissage, easing, angles) et glissement horizontal du texte (Musique, LED).
#pragma once
#include <math.h>
#include <stdint.h>

inline float wrap180(float a) { while (a > 180) a -= 360; while (a < -180) a += 360; return a; }
// Lissage exponentiel indépendant du FPS : x rejoint sa cible avec la constante de temps tau.
inline float smoothK(float dt, float tau) { return 1.f - expf(-dt / tau); }
inline float easeOutCubic(float t) { t = 1.f - t; return 1.f - t * t * t; }
inline float easeInCubic(float t) { return t * t * t; }
inline float easeInOutSine(float t) { return 0.5f - 0.5f * cosf(t * (float)M_PI); }  // vitesse max 1,57x la moyenne
inline float easeOutBack(float t) { t -= 1.f; return 1.f + 2.70158f * t * t * t + 1.70158f * t * t; }  // dépasse puis revient
inline float easeOutBackK(float t, float k) { t -= 1.f; return 1.f + (k + 1) * t * t * t + k * t * t; }  // rebond k

// Ressort (Euler semi-implicite), un peu sous-amorti : la valeur dépasse d'un cheveu sa cible puis s'y pose.
// Renvoie true tant qu'il bouge.
struct Spring {
  float x = 0, v = 0;
  bool step(float target, float dt, float k = 260, float c = 26) {
    if (dt > 0.05f) dt = 0.05f;
    v += (k * (target - x) - c * v) * dt;
    x += v * dt;
    if (fabsf(target - x) < 1e-3f && fabsf(v) < 1e-2f) { x = target; v = 0; return false; }
    return true;
  }
};

// Glissement : sortie (easeIn) puis entrée par le côté opposé (easeOut). Mis à jour une fois par image.
extern int slideDir;        // +1 le texte part à droite, -1 à gauche, 0 aucun glissement
extern float slideOff;      // décalage horizontal courant (px)
extern float slideAlpha;    // opacité du texte qui glisse (s'efface en s'éloignant)
extern bool slideOutPhase;  // true pendant la sortie : l'ancien texte reste affiché
void startSlide(int dir, bool inOnly);
void updateSlide(uint32_t now);
