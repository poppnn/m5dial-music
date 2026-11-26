// Page LED (design M5Dial Fluid). Deux modes de molette, l'un après l'autre à chaque appui long court (0,6 s) :
// - luminosité : l'arc (dans la couleur de la barre) suit la luminosité avec un ressort ;
// - teinte : l'anneau devient une roue des couleurs, la molette fait défiler la teinte (5° par cran) ; le bouton
//   (cerclé de blanc, rempli de la teinte) glisse vers la teinte choisie.
// Pastille de couleur avec son halo, nom du préréglage, puce d'état de la barre (Connected / Disconnected).
// Glisser = préréglage suivant / précédent (la pastille et le nom glissent, la pastille rebondit). Appui = on / off.
#include "pages.h"
#include "../anim.h"
#include "../config.h"
#include "../display.h"
#include "../govee.h"
#include "../settings.h"
#include "../ui.h"

#define HUE_STEP   5      // degrés par cran
#define RING_IN    107    // roue des teintes : 10 px autour du rayon 112, comme l'arc des autres pages
#define RING_OUT   117
#define SWATCH_Y   96

static Spring briS, hueS;

// Appelle f(x, y) pour chaque pixel de l'anneau, ligne par ligne (toujours dans le même ordre).
template <typename F>
static void forRing(F f) {
  for (int y = 0; y < 240; y++) {
    float dy = y + 0.5f - 120;
    if (fabsf(dy) >= RING_OUT) continue;
    int xo = (int)sqrtf(RING_OUT * RING_OUT - dy * dy);
    int xi = fabsf(dy) < RING_IN ? (int)sqrtf(RING_IN * RING_IN - dy * dy) : -1;
    for (int x = 120 - xo; x < 120 + xo; x++) {
      if (xi > 0 && x == 120 - xi) { x = 120 + xi - 1; continue; }  // saute l'intérieur de l'anneau
      f(x, y);
    }
  }
}

// Roue des couleurs : la teinte de chaque pixel (pas de 2°, 0° en haut, sens horaire) est calculée une seule fois ;
// ensuite chaque image n'est qu'une recopie de couleurs dans le tampon (180 fillArc par image étaient trop lents).
static uint8_t* ringIdx = nullptr;

static void drawHueRing(float hue, bool on) {
  if (!ringIdx) {
    size_t n = 0;
    forRing([&](int, int) { n++; });
    ringIdx = (uint8_t*)heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (ringIdx) {
      n = 0;
      forRing([&](int x, int y) {
        float a = atan2f(y + 0.5f - 120, x + 0.5f - 120) * RAD_TO_DEG + 90;  // 0° en haut
        ringIdx[n++] = (uint8_t)((int)(a + 360) % 360 / 2);
      });
    }
  }
  if (ringIdx) {
    uint16_t pal[180];  // couleurs de la roue, octets inversés comme dans le tampon du canvas (swap565)
    for (int i = 0; i < 180; i++) {
      LedColor k = hueColor(i * 2 + 1);
      uint16_t col = canvas.color565(k.r, k.g, k.b);
      pal[i] = __builtin_bswap16(uiCol(on ? col : mix565(UI_GROUND, col, 0.25f)));
    }
    uint16_t* buf = (uint16_t*)canvas.getBuffer();
    size_t n = 0;
    forRing([&](int x, int y) { buf[y * 240 + x] = pal[ringIdx[n++]]; });
  } else {
    uiRingStroke(120, 120, 112, 10, UI_SURFACE2);  // mémoire insuffisante : anneau neutre
  }
  float t = (hue - 90) * DEG_TO_RAD, x = 120 + 112 * cosf(t), y = 120 + 112 * sinf(t);
  LedColor k = hueColor((int)lroundf(hue));
  uiDisc(x, y, 9, solid(UI_INK));
  uiDisc(x, y, 6, solid(canvas.color565(k.r, k.g, k.b)));
}

static bool draw(float dt) {
  static int shownKey = 0;  // préréglage affiché, ou -1 pour la teinte continue
  static uint32_t popStart = 0;
  static bool init = false;
  const int c = 120;
  uint32_t now = millis();
  if (!init) { briS.x = ledBri; hueS.x = ledHue; init = true; }
  int key = ledCustom ? -1 : ledColor;
  if (!slideOutPhase && shownKey != key) {
    if (key >= 0) popStart = now;  // rebond seulement pour un préréglage (la teinte défile en continu)
    shownKey = key;
  }
  float dh = wrap180(ledHue - hueS.x);  // par le chemin le plus court (359° -> 0°)
  hueS.x = ledHue - dh;
  bool moving = hueS.step(ledHue, dt, 300, 28);
  hueS.x = fmodf(hueS.x + 360.f, 360.f);
  moving |= briS.step(ledBri, dt, 260, 24);
  float hue = hueS.x;
  LedColor lc = shownKey < 0 ? hueColor((int)lroundf(hue)) : LED_COLORS[shownKey];
  uint16_t col = canvas.color565(lc.r, lc.g, lc.b);
  uint32_t tp = now - popStart;
  float r = 38.f * (tp < POP_MS ? 0.7f + 0.3f * easeOutBackK(tp / (float)POP_MS, 2.4f) : 1.f);
  int off = (int)(slideOff * 0.6f);

  char cap[32];
  if (ledWheel == WHEEL_HUE) {
    drawHueRing(hue, ledOn);
    snprintf(cap, sizeof(cap), "HUE %d\xC2\xB0", (int)lroundf(hue) % 360);
  } else {
    uiValueArc(ledOn ? briS.x / 100.f : 0, solid(col), 6, ledOn, col);
    if (ledOn) snprintf(cap, sizeof(cap), "BRIGHTNESS %d %%", (int)lroundf(briS.x));
    else snprintf(cap, sizeof(cap), "LIGHT OFF");
  }
  uiText(cap, c, 40, F_CAPTION, UI_INK3);
  float keep = uiAlpha;
  uiAlpha *= slideAlpha;
  if (ledOn) {
    uiDisc(c + off, SWATCH_Y, r + 20, solid(col), 0.22f);  // halo
    uiDisc(c + off, SWATCH_Y, r, solid(col));
  } else {
    uiDisc(c + off, SWATCH_Y, r, solid(UI_INK4));
    uiDisc(c + off, SWATCH_Y, r - 1.5f, solid(UI_SURFACE1));
  }
  uiText(shownKey < 0 ? "Custom" : LED_COLORS[shownKey].name, c + off, 156, F_TITLE, ledOn ? UI_INK : UI_INK3);
  uiAlpha = keep;
  bool conn = goveeConnected();
  uiChip(190, conn ? "Connected" : "Disconnected", conn ? UI_POSITIVE : UI_NEGATIVE);
  return moving || tp < POP_MS;
}

static void wheel(int detents) {
  if (!detents) return;
  if (ledWheel == WHEEL_HUE) {
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
  goveeColorChanged();  // mémorisé après 2 s
  startSlide(dir, false);
}

static void longPress() { ledWheel = (ledWheel + 1) % WHEEL_COUNT; }  // luminosité <-> teinte

extern const PageOps ledPage = {draw, wheel, click, swipeX, nullptr, longPress};
