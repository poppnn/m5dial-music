// Page Musique : arc de progression, titre / artiste (iPhone), anneau de volume, heure.
// iPhone : icônes aléatoire / répétition (tap dessus = changer), double tap ailleurs = "J'aime" (cœur),
// tap simple = lecture / pause, envoyé après DOUBLE_TAP_MS pour laisser la place au double tap.
#include <algorithm>
#include "pages.h"
#include "../anim.h"
#include "../ble_phone.h"
#include "../config.h"
#include "../clock.h"
#include "../display.h"
#include "../settings.h"

#define DOUBLE_TAP_MS 300  // < 500 ms : délai au-delà duquel M5Unified remet le compte de taps à zéro
#define HEART_MS      900
#define SHUF_X        92   // icônes aléatoire / répétition, en bas de l'écran
#define REP_X         148
#define ICON_Y        204

static Media m;
static float el = 0, arcTarget = 0, arcShown = 0, volShown = 0;
static uint32_t volUntil = 0, tapDeadline = 0, heartStart = 0;
static std::string shownTitle, shownArtist;

void musicTick(float dt, uint32_t now) {
  m = mediaSnapshot();
  el = m.elapsed + (m.playing ? m.rate * (now - m.stamp) / 1000.f : 0);
  if (m.duration > 0 && el > m.duration) el = m.duration;
  arcTarget = m.duration > 0 ? el / m.duration : 0;
  arcShown += (arcTarget - arcShown) * smoothK(dt, ARC_TAU_S);
  if (m.volume >= 0) volShown += (m.volume - volShown) * smoothK(dt, VOL_TAU_S);
  if (!slideOutPhase) { shownTitle = m.title; shownArtist = m.artist; }  // ancien texte figé pendant la sortie
  if (tapDeadline && (int32_t)(now - tapDeadline) >= 0) {  // pas de second tap : c'était un tap simple
    tapDeadline = 0;
    sendCmd(CMD_TOGGLE);
  }
}

// Aléatoire : deux flèches croisées. Répétition : boucle fléchée, "1" pour répéter le morceau.
static void drawShuffle(int x, int y, uint16_t col) {
  canvas.drawWideLine(x - 9, y - 5, x + 6, y + 5, 1.5f, col);
  canvas.drawWideLine(x - 9, y + 5, x + 6, y - 5, 1.5f, col);
  canvas.fillTriangle(x + 10, y - 5, x + 5, y - 9, x + 5, y - 1, col);
  canvas.fillTriangle(x + 10, y + 5, x + 5, y + 1, x + 5, y + 9, col);
}

static void drawRepeat(int x, int y, uint16_t col, bool one) {
  canvas.drawRoundRect(x - 10, y - 7, 20, 14, 5, col);
  canvas.drawRoundRect(x - 9, y - 6, 18, 12, 4, col);
  canvas.fillRect(x - 2, y - 8, 4, 4, TFT_BLACK);  // ouverture de la boucle, puis la flèche
  canvas.fillTriangle(x + 3, y - 7, x - 2, y - 11, x - 2, y - 3, col);
  if (one) {
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(col);
    canvas.drawString("1", x, y + 1);
  }
}

static void drawHeart(int x, int y, float r, uint16_t col) {
  canvas.fillCircle(x - r * 0.5f, y, r * 0.55f, col);
  canvas.fillCircle(x + r * 0.5f, y, r * 0.55f, col);
  canvas.fillTriangle(x - r * 1.02f, y + r * 0.18f, x + r * 1.02f, y + r * 0.18f, x, y + r * 1.25f, col);
}

void musicVolumePing() { volUntil = millis() + VOL_SHOW_MS; }

void volumeWheel(int detents) {
  for (int i = 0; i < abs(detents); i++) sendCmd(detents > 0 ? CMD_VOL_UP : CMD_VOL_DOWN);
  if (detents) volPing = true;
}

static bool draw(float) {
  const int c = 120;
  uint32_t now = millis();
  canvas.fillArc(c, c, 119, 111, 0, 360, 0x2104);  // piste de fond de l'arc
  if (arcShown > 0.002f) canvas.fillArc(c, c, 119, 111, -90, -90 + 360.f * arcShown, accent());

  // Anneau de volume intérieur : visible 1,5 s après un changement, fondu au noir sur les 300 dernières ms
  bool volVisible = (int32_t)(volUntil - now) > 0 && m.volume >= 0;
  if (volVisible) {
    float a = std::min(1.f, (volUntil - now) / 300.f);
    canvas.fillArc(c, c, 106, 101, 0, 360, canvas.color565(30 * a, 30 * a, 34 * a));
    if (volShown > 0.005f)
      canvas.fillArc(c, c, 106, 101, -90, -90 + 360.f * volShown, canvas.color565(70 * a, 160 * a, 255 * a));
  }
  canvas.setTextDatum(middle_center);

  bool info = linkMode == LINK_AMS || (linkMode == LINK_HID && !m.title.empty());  // HID + Gadgetbridge / PC : titre
  if (linkMode == LINK_HID && !info) {
    canvas.setFont(&fonts::efontJA_24);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("Connected", c, 108);
    canvas.setFont(&fonts::efontJA_16);
    canvas.setTextColor(0xAD55);
    canvas.drawString("Remote control", c, 140);
  } else if (info) {  // LINK_NONE : rien, l'interface est en train de disparaître
    if (m.playing) {  // icône lecture / pause
      canvas.fillRect(c - 9, 52, 6, 18, TFT_WHITE);
      canvas.fillRect(c + 3, 52, 6, 18, TFT_WHITE);
    } else {
      canvas.fillTriangle(c - 7, 52, c - 7, 70, c + 9, 61, TFT_WHITE);
    }
    int x = c + (int)slideOff;
    canvas.setFont(&fonts::efontJA_24);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(fit(shownTitle.empty() ? "—" : shownTitle, 190).c_str(), x, 105);
    canvas.setFont(&fonts::efontJA_16);
    canvas.setTextColor(0xAD55);
    canvas.drawString(fit(shownArtist, 170).c_str(), x, 137);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(mmss(el) + " / " + mmss(m.duration), c, 178);
    if (m.shuffle >= 0) drawShuffle(SHUF_X, ICON_Y, m.shuffle ? accent() : 0x4208);
    if (m.repeat >= 0) drawRepeat(REP_X, ICON_Y, m.repeat ? accent() : 0x4208, m.repeat == 1);
  }
  tm t;
  if (clockNow(t)) {
    char hm[8];
    snprintf(hm, sizeof(hm), "%02d:%02d", t.tm_hour, t.tm_min);
    canvas.setFont(&fonts::efontJA_16);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(0x8410);
    canvas.drawString(hm, c, 30);
  }
  uint32_t th = now - heartStart;
  bool heart = heartStart && th < HEART_MS;
  if (heart) {  // "J'aime" envoyé : cœur qui surgit puis s'efface
    float k = th < HEART_MS / 2 ? easeOutBack(th / (HEART_MS / 2.f)) : 1.f;
    float fade = th < HEART_MS / 2 ? 1.f : 1.f - (th - HEART_MS / 2) / (HEART_MS / 2.f);
    drawHeart(c, 100, 34 * k, canvas.color565(255 * fade, 40 * fade, 90 * fade));
  }
  return volVisible || heart || tapDeadline || fabsf(arcTarget - arcShown) > 0.004f;
}

static void click() { sendCmd(CMD_TOGGLE); }

static bool near(int x, int y, int cx) { return abs(x - cx) < 22 && abs(y - ICON_Y) < 20; }

static void tap(int x, int y, uint8_t clicks) {
  if (linkMode != LINK_AMS) { click(); return; }  // HID : ni J'aime ni icônes, pas d'attente
  Media cur = mediaSnapshot();
  if (cur.shuffle >= 0 && near(x, y, SHUF_X)) { sendCmd(CMD_SHUFFLE); return; }
  if (cur.repeat >= 0 && near(x, y, REP_X)) { sendCmd(CMD_REPEAT); return; }
  if (clicks >= 2 && tapDeadline) {  // second tap rapproché : J'aime, et le tap simple en attente est annulé
    tapDeadline = 0;
    sendCmd(CMD_LIKE);
    heartStart = millis() | 1;
    return;
  }
  tapDeadline = millis() + DOUBLE_TAP_MS;
}

static void swipeX(int dir) {
  sendCmd(dir < 0 ? CMD_NEXT : CMD_PREV);  // vers la gauche = suivante (comme un carrousel)
  startSlide(dir, false);
}

extern const PageOps musicPage = {draw, volumeWheel, click, swipeX, tap};
