# M5Dial Music

Télécommande Apple Music pour iPhone sur M5Stack M5Dial (ESP32-S3), via le service BLE natif d'iOS **Apple Media Service (AMS)**.

- Molette : volume − / +
- Appui molette ou tap écran : lecture / pause
- Swipe droite / gauche : piste suivante / précédente
- Écran : titre, artiste, temps écoulé / durée, arc de progression circulaire

**Installer depuis le navigateur (Chrome/Edge) :** https://poppnn.github.io/m5dial-music/

## Compiler

```bash
pio run -t upload
```

## Notes

- L'ESP32-S3 n'a pas de Bluetooth Classic : A2DP/AVRCP sont impossibles, d'où AMS (BLE).
- AMS ne transmet pas la pochette d'album : affichage titre + artiste à la place.
- `ENC_STEPS_PER_DETENT` (main.cpp) est à ajuster si un cran de molette ne donne pas un pas de volume.
