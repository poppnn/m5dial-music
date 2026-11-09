// Animation de démarrage : réception par le port USB (protocole texte + données par blocs de 4 Ko acquittés, pour
// ne jamais déborder le tampon de réception) et lecture directe des JPEG dans la flash projetée en mémoire.
//   -> "BOOTANIM INFO"      <- "BOOTANIM CAP <taille partition> <images>"
//   -> "BOOTANIM CLEAR"     <- "BOOTANIM OK"
//   -> "BOOTANIM PUT <n>"   <- "BOOTANIM READY", puis n octets par blocs de 4096, chacun acquitté par
//                              "BOOTANIM ACK <reçus>" ; à la fin "BOOTANIM DONE <images>" ou "BOOTANIM ERR <raison>"
#include <Arduino.h>
#include <esp_partition.h>
#include "bootanim.h"
#include "app.h"
#include "display.h"

#define HDR        12
#define ENTRY      12
#define CHUNK      4096   // = un secteur de flash
#define MIN_FRAME_MS 20
#define RX_TIMEOUT_MS 5000

static const esp_partition_t* part = nullptr;
static const uint8_t* anim = nullptr;  // partition projetée en mémoire : les JPEG sont lus sur place
static spi_flash_mmap_handle_t mapHandle;
static uint16_t frames = 0;
static uint32_t animMs = 0;

static uint32_t rd32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t* p) { return p[0] | p[1] << 8; }
static uint32_t frameMs(const uint8_t* e) { return max<uint32_t>(rd16(e + 8), MIN_FRAME_MS); }

bool bootAnimParse(const uint8_t* d, size_t n, uint16_t& count, uint32_t& totalMs) {
  if (n < HDR || memcmp(d, "M5BA", 4)) return false;
  count = rd16(d + 4);
  uint32_t total = rd32(d + 8), table = HDR + (uint32_t)count * ENTRY;
  if (!count || total > n || table > total) return false;
  totalMs = 0;
  for (int i = 0; i < count; i++) {
    const uint8_t* e = d + HDR + i * ENTRY;
    uint32_t off = rd32(e), len = rd32(e + 4);
    if (off < table || len < 4 || len > total || off > total - len) return false;
    totalMs += frameMs(e);
  }
  return true;
}

static void unmap() {
  if (anim) spi_flash_munmap(mapHandle);
  anim = nullptr;
  frames = 0;
}

void bootAnimInit() {
  unmap();
  if (!part) part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "bootanim");
  if (!part) return;  // ancienne table de partitions (mise à jour "firmware seul") : pas d'animation possible
  uint8_t h[HDR];
  if (esp_partition_read(part, 0, h, HDR) != ESP_OK || memcmp(h, "M5BA", 4)) return;
  uint32_t total = rd32(h + 8);
  const void* p;
  if (total < HDR || total > part->size || esp_partition_mmap(part, 0, total, SPI_FLASH_MMAP_DATA, &p, &mapHandle) != ESP_OK)
    return;
  anim = (const uint8_t*)p;
  uint16_t n;
  uint32_t ms;
  if (bootAnimParse(anim, total, n, ms)) { frames = n; animMs = ms; }
  else unmap();
}

bool bootAnimDraw(uint32_t now) {
  if (!frames) return false;
  uint32_t t = now % animMs;
  for (int i = 0; i < frames; i++) {
    const uint8_t* e = anim + HDR + i * ENTRY;
    if (t < frameMs(e) || i == frames - 1) { canvas.drawJpg(anim + rd32(e), rd32(e + 4), 0, 0); break; }
    t -= frameMs(e);
  }
  return true;
}

// ---------- réception ----------
static bool receiving = false;
static uint8_t* buf = nullptr;    // bloc en cours ; buf + CHUNK = premier bloc (en-tête), écrit en tout dernier
static uint32_t want = 0, got = 0, bufLen = 0, firstLen = 0, lastRx = 0;
static char line[40];
static uint8_t lineLen = 0;

static bool writeSector(uint32_t off, const uint8_t* d, uint32_t n) {
  return esp_partition_erase_range(part, off, CHUNK) == ESP_OK && esp_partition_write(part, off, d, n) == ESP_OK;
}

static void endRx(const char* err) {
  receiving = false;
  free(buf);
  buf = nullptr;
  if (err) { Serial.printf("BOOTANIM ERR %s\n", err); return; }
  bootAnimInit();
  if (frames) Serial.printf("BOOTANIM DONE %u\n", frames); else Serial.println("BOOTANIM ERR format");
  dirty = true;
}

static void command(const char* s) {
  if (strncmp(s, "BOOTANIM ", 9)) return;
  s += 9;
  if (!strcmp(s, "INFO")) {
    Serial.printf("BOOTANIM CAP %u %u\n", part ? (unsigned)part->size : 0u, frames);
  } else if (!part) {
    Serial.println("BOOTANIM ERR nopart");  // reflasher l'image complète (nouvelle table de partitions)
  } else if (!strcmp(s, "CLEAR")) {
    unmap();
    esp_partition_erase_range(part, 0, CHUNK);
    dirty = true;
    Serial.println("BOOTANIM OK");
  } else if (!strncmp(s, "PUT ", 4)) {
    uint32_t n = strtoul(s + 4, nullptr, 10);
    if (n < HDR || n > part->size) { Serial.println("BOOTANIM ERR size"); return; }
    if (!(buf = (uint8_t*)malloc(2 * CHUNK))) { Serial.println("BOOTANIM ERR memory"); return; }
    unmap();
    esp_partition_erase_range(part, 0, CHUNK);  // l'ancienne animation est invalidée tout de suite
    want = n;
    got = bufLen = firstLen = 0;
    lastRx = millis();
    receiving = true;
    Serial.println("BOOTANIM READY");
  }
}

void bootAnimService() {
  if (!receiving) {
    while (Serial.available()) {
      int c = Serial.read();
      if (c == '\n' || c == '\r') {
        if (lineLen) { line[lineLen] = 0; lineLen = 0; command(line); }
        if (receiving) return;  // la suite : données binaires
      } else if (lineLen < sizeof(line) - 1) {
        line[lineLen++] = c;
      }
    }
    return;
  }
  while (got < want && Serial.available()) {
    int n = Serial.read(buf + bufLen, min<uint32_t>(CHUNK - bufLen, want - got));
    if (n <= 0) break;
    bufLen += n;
    got += n;
    lastRx = millis();
    if (bufLen < CHUNK && got < want) continue;
    uint32_t off = got - bufLen;
    if (!off) { memcpy(buf + CHUNK, buf, bufLen); firstLen = bufLen; }  // en-tête : écrit à la fin seulement
    else if (!writeSector(off, buf, bufLen)) { endRx("flash"); return; }
    bufLen = 0;
    if (got < want) Serial.printf("BOOTANIM ACK %u\n", (unsigned)got);
  }
  if (got == want) endRx(writeSector(0, buf + CHUNK, firstLen) ? nullptr : "flash");
  else if (millis() - lastRx > RX_TIMEOUT_MS) endRx("timeout");
}

void bootAnimSelfTest() {
  uint8_t d[HDR + ENTRY + 4] = {'M', '5', 'B', 'A', 1, 0, 0, 0, sizeof(d), 0, 0, 0,
                                HDR + ENTRY, 0, 0, 0, 4, 0, 0, 0, 100, 0, 0, 0, 0xFF, 0xD8, 0xFF, 0xD9};
  uint16_t n;
  uint32_t ms;
  assert(bootAnimParse(d, sizeof(d), n, ms) && n == 1 && ms == 100);
  d[16] = 5;  // image qui dépasse la fin
  assert(!bootAnimParse(d, sizeof(d), n, ms));
}
