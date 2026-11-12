# M5Dial Music

Bluetooth (BLE) media remote on the M5Stack M5Dial (ESP32-S3), for **iPhone, Android and Windows**.

- **iPhone**: native **Apple Media Service (AMS)** → controls + title, artist, progress, shuffle / repeat,
  plus time (CTS), calls and notifications (ANCS).
- **Android**: **BLE HID** remote (standard media keys) for the controls; title, artist, progress, time, calls and
  notifications through [Gadgetbridge](https://gadgetbridge.org/) (free app), which takes the M5Dial for a Bangle.js watch.
- **Windows**: **BLE HID** remote for the controls; title, artist, progress, time and notifications through a small
  Python app on the PC ([`tools/windows`](tools/windows/m5dial_pc.py)).

**One firmware per platform**, picked in the [web flasher](https://poppnn.github.io/m5dial-music/) (iPhone by
default): each one only runs what its platform needs.

| Firmware | Bluetooth name | Track info, notifications | Build |
|---|---|---|---|
| iPhone | M5Dial Music | AMS, ANCS, CTS (built into iOS) | `pio run -e m5dial` |
| Android | Bangle.js M5Dial | Gadgetbridge (Bangle.js protocol over Nordic UART) | `pio run -e android` |
| Windows | M5Dial Music | `tools/windows/m5dial_pc.py` (same protocol) | `pio run -e windows` |

Android: pair **Bangle.js M5Dial** in the Bluetooth settings (controls), then in Gadgetbridge tap + and add it, and
allow notification access. The name has to start with "Bangle.js": Gadgetbridge only recognises its watches by name.

Windows: pair **M5Dial Music** in Settings › Bluetooth & devices (controls), then on the PC:

```bash
pip install -r tools/windows/requirements.txt
pythonw tools/windows/m5dial_pc.py
```

It reads what Windows shows in its media panel (Spotify, Apple Music, browser...) and the Windows notifications
(Settings › Privacy › Notifications must allow access). To start it with Windows: shortcut in `shell:startup`.

7 pages, vertical swipe (up = next): **Music**, **LED**, **Stopwatch**, **Clock**, **Sensors**, **System**, **Settings**.

- Wheel: volume − / + (LED: brightness or hue)
- Wheel press or screen tap: play / pause (LED: on / off, Stopwatch: start / pause)
- Short hold (0.6 s) on the wheel: LED page, switch the wheel between brightness and hue
- Hold (2 s) on the wheel: power off on battery (on USB: deep sleep, unplug / replug to restart)
- Battery saving: screen dims after 20 s without input (any input wakes it), automatic power off after 10 min
  without a connected Bluetooth device; the WAKE button turns it back on. Both delays are adjustable.
- Swipe left / right: next / previous track (LED: colour, Stopwatch: reset)
- Screen (iPhone): title, artist, elapsed / total time, circular progress arc, time of day
- iPhone: **shuffle** and **repeat** icons at the bottom (accent colour = on, "1" = repeat one): tap them to change.
  A tap elsewhere = play / pause.

## Calls and notifications (iPhone)

Through Apple's ANCS service (no app to install, pairing is enough):

- **Incoming call**: full screen on any page, iPhone style: caller name in large type, a red button (decline)
  and a green one (answer) to tap. Or with the wheel: **press** = answer, **hold** (0.8 s) = decline. The screen
  then goes back to the previous page.
- **Notification**: for safety while driving, the **app name**, the **sender** (contact name in messaging apps)
  and the notification count (5 s). Settings › Notifs: **off** (none shown), **sender** (default), **full** (plus
  the message text, 8 s). Press, tap or swipe = close (it slides away). Emojis are removed (the screen font has none; they would show as squares).
- Simple gestures only (wheel, press): no precise swipes on these screens.
- If the iPhone does not offer notifications for "M5Dial Music": iOS Settings › Bluetooth › (i) ›
  "Share System Notifications".

**Install from the browser (Chrome / Edge):** https://poppnn.github.io/m5dial-music/
Or download a version's binaries from [Releases](https://github.com/poppnn/m5dial-music/releases).

## Photos

<!-- Drop the photos into docs/img/ with these names, then remove the comment markers around the lines. -->
<!-- ![M5Dial mounted on the steering wheel](docs/img/volant.jpg) -->
<!-- ![Music page (iPhone)](docs/img/page-musique.jpg) -->
<!-- ![Govee LED page](docs/img/page-led.jpg) -->
<!-- ![Stopwatch page](docs/img/page-chrono.jpg) -->
<!-- ![System page](docs/img/page-systeme.jpg) -->
<!-- ![Printed mount, back with the battery](docs/img/support-dos.jpg) -->
*Photos coming: `docs/img/volant.jpg`, `page-musique.jpg`, `page-led.jpg`, `page-chrono.jpg`, `page-systeme.jpg`,
`support-dos.jpg`.*

## Govee H7090 LEDs

Swipe vertically to the LED page. The M5Dial connects to the bar over BLE (unofficial Govee protocol):
wheel = brightness, press = on / off, swipe left / right = colour (9 presets).
**Short hold** (0.6 s) on the wheel = **hue** mode: the ring becomes a colour wheel and the wheel scrolls the hue
(5° per detent, one wheel turn is about one colour circle); another short hold goes back to brightness. The bottom
line shows the bar connection: **Connected** in green, **Disconnected** in red (connecting runs in the background,
the page never freezes). The last colour (preset or hue) and brightness are remembered and sent to the bar on every
connection.
Close the Govee Home app: the bar only accepts one connection.

## Stopwatch, Clock, System and Settings

- **Stopwatch**: 7-segment display, hundredths, second-hand ring (one turn per minute). Press = start / pause,
  swipe left or right = reset. It keeps running when you change page.
- **Clock**: phone time and date, seconds ring, and the cabin **temperature** (ENV III, else the IMU Pro Mini
  barometer, else the IMU chip; not shown without a sensor). The M5Dial RTC keeps the time while
  disconnected and off; it is set again on every connection (and when daylight saving time changes).
- **Sensors**: temperature, humidity, pressure (hPa) and barometric altitude, plus the units found and their port
  (see [Sensors](#sensors)).
- **System**: last boot reason (normal, crash, watchdog, brownout), connected phone and its signal, Govee LEDs,
  CPU frequency and temperature, frames per second and render time, free RAM, uptime. Battery level cannot be
  measured: the M5Dial has no battery voltage or charge signal wired to the ESP32 (it would need a voltage divider
  soldered to an ADC GPIO).
- **Settings** (stored in the M5Dial memory, kept across power off and updates): turn = choose a row,
  press = edit (turn = value, applied immediately), press = done.
  - IMU direction (normal / reversed), encoder pulses per detent (1 to 4), dim delay (never to 120 s),
    power-off delay without a device (never to 60 min), day and night brightness, notifications (off / sender /
    full), **accent colour** of the interface (orange, red, pink, purple, blue, cyan, green, yellow, white: arcs,
    active icons, notification border).
  - Brightness: day, night or **auto** (night between "Night from" and "Night until", 21:00 → 7:00 by default,
    from the iPhone time; unknown time = day).
  - Paired devices (name read on connection, connected one in green): "Forget" a device, or "Erase all".
    First press = "confirm?", second press = do it; turning the wheel cancels. The forgotten device must also
    forget "M5Dial Music" in its Bluetooth settings before pairing again.

## Interface

- Eased animations: the progress arc and volume ring fill smoothly, and the title slides on a swipe or a track
  change.
- Up to ~60 fps while animating, 4 fps at rest to save battery.
- DMA output to the screen in ping-pong bands (2 × 19 KB) in internal SRAM: the encoder and BLE stay responsive
  during the transfer.
- Every text is fitted to the round screen: long titles, names and settings rows are truncated with "..." to the
  width available at their height.
- **Always upright**: with an **M5Stack IMU Pro Mini (or IMU Unit) on Port A**, the UI rotates to stay level. The M5Dial has no
  built-in IMU. Mount the unit flat on the back of the case. The orientation at power on is the "upright"
  reference. If the screen turns the wrong way: Settings › IMU dir › reversed.

## Boot screen

While no device is connected, the M5Dial shows a logo (by default a music note drawn by the firmware), a spinning
arc and "Waiting for phone". Controls are locked, except the hold to power off. On connection, the spinning arc
closes into a circle while shrinking to the centre, then bursts into a check mark badge with a white wave towards the
edge (0.8 s), and a fade leads to the UI. The Bluetooth setup (GATT discovery) runs in the background meanwhile, so
the screen never freezes.

**Boot image or GIF from the browser** (no build needed): on https://poppnn.github.io/m5dial-music/, section 6, pick a
picture or an animated GIF (square crop, 240 × 240, up to 300 frames), then "Send to the M5Dial" with the M5Dial on
USB-C (Chrome or Edge: Web Serial). The page turns it into JPEG frames, sends them over USB and the M5Dial stores them
in its own flash partition (`bootanim`, 4.9 MB, `partitions.csv`); it is then shown full screen while waiting for the
phone, under the spinning arc. "Remove the boot image" goes back to the logo. This needs firmware 1.1 or later
installed with the web flasher's full install (it adds the partition; settings are kept).

To build your own logo into the firmware instead (PNG or BMP, ideally on a transparent or black background):

```bash
pip install pillow
python tools/png2logo.py my_logo.png
```

The script scales the image to 150 × 130 max, converts it to RGB565 and writes `src/logo.h` (stored in flash, not
RAM). Then rebuild (`pio run -t upload`). To go back to the neutral logo: `git checkout src/logo.h`.

Example, your car brand logo (here Nissan): brand logos are trademarks, so they are not included in this repo.
Download the logo (e.g. `Nissan_2020_logo.svg` on Wikimedia Commons), export it as a **white on transparent**
PNG (Inkscape or any SVG → PNG converter, ~600 px wide), then `python tools/png2logo.py nissan_white.png`. Keep
that `logo.h` to yourself (do not publish it).

## Steering wheel mount (3D printed)

Made for the **OMP Corsica** steering wheel, reference **OD0-1954** (suede, black / blue spokes), **Ø 350 mm,
95 mm dish**, 3 spokes, 6 × 70 mm bolt circle: [ompracing.com](https://www.ompracing.com/p/corsica-steering-wheel-suede/).
The M5Dial sits between the **bottom spoke and the right spoke**, held by one M5 screw in each spoke's large hole
(Ø 16, the one nearest the hub). Other wheels: edit the dimensions in the Fusion 360 script (below).

The print files will also be on **MakerWorld** (link coming).

### Parts to print

| File | Qty | On the bed | Supports |
|---|---|---|---|
| [`support_volant.stl`](hardware/support_volant.stl) | 1 | flat underside (as exported) | only from the bed under the M5Dial ring |
| [`coque_batterie.stl`](hardware/coque_batterie.stl) | 1 | floor down (as exported) | none (the top bridges 38 mm) |
| [`support_volant_sans_capteurs.stl`](hardware/support_volant_sans_capteurs.stl) | instead | same as the mount, **without** the ENV III slot (bigger rim-side windows) | same |
| [`coque_batterie_sans_capteurs.stl`](hardware/coque_batterie_sans_capteurs.stl) | instead | same as the shell, **without** the IMU Pro Mini holder | none |
| [`couvercle_batterie.stl`](hardware/couvercle_batterie.stl) | 1 | **lay it on its flat plate** | none |
| [`cache_ecrou.stl`](hardware/cache_ecrou.stl) | 2 | flat base | none |
| [`cache_factice.stl`](hardware/cache_factice.stl) | 1 | peg on the bed | under the dome rim |
| [`support_volant_test.stl`](hardware/support_volant_test.stl) | optional | flat underside | none |

No sensor units? Print the two `_sans_capteurs` files instead (they fit the same lid, caps and screws).
PETG or ASA (PLA softens in a car in the sun), 4 perimeters, 30–40 % infill for the mount, 3 perimeters for the
other parts. The test part (two pads joined by a bar, ~20 cm³) checks the fit on the wheel before the full print.

### Hardware

- M5Stack **M5Dial** (v1.1). Optional: **IMU Pro Mini** unit on Port A (screen stays level, pressure) and **ENV III**
  unit on Port B (temperature, humidity, pressure), with short Grove cables.
- LiPo **103450** (50 × 34 × 10 mm) with its protection board and a lead long enough to reach the M5Dial.
- Per spoke (bottom and right): **M5 × 14 hex screw**, **fender washer M5 × 20** (a plain M5 washer drops into the Ø 16 hole), **M5 nut**.

### Assembly

1. Install the firmware for your phone or PC from the [web flasher](https://poppnn.github.io/m5dial-music/).
2. Put the M5Dial into the flat ring from the top, then screw its **clamp ring on from underneath** through the tube
   (the 3 large windows leave room for the fingers).
3. Plug the battery into the M5Dial (check the polarity), slide it into the shell through the open end, its lead
   going up through the slot in the top, then **clip the lid** on (to open it: press the 2 bumps through the side holes).
4. Push the shell's round collar into the tube from underneath (3 lugs into 3 notches) and **turn it 35° clockwise
   as seen from below** until it clicks.
5. Fit the mount behind the bottom and right spokes, one screw per spoke through the Ø 16 hole with the Ø 20
   washer, nut and screw head on either side (the hex pocket at the back of the pad holds a nut or a screw head),
   then press a **nut cap** onto each nut on the driver side, and clip the **dummy cap** into the left spoke's
   Ø 16 hole (nothing is fixed there; the three spokes look alike).
6. Optional sensors: slide the **IMU Pro Mini** flat into its holder on top of the battery shell before step 4
   (inside the tube, Grove socket towards the open side) and plug it into Port A; slide the **ENV III** into its slot on the outside
   of the tube (rim side, from the top) and plug it into Port B. They are found at power on.
7. Before driving: turn the wheel lock to lock and check that nothing touches the stalks.

### Design notes

- **Pads** as wide as the spoke at their rim end minus 1 mm (they do not stick out past the spoke edges), sloped like
  the spoke (40°); on the M5Dial side each pad carries on with the same slope into the tube wall, so pads and tube
  are one body. The low end of each slope is the first layer (`PAD_MIN` 0.2 mm): the part is 30.7 mm tall.
- Pads set by fit checks on the wheel: the right pad at 90° to the bottom one, **96 mm between the two hole
  centres**, both slopes 40°. Widths (bottom 40 mm, right 38 mm at the hole), 5 mm thickness and Ø 16 holes
  measured with a caliper.
- **M5Dial** centre 58.5 mm right of and 57.5 mm below the hub centre, 27.2 mm above the part's underside
  (`DIAL_RISE`): its body goes through a flat 3.5 mm ring (Ø 45.6, 6 crush ribs), its clamp ring (Ø 49.8 × 12.6)
  goes on from underneath through the Ø 52.8 bore. The ring sits on a 3.5 mm tube with 3 arched windows (up to
  the ring) between 3 narrow pillars, kept clear of where the pads meet the tube; USB-C and the battery lead go
  through them.
- **Battery shell**: cavity 54 × 38.5 × 11 mm, long side pointing away from the hub. Quarter-turn bayonet with
  3 lugs (30° wide, 2.4 mm tall) in a groove of the tube foot, a bump under each lug clicks into a dimple. Snap-on
  lid with 2 flexible tabs; vent slots in the floor, the sides and the top (into the tube). Remove the shell to
  reach the M5Dial clamp ring. Parameters `BAT_L`, `BAT_W`, `BAT_T`, `SHELL_WALL`, `BAT_ROT`, `BAT_TWIST`.
- **Nut caps**: low dome Ø 24 × 7.5 mm pushed onto the nut (hex socket 8.1 mm across flats), with or without the
  washer underneath; an M5 × 14 screw ends inside it. **Dummy cap**: same dome with a peg split into 4 fingers that
  clips into the Ø 16 hole (hook 0.3 mm under the 5 mm spoke). Source
  [`hardware/fusion/CacheEcrou`](hardware/fusion/CacheEcrou): `NUT_AF` 8.0 if too loose, 8.2 if too tight.
- **Sensor holders** (`ENV_HOLDER`, `IMU_HOLDER` = 1): the IMU Pro Mini (24 × 24 × 8) lies flat in a holder on top
  of the battery shell, inside the tube (parallel to the screen, as "always upright" needs), held by two 2 mm lips
  and tied to the shell collar by 5 ribs; the
  ENV III (32 × 24 × 8) slides from the top into a slot on the outside of the tube, rim side, in open air away from
  the battery (two ribs hold it, 6 mm front lips, ends open above 8 mm for the Grove plug). Holder walls,
  floor and lips are 2.4 mm (`HOLDER_WALL`).
- **Source model**: Fusion 360 script [`hardware/fusion/SupportVolant`](hardware/fusion/SupportVolant). Utilities ›
  Add-Ins › Scripts › "+" › pick the folder › Run. Dimensions are user parameters (Modify › Change Parameters);
  run the script again, then export the STLs. `TEST_FIT` = 1 builds the test part, 0 the full mount.

## Build

```bash
pio run -e m5dial -t upload    # iPhone ; -e android or -e windows for the other platforms
```

## Roadmap

To do next (owner's list):

- [ ] Print the new mount, battery shell and lid; check the bayonet and lid clicks, the 38 mm bridge of the shell
      top and the thin low end of the pads (0.2 mm).
- [ ] Mount it on the wheel, turn lock to lock, check the stalks and that the shell clears the bottom spoke.
- [ ] Test the **Android** firmware with Gadgetbridge (does it accept "Bangle.js M5Dial", track info, notifications,
      calls).
- [ ] Test the **Windows** firmware with the PC app (`tools/windows`).
- [ ] Boot GIF: send the exact error message shown under the buttons (section 6 of the web flasher).
- [ ] Publish the print files on **MakerWorld** and add the link here and on the web flasher.
- [ ] Add photos to `docs/img/` (wheel, pages, mount).
- [ ] Release **v1.2.0** (three firmwares) once Android and Windows are checked.
- [ ] Print the flatter nut caps and the dummy cap; plug the IMU Pro Mini (Port A) and the ENV III (Port B) and
      check the Sensors page (values, ports found).
- [ ] Later: DLight ambient light sensor, battery level (needs a voltage divider to an ADC pin).

## License

[MIT](LICENSE).

## Sensors

Ports: **A** = I2C (G13 SDA / G15 SCL), **B** = G1 / G2. Supported M5Stack units, each one optional and found at
power on, on Port A or Port B:

| Unit | Chips | Used for |
|---|---|---|
| **IMU Pro Mini** (Port A) | BMI270 + BMM150, BMP280 (0x76) | always upright (accelerometer), pressure, temperature |
| **ENV III** (Port B) | SHT30 (0x44), QMP6988 (0x70) | temperature, humidity, pressure |
| IMU Unit (Port A) | MPU6886 | always upright, chip temperature |

The ESP32-S3 has only two I2C controllers (the internal bus and Port A), so Port B is read by moving the Port A
controller onto its pins for each reading (once a second), then back. Temperature comes from the SHT30, else the
BMP280, else the QMP6988; pressure from the QMP6988, else the BMP280. Altitude is barometric (standard atmosphere,
1013.25 hPa at sea level), so it drifts with the weather. The BMM150 magnetometer is not used: on a steering wheel,
in a steel car body, a compass would not be reliable. Code: `src/sensors.cpp`, page `src/pages/sensors_page.cpp`.

Also worth a look later: **DLight** (BH1750, ambient light → automatic screen brightness).

## Notes

- The ESP32-S3 has no Bluetooth Classic: A2DP / AVRCP are impossible, hence AMS (BLE).
- AMS does not send album art: title + artist are shown instead.
- Android and Windows have no built-in BLE service for track info: Gadgetbridge (Android) and the PC app (Windows)
  fill that gap with the Bangle.js text protocol (`src/gb.cpp`).
- Encoder pulses per detent: 2 on this M5Dial; adjust in Settings › Wheel if one detent does not give one step.
- Code: `src/main.cpp` (loop, gestures, pages), `ble_phone` (AMS + HID + CTS), `ancs` (calls, notifications), `gb` (Gadgetbridge / PC app lines), `sensors` (ENV III, BMP280),
  `govee`, `display` (DMA rendering), `encoder` (wheel on the PCNT hardware counter), `bootanim` (boot image over USB), `imu`, `clock`, `power`, `settings` (NVS), `pages/` (one file per page).
  Settings defaults: `src/config.h`. Code comments are in French.
