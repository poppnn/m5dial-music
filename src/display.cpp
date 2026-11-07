#include "display.h"
#include <soc/soc_memory_layout.h>  // esp_ptr_dma_capable (IDF 4.4)
#include <algorithm>
#include "imu.h"

M5Canvas canvas(&M5Dial.Display);  // UI dans le repère "droit"
float frameShift = 0;
// 2 bandes de sortie (ping-pong) : le CPU tourne la bande N pendant que le DMA envoie la bande N-1.
#define BAND_H 40  // 240x40x2 = 19,2 Ko par bande, 6 bandes par image
static M5Canvas band[2] = {M5Canvas(&M5Dial.Display), M5Canvas(&M5Dial.Display)};

void displayInit() {
  // setPsram(false) => M5GFX utilise heap_caps_malloc(MALLOC_CAP_DMA). Total 115 + 2x19 = ~154 Ko.
  canvas.setPsram(false);
  canvas.setColorDepth(16);
  bool ok = canvas.createSprite(240, 240);
  for (auto& b : band) {
    b.setPsram(false);
    b.setColorDepth(16);
    ok = ok && b.createSprite(240, BAND_H);
  }
  assert(ok && esp_ptr_dma_capable(canvas.getBuffer()) && esp_ptr_dma_capable(band[1].getBuffer()));
  Serial.printf("Affichage : SRAM DMA libre %u o (plus grand bloc %u o)\n",
                heap_caps_get_free_size(MALLOC_CAP_DMA), heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
  M5Dial.Display.startWrite();  // bus SPI gardé : les transferts DMA ne bloquent plus loop()
}

// Envoie la frame : rendu de la bande k dans band[k&1] pendant que le DMA envoie band[(k-1)&1].
// La transaction SPI reste ouverte (startWrite) : pushImageDMA rend la main tout de suite,
// et le dernier transfert se termine pendant que loop() lit l'encodeur et compose la frame suivante.
void pushFrame() {
  auto& d = M5Dial.Display;
  const uint16_t* src = (const uint16_t*)canvas.getBuffer();
  int s = (int)lroundf(frameShift);
  float r = rot * DEG_TO_RAD;
  for (int k = 0; k < 240 / BAND_H; k++) {
    M5Canvas& b = band[k & 1];  // ce buffer a fini d'être envoyé : waitDMA() a précédé la bande k-1
    int y = k * BAND_H;
    if (fabsf(rot) < 0.5f) {
      uint16_t* dst = (uint16_t*)b.getBuffer();
      for (int row = 0; row < BAND_H; row++) {  // ligne source décalée de s (glissement de page), noir hors image
        int sr = y + row - s;
        if (sr >= 0 && sr < 240) memcpy(dst + row * 240, src + sr * 240, 240 * 2);
        else memset(dst + row * 240, 0, 240 * 2);
      }
    } else {
      if (s) b.fillScreen(TFT_BLACK);  // image décalée : elle ne recouvre plus tout le disque
      // décalage (0, s) de l'interface tourné de rot (sens horaire à l'écran)
      canvas.pushRotateZoom(&b, 120 - s * sinf(r), 120 - y + s * cosf(r), rot, 1.f, 1.f);  // coins hors du disque : invisibles
    }
    d.waitDMA();
    d.pushImageDMA(0, y, 240, BAND_H, (const lgfx::swap565_t*)b.getBuffer());  // format natif du panneau : 0 conversion
  }
}

std::string fit(std::string s, int maxW) {
  if (canvas.textWidth(s.c_str()) <= maxW) return s;
  while (!s.empty() && canvas.textWidth((s + "...").c_str()) > maxW) {
    do s.pop_back(); while (!s.empty() && (s.back() & 0xC0) == 0x80);
    if (!s.empty() && (uint8_t)s.back() >= 0xC0) s.pop_back();
  }
  return s + "...";
}

int textSpace(int y, int h, int r) {
  int dy = std::max(abs(y - h / 2 - 120), abs(y + h / 2 - 120));
  return dy >= r ? 0 : (int)sqrtf((float)(r * r - dy * dy));
}

String mmss(float sec) {
  int s = sec < 0 ? 0 : (int)sec;
  char b[12];
  snprintf(b, sizeof(b), "%d:%02d", s / 60, s % 60);
  return b;
}
