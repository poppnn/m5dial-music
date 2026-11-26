// Interface « Fluid » : couleurs, dégradé d'accent, polices Manrope lissées et primitives anti-crénelées (arcs à
// bouts ronds, disques, puces, icônes). Tout est dessiné dans `canvas` (display.h), coordonnées de l'écran rond
// 240 x 240, centre (120, 120), angles en degrés dans le sens horaire depuis 3 h (-90 = en haut).
//
// uiAlpha : opacité globale (fondus de page, voile du volume et des notifications). Le fond est uni, donc chaque
// couleur est simplement mêlée au fond (pas de vraie transparence) : uiCol(c).
#pragma once
#include <M5Dial.h>
#include <functional>
#include <string>

constexpr uint16_t rgb565(uint32_t rgb) {
  return (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
}

// Couleurs du design system M5Dial Fluid
#define UI_GROUND    rgb565(0x0b0b0f)  // fond des pages
#define UI_SURFACE1  rgb565(0x17171d)  // pastilles, carte de notification, puces
#define UI_SURFACE2  rgb565(0x23232b)  // partie vide des arcs
#define UI_HAIRLINE  rgb565(0x2e2e38)  // bord des pastilles
#define UI_INK       rgb565(0xf5f5f7)  // texte principal, bouton blanc des arcs
#define UI_INK2      rgb565(0xb3b3be)  // texte secondaire
#define UI_INK3      rgb565(0x8a8a97)  // légendes, libellés
#define UI_INK4      rgb565(0x4b4b56)  // éteint, désactivé
#define UI_ON_ACCENT rgb565(0x0b0b0f)  // icône sur un fond d'accent
#define UI_POSITIVE  rgb565(0x32d74b)
#define UI_NEGATIVE  rgb565(0xff453a)

enum UiFont : uint8_t { F_CAPTION, F_MICRO, F_SMALL, F_BODY, F_STRONG, F_TITLE, F_FIGURE, F_DISPLAY, F_DISPLAY_XL, F_COUNT };

extern float uiAlpha;

void uiInit();                       // charge les polices (une fois, après displayInit)
uint16_t mix565(uint16_t a, uint16_t b, float t);  // a -> b
uint16_t uiCol(uint16_t c);          // c à l'opacité uiAlpha
uint16_t uiAccentA();                // couleurs de l'accent choisi (Réglages > Accent), à l'opacité uiAlpha
uint16_t uiAccentB();
uint16_t uiGrad(int y);              // dégradé vertical accent-a (haut) -> accent-b (bas)

// Peinture : couleur unie, ou dégradé d'accent (la couleur dépend de la ligne).
struct Paint { uint16_t c; bool grad; };
inline Paint solid(uint16_t c) { return {c, false}; }
constexpr Paint GRAD = {0, true};

void uiBackground();                                  // fond + halo de l'accent venu du haut
void uiDisc(float x, float y, float r, Paint p, float alpha = 1);  // disque lissé
void uiRingStroke(float x, float y, float r, float w, uint16_t c, float alpha = 1);  // cercle (trait de largeur w)
void uiArc(float r, float w, float a0, float sweep, Paint p, float cx = 120, float cy = 120);  // arc à bouts ronds
// Arc de valeur : piste (surface-2), arc depuis le haut, bouton blanc et halo au bout. value 0..1.
void uiValueArc(float value, Paint p, float w = 6, bool knob = true, uint16_t glow = 0);
void uiKnob(float angle, float r, float w, uint16_t glow);  // glow 0 : accent-b
void uiPill(int x, int y, int w, int h, uint16_t fill, uint16_t stroke);
void uiChip(int y, const char* text, uint16_t dot = 0, uint16_t color = UI_INK2);
void uiPageDots(int count, int index);

// Texte (police Manrope ; efont si un caractère n'y est pas : japonais, cyrillique...)
void uiFont(UiFont f, const std::string& s);
int uiWidth(const std::string& s, UiFont f);
std::string uiFit(const std::string& s, int maxW, UiFont f);  // tronque avec "…"
void uiText(const std::string& s, int x, int y, UiFont f, uint16_t c, textdatum_t d = middle_center);
std::string uiUpper(const std::string& s);  // capitales (ASCII et Latin-1), pour les légendes
int uiWrap(const std::string& s, int maxW, UiFont f, std::string* lines, int maxLines);

// Icônes
void uiPlay(int x, int y, uint16_t c);
void uiPause(int x, int y, uint16_t c);
void uiSkip(int x, int y, int dir, uint16_t c);
void uiShuffle(int x, int y, uint16_t c);
void uiRepeat(int x, int y, uint16_t c, bool one);
void uiMask(int cx, int cy, const uint8_t* m, int w, int h, uint16_t c);  // masque d'opacité 8 bits (combiné)
void uiCheck(float k, uint16_t c);  // coche au centre, tracée jusqu'à k (0..1)

// Sélecteur courbé (Réglages, liste Wi-Fi) : les lignes glissent à travers une pastille fixe au centre, rapetissent
// et s'effacent en s'éloignant ; repère de défilement sur le bord gauche. scroll : position (fractionnaire pendant
// le glissement). value vide = ligne sans valeur.
struct PickRow { std::string label, value; uint16_t valueColor = UI_INK2; };
enum PickState : uint8_t { PICK_BROWSE, PICK_EDIT, PICK_CONFIRM, PICK_DANGER };
void uiPicker(const char* title, int n, float scroll, const std::function<PickRow(int)>& row, PickState st,
              const char* hint = nullptr);
