#include "ui.h"
#include <algorithm>
#include "display.h"
#include "settings.h"
#include "ui_fonts.h"

float uiAlpha = 1;

// ---------- couleurs ----------
static inline void unpack(uint16_t c, int& r, int& g, int& b) { r = c >> 11; g = (c >> 5) & 63; b = c & 31; }

static inline uint16_t mixi(uint16_t a, uint16_t b, int t) {  // t : 0..256
  int ar, ag, ab, br, bg, bb;
  unpack(a, ar, ag, ab);
  unpack(b, br, bg, bb);
  return ((ar + (((br - ar) * t) >> 8)) << 11) | ((ag + (((bg - ag) * t) >> 8)) << 5) | (ab + (((bb - ab) * t) >> 8));
}

uint16_t mix565(uint16_t a, uint16_t b, float t) { return mixi(a, b, (int)(std::min(1.f, std::max(0.f, t)) * 256)); }
uint16_t uiCol(uint16_t c) { return uiAlpha >= 1 ? c : mix565(UI_GROUND, c, uiAlpha); }
uint16_t uiAccentA() { return uiCol(rgb565(ACCENT_RGB[settings.accent][0])); }
uint16_t uiAccentB() { return uiCol(rgb565(ACCENT_RGB[settings.accent][1])); }

// Dégradé par ligne, recalculé quand l'accent ou l'opacité change.
static uint16_t gradRow[240];
static int gradAccent = -1;
static float gradAlpha = -1;

static void gradUpdate() {
  if (gradAccent == settings.accent && gradAlpha == uiAlpha) return;
  gradAccent = settings.accent;
  gradAlpha = uiAlpha;
  uint32_t a = ACCENT_RGB[settings.accent][0], b = ACCENT_RGB[settings.accent][1];
  for (int y = 0; y < 240; y++) {
    float t = std::min(1.f, std::max(0.f, (y - 8) / 224.f));
    auto ch = [&](int s) { return (uint32_t)lroundf(((a >> s) & 255) + ((int)((b >> s) & 255) - (int)((a >> s) & 255)) * t); };
    gradRow[y] = uiCol(rgb565(ch(16) << 16 | ch(8) << 8 | ch(0)));
  }
}

uint16_t uiGrad(int y) {
  gradUpdate();
  return gradRow[std::min(239, std::max(0, y))];
}

// ---------- pixels (tampon du canvas : octets inversés, swap565) ----------
static inline void blend(int x, int y, uint16_t c, int a) {  // a : 0..256
  if ((unsigned)x >= 240 || (unsigned)y >= 240 || a <= 0) return;
  uint16_t* p = (uint16_t*)canvas.getBuffer() + y * 240 + x;
  *p = __builtin_bswap16(a >= 256 ? c : mixi(__builtin_bswap16(*p), c, a));
}

static inline uint16_t paintAt(const Paint& p, int y) { return p.grad ? gradRow[std::min(239, std::max(0, y))] : p.c; }

void uiDisc(float cx, float cy, float r, Paint p, float alpha) {
  if (r <= 0) return;
  if (p.grad) gradUpdate(); else p.c = uiCol(p.c);
  int y0 = std::max(0, (int)floorf(cy - r - 1)), y1 = std::min(239, (int)ceilf(cy + r + 1));
  int a = (int)(alpha * 256);
  for (int y = y0; y <= y1; y++) {
    float dy = y + 0.5f - cy, h2 = (r + 1) * (r + 1) - dy * dy;
    if (h2 <= 0) continue;
    int xs = (int)ceilf(sqrtf(h2));
    uint16_t c = paintAt(p, y);
    for (int x = std::max(0, (int)(cx - xs)); x <= std::min(239, (int)(cx + xs)); x++) {
      float dx = x + 0.5f - cx, cov = r - sqrtf(dx * dx + dy * dy) + 0.5f;
      if (cov > 0) blend(x, y, c, cov >= 1 ? a : (int)(cov * a));
    }
  }
}

// Anneau entre ri et ro, limité à l'arc [a0, a0 + sweep] (degrés, sens horaire) ; bords radiaux lissés.
static void annulus(float cx, float cy, float ri, float ro, float a0, float sweep, const Paint& p, int alpha) {
  bool full = sweep >= 359.9f, big = sweep > 180;
  float s0 = a0 * DEG_TO_RAD, s1 = (a0 + sweep) * DEG_TO_RAD;
  float sx = cosf(s0), sy = sinf(s0), ex = cosf(s1), ey = sinf(s1);
  int y0 = std::max(0, (int)floorf(cy - ro - 1)), y1 = std::min(239, (int)ceilf(cy + ro + 1));
  for (int y = y0; y <= y1; y++) {
    float dy = y + 0.5f - cy, dy2 = dy * dy, o2 = (ro + 1) * (ro + 1) - dy2;
    if (o2 <= 0) continue;
    int xo = (int)ceilf(sqrtf(o2));
    float i2 = (ri - 1) * (ri - 1) - dy2;
    int xi = (ri > 1 && i2 > 0) ? (int)floorf(sqrtf(i2)) : -1;  // |dx| < xi : dans le trou, rien à faire
    uint16_t c = paintAt(p, y);
    auto px = [&](int x) {
      float dx = x + 0.5f - cx, d = sqrtf(dx * dx + dy2);
      float cov = std::min(ro - d, d - ri) + 0.5f;
      if (cov <= 0) return;
      if (!full) {
        float c1 = sx * dy - sy * dx, c2 = dx * ey - dy * ex;  // à droite du début, à gauche de la fin
        if (big ? (c1 < 0 && c2 < 0) : (c1 < 0 || c2 < 0)) return;
      }
      blend(x, y, c, cov >= 1 ? alpha : (int)(cov * alpha));
    };
    int xl = (int)floorf(cx - xo), xr = (int)ceilf(cx + xo);
    if (xi < 0) {
      for (int x = std::max(0, xl); x <= std::min(239, xr); x++) px(x);
    } else {
      for (int x = std::max(0, xl); x <= std::min(239, (int)ceilf(cx - xi)); x++) px(x);
      for (int x = std::max(0, (int)floorf(cx + xi)); x <= std::min(239, xr); x++) px(x);
    }
  }
}

void uiRingStroke(float x, float y, float r, float w, uint16_t c, float alpha) {
  annulus(x, y, r - w / 2, r + w / 2, 0, 360, solid(uiCol(c)), (int)(alpha * 256));
}

void uiArc(float r, float w, float a0, float sweep, Paint p, float cx, float cy) {
  if (sweep <= 0.3f) return;
  Paint raw = p;  // uiDisc applique l'opacité lui-même
  if (p.grad) gradUpdate(); else p.c = uiCol(p.c);
  annulus(cx, cy, r - w / 2, r + w / 2, a0, std::min(sweep, 360.f), p, 256);
  if (sweep >= 359.9f) return;
  for (float a : {a0, a0 + sweep}) {  // bouts ronds
    float t = a * DEG_TO_RAD;
    uiDisc(cx + r * cosf(t), cy + r * sinf(t), w / 2, raw);
  }
}

void uiKnob(float angle, float r, float w, uint16_t glow) {
  float t = angle * DEG_TO_RAD, x = 120 + r * cosf(t), y = 120 + r * sinf(t);
  uiDisc(x, y, std::max(4.f, w * 0.75f), solid(UI_INK));
  (void)glow;
}

void uiValueArc(float value, Paint p, float w, bool knob, uint16_t glow) {
  value = std::min(1.f, std::max(0.f, value));
  uiRingStroke(120, 120, 112, w, UI_SURFACE2);
  if (value < 0.002f) return;
  float sweep = 360 * value, end = -90 + std::min(sweep, 359.9f), t = end * DEG_TO_RAD;
  float kx = 120 + 112 * cosf(t), ky = 120 + 112 * sinf(t);
  if (knob) {  // halo sous l'arc : seul le bord extérieur dépasse
    uint16_t g = glow ? glow : rgb565(ACCENT_RGB[settings.accent][1]);
    uiDisc(kx, ky, w + 7, solid(g), glow ? 0.25f : 0.45f);
  }
  uiArc(112, w, -90, sweep, p);
  if (knob) uiDisc(kx, ky, std::max(4.f, w * 0.75f), solid(UI_INK));
}

// Halo : accent-b mêlé au fond selon la distance à (120, -30) : 32 % au centre, 6 % à mi-rayon (115), 0 à 210.
// Une seule passe sur le tampon, par anneaux de 2 px (couleurs précalculées quand l'accent ou l'opacité change) ;
// sur chaque ligne on part du centre vers les bords, la distance ne fait que croître.
void uiBackground() {
  const int R0 = 30, STEP = 2, NB = 90;  // anneaux de 30 à 210 px
  static uint16_t band[NB];
  static int bandAccent = -1;
  static float bandAlpha = -1;
  if (bandAccent != settings.accent || bandAlpha != uiAlpha) {
    bandAccent = settings.accent;
    bandAlpha = uiAlpha;
    uint16_t b = rgb565(ACCENT_RGB[settings.accent][1]);
    for (int k = 0; k < NB; k++) {
      float r = R0 + k * STEP + 1;
      float a = r > 115 ? 0.06f * (210 - r) / 95.f : 0.06f + 0.26f * (115 - r) / 85.f;
      band[k] = __builtin_bswap16(uiCol(mix565(UI_GROUND, b, std::min(0.32f, std::max(0.f, a)))));
    }
  }
  const uint16_t g = __builtin_bswap16(uiCol(UI_GROUND));
  uint16_t* buf = (uint16_t*)canvas.getBuffer();
  for (int y = 0; y < 240; y++) {
    uint16_t* row = buf + y * 240;
    float dy = y + 30.5f, dy2 = dy * dy;
    if (dy >= R0 + NB * STEP) { std::fill(row, row + 240, g); continue; }
    int k = std::max(0, (int)((dy - R0) / STEP));
    float thr = (float)(R0 + (k + 1) * STEP) * (R0 + (k + 1) * STEP);
    for (int i = 0; i < 120; i++) {
      float dx = i + 0.5f, d2 = dx * dx + dy2;
      while (k < NB && d2 > thr) { k++; thr = (float)(R0 + (k + 1) * STEP) * (R0 + (k + 1) * STEP); }
      uint16_t c = k < NB ? band[k] : g;
      row[120 + i] = c;
      row[119 - i] = c;
    }
  }
}

void uiPill(int x, int y, int w, int h, uint16_t fill, uint16_t stroke) {
  int r = std::min(w, h) / 2;
  if (stroke != fill) canvas.fillSmoothRoundRect(x, y, w, h, r, uiCol(stroke));
  int i = stroke != fill ? 1 : 0;
  canvas.fillSmoothRoundRect(x + i, y + i, w - 2 * i, h - 2 * i, r - i, uiCol(fill));
}

void uiChip(int y, const char* text, uint16_t dot, uint16_t color) {
  std::string t = uiUpper(text);
  int tw = uiWidth(t, F_CAPTION), w = tw + 23 + (dot ? 12 : 0), x = 120 - w / 2;
  uiPill(x, y - 12, w, 24, UI_SURFACE1, UI_HAIRLINE);
  if (dot) uiDisc(x + 15, y, 3.5f, solid(dot));
  uiText(t, x + 12 + (dot ? 12 : 0), y + 1, F_CAPTION, color, middle_left);
}

void uiPageDots(int count, int index) {
  for (int i = 0; i < count; i++) {
    float a = (i - (count - 1) / 2.f) * 7 * DEG_TO_RAD, x = 120 + 104 * cosf(a), y = 120 + 104 * sinf(a);
    if (i == index) {
      float tx = -sinf(a) * 3.5f, ty = cosf(a) * 3.5f;
      canvas.drawWideLine(x - tx, y - ty, x + tx, y + ty, 2.5f, uiCol(UI_INK));
    } else {
      uiDisc(x, y, 2.2f, solid(UI_INK4));
    }
  }
}

// ---------- texte ----------
static lgfx::PointerWrapper fontData[F_COUNT];
static lgfx::VLWfont fontVlw[F_COUNT];
static bool fontOk[F_COUNT];
static bool vlwActive = false;

void uiInit() {
  const uint8_t* data[F_COUNT] = {FONT_CAPTION, FONT_MICRO, FONT_SMALL, FONT_BODY, FONT_STRONG,
                                  FONT_TITLE, FONT_FIGURE, FONT_DISPLAY, FONT_DISPLAY_XL};
  const uint32_t size[F_COUNT] = {sizeof(FONT_CAPTION), sizeof(FONT_MICRO), sizeof(FONT_SMALL), sizeof(FONT_BODY),
                                  sizeof(FONT_STRONG), sizeof(FONT_TITLE), sizeof(FONT_FIGURE), sizeof(FONT_DISPLAY),
                                  sizeof(FONT_DISPLAY_XL)};
  for (int i = 0; i < F_COUNT; i++) {
    fontData[i].set(data[i], size[i]);
    fontOk[i] = fontVlw[i].loadFont(&fontData[i]);
    if (!fontOk[i]) Serial.printf("UI : police %d non chargée\n", i);
  }
}

static bool covers(UiFont f, const std::string& s) {
  for (size_t i = 0; i < s.size();) {
    uint8_t b = s[i];
    size_t n = b < 0x80 ? 1 : b < 0xE0 ? 2 : b < 0xF0 ? 3 : 4;
    if (i + n > s.size() || n == 4) return false;
    uint16_t cp = n == 1 ? b : n == 2 ? ((b & 0x1F) << 6 | (s[i + 1] & 0x3F))
                                      : ((b & 0x0F) << 12 | (s[i + 1] & 0x3F) << 6 | (s[i + 2] & 0x3F));
    uint16_t idx;
    if (!fontVlw[f].getUnicodeIndex(cp, &idx)) return false;
    i += n;
  }
  return true;
}

void uiFont(UiFont f, const std::string& s) {
  vlwActive = fontOk[f] && covers(f, s);
  if (vlwActive) canvas.setFont(&fontVlw[f]);
  else canvas.setFont(f >= F_TITLE ? &fonts::efontJA_24 : &fonts::efontJA_16);
  canvas.setTextSize(1);
}

int uiWidth(const std::string& s, UiFont f) {
  uiFont(f, s);
  return canvas.textWidth(s.c_str());
}

std::string uiFit(const std::string& s, int maxW, UiFont f) {
  if (uiWidth(s, f) <= maxW) return s;
  const char* dots = vlwActive ? "\xE2\x80\xA6" : "...";
  std::string t = s;
  while (!t.empty() && canvas.textWidth((t + dots).c_str()) > maxW) {
    do t.pop_back(); while (!t.empty() && (t.back() & 0xC0) == 0x80);
    if (!t.empty() && (uint8_t)t.back() >= 0xC0) t.pop_back();
  }
  while (!t.empty() && t.back() == ' ') t.pop_back();
  return t + dots;
}

void uiText(const std::string& s, int x, int y, UiFont f, uint16_t c, textdatum_t d) {
  if (s.empty()) return;
  uiFont(f, s);
  canvas.setTextDatum(d);
  canvas.setTextColor(uiCol(c));
  canvas.drawString(s.c_str(), x, y);
}

std::string uiUpper(const std::string& s) {
  std::string o = s;
  for (size_t i = 0; i < o.size(); i++) {
    uint8_t b = o[i];
    if (b >= 'a' && b <= 'z') o[i] = b - 32;
    else if (b == 0xC3 && i + 1 < o.size()) {  // à..þ (sauf ÷) -> À..Þ : U+00E0..FE = C3 A0..BE
      uint8_t n = o[i + 1];
      if (n >= 0xA0 && n <= 0xBE && n != 0xB7) o[i + 1] = n - 0x20;
      i++;
    }
  }
  return o;
}

int uiWrap(const std::string& s, int maxW, UiFont f, std::string* lines, int maxLines) {
  int n = 0;
  size_t i = 0;
  while (i < s.size() && n < maxLines) {
    size_t end = i, lastFit = i;
    while (end < s.size()) {
      size_t next = s.find(' ', end + 1);
      if (next == std::string::npos) next = s.size();
      if (uiWidth(s.substr(i, next - i), f) > maxW) break;
      lastFit = end = next;
    }
    if (lastFit == i) lastFit = std::min(s.size(), i + 12);  // mot plus long qu'une ligne : coupé
    bool last = n + 1 == maxLines && lastFit < s.size();
    lines[n++] = last ? uiFit(s.substr(i), maxW, f) : s.substr(i, lastFit - i);
    i = lastFit;
    while (i < s.size() && s[i] == ' ') i++;
  }
  return n;
}

// ---------- icônes ----------
static void poly(const float* p, int n, float r, uint16_t c) {
  for (int i = 0; i + 1 < n; i++) canvas.drawWideLine(p[2 * i], p[2 * i + 1], p[2 * i + 2], p[2 * i + 3], r, c);
}

static void triangle(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t c) {
  canvas.fillTriangle(x0, y0, x1, y1, x2, y2, c);
  const float p[] = {x0, y0, x1, y1, x2, y2, x0, y0};
  poly(p, 4, 1.1f, c);  // bords lissés et coins arrondis
}

void uiPlay(int x, int y, uint16_t c) { triangle(x - 5, y - 8, x + 8, y, x - 5, y + 8, uiCol(c)); }

void uiPause(int x, int y, uint16_t c) {
  canvas.fillSmoothRoundRect(x - 7, y - 8, 5, 16, 2, uiCol(c));
  canvas.fillSmoothRoundRect(x + 2, y - 8, 5, 16, 2, uiCol(c));
}

void uiSkip(int x, int y, int d, uint16_t c) {
  c = uiCol(c);
  for (int o : {-4, 4}) {
    int ox = x + o * d;
    triangle(ox - 5 * d, y - 6, ox + 4 * d, y, ox - 5 * d, y + 6, c);
  }
}

static void bezier(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float r, uint16_t c) {
  float px = x0, py = y0;
  for (int i = 1; i <= 6; i++) {
    float t = i / 6.f, u = 1 - t;
    float qx = u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3;
    float qy = u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3;
    canvas.drawWideLine(px, py, qx, qy, r, c);
    px = qx;
    py = qy;
  }
}

void uiShuffle(int x, int y, uint16_t c) {
  c = uiCol(c);
  const float r = 0.9f;
  for (int s : {1, -1}) {  // deux courbes qui se croisent
    canvas.drawWideLine(x - 7, y - 4 * s, x - 4.5f, y - 4 * s, r, c);
    bezier(x - 4.5f, y - 4 * s, x - 0.5f, y - 4 * s, x + 0.5f, y + 4 * s, x + 4.5f, y + 4 * s, r, c);
    canvas.drawWideLine(x + 4.5f, y + 4 * s, x + 7, y + 4 * s, r, c);
  }
  const float a1[] = {x + 5.f, y - 6.5f, x + 7.5f, y - 4.f, x + 5.f, y - 1.5f};
  const float a2[] = {x + 5.f, y + 1.5f, x + 7.5f, y + 4.f, x + 5.f, y + 6.5f};
  poly(a1, 3, r, c);
  poly(a2, 3, r, c);
}

void uiRepeat(int x, int y, uint16_t c, bool one) {
  uint16_t k = uiCol(c);
  const float r = 0.9f;
  // boucle : deux demi-rectangles arrondis, chacun avec sa flèche
  bezier(x - 7, y - 0.5f, x - 7, y - 2.5f, x - 5.5f, y - 4, x - 3.5f, y - 4, r, k);
  canvas.drawWideLine(x - 7, y + 1, x - 7, y - 0.5f, r, k);
  canvas.drawWideLine(x - 3.5f, y - 4, x + 6, y - 4, r, k);
  bezier(x + 7, y + 0.5f, x + 7, y + 2.5f, x + 5.5f, y + 4, x + 3.5f, y + 4, r, k);
  canvas.drawWideLine(x + 7, y - 1, x + 7, y + 0.5f, r, k);
  canvas.drawWideLine(x + 3.5f, y + 4, x - 6, y + 4, r, k);
  const float a1[] = {x + 4.f, y - 6.5f, x + 6.5f, y - 4.f, x + 4.f, y - 1.5f};
  const float a2[] = {x - 4.f, y + 1.5f, x - 6.5f, y + 4.f, x - 4.f, y + 6.5f};
  poly(a1, 3, r, k);
  poly(a2, 3, r, k);
  if (one) {  // pastille "1"
    uiDisc(x + 8, y - 7, 4.5f, solid(rgb565(ACCENT_RGB[settings.accent][0])));
    canvas.drawWideLine(x + 8, y - 9.2f, x + 8, y - 4.8f, 0.7f, uiCol(UI_ON_ACCENT));
    canvas.drawWideLine(x + 8, y - 9.2f, x + 6.9f, y - 8.3f, 0.6f, uiCol(UI_ON_ACCENT));
  }
}

void uiMask(int cx, int cy, const uint8_t* m, int w, int h, uint16_t c) {
  c = uiCol(c);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) blend(cx - w / 2 + x, cy - h / 2 + y, c, m[y * w + x] + (m[y * w + x] >> 7));
}

void uiCheck(float k, uint16_t c) {
  if (k <= 0) return;
  c = uiCol(c);
  // M104 121 L115 132 L137 109 : longueurs 15,6 et 31,8
  float l1 = 15.6f, l2 = 31.8f, L = (l1 + l2) * std::min(1.f, k);
  float e1 = std::min(L, l1) / l1;
  canvas.drawWideLine(104, 121, 104 + 11 * e1, 121 + 11 * e1, 2.5f, c);
  if (L > l1) {
    float e2 = (L - l1) / l2;
    canvas.drawWideLine(115, 132, 115 + 22 * e2, 132 - 23 * e2, 2.5f, c);
  }
}

// ---------- sélecteur courbé ----------
void uiPicker(const char* title, int n, float scroll, const std::function<PickRow(int)>& row, PickState st,
              const char* hint) {
  if (n <= 0) return;
  int sel = std::min(n - 1, std::max(0, (int)lroundf(scroll)));
  uint16_t accentA = rgb565(ACCENT_RGB[settings.accent][0]);
  uint16_t edge = st == PICK_EDIT || st == PICK_CONFIRM ? accentA : st == PICK_DANGER ? UI_NEGATIVE : UI_HAIRLINE;
  int bw = st == PICK_BROWSE ? 1 : 2;
  canvas.fillSmoothRoundRect(26, 102, 188, 36, 18, uiCol(edge));
  canvas.fillSmoothRoundRect(26 + bw, 102 + bw, 188 - 2 * bw, 36 - 2 * bw, 18 - bw, uiCol(UI_SURFACE1));
  for (int i = std::max(0, sel - 3); i <= std::min(n - 1, sel + 3); i++) {
    float d = i - scroll, ad = fabsf(d);
    if (ad > 2.6f) continue;  // au-delà, la ligne toucherait le titre
    int y = (int)lroundf(120 + d * 30 - (d > 0 ? 1 : -1) * std::max(0.f, ad - 1) * 3);
    PickRow r = row(i);
    if (ad < 0.5f) {
      int vw = 0;
      if (!r.value.empty()) {
        std::string v = i == sel && st >= PICK_CONFIRM ? "Confirm?" : r.value;
        uint16_t vc = i == sel && st != PICK_BROWSE ? (st == PICK_DANGER ? UI_NEGATIVE : accentA) : r.valueColor;
        v = uiFit(v, 86, F_BODY);
        vw = uiWidth(v, F_BODY);
        uiText(v, 196, y, F_BODY, vc, middle_right);
        if (st == PICK_EDIT && i == sel) {  // flèches : la molette change la valeur
          uint16_t k = uiCol(accentA);
          canvas.drawWideLine(200, y - 9, 203, y - 12, 0.7f, k);
          canvas.drawWideLine(203, y - 12, 206, y - 9, 0.7f, k);
          canvas.drawWideLine(200, y + 9, 203, y + 12, 0.7f, k);
          canvas.drawWideLine(203, y + 12, 206, y + 9, 0.7f, k);
        }
      }
      int lw = r.value.empty() ? 150 : std::max(40, 150 - vw - 10);
      uiText(uiFit(r.label, lw, F_STRONG), r.value.empty() ? 120 : 46, y, F_STRONG, UI_INK,
             r.value.empty() ? middle_center : middle_left);
    } else {
      float keep = uiAlpha;
      uiAlpha *= std::min(0.75f, std::max(0.f, 1 - (ad - 0.5f) * 0.38f));
      UiFont f = ad < 1.5f ? F_BODY : F_SMALL;
      int room = 2 * textSpace(y, 18, 104) - 16;
      std::string t = r.value.empty() ? r.label : r.label + "  " + r.value;
      uiText(uiFit(t, room, f), 120, y, f, r.valueColor == UI_POSITIVE ? UI_POSITIVE : UI_INK2);
      uiAlpha = keep;
    }
  }
  uiText(uiFit(uiUpper(title), 160, F_CAPTION), 120, 30, F_CAPTION, UI_INK3);
  if (hint) uiChip(214, hint);
  if (n > 1) {  // repère de défilement à gauche : en haut = première ligne
    uiArc(108, 3, 158, 44, solid(UI_SURFACE2));
    float at = 202 - 44 * std::min(1.f, std::max(0.f, scroll / (n - 1)));
    uiArc(108, 3, at - 3, 6, solid(UI_INK2));
  }
}
