// Maths d'animation (lissage, easing, angles) et glissement horizontal du texte (Musique, LED).
#pragma once
#include <math.h>
#include <stdint.h>

inline float wrap180(float a) { while (a > 180) a -= 360; while (a < -180) a += 360; return a; }
// Lissage exponentiel indépendant du FPS : x rejoint sa cible avec la constante de temps tau.
inline float smoothK(float dt, float tau) { return 1.f - expf(-dt / tau); }
inline float easeOutCubic(float t) { t = 1.f - t; return 1.f - t * t * t; }
inline float easeInCubic(float t) { return t * t * t; }
inline float easeOutBack(float t) { t -= 1.f; return 1.f + 2.70158f * t * t * t + 1.70158f * t * t; }  // dépasse puis revient

// Glissement : sortie (easeIn) puis entrée par le côté opposé (easeOut). Mis à jour une fois par image.
extern int slideDir;        // +1 le texte part à droite, -1 à gauche, 0 aucun glissement
extern float slideOff;      // décalage horizontal courant (px)
extern bool slideOutPhase;  // true pendant la sortie : l'ancien texte reste affiché
void startSlide(int dir, bool inOnly);
void updateSlide(uint32_t now);
