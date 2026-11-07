// Page Musique : arc de progression, titre / artiste (iPhone), anneau de volume.
#include <algorithm>
#include "pages.h"
#include "../anim.h"
#include "../ble_phone.h"
#include "../config.h"
#include "../display.h"

static Media m;
static float el = 0, arcTarget = 0, arcShown = 0, volShown = 0;
static uint32_t volUntil = 0;
static std::string shownTitle, shownArtist;

void musicTick(float dt, uint32_t now) {
  m = mediaSnapshot();
  el = m.elapsed + (m.playing ? m.rate * (now - m.stamp) / 1000.f : 0);
  if (m.duration > 0 && el > m.duration) el = m.duration;
  arcTarget = m.duration > 0 ? el / m.duration : 0;
  arcShown += (arcTarget - arcShown) * smoothK(dt, ARC_TAU_S);
  if (m.volume >= 0) volShown += (m.volume - volShown) * smoothK(dt, VOL_TAU_S);
  if (!slideOutPhase) { shownTitle = m.title; shownArtist = m.artist; }  // ancien texte figé pendant la sortie
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
  if (arcShown > 0.002f) canvas.fillArc(c, c, 119, 111, -90, -90 + 360.f * arcShown, 0xFA20);

  // Anneau de volume intérieur : visible 1,5 s après un changement, fondu au noir sur les 300 dernières ms
  bool volVisible = (int32_t)(volUntil - now) > 0 && m.volume >= 0;
  if (volVisible) {
    float a = std::min(1.f, (volUntil - now) / 300.f);
    canvas.fillArc(c, c, 106, 101, 0, 360, canvas.color565(30 * a, 30 * a, 34 * a));
    if (volShown > 0.005f)
      canvas.fillArc(c, c, 106, 101, -90, -90 + 360.f * volShown, canvas.color565(70 * a, 160 * a, 255 * a));
  }
  canvas.setTextDatum(middle_center);

  if (linkMode == LINK_HID) {
    canvas.setFont(&fonts::efontJA_24);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("Connecté", c, 108);
    canvas.setFont(&fonts::efontJA_16);
    canvas.setTextColor(0xAD55);
    canvas.drawString("Télécommande", c, 140);
  } else if (linkMode == LINK_AMS) {  // LINK_NONE : rien, l'interface est en train de disparaître
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
  }
  return volVisible || fabsf(arcTarget - arcShown) > 0.004f;
}

static void click() { sendCmd(CMD_TOGGLE); }

static void swipeX(int dir) {
  sendCmd(dir < 0 ? CMD_NEXT : CMD_PREV);  // vers la gauche = suivante (comme un carrousel)
  startSlide(dir, false);
}

extern const PageOps musicPage = {draw, volumeWheel, click, swipeX};
