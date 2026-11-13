// Page LED. Trois modes de molette, l'un après l'autre à chaque appui long court (0,6 s) :
// - luminosité : l'anneau montre la luminosité, en fondu vers la nouvelle couleur ;
// - teinte : l'anneau devient une roue des couleurs, la molette fait défiler la teinte (5° par cran : un tour de
//   molette ~ un tour de roue) ; le repère et la pastille glissent en douceur vers la teinte choisie ;
// - effet : la molette choisit fixe / clignotement / respiration / arc-en-ciel ; la pastille le montre.
// Glisser = préréglage suivant / précédent : la pastille et le nom sortent en glissant, la nouvelle pastille
// entre avec un rebond. Appui = allumer / éteindre.
#include "pages.h"
#include "../anim.h"
#include "../config.h"
#include "../display.h"
#include "../govee.h"
#include "../settings.h"

#define HUE_STEP   5      // degrés par cran
#define HUE_TAU_S  0.07f  // le repère rejoint la teinte choisie en douceur
#define RING_IN    111    // anneau de la roue : mêmes rayons que les autres anneaux
#define RING_OUT   120

static float ledBriShown = 1, hueShown = 0;

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

// Roue des couleurs : la teinte de chaque pixel (pas de 2°, 0° en haut, sens horaire) est calculée une seule
// fois (~6,5 Ko) ; ensuite chaque image n'est qu'une recopie de couleurs dans le tampon. 180 fillArc par image
// prenaient trop de temps : la boucle ralentissait et le tactile ne voyait plus les glissements.
static uint8_t* ringIdx = nullptr;

static void drawHueRing(int c, float hue, bool on) {
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
      uint16_t col = on ? canvas.color565(k.r, k.g, k.b) : canvas.color565(k.r / 5, k.g / 5, k.b / 5);
      pal[i] = __builtin_bswap16(col);
    }
    uint16_t* buf = (uint16_t*)canvas.getBuffer();
    size_t n = 0;
    forRing([&](int x, int y) { buf[y * 240 + x] = pal[ringIdx[n++]]; });
  } else {
    canvas.fillArc(c, c, 119, 111, 0, 360, 0x2104);  // mémoire insuffisante : anneau neutre, la pastille suffit
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

  if (ledWheel == WHEEL_HUE) {
    drawHueRing(c, hueShown, ledOn);
  } else {
    canvas.fillArc(c, c, 119, 111, 0, 360, 0x2104);
    if (ledBriShown > 0.005f)
      canvas.fillArc(c, c, 119, 111, -90, -90 + 360.f * ledBriShown, canvas.color565(rgb[0], rgb[1], rgb[2]));
  }
  if (ledFx != FX_FIXED) lc = ledShown(now);  // la pastille joue l'effet (au rythme des images : 4/s au repos)
  canvas.fillCircle(c + off, 92, (int)radius, ledOn ? canvas.color565(lc.r, lc.g, lc.b) : 0x2104);
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_24);
  char name[16];
  if (ledWheel == WHEEL_FX) snprintf(name, sizeof(name), "%s", LED_FX_NAMES[ledFx]);  // la molette choisit l'effet
  else if (shownKey < 0) snprintf(name, sizeof(name), "Hue %d°", ledHue);
  else snprintf(name, sizeof(name), "%s", LED_COLORS[shownKey].name);
  canvas.setTextColor(ledWheel == WHEEL_FX ? accent() : TFT_WHITE);
  canvas.drawString(name, c + off, 150);
  canvas.setFont(&fonts::efontJA_16);
  bool conn = goveeConnected();
  canvas.setTextColor(conn ? 0x07E8 : 0xF8A2);  // barre LED connectée : vert, sinon rouge
  canvas.drawString(conn ? "Connected" : "Disconnected", c, 180);
  if (ledWheel == WHEEL_FX || ledFx != FX_FIXED) {  // en mode effet : le rappeler ; sinon l'effet en cours
    canvas.setTextColor(0x8410);
    canvas.drawString(ledWheel == WHEEL_FX ? "Effect" : LED_FX_NAMES[ledFx], c, 204);
  }
  return fabsf(target - ledBriShown) > 0.004f || dist > 3 || tp < POP_MS || fabsf(dh) > 0.3f;
}

static void wheel(int detents) {
  if (!detents) return;
  if (ledWheel == WHEEL_HUE) {
    ledHue = ((ledHue + detents * HUE_STEP) % 360 + 360) % 360;
    ledCustom = ledOn = true;
    goveeColorChanged();
  } else if (ledWheel == WHEEL_FX) {
    ledFx = ((ledFx + detents) % FX_COUNT + FX_COUNT) % FX_COUNT;
    ledOn = true;
    goveeColorChanged();  // "Fixe" : la couleur unie revient tout de suite ; mémorisé après 2 s
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

static void longPress() { ledWheel = (ledWheel + 1) % WHEEL_COUNT; }  // luminosité -> teinte -> effet -> ...

extern const PageOps ledPage = {draw, wheel, click, swipeX, nullptr, longPress};
