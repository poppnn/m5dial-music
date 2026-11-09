// Décor Nissan Micra K10 : bandes latérales relevées sur une photo de la voiture (arrière à gauche) : filet noir
// en haut qui plonge en biais et devient la bande noire de la porte, bloc noir incliné, bande grise qui monte en
// biais à l'arrière. Repère du dessin : 1000 x 300, pente des biais dy/dx = 1,38.
#include "k10.h"
#include "display.h"

struct Quad { float x[4], y[4]; };

// Morceaux convexes (chacun = 2 triangles), qui se chevauchent là où le dessin se plie.
static const Quad BLACK_PARTS[] = {
  {{0, 320, 333, 0}, {45, 45, 63, 63}},                // filet du haut
  {{300, 333, 436, 402.6f}, {63, 63, 205, 205}},       // plongée en biais
  {{402.6f, 1000, 1000, 428}, {205, 205, 240, 240}},   // bande noire de la porte
  {{148, 298, 404, 254}, {85, 85, 232, 232}},          // bloc noir incliné
};
static const Quad GREY_PARTS[] = {
  {{40, 123, 285.7f, 203}, {70, 70, 295, 295}},        // montée grise en biais
  {{254.5f, 1000, 1000, 285.7f}, {252, 252, 295, 295}},  // bande grise de la porte
};

static void fill(const Quad& q, int ox, int oy, float k, uint16_t col) {
  int x[4], y[4];
  for (int i = 0; i < 4; i++) { x[i] = ox + lroundf(q.x[i] * k); y[i] = oy + lroundf(q.y[i] * k); }
  canvas.fillTriangle(x[0], y[0], x[1], y[1], x[2], y[2], col);
  canvas.fillTriangle(x[0], y[0], x[2], y[2], x[3], y[3], col);
}

void drawK10(int ox, int oy, float k, uint16_t black, uint16_t grey) {
  for (const Quad& q : GREY_PARTS) fill(q, ox, oy, k, grey);
  for (const Quad& q : BLACK_PARTS) fill(q, ox, oy, k, black);
}
