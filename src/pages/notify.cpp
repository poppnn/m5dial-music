// Écrans iPhone (ANCS) : appel entrant plein écran (prioritaire sur toutes les pages) et notification en
// surimpression. Gestes simples uniquement : appui molette = décrocher / fermer, appui long = refuser.
#include <algorithm>
#include "pages.h"
#include "../ancs.h"
#include "../anim.h"
#include "../display.h"
#include "../settings.h"

#define TOAST_MS      4000  // app + compteur
#define TOAST_TEXT_MS 8000  // avec titre et texte (option des Réglages)
#define TOAST_IN_MS   250

// Appel : nom de l'appelant en très gros, anneau vert qui pulse.
bool drawCall(uint32_t now, const std::string& caller) {
  const int c = 120;
  float p = 0.5f + 0.5f * sinf(now * 0.006f);
  canvas.fillArc(c, c, 119, 111, 0, 360, canvas.color565(0, 90 + 110 * p, 40 + 30 * p));
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0xAD55);
  canvas.drawString("Incoming call", c, 58);
  canvas.setFont(&fonts::efontJA_24);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextSize(1.6f);
  if (canvas.textWidth(caller.c_str()) > 196) canvas.setTextSize(1.f);  // nom long : taille normale, tronqué
  canvas.drawString(fit(caller, 196).c_str(), c, 112);
  canvas.setTextSize(1);
  canvas.setFont(&fonts::efontJA_16);
  canvas.setTextColor(0x07E8);
  canvas.drawString("Press: answer", c, 166);
  canvas.setTextColor(0xF8A2);
  canvas.drawString("Hold: decline", c, 190);
  return true;  // anneau animé
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

static bool withText(const NotifToast& t) { return settings.notifText && (!t.title.empty() || !t.message.empty()); }

bool toastShown(uint32_t now) {
  const NotifToast& t = ancsToast();
  return t.since && now - t.since <= (withText(t) ? TOAST_TEXT_MS : TOAST_MS);
}

// Notification : panneau au centre, entre par le haut. true tant qu'il est visible.
bool drawToast(uint32_t now) {
  if (!toastShown(now)) return false;
  const NotifToast& t = ancsToast();
  bool text = withText(t);
  uint32_t age = now - t.since;
  const int c = 120;
  int dy = age < TOAST_IN_MS ? (int)(-140 * (1.f - easeOutCubic(age / (float)TOAST_IN_MS))) : 0;
  int top = (text ? 48 : 70) + dy, h = text ? 144 : 100, x0 = text ? 24 : 18;
  canvas.fillRoundRect(x0, top, 240 - 2 * x0, h, 14, 0x18E3);
  canvas.drawRoundRect(x0, top, 240 - 2 * x0, h, 14, 0xFA20);
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::efontJA_24);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(fit(t.app.empty() ? "Notification" : t.app, 172).c_str(), c, top + 28);
  canvas.setFont(&fonts::efontJA_16);
  if (!text) {
    char b[32];
    int n = ancsCount();
    snprintf(b, sizeof(b), "%d notification%s", n, n > 1 ? "s" : "");
    canvas.setTextColor(0xAD55);
    canvas.drawString(b, c, top + 66);
    return true;
  }
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(fit(t.title, 172).c_str(), c, top + 56);
  std::string lines[3];
  int n = wrap(t.message, 172, lines, 3);
  canvas.setTextColor(0xAD55);
  for (int i = 0; i < n; i++) canvas.drawString(lines[i].c_str(), c, top + 80 + i * 20);
  return true;
}
