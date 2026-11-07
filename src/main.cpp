/*
 * M5Dial -> télécommande multimédia BLE : iPhone via "Apple Media Service" (AMS, avec titre et
 * progression), Android et Windows via HID (touches multimédia). Plus LED Govee, chrono, page système.
 *
 * Modules : ble_phone (AMS + HID), govee (LED), display (rendu DMA), imu (toujours droit),
 * power (rétroéclairage, extinction), anim (easing, glissements), pages/ (une page par fichier).
 * Ici : setup, boucle principale, gestes, changement de page et cadence d'images.
 *
 * iPhone : un appel entrant prend tout l'écran (appui = décrocher, appui long = refuser) ; une notification
 * s'affiche par-dessus la page (nom de l'app + compteur ; texte en option), appui, tap ou glissement = la fermer.
 *
 * Pages, swipe vertical pour passer de l'une à l'autre : Musique, LED, Chrono, Horloge, Système, Réglages.
 *   Encodeur  gauche/droite : volume -/+ (LED : luminosité)
 *   Bouton central / tap    : play/pause (LED : on/off, Chrono : départ/pause)
 *   Bouton central 0,6 s    : LED : molette = luminosité <-> teinte
 *   Bouton central 2 s      : extinction (sur batterie)
 *   Swipe gauche / droite   : piste suivante / précédente (LED : couleur, Chrono : remise à zéro)
 */
#include <M5Dial.h>
#include <algorithm>
#include "ancs.h"
#include "anim.h"
#include "app.h"
#include "ble_phone.h"
#include "config.h"
#include "display.h"
#include "govee.h"
#include "imu.h"
#include "power.h"
#include "clock.h"
#include "settings.h"
#include "text.h"
#include "pages/pages.h"

volatile bool dirty = true;
uint32_t lastInput = 0;
static Page page = PAGE_MUSIC;

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
    page = (Page)((page + (pageDir < 0 ? 1 : PAGE_COUNT - 1)) % PAGE_COUNT);  // vers le haut = page suivante
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
  return anim || dimming || fadeStart || slideDir || pageDir || imuTurning();
}

void setup() {
  setCpuFrequencyMhz(CPU_MHZ);  // ~-20 mA ; le SPI de l'écran (APB 80 MHz) et le BLE ne changent pas
  auto cfg = M5.config();
  cfg.external_imu = true;  // le M5Dial n'a pas d'IMU : cherche une Unit IMU sur le Port A
  M5Dial.begin(cfg, true, false);  // encodeur oui, RFID non
  M5Dial.BtnA.setHoldThresh(2000);
  Serial.begin(115200);
  settingsLoad();
  settingsSelfTest();
  ancsSelfTest();
  cleanTextSelfTest();
  LedColor red = hueColor(0), green = hueColor(120), blue = hueColor(240);  // roue des teintes HSV
  assert(red.r == 255 && !red.g && !red.b && green.g == 255 && !green.r && blue.b == 255 && !blue.g);

  int st; float r, e;  // auto-tests : parseur AMS et repliement d'angle
  assert(parsePlaybackInfo("1,1.0,12.5", st, r, e) && st == 1 && e == 12.5f);
  assert(fabsf(wrap180(350.f) + 10.f) < 1e-3f && fabsf(wrap180(-190.f) - 170.f) < 1e-3f);
  assert(fabsf(easeOutBack(0)) < 1e-4f && fabsf(easeOutBack(1) - 1) < 1e-4f && easeOutBack(0.7f) > 1);  // rebond

  imuInit();      // référence "droit" = orientation au démarrage (volant au centre)
  displayInit();  // buffers d'affichage AVANT le BLE : blocs contigus en SRAM interne
  startBle();
  goveeInit();
  lastInput = millis();
}

void loop() {
  static uint32_t lastUs = micros();
  uint32_t nowUs = micros();
  float dt = (nowUs - lastUs) / 1e6f;
  lastUs = nowUs;

  M5Dial.update();
  if (needSetup) setupAms();
  static Link prevLink = LINK_NONE;
  if (linkMode == LINK_NONE && prevLink != LINK_NONE) ancsReset();
  prevLink = linkMode;
  if (ancsService()) { lastInput = millis(); dirty = true; }  // appel ou notification : écran rallumé
  clockService();  // heure reçue de l'iPhone : appliquée ici (I2C du RTC hors de la tâche BLE)
  imuUpdate(dt);
  goveeService(page == PAGE_LED);

  // Encodeur (accumule les impulsions jusqu'à un cran complet)
  static long encLast = M5Dial.Encoder.read();
  long enc = M5Dial.Encoder.read();
  int detents = 0;
  const long steps = settings.encSteps;
  while (enc - encLast >= steps) { detents++; encLast += steps; }
  while (encLast - enc >= steps) { detents--; encLast -= steps; }
  std::string caller;
  bool call = !bootShown && ancsCall(caller);
  if (!bootShown && !call && detents) PAGES[page]->wheel(detents);  // démarrage, appel : molette ignorée
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
    longDone = true;
    dirty = true;
  }

  if (call) {  // appel entrant : seulement le bouton (gestes simples en conduisant)
    if (!swallowBtn && M5Dial.BtnA.pressedFor(CALL_REJECT_MS)) { ancsAnswer(false); swallowBtn = true; dirty = true; }
    else if (btnClick) { ancsAnswer(true); dirty = true; }
    btnClick = btnHold = false;
  } else if (toastShown(now) && (btnClick || t.wasClicked())) {  // notification affichée : appui = la fermer
    ancsToastDismiss();
    btnClick = false;
    dirty = true;
  } else if (!bootShown && btnClick) {  // au relâcher, pour distinguer l'appui long
    PAGES[page]->click();
    dirty = true;
  } else if (!bootShown && t.wasClicked()) {
    if (PAGES[page]->tap) {  // position ramenée dans le repère de l'interface, tournée de rot degrés
      float r = rot * DEG_TO_RAD, x = t.x - 120, y = t.y - 120;
      PAGES[page]->tap(120 + cosf(r) * x + sinf(r) * y, 120 - sinf(r) * x + cosf(r) * y, t.getClickCount());
    } else {
      PAGES[page]->click();
    }
    dirty = true;
  }

  if (btnHold) shutdown("Power off...");  // appui long 2 s
  if (!bootShown && !call && (t.wasFlicked() || t.wasDragged())) {
    // Geste ramené dans le repère de l'interface, qui peut être tournée de rot degrés.
    float r = rot * DEG_TO_RAD;
    float dx = cosf(r) * t.distanceX() + sinf(r) * t.distanceY();
    float dy = -sinf(r) * t.distanceX() + cosf(r) * t.distanceY();
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
