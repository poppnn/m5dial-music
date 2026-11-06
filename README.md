# M5Dial Music

Télécommande multimédia Bluetooth (BLE) sur M5Stack M5Dial (ESP32-S3), pour **iPhone, Android et Windows**.

- **iPhone** : service natif **Apple Media Service (AMS)** → commandes + titre, artiste, progression.
- **Android / Windows** : télécommande **BLE HID** (touches multimédia standard) → commandes seules.

- Molette : volume − / +
- Appui molette ou tap écran : lecture / pause
- Appui long (2 s) sur la molette : extinction sur batterie (sur USB : veille, débrancher/rebrancher pour relancer)
- Swipe droite / gauche : piste suivante / précédente
- Écran (iPhone) : titre, artiste, temps écoulé / durée, arc de progression circulaire

**Installer depuis le navigateur (Chrome/Edge) :** https://poppnn.github.io/m5dial-music/

## LED Govee H7090

Swipe vertical pour passer à la page LED. Le M5Dial s'y connecte en BLE (protocole Govee non officiel) :
molette = luminosité, appui = on/off, swipe gauche/droite = couleur (9 couleurs).
L'app Govee Home doit être fermée : la barre n'accepte qu'une connexion.

## Interface

- Animations avec easing : l'arc de progression et l'anneau de volume se remplissent en douceur, et le titre glisse lors d'un swipe ou d'un changement de piste.
- Jusqu'à ~60 images/s pendant une animation, 4/s au repos pour économiser la batterie.
- **Toujours droit** : avec une **Unit IMU M5Stack branchée sur le Port A**, l'interface pivote pour rester horizontale. Le M5Dial n'a pas d'IMU intégrée. Fixe l'unité à plat au dos du boîtier. L'orientation au démarrage sert de référence « droit ». Si l'écran tourne dans le mauvais sens, mets `IMU_DIR` à `-1` dans `main.cpp`.

## Compiler

```bash
pio run -t upload
```

## Notes

- L'ESP32-S3 n'a pas de Bluetooth Classic : A2DP/AVRCP sont impossibles, d'où AMS (BLE).
- AMS ne transmet pas la pochette d'album : affichage titre + artiste à la place.
- Android et Windows n'exposent pas les infos du morceau en BLE : il faudrait une app compagnon.
- `ENC_STEPS_PER_DETENT` (main.cpp) est à ajuster si un cran de molette ne donne pas un pas de volume.
