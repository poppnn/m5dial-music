# M5Dial Music

Télécommande multimédia Bluetooth (BLE) sur M5Stack M5Dial (ESP32-S3), pour **iPhone, Android et Windows**.

- **iPhone** : service natif **Apple Media Service (AMS)** → commandes + titre, artiste, progression.
- **Android / Windows** : télécommande **BLE HID** (touches multimédia standard) → commandes seules.

- Molette : volume − / +
- Appui molette ou tap écran : lecture / pause
- Swipe droite / gauche : piste suivante / précédente
- Écran (iPhone) : titre, artiste, temps écoulé / durée, arc de progression circulaire

**Installer depuis le navigateur (Chrome/Edge) :** https://poppnn.github.io/m5dial-music/

## Compiler

```bash
pio run -t upload
```

## Notes

- L'ESP32-S3 n'a pas de Bluetooth Classic : A2DP/AVRCP sont impossibles, d'où AMS (BLE).
- AMS ne transmet pas la pochette d'album : affichage titre + artiste à la place.
- Android et Windows n'exposent pas les infos du morceau en BLE : il faudrait une app compagnon.
- `ENC_STEPS_PER_DETENT` (main.cpp) est à ajuster si un cran de molette ne donne pas un pas de volume.
