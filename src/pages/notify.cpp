// Écrans iPhone (ANCS) : appel entrant plein écran (prioritaire sur toutes les pages) et notification en
// surimpression. Appel : tap sur le rond vert / rouge, ou appui molette = décrocher, appui long = refuser.
#include <algorithm>
#include "pages.h"
#include "../ancs.h"
#include "../anim.h"
#include "../display.h"
#include "../phone_icon.h"
#include "../settings.h"

#define TOAST_MS      5000  // app + expéditeur
#define TOAST_TEXT_MS 8000  // avec titre et texte (option des Réglages)
#define TOAST_IN_MS   320
#define TOAST_OUT_MS  220

static int wrap(const std::string& s, int maxW, std::string* lines, int maxLines);

// Boutons de l'écran d'appel (style iPhone) : refuser à gauche (rouge), décrocher à droite (vert).
#define CALL_BTN_Y   162
#define CALL_BTN_DX  44   // écart au centre de l'écran
#define CALL_BTN_R   27
#define CALL_GREEN   0x362B  // vert iOS (52, 199, 89) en RGB565
#define CALL_RED     0xF9C6  // rouge iOS (255, 59, 48)

// Combiné (masque d'opacité lissé, phone_icon.h) centré en (cx, cy), blanc sur la couleur du rond : chaque pixel
// est le mélange des deux, pas d'escalier sur les bords.
static void drawHandset(int cx, int cy, const uint8_t* m, int w, int h, uint16_t bg) {
  uint8_t br = (bg >> 11) << 3, bgc = ((bg >> 5) & 63) << 2, bb = (bg & 31) << 3;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      int a = m[y * w + x];
      if (a < 8) continue;
      canvas.drawPixel(cx - w / 2 + x, cy - h / 2 + y,
                       canvas.color565(br + (255 - br) * a / 255, bgc + (255 - bgc) * a / 255, bb + (255 - bb) * a / 255));
    }
}

// Appel : nom de l'appelant en gros, rond rouge (raccrocher) et rond vert (décrocher) qui pulse doucement.
bool drawCall(uint32_t now, const std::string& caller) {
  const int c = 120;
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0xAD55);
  canvas.drawString("Incoming call", c, 44);
  // Nom : le plus gros qui tient sur une ligne, sinon 2 lignes en taille normale
  canvas.setFont(&fonts::efontJA_24);
  canvas.setTextColor(TFT_WHITE);
  float size = 0;
  for (float s : {2.f, 1.5f, 1.f}) {
    canvas.setTextSize(s);
    if (canvas.textWidth(caller.c_str()) <= 204) { size = s; break; }
  }
  if (size) {
    canvas.drawString(caller.c_str(), c, 98);
  } else {
    std::string lines[2];
    int n = wrap(caller, 204, lines, 2);
    for (int i = 0; i < n; i++) canvas.drawString(lines[i].c_str(), c, 84 + i * 28);
  }
  canvas.setTextSize(1);
  // Boutons ; le vert pulse (halo) pour attirer l'œil
  float p = 0.5f + 0.5f * sinf(now * 0.006f);
  int gx = c + CALL_BTN_DX, rx = c - CALL_BTN_DX;
  canvas.fillCircle(gx, CALL_BTN_Y, CALL_BTN_R + 3 + (int)(5 * p), canvas.color565(10, 60 + 40 * p, 25));
  canvas.fillCircle(gx, CALL_BTN_Y, CALL_BTN_R, CALL_GREEN);
  canvas.fillCircle(rx, CALL_BTN_Y, CALL_BTN_R, CALL_RED);
  drawHandset(gx, CALL_BTN_Y, PHONE_ACCEPT, PHONE_ACCEPT_W, PHONE_ACCEPT_H, CALL_GREEN);
  drawHandset(rx, CALL_BTN_Y, PHONE_DECLINE, PHONE_DECLINE_W, PHONE_DECLINE_H, CALL_RED);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0xAD55);
  canvas.drawString("Decline", rx, CALL_BTN_Y + CALL_BTN_R + 14);
  canvas.drawString("Accept", gx, CALL_BTN_Y + CALL_BTN_R + 14);
  return true;  // halo animé
}

// Tap pendant un appel (repère de l'interface) : +1 = rond vert (décrocher), -1 = rond rouge (refuser), 0 = ailleurs.
int callButtonAt(int x, int y) {
  const int hit = CALL_BTN_R + 10;  // zone un peu plus large que le rond : on vise mal en conduisant
  auto near = [&](int bx) { return (x - bx) * (x - bx) + (y - CALL_BTN_Y) * (y - CALL_BTN_Y) <= hit * hit; };
  return near(120 + CALL_BTN_DX) ? 1 : near(120 - CALL_BTN_DX) ? -1 : 0;
}

// Coupe un texte en lignes de largeur maxW (mots entiers), la dernière tronquée avec "...".
static int wrap(const std::string& s, int maxW, std::string* lines, int maxLines) {
  int n = 0;
  size_t i = 0;
  while (i < s.size() && n < maxLines) {
    size_t end = i, lastFit = i;
    while (end < s.size()) {
      size_t next = s.find(' ', end + 1);
      if (next == std::string::npos) next = s.size();
      if (canvas.textWidth(s.substr(i, next - i).c_str()) > maxW) break;
      lastFit = end = next;
    }
    if (lastFit == i) lastFit = std::min(s.size(), i + 12);  // mot plus long qu'une ligne : coupé
    bool last = n + 1 == maxLines && lastFit < s.size();  // plus de place : reste du texte tronqué
    lines[n++] = last ? fit(s.substr(i), maxW) : s.substr(i, lastFit - i);
    i = lastFit;
    while (i < s.size() && s[i] == ' ') i++;
  }
  return n;
}

static bool withText(const NotifToast& t) { return settings.notifMode == NOTIF_FULL && (!t.title.empty() || !t.message.empty()); }

static uint32_t life(const NotifToast& t) { return withText(t) ? TOAST_TEXT_MS : TOAST_MS; }

// Ouverte (reçoit les gestes) : affichée et pas déjà en train de se fermer.
bool toastShown(uint32_t now) {
  const NotifToast& t = ancsToast();
  return t.since && !t.closing && now - t.since <= life(t);
}

// Notification : panneau au centre. Entre par le haut avec un léger rebond, ressort vers le haut à la fermeture
// (geste) ou à la fin de sa durée. true pendant ces animations (sinon 4 images/s suffisent).
bool drawToast(uint32_t now) {
  const NotifToast& t = ancsToast();
  if (!t.since) return false;
  uint32_t age = now - t.since;
  int32_t outAge = (int32_t)(now - (t.closing ? t.closing : t.since + life(t)));  // < 0 : sortie pas commencée
  if (outAge >= TOAST_OUT_MS) return false;
  float in = age < TOAST_IN_MS ? easeOutBack(age / (float)TOAST_IN_MS) : 1.f;
  float out = outAge > 0 ? easeInCubic(outAge / (float)TOAST_OUT_MS) : 0.f;
  int dy = (int)(-170 * (1.f - in) - 170 * out);
  bool text = withText(t);
  const int c = 120;
  // Tailles choisies pour que les coins arrondis restent dans l'écran rond
  int x0 = text ? 28 : 18, top = (text ? 40 : 56) + dy, h = text ? 164 : 128, room = 240 - 2 * x0 - 16;
  canvas.fillRoundRect(x0, top, 240 - 2 * x0, h, 14, 0x18E3);
  canvas.drawRoundRect(x0, top, 240 - 2 * x0, h, 14, accent());
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_24);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(fit(t.app.empty() ? "Notification" : t.app, room).c_str(), c, top + 28);
  canvas.setFont(&fonts::efontJA_16);
  if (!text) {  // app + expéditeur (titre : nom du contact dans une messagerie), puis le compteur
    char b[32];
    int n = ancsCount();
    snprintf(b, sizeof(b), "%d notification%s", n, n > 1 ? "s" : "");
    canvas.setTextColor(TFT_WHITE);
    if (!t.title.empty()) canvas.drawString(fit(t.title, room).c_str(), c, top + 56);
    canvas.setTextColor(0xAD55);
    canvas.drawString(b, c, t.title.empty() ? top + 60 : top + 80);
  } else {
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(fit(t.title, room).c_str(), c, top + 54);
    std::string lines[3];
    int n = wrap(t.message, room, lines, 3);
    canvas.setTextColor(0xAD55);
    for (int i = 0; i < n; i++) canvas.drawString(lines[i].c_str(), c, top + 78 + i * 20);
  }
  canvas.setTextColor(0x5AEB);
  canvas.drawString("Press to close", c, top + h - 22);
  return age < TOAST_IN_MS || outAge > -300;
}
