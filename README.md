# M5Dial Music

Télécommande multimédia Bluetooth (BLE) sur M5Stack M5Dial (ESP32-S3), pour **iPhone, Android et Windows**.

- **iPhone** : service natif **Apple Media Service (AMS)** → commandes + titre, artiste, progression.
- **Android / Windows** : télécommande **BLE HID** (touches multimédia standard) → commandes seules.

6 pages, swipe vertical (haut = suivante) : **Musique**, **LED**, **Chrono**, **Horloge**, **Système**, **Réglages**.

- Molette : volume − / + (LED : luminosité)
- Appui molette ou tap écran : lecture / pause (LED : on/off, Chrono : départ/pause)
- Appui long (2 s) sur la molette : extinction sur batterie (sur USB : veille, débrancher/rebrancher pour relancer)
- Économie de batterie : écran atténué après 20 s sans geste (le moindre geste le rallume), extinction automatique
  après 10 min sans appareil Bluetooth connecté ; le bouton WAKE le rallume. Les deux délais se règlent.
- Swipe gauche / droite : piste suivante / précédente (LED : couleur, Chrono : remise à zéro)
- Écran (iPhone) : titre, artiste, temps écoulé / durée, arc de progression circulaire, heure
- iPhone : icônes **aléatoire** et **répétition** en bas (orange = actif, « 1 » = répéter le morceau) : un tap
  dessus les change. **Double tap** ailleurs = « J'aime » (un cœur s'affiche). Le tap simple (lecture / pause)
  part après 0,3 s, le temps de voir si un second tap arrive ; l'appui sur la molette reste immédiat.

**Installer depuis le navigateur (Chrome/Edge) :** https://poppnn.github.io/m5dial-music/
Ou télécharger les binaires d'une version dans [Releases](https://github.com/poppnn/m5dial-music/releases).

## Photos

<!-- Dépose les photos dans docs/img/ avec ces noms, puis retire les commentaires autour des lignes. -->
<!-- ![M5Dial monté sur le volant](docs/img/volant.jpg) -->
<!-- ![Page Musique (iPhone)](docs/img/page-musique.jpg) -->
<!-- ![Page LED Govee](docs/img/page-led.jpg) -->
<!-- ![Page Chrono](docs/img/page-chrono.jpg) -->
<!-- ![Page Système](docs/img/page-systeme.jpg) -->
<!-- ![Support imprimé, dos avec la batterie](docs/img/support-dos.jpg) -->
*Photos à venir : `docs/img/volant.jpg`, `page-musique.jpg`, `page-led.jpg`, `page-chrono.jpg`, `page-systeme.jpg`,
`support-dos.jpg`.*

## LED Govee H7090

Swipe vertical pour passer à la page LED. Le M5Dial s'y connecte en BLE (protocole Govee non officiel) :
molette = luminosité, appui = on/off, swipe gauche/droite = couleur (9 couleurs).
L'app Govee Home doit être fermée : la barre n'accepte qu'une connexion.

## Chrono, Horloge, Système et Réglages

- **Chrono** : afficheur 7 segments, centièmes, anneau trotteuse (1 tour par minute). Appui = départ / pause,
  swipe gauche ou droite = remise à zéro. Il continue de tourner quand on change de page.
- **Horloge** : heure et date de l'iPhone (Current Time Service), anneau des secondes. Le RTC du M5Dial garde
  l'heure déconnecté et éteint ; elle est remise à l'heure à chaque connexion (et au passage à l'heure d'été).
- **Système** : cause du dernier démarrage (normal, plantage, blocage, tension basse), appareil Bluetooth
  connecté et son signal, LED Govee, fréquence et température du processeur, images/s et temps de rendu,
  RAM libre, durée depuis l'allumage. Le niveau de batterie n'est pas mesurable : le M5Dial n'a aucune mesure de
  tension ni signal de charge câblé vers l'ESP32 (il faudrait souder un pont diviseur sur un GPIO ADC).
- **Réglages** (enregistrés dans la mémoire du M5Dial, gardés à l'extinction et aux mises à jour) : molette =
  choisir une ligne, appui = modifier (molette = valeur, appliquée tout de suite), appui = terminer.
  - Sens IMU (normal / inversé), impulsions par cran de molette (1 à 4), délai d'atténuation (jamais à 120 s),
    délai d'extinction sans appareil (jamais à 60 min), luminosité de jour et de nuit.
  - Luminosité : jour, nuit ou **auto** (nuit entre « Nuit dès » et « Nuit jusqu'à », 21 h -> 7 h par défaut,
    selon l'heure de l'iPhone ; heure inconnue = jour).
  - Appareils appairés (nom lu à la connexion) : « Oublier » un appareil, ou « Tout effacer ». Premier appui =
    « confirmer ? », second appui = action ; tourner la molette annule. L'appareil oublié doit aussi oublier
    « M5Dial Music » dans ses réglages Bluetooth avant de se réappairer.

## Interface

- Animations avec easing : l'arc de progression et l'anneau de volume se remplissent en douceur, et le titre glisse lors d'un swipe ou d'un changement de piste.
- Jusqu'à ~60 images/s pendant une animation, 4/s au repos pour économiser la batterie.
- Envoi à l'écran en DMA par bandes ping-pong (2 × 19 Ko), en SRAM interne : l'encodeur et le BLE restent réactifs pendant le transfert.
- **Toujours droit** : avec une **Unit IMU M5Stack branchée sur le Port A**, l'interface pivote pour rester horizontale. Le M5Dial n'a pas d'IMU intégrée. Fixe l'unité à plat au dos du boîtier. L'orientation au démarrage sert de référence « droit ». Si l'écran tourne dans le mauvais sens : Réglages › Sens IMU › inversé.

## Écran de démarrage

Tant qu'aucun appareil n'est connecté, le M5Dial affiche un logo (par défaut une double croche dessinée par le
firmware), un arc qui tourne et « Connexion Bluetooth... ».
Les commandes sont bloquées, sauf l'appui long pour éteindre. À la connexion, un fondu mène à l'interface.

Pour mettre ton logo (PNG ou BMP, idéalement sur fond transparent ou noir) :

```bash
pip install pillow
python tools/png2logo.py mon_logo.png
```

Le script réduit l'image à 150 × 130 max, la convertit en RGB565 et écrit `src/logo.h` (stocké en flash, pas en RAM).
Recompile ensuite (`pio run -t upload`). Pour revenir au logo neutre : `git checkout src/logo.h`.

Exemple, logo de ta marque de voiture (ici Nissan) : les logos de marque sont déposés, donc non fournis dans ce
repo. Télécharge le logo (par ex. `Nissan_2020_logo.svg` sur Wikimedia Commons), exporte-le en PNG **blanc sur fond
transparent** (Inkscape ou un convertisseur SVG → PNG, ~600 px de large), puis
`python tools/png2logo.py nissan_blanc.png`. Garde ce `logo.h` pour toi (ne le publie pas).

## Support volant (impression 3D)

[`hardware/support_volant.stl`](hardware/support_volant.stl) : une seule pièce **100 % imprimée** (sans vis ;
seule la batterie est collée) pour la zone entre la branche du bas et la branche droite d'un volant GT2i Race 350 mm (tulipage 90 mm).

- **Plaque fine et pleine** (6 mm) entre les deux branches : anneau du M5Dial, bras et zone entre eux, d'un seul bloc.
- **M5Dial** : son corps s'enfonce dans l'anneau (cercle complet Ø 45 / Ø 51, **3,5 mm de haut**), serré par 6
  nervures d'écrasement ; la collerette repose dessus et le corps ressort dessous, donc **l'USB-C** et le Port A
  restent libres. Autour, la plaque est abaissée au niveau de l'anneau avec 1 mm de jeu : rien ne frotte.
- **Batterie LiPo 103450** (50 × 34 × 10 mm) : collée au **dos** de la plaque (adhésif double face), en diagonale,
  le grand côté parallèle au bord intérieur (côté moyeu) et ce bord aligné dessus, entre les deux bras. Là, elle ne
  touche ni les branches (qui passent plus en arrière près du moyeu) ni le corps du M5Dial ; elle repose à 99 % sur
  la plaque.
- **Fixation** : chaque patte se clipse sur sa branche (on appuie la branche entre les deux parois à crochet, qui
  s'écartent puis se referment sur sa face avant) ; un téton entre dans le trou de la branche pour qu'elle ne glisse
  pas.

- Modèle source : script Fusion 360 [`hardware/fusion/SupportVolant`](hardware/fusion/SupportVolant). Utilitaires ›
  Compléments › Scripts › « + » › choisir ce dossier › Exécuter. Les cotes sont des paramètres utilisateur
  (Modifier › Changer les paramètres). **Avant d'imprimer, mesure les valeurs `MESURER`** : épaisseur de la branche
  (`SPOKE_T`, le clip en dépend), Ø des trous des branches (`PEG_D`), Ø du corps du M5Dial (`BODY_D`), position des trous et pente des branches. Relance le script, puis exporte le STL.
- Impression : à plat telle quelle (une seule face posée, le dos), **sans supports**.
  PETG ou ASA (le PLA se déforme au soleil et casse au clipsage), 4 périmètres, 30-40 % de remplissage.
- Teste le clip sur une impression rapide d'abord : trop dur, baisse `CLIP_LIP` ; trop lâche, baisse `SPOKE_T`
  de 0,2 mm.
- Avec l'IMU sur le Port A, l'écran reste droit quand le volant tourne.
- Vérifie que rien ne touche les commodos en tournant le volant de butée à butée avant de rouler.

## Compiler

```bash
pio run -t upload
```

## Licence

[MIT](LICENSE).

## Notes

- L'ESP32-S3 n'a pas de Bluetooth Classic : A2DP/AVRCP sont impossibles, d'où AMS (BLE).
- AMS ne transmet pas la pochette d'album : affichage titre + artiste à la place.
- Android et Windows n'exposent pas les infos du morceau en BLE : il faudrait une app compagnon.
- Impulsions par cran de molette : 2 sur ce M5Dial ; à ajuster dans Réglages › Molette si un cran ne donne pas un pas.
- Code : `src/main.cpp` (boucle, gestes, pages), `ble_phone` (AMS + HID), `govee`, `display` (rendu DMA), `imu`,
  `power`, `settings` (NVS), `pages/` (une page par fichier). Valeurs par défaut des réglages : `src/config.h`.
