// ANCS (spec Apple) : Notification Source (événements de 8 octets), Control Point (requêtes), Data Source
// (réponses, éventuellement en plusieurs notifications). Les callbacks BLE ne font que mettre en file : les
// requêtes (écriture avec réponse) partent de loop(), jamais de la tâche BLE.
#include <Arduino.h>
#include <map>
#include <mutex>
#include <vector>
#include "ancs.h"
#include "settings.h"

static const char* ANCS_SVC = "7905F431-B5CE-4E99-A40F-4B1E122D00D0";
static const char* ANCS_NS  = "9FBF120D-6301-42D9-8C58-25E699A21DBD";  // Notification Source
static const char* ANCS_CP  = "69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9";  // Control Point
static const char* ANCS_DS  = "22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB";  // Data Source

enum : uint8_t { EV_ADDED = 0, EV_MODIFIED = 1, EV_REMOVED = 2 };
enum : uint8_t { FLAG_SILENT = 1, FLAG_PREEXISTING = 4 };
enum : uint8_t { CAT_INCOMING_CALL = 1 };
enum : uint8_t { CMD_NOTIF_ATTR = 0, CMD_APP_ATTR = 1, CMD_ACTION = 2 };
enum : uint8_t { ATTR_APP_ID = 0, ATTR_TITLE = 1, ATTR_MESSAGE = 3 };
enum : uint8_t { ACTION_POSITIVE = 0, ACTION_NEGATIVE = 1 };

struct Event { uint8_t id, flags, cat; uint32_t uid; };

static NimBLERemoteCharacteristic* cp = nullptr;
static QueueHandle_t events = nullptr;
static std::mutex rxMtx;
static std::vector<uint8_t> rx;  // Data Source accumulé jusqu'à une réponse complète

static int count = 0;
static bool callActive = false;
static uint32_t callUid = 0;
static std::string callName;
static NotifToast toast;
static std::map<std::string, std::string> appNames;  // identifiant d'app -> nom affiché (cache)

// Requête en cours (une seule à la fois) : la réponse arrive sur Data Source.
enum Pending : uint8_t { PEND_NONE, PEND_NOTIF, PEND_APP };
static Pending pending = PEND_NONE;
static uint32_t pendingUid = 0, pendingSince = 0;
static std::string pendingApp, pendingTitle, pendingMsg;

static uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static void onSource(NimBLERemoteCharacteristic*, uint8_t* d, size_t len, bool) {
  if (len < 8 || !events) return;
  Event e = {d[0], d[1], d[2], le32(d + 4)};
  xQueueSend(events, &e, 0);  // file pleine : événement perdu (le compteur se recale à la reconnexion)
}

static void onData(NimBLERemoteCharacteristic*, uint8_t* d, size_t len, bool) {
  std::lock_guard<std::mutex> lock(rxMtx);
  if (rx.size() + len < 1024) rx.insert(rx.end(), d, d + len);
}

void ancsSetup(NimBLEClient* client) {
  ancsReset();
  NimBLERemoteService* svc = client->getService(ANCS_SVC);
  NimBLERemoteCharacteristic* ns = svc ? svc->getCharacteristic(ANCS_NS) : nullptr;
  NimBLERemoteCharacteristic* ds = svc ? svc->getCharacteristic(ANCS_DS) : nullptr;
  NimBLERemoteCharacteristic* c = svc ? svc->getCharacteristic(ANCS_CP) : nullptr;
  if (!ns || !ds || !c) return;
  if (!events) events = xQueueCreate(24, sizeof(Event));
  // Data Source d'abord (recommandation Apple), puis Notification Source : l'iPhone envoie alors les
  // notifications déjà présentes (drapeau "pre-existing").
  if (ds->subscribe(true, onData) && ns->subscribe(true, onSource)) cp = c;
}

void ancsReset() {
  cp = nullptr;
  if (events) xQueueReset(events);
  { std::lock_guard<std::mutex> lock(rxMtx); rx.clear(); }
  count = 0;
  callActive = false;
  pending = PEND_NONE;
  toast.since = 0;
}

static bool write(const std::vector<uint8_t>& v) {
  NimBLERemoteCharacteristic* c = cp;
  return c && c->writeValue(v.data(), v.size(), true);  // le Control Point exige une écriture avec réponse
}

// GetNotificationAttributes : identifiant d'app, titre (64 o max), et message (120 o) si le texte est affiché.
static void requestNotif(uint32_t uid, bool withMessage) {
  std::vector<uint8_t> v = {CMD_NOTIF_ATTR, (uint8_t)uid, (uint8_t)(uid >> 8), (uint8_t)(uid >> 16), (uint8_t)(uid >> 24),
                            ATTR_APP_ID, ATTR_TITLE, 64, 0};
  if (withMessage) v.insert(v.end(), {ATTR_MESSAGE, 120, 0});
  { std::lock_guard<std::mutex> lock(rxMtx); rx.clear(); }
  if (write(v)) { pending = PEND_NOTIF; pendingUid = uid; pendingSince = millis(); }
}

static void requestApp(const std::string& appId) {
  std::vector<uint8_t> v = {CMD_APP_ATTR};
  v.insert(v.end(), appId.begin(), appId.end());
  v.insert(v.end(), {0, 0});  // fin de l'identifiant, attribut 0 = nom affiché
  { std::lock_guard<std::mutex> lock(rxMtx); rx.clear(); }
  if (write(v)) { pending = PEND_APP; pendingSince = millis(); }
}

// Lit les attributs [id][longueur LE16][valeur] à partir de p. Renvoie false si la réponse est incomplète.
static bool parseAttrs(const uint8_t* p, size_t n, int want, std::map<uint8_t, std::string>& out) {
  size_t i = 0;
  for (int k = 0; k < want; k++) {
    if (i + 3 > n) return false;
    uint8_t id = p[i];
    size_t len = p[i + 1] | p[i + 2] << 8;
    if (i + 3 + len > n) return false;
    out[id] = std::string((const char*)p + i + 3, len);
    i += 3 + len;
  }
  return true;
}

static void showToast() {
  auto it = appNames.find(pendingApp);
  toast.app = it != appNames.end() ? it->second : pendingApp.substr(pendingApp.rfind('.') + 1);
  toast.title = pendingTitle;
  toast.message = pendingMsg;
  toast.since = millis() | 1;
}

// Traite la réponse en attente si elle est complète. true = quelque chose à montrer.
static bool handleResponse() {
  std::vector<uint8_t> buf;
  { std::lock_guard<std::mutex> lock(rxMtx); buf = rx; }
  if (pending == PEND_NOTIF) {
    if (buf.size() < 5 || buf[0] != CMD_NOTIF_ATTR || le32(&buf[1]) != pendingUid) return false;
    std::map<uint8_t, std::string> a;
    if (!parseAttrs(&buf[5], buf.size() - 5, settings.notifText ? 3 : 2, a)) return false;
    pending = PEND_NONE;
    if (callActive && pendingUid == callUid) {  // appel : le titre est le nom (ou le numéro) de l'appelant
      if (!a[ATTR_TITLE].empty()) callName = a[ATTR_TITLE];
      return true;
    }
    pendingApp = a[ATTR_APP_ID];
    pendingTitle = a[ATTR_TITLE];
    pendingMsg = a[ATTR_MESSAGE];
    if (appNames.count(pendingApp) || appNames.size() >= 24) { showToast(); return true; }
    requestApp(pendingApp);  // nom affiché de l'app, puis le toast
    if (pending == PEND_NONE) { showToast(); return true; }
    return false;
  }
  if (pending == PEND_APP) {
    size_t z = buf.size() > 1 ? std::string((const char*)buf.data() + 1, buf.size() - 1).find('\0') : std::string::npos;
    if (buf.empty() || buf[0] != CMD_APP_ATTR || z == std::string::npos) return false;
    std::map<uint8_t, std::string> a;
    if (!parseAttrs(&buf[2 + z], buf.size() - 2 - z, 1, a)) return false;
    pending = PEND_NONE;
    if (!a[0].empty()) appNames[pendingApp] = a[0];
    showToast();
    return true;
  }
  return false;
}

bool ancsService() {
  if (!cp) return false;
  bool fresh = false;
  if (pending != PEND_NONE) {
    fresh = handleResponse();
    if (pending != PEND_NONE && millis() - pendingSince > 3000) pending = PEND_NONE;  // réponse perdue
  }
  Event e;
  while (pending == PEND_NONE && events && xQueueReceive(events, &e, 0)) {
    if (e.id == EV_REMOVED) {
      if (count > 0) count--;
      if (callActive && e.uid == callUid) { callActive = false; fresh = true; }  // décroché, refusé ou raccroché
    } else if (e.id == EV_ADDED) {
      count++;
      if (e.flags & FLAG_PREEXISTING) continue;  // déjà là à la connexion : compté, pas affiché
      if (e.cat == CAT_INCOMING_CALL) {
        callActive = true;
        callUid = e.uid;
        callName = "Incoming call";
        fresh = true;
        requestNotif(e.uid, false);
      } else if (!(e.flags & FLAG_SILENT)) {
        requestNotif(e.uid, settings.notifText);
      }
    }
  }
  return fresh;
}

bool ancsCall(std::string& caller) {
  if (callActive) caller = callName;
  return callActive;
}

void ancsAnswer(bool accept) {
  uint32_t u = callUid;
  write({CMD_ACTION, (uint8_t)u, (uint8_t)(u >> 8), (uint8_t)(u >> 16), (uint8_t)(u >> 24),
         accept ? ACTION_POSITIVE : ACTION_NEGATIVE});
  callActive = false;  // l'iPhone confirme ensuite en retirant la notification
}

int ancsCount() { return count; }
const NotifToast& ancsToast() { return toast; }
void ancsToastDismiss() { toast.since = 0; }

void ancsSelfTest() {
  const uint8_t r[] = {ATTR_APP_ID, 3, 0, 'a', 'b', 'c', ATTR_TITLE, 2, 0, 'h', 'i'};
  std::map<uint8_t, std::string> a;
  assert(parseAttrs(r, sizeof(r), 2, a) && a[ATTR_APP_ID] == "abc" && a[ATTR_TITLE] == "hi");
  assert(!parseAttrs(r, sizeof(r) - 1, 2, a));  // réponse coupée : on attend la suite
}
