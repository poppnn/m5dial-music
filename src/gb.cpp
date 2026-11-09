// Protocole Gadgetbridge "Bangle.js" : le téléphone écrit des lignes terminées par \n sur le RX du Nordic UART
// Service, par morceaux de la taille du MTU. Lignes utiles :
//   \x10setTime(1700000000);E.setTimeZone(1.0);...           heure (UTC + fuseau en heures)
//   \x10GB({"t":"notify","src":"WhatsApp","title":"Bob","body":"Salut"})
//   \x10GB({"t":"musicinfo","artist":"..","track":"..","dur":215})   puis "musicstate" : state play/pause, position (s)
//   \x10GB({"t":"call","cmd":"incoming","name":"Bob"})     cmd : incoming, accept, reject, start, end...
// Réponse vers le téléphone (TX) : {"t":"call","n":"ACCEPT"}\n
// Les chaînes sont du JSON façon Espruino : \xHH (Latin-1), \uXXXX, octets bruts >= 0x80 en Latin-1.
#include <Arduino.h>
#include <assert.h>
#include <deque>
#include <mutex>
#include <set>
#include "gb.h"
#include "ancs.h"
#include "ble_phone.h"
#include "clock.h"
#include "text.h"

static const char* NUS_SVC = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* NUS_RX  = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";  // écrit par le téléphone / le PC
static const char* NUS_TX  = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";  // notifié vers eux

static NimBLECharacteristic* tx = nullptr;
static std::mutex mtx;
static std::string partial;
static std::deque<std::string> lines;  // lignes complètes, traitées dans loop()
static std::set<std::string> ids;     // notifications présentes sur le téléphone (compteur)

// Tâche BLE : on ne fait que découper en lignes ; tout le reste (toasts, heure) dans loop().
class RxCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    NimBLEAttValue v = c->getValue();
    std::lock_guard<std::mutex> lock(mtx);
    for (size_t i = 0; i < v.size(); i++) {
      char ch = v.data()[i];
      if (ch == '\n') {
        if (lines.size() < 16) lines.push_back(partial);  // file pleine : ligne perdue
        partial.clear();
      } else if (partial.size() < 2048) {
        partial += ch;  // ligne trop longue : la fin (texte de notification) est coupée
      }
    }
  }
};

static void putUtf8(std::string& o, uint32_t c) {
  if (c < 0x80) {
    o += (char)c;
  } else if (c < 0x800) {
    o += (char)(0xC0 | c >> 6);
    o += (char)(0x80 | (c & 0x3F));
  } else {
    o += (char)(0xE0 | c >> 12);
    o += (char)(0x80 | (c >> 6 & 0x3F));
    o += (char)(0x80 | (c & 0x3F));
  }
}

static int hexv(const std::string& s, size_t i, int n) {  // -1 si pas n chiffres hexadécimaux
  if (i + n > s.size()) return -1;
  int v = 0;
  for (int k = 0; k < n; k++) {
    char c = s[i + k] | 0x20;
    int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
    if (d < 0) return -1;
    v = v * 16 + d;
  }
  return v;
}

std::string gbDecode(const std::string& s) {
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    uint8_t c = s[i];
    if (c == '\\' && i + 1 >= s.size()) break;  // ligne coupée au milieu d'un échappement
    if (c != '\\') {
      // Octet brut : UTF-8 s'il forme une séquence valide (Gadgetbridge échappe les Latin-1 0xC2-0xF4 en \xHH,
      // donc une séquence valide ne peut être que de l'UTF-8), sinon Latin-1.
      size_t n = c >= 0xC2 && c <= 0xDF ? 2 : c >= 0xE0 && c <= 0xEF ? 3 : c >= 0xF0 && c <= 0xF4 ? 4 : 1;
      bool utf8 = n > 1 && i + n <= s.size();
      for (size_t k = 1; utf8 && k < n; k++) utf8 = ((uint8_t)s[i + k] & 0xC0) == 0x80;
      if (utf8) {
        o.append(s, i, n);
        i += n - 1;
      } else if (c >= 32) {
        putUtf8(o, c);
      }
      continue;
    }
    char e = s[++i];
    int v;
    if (e == 'n' || e == 'r' || e == 't' || e == 'v' || e == 'f') {
      o += ' ';  // retours à la ligne : une seule ligne à l'écran
    } else if (e == 'x' && (v = hexv(s, i + 1, 2)) >= 0) {
      if (v >= 32) putUtf8(o, v);
      i += 2;
    } else if (e == 'u' && (v = hexv(s, i + 1, 4)) >= 0) {
      if (v >= 32 && (v < 0xD800 || v > 0xDFFF)) putUtf8(o, v);  // moitiés d'emoji : la police n'en a pas
      i += 4;
    } else if (e != 'b' && (e < '0' || e > '7')) {
      o += e;  // \" \\ \/ ; \b et \0..\7 (contrôles) ignorés
    }
  }
  return o;
}

bool gbField(const std::string& js, const char* key, std::string& out) {
  // "clé": n'apparaît pas dans un texte échappé (on y lirait \"clé\":), le premier trouvé est le bon
  std::string k = std::string("\"") + key + "\":";
  size_t p = js.find(k);
  if (p == std::string::npos) return false;
  p += k.size();
  while (p < js.size() && js[p] == ' ') p++;
  if (p < js.size() && js[p] == '"') {
    size_t e = ++p;
    while (e < js.size() && js[e] != '"') e += js[e] == '\\' ? 2 : 1;
    out = gbDecode(js.substr(p, std::min(e, js.size()) - p));  // guillemet final absent (ligne coupée) : jusqu'au bout
    return true;
  }
  size_t e = js.find_first_of(",}", p);
  out = js.substr(p, e == std::string::npos ? std::string::npos : e - p);
  return !out.empty();
}

static std::string field(const std::string& js, const char* key) {
  std::string v;
  gbField(js, key, v);
  return v;
}

static void handle(const std::string& line) {
  size_t p = line.find("setTime(");
  if (p != std::string::npos) {
    long long t = atoll(line.c_str() + p + 8);
    size_t z = line.find("setTimeZone(");
    float tz = z != std::string::npos ? atof(line.c_str() + z + 12) : 0;
    clockFromLocalEpoch(t + (long long)(tz * 3600));  // l'horloge garde l'heure locale
  }
  p = line.find("GB(");
  size_t b = p == std::string::npos ? p : line.find('{', p);
  if (b == std::string::npos) return;
  std::string js = line.substr(b), t = field(js, "t");
  if (t == "notify" || t == "notify-") {
    std::string id = field(js, "id");
    if (t == "notify-") ids.erase(id);
    else if (ids.size() < 99) ids.insert(id);
    notifCount(ids.size());
    if (t == "notify-") return;
    std::string title = field(js, "title");
    notifPush(field(js, "src"), title.empty() ? field(js, "sender") : title, field(js, "body"));
  } else if (t == "call") {
    std::string name = field(js, "name");
    callPush(field(js, "cmd") == "incoming", name.empty() ? field(js, "number") : name);
  } else if (t == "musicinfo") {
    std::string title = cleanText(field(js, "track")), artist = cleanText(field(js, "artist"));
    float dur = atof(field(js, "dur").c_str());
    mediaEdit([&](Media& m) {
      if (title != m.title) { titleChanged = true; m.elapsed = 0; m.stamp = millis(); }
      m.title = title;
      m.artist = artist;
      m.duration = dur > 0 ? dur : 0;
    });
  } else if (t == "musicstate") {
    std::string pos = field(js, "position");
    bool playing = field(js, "state") == "play";
    mediaEdit([&](Media& m) {
      if (!pos.empty()) m.elapsed = atof(pos.c_str());
      else if (m.playing) m.elapsed += m.rate * (millis() - m.stamp) / 1000.f;  // pause sans position : figée ici
      m.playing = playing;
      m.rate = 1;
      m.stamp = millis();
    });
  }
}

void gbService() {
  if (linkMode == LINK_NONE) ids.clear();  // ancsReset() remet aussi le compteur à 0
  for (;;) {
    std::string line;
    {
      std::lock_guard<std::mutex> lock(mtx);
      if (lines.empty()) return;
      line = std::move(lines.front());
      lines.pop_front();
    }
    handle(line);
  }
}

void gbAnswer(bool accept) {
  if (!tx) return;
  std::string s = accept ? "{\"t\":\"call\",\"n\":\"ACCEPT\"}\n" : "{\"t\":\"call\",\"n\":\"REJECT\"}\n";
  tx->setValue((const uint8_t*)s.data(), s.size());
  tx->notify();
}

void gbInit(NimBLEServer* server) {
  NimBLEService* s = server->createService(NUS_SVC);
  s->createCharacteristic(NUS_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR)->setCallbacks(new RxCB());
  tx = s->createCharacteristic(NUS_TX, NIMBLE_PROPERTY::NOTIFY);
}

void gbSelfTest() {
  assert(gbDecode("Caf\\xe9 \\u20ac\\n!") == "Caf\xC3\xA9 \xE2\x82\xAC !");
  assert(gbDecode("Caf\xE9") == "Caf\xC3\xA9");  // octet Latin-1 brut
  assert(gbDecode("t\xE2\x80\x99" "aime") == "t\xE2\x80\x99" "aime");  // UTF-8 brut gardé tel quel
  std::string v;
  const std::string js = "{\"t\":\"notify\",\"body\":\"a \\\"src\\\": b\",\"src\":\"Messages\",\"dur\":215}";
  assert(gbField(js, "src", v) && v == "Messages");
  assert(gbField(js, "body", v) && v == "a \"src\": b");
  assert(gbField(js, "dur", v) && v == "215");
  assert(!gbField(js, "title", v));
}
