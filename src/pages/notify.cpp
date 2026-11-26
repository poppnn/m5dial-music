// Écrans d'appel et de notification (design M5Dial Fluid).
// Appel entrant (prioritaire sur toutes les pages) : avatar aux initiales dans le dégradé d'accent avec des ondes
// vertes, nom, boutons refuser (rouge) / décrocher (vert). Tap sur un bouton, ou appui molette = décrocher, appui long
// = refuser. Il apparaît en fondu.
// Notification : carte arrondie qui descend avec un ressort par-dessus la page assombrie (app, expéditeur, compteur
// ou texte selon Réglages > Notifs), puis remonte à la fermeture (geste) ou à la fin de sa durée.
#include <algorithm>
#include <vector>
#include "pages.h"
#include "../ancs.h"
#include "../anim.h"
#include "../display.h"
#include "../phone_icon.h"
#include "../settings.h"
#include "../ui.h"

#define TOAST_MS      5000  // app + expéditeur
#define TOAST_TEXT_MS 8000  // avec le texte (option des Réglages)
#define CALL_IN_MS    260

#define CALL_BTN_Y   192
#define CALL_BTN_DX  34   // écart au centre de l'écran
#define CALL_BTN_R   23
#define AVATAR_Y     70

// Premier caractère (UTF-8) du premier et du dernier mot, en capitales.
static std::string initials(const std::string& s) {
  auto firstChar = [](const std::string& w) {
    if (w.empty()) return std::string();
    uint8_t b = w[0];
    size_t n = b < 0x80 ? 1 : b < 0xE0 ? 2 : b < 0xF0 ? 3 : 4;
    return w.substr(0, std::min(n, w.size()));
  };
  std::vector<std::string> words;
  size_t i = 0;
  while (i < s.size()) {
    size_t j = s.find_first_of(" -", i);
    if (j == std::string::npos) j = s.size();
    if (j > i) words.push_back(s.substr(i, j - i));
    i = j + 1;
  }
  if (words.empty()) return "?";
  std::string o = firstChar(words[0]);
  if (words.size() > 1) o += firstChar(words.back());
  return uiUpper(o);
}

bool drawCall(uint32_t now, const std::string& caller) {
  static std::string shownFor;
  static uint32_t since = 0;
  if (caller != shownFor || !since || now - since > 600000) { shownFor = caller; since = now; }
  const int c = 120;
  float keep = uiAlpha;
  uiAlpha = std::min(1.f, (now - since) / (float)CALL_IN_MS);
  uiBackground();
  // Ondes : deux cercles verts qui partent de l'avatar et s'effacent (un cycle toutes les 1,6 s)
  for (int k = 0; k < 2; k++) {
    float q = fmodf(now / 1600.f + k * 0.5f, 1.f);
    uiRingStroke(c, AVATAR_Y, 30 + 16 * easeOutCubic(q), 2, UI_POSITIVE, 0.5f * (1 - q));
  }
  uiDisc(c, AVATAR_Y, 30, GRAD);
  uiText(initials(caller), c, AVATAR_Y + 1, F_TITLE, UI_ON_ACCENT);
  uiText("INCOMING CALL", c, 124, F_CAPTION, UI_POSITIVE);
  uiText(uiFit(caller, 184, F_TITLE), c, 148, F_TITLE, UI_INK);
  int gx = c + CALL_BTN_DX, rx = c - CALL_BTN_DX;
  uiDisc(rx, CALL_BTN_Y, CALL_BTN_R, solid(UI_NEGATIVE));
  uiDisc(gx, CALL_BTN_Y, CALL_BTN_R, solid(UI_POSITIVE));
  uiMask(rx, CALL_BTN_Y, PHONE_DECLINE, PHONE_DECLINE_W, PHONE_DECLINE_H, TFT_WHITE);
  uiMask(gx, CALL_BTN_Y, PHONE_ACCEPT, PHONE_ACCEPT_W, PHONE_ACCEPT_H, TFT_WHITE);
  uiAlpha = keep;
  return true;  // ondes animées
}

// Tap pendant un appel (repère de l'interface) : +1 = bouton vert (décrocher), -1 = rouge (refuser), 0 = ailleurs.
int callButtonAt(int x, int y) {
  const int hit = 34;  // zone plus large que le bouton : on vise mal en conduisant
  auto near = [&](int bx) { return (x - bx) * (x - bx) + (y - CALL_BTN_Y) * (y - CALL_BTN_Y) <= hit * hit; };
  return near(120 + CALL_BTN_DX) ? 1 : near(120 - CALL_BTN_DX) ? -1 : 0;
}

static bool withText(const NotifToast& t) { return settings.notifMode == NOTIF_FULL && !t.message.empty(); }

static uint32_t life(const NotifToast& t) { return withText(t) ? TOAST_TEXT_MS : TOAST_MS; }

// Ouverte (reçoit les gestes) : affichée et pas déjà en train de se fermer.
bool toastShown(uint32_t now) {
  const NotifToast& t = ancsToast();
  return t.since && !t.closing && now - t.since <= life(t);
}

static Spring card;           // position verticale de la carte (0 = posée, -260 = hors écran)
static uint32_t cardFor = 0;  // notification affichée (son instant d'arrivée)

float toastScrim(uint32_t) {
  if (!cardFor) return 0;
  return std::min(1.f, std::max(0.f, 1 + card.x / 200.f));
}

bool drawToast(uint32_t now) {
  const NotifToast& t = ancsToast();
  static uint32_t last = 0;
  float dt = last ? std::min(0.05f, (now - last) / 1000.f) : 0.016f;
  last = now;
  if (!t.since) { cardFor = 0; return false; }
  bool open = !t.closing && now - t.since <= life(t);
  if (cardFor != t.since) {  // nouvelle notification : la carte part d'en haut
    if (!open) return false;
    cardFor = t.since;
    card.x = -260;
    card.v = 0;
  }
  bool moving = card.step(open ? 0.f : -260.f, dt, 210, 22);
  if (!open && card.x < -250) { cardFor = 0; return false; }
  bool text = withText(t);
  // Mise en page (troncatures, coupure en lignes) calculée une fois par notification : refaite à chaque image, ses
  // dizaines de mesures de texte ralentissaient justement les images de l'animation.
  static uint32_t laidFor = 0;
  static bool laidText = false;
  static std::string app, initial, title, lines[3];
  static int nLines = 0;
  if (laidFor != t.since || laidText != text) {
    laidFor = t.since;
    laidText = text;
    std::string a = t.app.empty() ? "Notification" : t.app;
    std::string ini = initials(a);
    uint8_t b0 = ini[0];
    initial = ini.substr(0, b0 < 0x80 ? 1 : b0 < 0xE0 ? 2 : 3);
    app = uiFit(uiUpper(a), 150, F_CAPTION);
    title = uiFit(t.title.empty() ? a : t.title, 168, F_TITLE);
    nLines = text ? uiWrap(t.message, 164, F_BODY, lines, 3) : 0;
  }
  const int c = 120;
  int dy = (int)lroundf(card.x);
  uiPill(22, 38 + dy, 196, 166, UI_SURFACE1, UI_HAIRLINE);
  uiDisc(c, 64 + dy, 13, GRAD);
  uiText(initial, c, 65 + dy, F_STRONG, UI_ON_ACCENT);
  uiText(app, c, 90 + dy, F_CAPTION, rgb565(ACCENT_RGB[settings.accent][0]));
  uiText(title, c, 114 + dy, F_TITLE, UI_INK);
  if (text) {
    for (int i = 0; i < nLines; i++) uiText(lines[i], c, 140 + i * 19 + dy, F_BODY, UI_INK2);
  } else {
    char b[32];
    int n = std::max(1, ancsCount());
    snprintf(b, sizeof(b), "%d new notification%s", n, n > 1 ? "s" : "");
    uiText(b, c, 142 + dy, F_BODY, UI_INK3);
  }
  float keep = uiAlpha;
  uiAlpha *= std::min(1.f, std::max(0.f, 1 + dy / 30.f));
  uiText("PRESS TO DISMISS", c, 218, F_CAPTION, UI_INK3);
  uiAlpha = keep;
  return moving || !open;
}
