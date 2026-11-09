"""M5Dial Music : programme PC pour le firmware "Windows" du M5Dial.

Envoie au M5Dial le morceau en cours (toute appli visible dans le panneau multimédia de Windows : Spotify,
Apple Music, navigateur...), les notifications Windows et l'heure. Les commandes (lecture, piste, volume)
passent déjà en touches multimédia Bluetooth, sans ce programme.

Prérequis : M5Dial avec le firmware Windows, appairé dans Paramètres > Bluetooth et appareils ; Python 3.10+.
  pip install -r requirements.txt
  pythonw m5dial_pc.py        (sans fenêtre ; "python" pour voir le journal)
Démarrage avec Windows : raccourci vers m5dial_pc.py dans le dossier shell:startup.
Notifications : Paramètres > Confidentialité > Notifications > autoriser l'accès aux notifications.

Protocole : les mêmes lignes que Gadgetbridge (Android) envoie à une montre Bangle.js, écrites sur le
Nordic UART Service du M5Dial (voir src/gb.cpp).
"""
import asyncio
import datetime
import json
import time
import uuid

from winrt.windows.devices.bluetooth import BluetoothCacheMode, BluetoothConnectionStatus, BluetoothLEDevice
from winrt.windows.devices.bluetooth.genericattributeprofile import GattCommunicationStatus, GattSession, GattWriteOption
from winrt.windows.devices.enumeration import DeviceInformation
from winrt.windows.media.control import GlobalSystemMediaTransportControlsSessionManager as MediaManager
from winrt.windows.media.control import GlobalSystemMediaTransportControlsSessionPlaybackStatus as PlaybackStatus
from winrt.windows.storage.streams import DataWriter
from winrt.windows.ui.notifications import KnownNotificationBindings, NotificationKinds
from winrt.windows.ui.notifications.management import UserNotificationListener, UserNotificationListenerAccessStatus

NAME = "M5Dial Music"
NUS_SVC = uuid.UUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
NUS_RX = uuid.UUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
POLL_S = 1.0     # morceau en cours relu chaque seconde
RETRY_S = 5.0    # M5Dial éteint ou hors de portée : nouvel essai


def log(msg):
    print(time.strftime("%H:%M:%S"), msg, flush=True)


def gb_line(obj):
    """Ligne Gadgetbridge en ASCII pur : accents en \\uXXXX, décodés par le M5Dial."""
    return "GB(" + json.dumps(obj, ensure_ascii=True, separators=(",", ":")) + ")\n"


def time_line(now=None):
    """Heure UTC + fuseau (heure d'été comprise) : le M5Dial affiche l'heure locale."""
    now = time.time() if now is None else now
    off = datetime.datetime.fromtimestamp(now).astimezone().utcoffset().total_seconds() / 3600
    return f"\x10setTime({int(now)});E.setTimeZone({off:g})\n"


def chunks(data, size):
    return [data[i:i + size] for i in range(0, len(data), size)]


assert gb_line({"t": "x", "a": "é"}) == 'GB({"t":"x","a":"\\u00e9"})\n'
assert chunks(b"abcde", 2) == [b"ab", b"cd", b"e"]
assert time_line(1700000000).startswith("\x10setTime(1700000000);E.setTimeZone(")


class Dial:
    def __init__(self, dev, rx, session):
        self.dev, self.rx, self.session = dev, rx, session

    def connected(self):
        return self.dev.connection_status == BluetoothConnectionStatus.CONNECTED

    async def send(self, line):
        for c in chunks(line.encode("ascii"), max(20, self.session.max_pdu_size - 3)):
            w = DataWriter()
            w.write_bytes(c)
            st = await self.rx.write_value_with_option_async(w.detach_buffer(), GattWriteOption.WRITE_WITH_RESPONSE)
            if st != GattCommunicationStatus.SUCCESS:
                raise ConnectionError(f"écriture refusée ({st})")


async def connect():
    """M5Dial appairé (pas de recherche : relié en HID, il n'émet plus d'annonces)."""
    sel = BluetoothLEDevice.get_device_selector_from_pairing_state(True)
    for info in await DeviceInformation.find_all_async_aqs_filter(sel):
        if info.name != NAME:
            continue
        dev = await BluetoothLEDevice.from_id_async(info.id)
        # Sans cache : Windows garde la liste des services d'avant un changement de firmware
        res = await dev.get_gatt_services_for_uuid_with_cache_mode_async(NUS_SVC, BluetoothCacheMode.UNCACHED)
        if res.status != GattCommunicationStatus.SUCCESS or not len(res.services):
            log("M5Dial injoignable, ou firmware autre que Windows")
            continue
        cr = await res.services[0].get_characteristics_for_uuid_with_cache_mode_async(NUS_RX, BluetoothCacheMode.UNCACHED)
        if cr.status != GattCommunicationStatus.SUCCESS or not len(cr.characteristics):
            continue
        session = await GattSession.from_device_id_async(dev.bluetooth_device_id)
        session.maintain_connection = True
        return Dial(dev, cr.characteristics[0], session)
    return None


async def media_now(mgr):
    """(titre/artiste/durée, en lecture, position en s) du lecteur que Windows affiche ; titre vide sans lecteur."""
    s = mgr.get_current_session()
    if s is None:
        return {"track": "", "artist": "", "dur": 0}, False, 0
    p = await s.try_get_media_properties_async()
    tl = s.get_timeline_properties()
    playing = s.get_playback_info().playback_status == PlaybackStatus.PLAYING
    dur = max(0, int(tl.end_time.total_seconds()))
    pos = tl.position.total_seconds()
    if playing:  # position datée de la dernière mise à jour du lecteur
        try:
            pos += (datetime.datetime.now(datetime.timezone.utc) - tl.last_updated_time).total_seconds()
        except (TypeError, ValueError):
            pass
    pos = max(0, min(pos, dur) if dur else pos)
    return {"track": p.title or "", "artist": p.artist or "", "dur": dur}, playing, pos


async def notif_listener():
    lis = UserNotificationListener.current
    st = lis.get_access_status()
    if st != UserNotificationListenerAccessStatus.ALLOWED:
        st = await lis.request_access_async()
    if st != UserNotificationListenerAccessStatus.ALLOWED:
        log("Notifications non autorisées (Paramètres > Confidentialité > Notifications) : musique et heure seulement")
        return None
    return lis


async def read_notifs(lis):
    out = {}
    for n in await lis.get_notifications_async(NotificationKinds.TOAST):
        try:
            app = n.app_info.display_info.display_name
        except Exception:  # appli sans identité (rare) : pas de nom
            app = ""
        b = n.notification.visual.get_binding(KnownNotificationBindings.toast_generic)
        texts = [t.text for t in b.get_text_elements()] if b else []
        out[n.id] = (app, texts)
    return out


async def serve(dial, mgr, lis):
    await dial.send(time_line())
    known = await read_notifs(lis) if lis else {}  # déjà là à la connexion : comptées par Windows, pas affichées
    info, state, tick = None, None, 0
    while dial.connected():
        new_info, playing, pos = await media_now(mgr)
        if new_info != info:
            await dial.send(gb_line({"t": "musicinfo", **new_info}))
            info, state = new_info, None
        # Position renvoyée seulement si elle s'écarte de ce que le M5Dial calcule lui-même (saut, pause, lecture)
        expected = state and (state[1] + (time.monotonic() - state[2]) if state[0] else state[1])
        if state is None or playing != state[0] or abs(pos - expected) > 3:
            await dial.send(gb_line({"t": "musicstate", "state": "play" if playing else "pause", "position": int(pos)}))
            state = (playing, pos, time.monotonic())
        if lis and tick % 2 == 0:
            now = await read_notifs(lis)
            for nid in now.keys() - known.keys():
                app, texts = now[nid]
                await dial.send(gb_line({"t": "notify", "id": nid, "src": app,
                                         "title": texts[0] if texts else "", "body": " ".join(texts[1:])}))
            for nid in known.keys() - now.keys():
                await dial.send(gb_line({"t": "notify-", "id": nid}))
            known = now
        if tick % 3600 == 0 and tick:
            await dial.send(time_line())  # heure d'été, dérive du RTC
        tick += 1
        await asyncio.sleep(POLL_S)


async def main():
    mgr = await MediaManager.request_async()
    lis = await notif_listener()
    log(f"Recherche de « {NAME} » parmi les appareils Bluetooth appairés...")
    while True:
        try:
            dial = await connect()
            if dial:
                log("M5Dial connecté")
                await serve(dial, mgr, lis)
                log("M5Dial déconnecté")
        except OSError as e:  # lien coupé pendant un échange (WinRT lève OSError)
            log(f"Lien perdu : {e}")
        await asyncio.sleep(RETRY_S)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
