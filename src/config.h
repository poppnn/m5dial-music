// Constantes de compilation : calibrations, durées d'animation, économie d'énergie.
#pragma once

#ifndef FW_VERSION
#define FW_VERSION "dev"  // le CI injecte la version (tag v*, sinon 1.<nombre de commits>.0)
#endif

// Calibration matérielle : impulsions de l'encodeur par cran.
#define ENC_STEPS_PER_DETENT 2  // défaut (Réglages > Molette) : 2 impulsions par cran mesurées sur ce M5Dial
#define SWIPE_MIN_PX         40

// "Toujours droit" (IMU externe).
#define IMU_DIR        1      // défaut (Réglages > Sens IMU)
#define IMU_TAU_S      0.40f  // passe-bas de la gravité : les vibrations (> ~1 Hz) sont écrasées
#define MIN_TILT_G     0.35f  // gravité dans le plan de l'écran trop faible (posé à plat) : angle gelé
#define ROT_HYST_DEG   8.0f   // hystérésis : on ne suit un nouvel angle qu'au-delà de 8°
#define ROT_TAU_S      0.12f  // lissage visuel de la rotation affichée

// Animations
#define ARC_TAU_S      0.25f  // l'arc de progression rejoint sa valeur en douceur
#define VOL_TAU_S      0.12f
#define VOL_SHOW_MS    1500
#define SLIDE_OUT_MS   200
#define SLIDE_IN_MS    280
#define COLOR_TAU_S    0.18f  // fondu de la couleur de l'anneau LED
#define POP_MS         380    // rebond de la pastille de couleur
#define PAGE_OUT_MS    170    // glissement vertical entre pages : sortie puis entrée
#define PAGE_IN_MS     280
#define FADE_MS        300    // fondu écran de démarrage <-> interface (sortie puis entrée)

// Batterie
#define CPU_MHZ        80     // largement assez pour l'UI ; 80 MHz = minimum pour la radio BLE
#define DIM_AFTER_MS   20000  // sans geste : écran atténué...
#define DIM_LEVEL      0.12f  // ... à 12 % de la luminosité normale
#define DIM_TAU_S      0.35f
#define SLEEP_AFTER_MS (10UL * 60 * 1000)  // aucun appareil connecté pendant 10 min : extinction
