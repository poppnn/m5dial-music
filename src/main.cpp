/*
 * M5Dial -> télécommande multimédia BLE : iPhone via "Apple Media Service" (AMS, avec titre et
 * progression), Android et Windows via HID (touches multimédia). Plus LED Govee, chrono, page système.
 *
 * Modules : ble_phone (AMS + HID), govee (LED), display (rendu DMA),
 * power (rétroéclairage, extinction), anim (easing, glissements), pages/ (une page par fichier).
 * Ici : setup, boucle principale, gestes, changement de page et cadence d'images.
 *
 * iPhone : un appel entrant prend tout l'écran (appui = décrocher, appui long = refuser) ; une notification
 * s'affiche par-dessus la page (app + expéditeur ; texte ou rien : Réglages), appui, tap ou glissement = la fermer.
 *
 * Pages, swipe vertical pour passer de l'une à l'autre : Musique, LED, Chrono, Horloge, Système, Réglages (ordre
 * choisi dans la Web UI). Sans téléphone, l'écran de démarrage attend ; appui, tap ou glissement = ouvrir les pages
 * quand même (LED, chrono...). Il revient quand le téléphone se déconnecte.
 *   Encodeur  gauche/droite : volume -/+ (LED : luminosité)
 *   Bouton central / tap    : play/pause (LED : on/off, Chrono : départ/pause)
 *   Bouton central 0,6 s    : LED : molette = luminosité <-> teinte
 *   Bouton central 2 s      : extinction (sur batterie)
 *   Swipe gauche / droite   : piste suivante / précédente (LED : couleur, Chrono : remise à zéro)
 */
#include <M5Dial.h>
#include <algorithm>
#include "ancs.h"
#include "gb.h"
#include "anim.h"
#include "app.h"
#include "ble_phone.h"
#include "bootanim.h"
#include "config.h"
#include "display.h"
#include "encoder.h"
#include "govee.h"
#include "ota.h"
#include "webui.h"
#include "power.h"
#include "clock.h"
#include "settings.h"
#include "text.h"
#include "pages/pages.h"

volatile bool dirty = true;
uint32_t lastInput = 0;
static uint8_t pagePos = 0;     // rang dans settings.pageOrder
static Page page = PAGE_MUSIC;  // = settings.pageOrder[pagePos]

// Retour « haptique » : le M5Dial n'a pas de vibreur, le buzzer bipe (Réglages > Click). Mêmes sons que le firmware
// d'usine (M5Dial-UserDemo, main/hal/hal.cpp) : molette 7000 Hz dans un sens, 6000 Hz dans l'autre, bouton 2000 Hz,
// 20 ms chacun, à pleine puissance.
void haptic(int dir) {
  if (settings.haptic) M5Dial.Speaker.tone(dir < 0 ? 7000 : dir > 0 ? 6000 : 2000, 20);
}

// Changement de page : tout l'écran glisse verticalement (sortie easeIn, puis entrée easeOut), la page
// bascule à mi-parcours. Le décalage est appliqué au moment de l'envoi (pushFrame), sans 2e tampon d'image.
static int pageDir = 0;  // -1 = le contenu part vers le haut, +1 vers le bas, 0 = aucun glissement
static bool pageFlipped = false;
static uint32_t pageStart = 0;

static void startPageSlide(int dir) {
  if (pageDir) return;
  pageDir = dir;
  pageFlipped = false;
  pageStart = millis();
}

static float pageOffset(uint32_t now) {
  if (!pageDir) return 0;
  uint32_t t = now - pageStart;
  if (t < PAGE_OUT_MS) return pageDir * 240.f * easeInCubic(t / (float)PAGE_OUT_MS);
  if (!pageFlipped) {
    pageFlipped = true;
    pagePos = (pagePos + (pageDir < 0 ? 1 : PAGE_COUNT - 1)) % PAGE_COUNT;  // vers le haut = page suivante
    page = (Page)settings.pageOrder[pagePos];
    goveeRetry = 0;  // cherche les LED tout de suite
  }
  if (t < PAGE_OUT_MS + PAGE_IN_MS)
    return -pageDir * 240.f * (1.f - easeOutCubic((t - PAGE_OUT_MS) / (float)PAGE_IN_MS));
  pageDir = 0;
  return 0;
}

// Rend une frame. Retourne true tant qu'une animation est en cours (-> FPS élevé).
static bool draw(float dt) {
  uint32_t now = millis();
  updateSlide(now);
  musicTick(dt, now);  // l'arc et les titres suivent même hors de la page Musique
  canvas.fillScreen(TFT_BLACK);
  updateFade(now);
  bool dimming = updateBacklight(now, dt);
  frameShift = bootShown ? 0 : pageOffset(now);  // avant de dessiner : la page bascule à mi-glissement
  if (bootShown) {
    drawBoot(now);
    pushFrame();
    return true;  // spinner animé en continu
  }
  std::string caller;
  if (ancsCall(caller)) {  // appel entrant : prioritaire sur toutes les pages
    frameShift = 0;
    drawCall(now, caller);
    pushFrame();
    return true;
  }
  bool anim = PAGES[page]->draw(dt);
  anim |= drawToast(now);
  pushFrame();
  return anim || dimming || fadeStart || slideDir || pageDir;
}

void setup() {
  // Moins que les 240 MHz par défaut : le SPI de l'écran (APB 80 MHz) et le BLE ne changent pas. Fixé ici une fois
  // pour toutes : passer ensuite de 80 à 240 MHz pendant le fonctionnement plantait le M5Dial (écran noir).
  setCpuFrequencyMhz(CPU_MHZ);
  Serial.setRxBufferSize(2 * 4096 + 256);  // avant M5Dial.begin (qui ouvre Serial) : un bloc d'image de démarrage tient
  auto cfg = M5.config();
  M5Dial.begin(cfg, false, false);  // ni l'encodeur de la lib (lu par sondage : perdait des pas), ni RFID
  encoderInit();  // molette : compteur matériel PCNT
  M5Dial.BtnA.setHoldThresh(2000);
  Serial.begin(115200);
  settingsLoad();
  settingsSelfTest();
  page = (Page)settings.pageOrder[0];  // première page choisie dans la Web UI
  ancsSelfTest();
  gbSelfTest();
  cleanTextSelfTest();
  bootAnimSelfTest();
  goveeSelfTest();
  otaSelfTest();
  webuiSelfTest();
  LedColor red = hueColor(0), green = hueColor(120), blue = hueColor(240);  // roue des teintes HSV
  assert(red.r == 255 && !red.g && !red.b && green.g == 255 && !green.r && blue.b == 255 && !blue.g);

  int st; float r, e;  // auto-tests : parseur AMS et repliement d'angle
  assert(parsePlaybackInfo("1,1.0,12.5", st, r, e) && st == 1 && e == 12.5f);
  assert(fabsf(wrap180(350.f) + 10.f) < 1e-3f && fabsf(wrap180(-190.f) - 170.f) < 1e-3f);
  assert(fabsf(easeOutBack(0)) < 1e-4f && fabsf(easeOutBack(1) - 1) < 1e-4f && easeOutBack(0.7f) > 1);  // rebond

  M5Dial.Speaker.setVolume(255);  // pleine puissance, comme le firmware d'usine
  displayInit();  // buffers d'affichage AVANT le BLE : blocs contigus en SRAM interne
  bootAnimInit();
  startBle();
  goveeInit();
  lastInput = millis();
}

void loop() {
  M5Dial.update();
  if (needSetup) lastInput = millis();  // téléphone connecté : écran rallumé (il était atténué pendant l'attente) avant l'animation de validation
  startSetup();  // téléphone connecté et chiffré : découverte GATT en tâche de fond
  bootAnimService();  // image / GIF de démarrage envoyé depuis le web flasher (USB)
  static Link prevLink = LINK_NONE;
  if (linkMode == LINK_NONE && prevLink != LINK_NONE) { ancsReset(); bootSkipped = false; }  // retour à l'attente
  prevLink = linkMode;
  gbService();  // Android / Windows : lignes reçues de Gadgetbridge ou du programme PC
  if (ancsService()) { lastInput = millis(); dirty = true; }  // appel ou notification : écran rallumé
  clockService();  // heure reçue de l'iPhone : appliquée ici (I2C du RTC hors de la tâche BLE)
  goveeService(page == PAGE_LED);

  // Encodeur (accumule les impulsions jusqu'à un cran complet)
  static long encLast = encoderRead();
  long enc = encoderRead();
  int detents = 0;
  const long steps = settings.encSteps;
  while (enc - encLast >= steps) { detents++; encLast += steps; }
  while (encLast - enc >= steps) { detents--; encLast -= steps; }
  std::string caller;
  bool call = !bootShown && ancsCall(caller);
  if (!bootShown && !call && detents) { PAGES[page]->wheel(detents); haptic(detents); }  // démarrage, appel : molette ignorée
  if (detents) dirty = true;  // image tout de suite : sinon jusqu'à 250 ms d'attente au repos
  if (volPing) { volPing = false; musicVolumePing(); dirty = true; }

  auto t = M5Dial.Touch.getDetail();
  uint32_t now = millis();
  if (detents || M5Dial.BtnA.isPressed() || t.isPressed()) lastInput = now;  // réveille l'écran atténué

  // Aucun appareil connecté (ni geste) pendant settings.sleepAfterMin : extinction (0 = jamais)
  static uint32_t lastActive = 0;
  if (linkMode != LINK_NONE || lastInput == now) lastActive = now;
  if (settings.sleepAfterMin && now - lastActive > settings.sleepAfterMin * 60000UL) shutdown("Sleeping...");

  // Bouton déjà utilisé (refus d'appel) : ignoré jusqu'à ce qu'il soit relâché, sinon le relâcher ferait un clic.
  static bool swallowBtn = false, longDone = false;  // longDone : appui long court déjà traité, pas de clic au relâcher
  bool btnClick = M5Dial.BtnA.wasClicked() && !swallowBtn && !longDone;
  bool btnHold = M5Dial.BtnA.wasHold() && !swallowBtn;
  if (M5Dial.BtnA.isReleased()) swallowBtn = longDone = false;
  if (!bootShown && !call && !longDone && !swallowBtn && PAGES[page]->longPress && M5Dial.BtnA.pressedFor(LONG_PRESS_MS)) {
    PAGES[page]->longPress();  // l'extinction reste possible en continuant d'appuyer jusqu'à 2 s
    haptic(0);
    longDone = true;
    dirty = true;
  }

  int ux = t.x, uy = t.y;
  // Écran de démarrage (aucun téléphone) : appui, tap ou glissement = ouvrir les pages quand même
  if (bootShown && !linkedAt && !fadeStart && (btnClick || t.wasClicked() || t.wasFlicked() || t.wasDragged())) {
    bootSkipped = true;
    haptic(0);
    btnClick = false;
    dirty = true;
  }
  if (call) {  // appel entrant : rond vert / rouge, ou molette (appui = décrocher, appui long = refuser)
    int b = t.wasClicked() ? callButtonAt(ux, uy) : 0;
    if (b) { ancsAnswer(b > 0); dirty = true; }
    else if (!swallowBtn && M5Dial.BtnA.pressedFor(CALL_REJECT_MS)) { ancsAnswer(false); swallowBtn = true; dirty = true; }
    else if (btnClick) { ancsAnswer(true); dirty = true; }
    btnClick = btnHold = false;
  } else if (toastShown(now) && (btnClick || t.wasClicked())) {  // notification affichée : appui = la fermer
    ancsToastDismiss();
    btnClick = false;
    dirty = true;
  } else if (!bootShown && btnClick) {  // au relâcher, pour distinguer l'appui long
    PAGES[page]->click();
    haptic(0);
    dirty = true;
  } else if (!bootShown && t.wasClicked()) {
    if (PAGES[page]->tap) {
      PAGES[page]->tap(ux, uy, t.getClickCount());
    } else {
      PAGES[page]->click();
    }
    haptic(0);
    dirty = true;
  }

  if (btnHold) shutdown("Power off...");  // appui long 2 s
  if (!bootShown && !call && (t.wasFlicked() || t.wasDragged())) {
    float dx = t.distanceX(), dy = t.distanceY();
    if (toastShown(now)) ancsToastDismiss();  // notification affichée : n'importe quel glissement la ferme
    else if (fabsf(dy) > fabsf(dx) && fabsf(dy) > SWIPE_MIN_PX) startPageSlide(dy < 0 ? -1 : +1);  // vertical : page
    else if (fabsf(dx) > SWIPE_MIN_PX) PAGES[page]->swipeX(dx > 0 ? +1 : -1);
    dirty = true;
  }
  if (titleChanged) {  // piste changée depuis l'iPhone : le nouveau titre entre par la droite
    titleChanged = false;
    if (!slideDir) startSlide(-1, true);
  }

  // ~60 FPS max pendant une animation, 4 Hz au repos (économie de batterie)
  static uint32_t lastDraw = 0, lastFrameUs = micros();
  static bool animating = true;
  if (dirty || millis() - lastDraw >= (animating ? 16u : 250u)) {
    // Plafonné : après 250 ms de repos, un pas de temps entier ferait sauter les animations lissées (~87 % d'un coup)
    float fdt = std::min((micros() - lastFrameUs) / 1e6f, 1 / 30.f);
    lastFrameUs = micros();
    dirty = false;
    lastDraw = millis();
    uint32_t t0 = micros();
    animating = draw(fdt);
    frameMsShown += ((micros() - t0) / 1000.f - frameMsShown) * 0.2f;
    static uint16_t frames = 0;
    static uint32_t fpsStart = 0;
    frames++;
    if (millis() - fpsStart >= 1000) { fpsShown = frames; frames = 0; fpsStart = millis(); }
  }
  // Rend la main à FreeRTOS : le cœur s'arrête (tâche idle) au lieu de tourner à vide. 10 ms au repos suffisent :
  // l'encodeur compte en matériel (PCNT) et le BLE vit dans sa propre tâche.
  delay(animating || dirty ? 1 : 10);
}
