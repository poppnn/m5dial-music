// Écran de démarrage (aucun appareil connecté) et table des pages.
#include <algorithm>
#include "pages.h"
#include "../anim.h"
#include "../bootanim.h"
#include "../config.h"
#include "../display.h"
#include "../power.h"
#include "../settings.h"
#include "../logo.h"
#include "../ui.h"

extern const PageOps musicPage, ledPage, chronoPage, clockPage, systemPage, settingsPage;
const PageOps* const PAGES[PAGE_COUNT] = {&musicPage, &ledPage, &chronoPage, &clockPage, &systemPage, &settingsPage};
const char* const PAGE_NAMES[PAGE_COUNT] = {"Music", "LED", "Stopwatch", "Clock", "System", "Settings"};

// Téléphone connecté : un anneau part vers le bord en s'effaçant, une pastille d'accent gonfle avec un rebond et la
// coche s'y trace, puis fondu vers l'interface (power.cpp).
static void drawLinked(uint32_t t) {
  const int c = 120;
  float q = std::min(1.f, t / (float)LINK_ANIM_MS);
  uint16_t a = rgb565(ACCENT_RGB[settings.accent][0]);
  if (q < 1) uiRingStroke(c, c, 36 + 80 * easeOutCubic(q), 2, a, 1 - q);
  uiDisc(c, c, 36 * easeOutBackK(std::min(1.f, q / 0.6f), 2.f), GRAD);
  float k = std::min(1.f, std::max(0.f, (q - 0.25f) / 0.35f));
  uiCheck(k, UI_ON_ACCENT);
  float keep = uiAlpha;
  uiAlpha *= k;
  uiText("CONNECTED", c, 182, F_CAPTION, a);
  uiAlpha = keep;
}

// Logo par défaut : trois arcs concentriques en dégradé qui tournent à des vitesses différentes.
static void drawArcsLogo(uint32_t now) {
  const float radii[3] = {26, 38, 50}, speed[3] = {1.4f, -1.f, 0.7f}, len[3] = {200, 140, 90};
  float t = now / 4000.f;
  for (int i = 0; i < 3; i++) {
    uiRingStroke(120, 92, radii[i], 6, UI_SURFACE2);
    float a0 = fmodf(-90 + t * 360 * speed[i] + i * 70, 360.f);
    uiArc(radii[i], 6, a0, len[i], GRAD, 120, 92);
  }
}

// Logo (ou image en arrière-plan), nom Bluetooth à choisir, "Waiting for phone", puce "Press to open".
void drawBoot(uint32_t now) {
  const int c = 120;
  if (linkedAt) { drawLinked(now - linkedAt); return; }
  bool custom = bootAnimDraw(now);  // image ou GIF envoyé (web flasher, Web UI) : plein écran, en arrière-plan
  if (custom) canvas.fillSmoothRoundRect(30, 140, 180, 88, 24, uiCol(UI_GROUND));  // textes lisibles sur l'image
  else if (LOGO_W) canvas.pushImage(c - LOGO_W / 2, 92 - LOGO_H / 2, LOGO_W, LOGO_H, logo_data);
  else drawArcsLogo(now);
  uiText(uiFit(BLE_NAME, 190, F_TITLE), c, 160, F_TITLE, UI_INK);  // nom à choisir dans les réglages Bluetooth
  uiText("Waiting for phone", c, 182, F_BODY, UI_INK3);
  uiChip(212, "Press to open", 0, rgb565(ACCENT_RGB[settings.accent][0]));  // pages sans téléphone (LED, chrono...)
}
