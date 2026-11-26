// Page Musique (design M5Dial Fluid) : arc de progression en dégradé avec son bouton, heure, légende (NOW PLAYING /
// PAUSED), titre et artiste qui glissent au changement de piste, rangée de transport (précédent, lecture / pause
// dans un disque d'accent, suivant), temps écoulé et restant, aléatoire et répétition (iPhone).
// Sans téléphone (pages ouvertes depuis l'écran de démarrage) : "Waiting for phone".
// Voile du volume (toutes les pages où la molette règle le volume) : la page s'assombrit, grand arc blanc et chiffre.
#include <algorithm>
#include "pages.h"
#include "../anim.h"
#include "../ble_phone.h"
#include "../config.h"
#include "../clock.h"
#include "../display.h"
#include "../settings.h"
#include "../ui.h"

#define TRANSPORT_Y 160
#define PREV_X      74
#define NEXT_X      166
#define ICON_Y      216   // aléatoire / répétition
#define SHUF_X      100
#define REP_X       140

static Media m;
static float el = 0, arcTarget = 0, arcShown = 0, arcLag = 0;
static uint32_t volUntil = 0;
static Spring volS, volA;
static bool volMoving = false;
static std::string shownTitle, shownArtist;

void musicTick(float dt, uint32_t now) {
  m = mediaSnapshot();
  el = m.elapsed + (m.playing ? m.rate * (now - m.stamp) / 1000.f : 0);
  if (m.duration > 0 && el > m.duration) el = m.duration;
  arcTarget = m.duration > 0 ? el / m.duration : 0;
  // Seuls les sauts (piste suivante, avance rapide : > 1 % d'un coup) sont lissés : l'arc part de son ancienne
  // position et rattrape. La lecture normale est suivie directement (sinon l'arc reste en retard et relance les
  // 60 images/s par à-coups pendant toute la lecture).
  static float prevTarget = 0;
  if (fabsf(arcTarget - prevTarget) > 0.01f) arcLag -= arcTarget - prevTarget;
  prevTarget = arcTarget;
  arcLag *= 1 - smoothK(dt, ARC_TAU_S);
  if (fabsf(arcLag) < 0.004f) arcLag = 0;
  arcShown = arcTarget + arcLag;
  bool show = (int32_t)(volUntil - now) > 0 && m.volume >= 0;
  if (m.volume >= 0 && !volA.x) volS.x = m.volume;  // voile fermé : le chiffre part de la valeur actuelle
  volMoving = volA.step(show ? 1.f : 0.f, dt, 300, 30);
  if (m.volume >= 0) volMoving |= volS.step(m.volume, dt, 320, 28);
  if (volA.x < 0.003f && !show) volA.x = 0;
  if (!slideOutPhase) { shownTitle = m.title; shownArtist = m.artist; }  // ancien texte figé pendant la sortie
}

void musicVolumePing() { volUntil = millis() + VOL_SHOW_MS; }

void volumeWheel(int detents) {
  for (int i = 0; i < abs(detents); i++) sendCmd(detents > 0 ? CMD_VOL_UP : CMD_VOL_DOWN);
  if (detents) volPing = true;
}

float volumeAlpha() { return std::min(1.f, std::max(0.f, volA.x)); }
bool volumeAnimating() { return volMoving || volA.x > 0; }

void drawVolumeOverlay() {
  float v = std::min(1.f, std::max(0.f, volS.x));
  uiValueArc(v, solid(UI_INK), 10, true, UI_INK);
  uiText("VOLUME", 120, 90, F_CAPTION, UI_INK3);
  uiText(std::to_string((int)lroundf(v * 100)), 120, 124, F_DISPLAY, UI_INK);
  uiText("turn to adjust", 120, 158, F_MICRO, UI_INK3);
}

static bool draw(float) {
  const int c = 120;
  bool info = linkMode == LINK_AMS || (linkMode == LINK_HID && !m.title.empty());  // HID + Gadgetbridge / PC : titre
  uiValueArc(linkMode == LINK_NONE ? 0 : arcShown, GRAD, 6, linkMode != LINK_NONE);
  tm t;
  if (clockNow(t)) {
    char hm[8];
    snprintf(hm, sizeof(hm), "%02d:%02d", t.tm_hour, t.tm_min);
    uiText(hm, c, 40, F_BODY, UI_INK2);
  }
  if (linkMode == LINK_NONE) {
    uiText("NO PHONE", c, 92, F_CAPTION, UI_INK3);
    uiText("Waiting for phone", c, 118, F_TITLE, UI_INK);
    uiText("Other pages still work", c, 142, F_BODY, UI_INK3);
    return arcLag;
  }
  const char* cap = !info ? "REMOTE CONTROL" : m.playing ? "NOW PLAYING" : "PAUSED";
  uiText(cap, c, 68, F_CAPTION, info && m.playing ? rgb565(ACCENT_RGB[settings.accent][0]) : UI_INK3);
  float keep = uiAlpha;
  uiAlpha *= slideAlpha;  // titre et artiste s'effacent en glissant
  int off = (int)slideOff;
  uiText(info ? uiFit(shownTitle.empty() ? "—" : shownTitle, 178, F_TITLE) : "Connected", c + off, 96, F_TITLE, UI_INK);
  uiText(info ? uiFit(shownArtist, 160, F_BODY) : "Android / PC", c + off * 4 / 5, 120, F_BODY, UI_INK2);
  uiAlpha = keep;
  uiSkip(PREV_X, TRANSPORT_Y, -1, UI_INK2);
  uiDisc(c, TRANSPORT_Y, 23, GRAD);
  if (m.playing) uiPause(c, TRANSPORT_Y, UI_ON_ACCENT); else uiPlay(c + 2, TRANSPORT_Y, UI_ON_ACCENT);
  uiSkip(NEXT_X, TRANSPORT_Y, 1, UI_INK2);
  if (info && m.duration > 0) {
    std::string tt = std::string(mmss(el).c_str()) + "  \xC2\xB7  -" + mmss(m.duration - el).c_str();
    uiText(tt, c, 196, F_MICRO, UI_INK3);
  }
  uint16_t on = rgb565(ACCENT_RGB[settings.accent][0]);
  if (linkMode == LINK_AMS && m.shuffle >= 0) uiShuffle(SHUF_X, ICON_Y, m.shuffle ? on : UI_INK4);
  if (linkMode == LINK_AMS && m.repeat >= 0) uiRepeat(REP_X, ICON_Y, m.repeat ? on : UI_INK4, m.repeat == 1);
  return arcLag;
}

static void click() { sendCmd(CMD_TOGGLE); }

static void swipeX(int dir) {
  sendCmd(dir < 0 ? CMD_NEXT : CMD_PREV);  // vers la gauche = suivante (comme un carrousel)
  startSlide(dir, false);
}

static void tap(int x, int y, uint8_t) {
  if (linkMode != LINK_NONE && abs(y - TRANSPORT_Y) < 24) {  // rangée de transport
    if (abs(x - PREV_X) < 22) { swipeX(+1); return; }
    if (abs(x - NEXT_X) < 22) { swipeX(-1); return; }
  }
  Media cur = mediaSnapshot();
  if (linkMode == LINK_AMS && abs(y - ICON_Y) < 16) {
    if (cur.shuffle >= 0 && abs(x - SHUF_X) < 18) { sendCmd(CMD_SHUFFLE); return; }
    if (cur.repeat >= 0 && abs(x - REP_X) < 18) { sendCmd(CMD_REPEAT); return; }
  }
  click();  // tout de suite : plus d'attente d'un double tap
}

extern const PageOps musicPage = {draw, volumeWheel, click, swipeX, tap};
