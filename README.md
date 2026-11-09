# M5Dial Music

Bluetooth (BLE) media remote on the M5Stack M5Dial (ESP32-S3), for **iPhone, Android and Windows**.

- **iPhone**: native **Apple Media Service (AMS)** → controls + title, artist, progress, shuffle / repeat / like,
  plus time (CTS), calls and notifications (ANCS).
- **Android / Windows**: **BLE HID** remote (standard media keys) → controls only.

6 pages, vertical swipe (up = next): **Music**, **LED**, **Stopwatch**, **Clock**, **System**, **Settings**.

- Wheel: volume − / + (LED: brightness or hue)
- Wheel press or screen tap: play / pause (LED: on / off, Stopwatch: start / pause)
- Short hold (0.6 s) on the wheel: LED page, switch the wheel between brightness and hue
- Hold (2 s) on the wheel: power off on battery (on USB: deep sleep, unplug / replug to restart)
- Battery saving: screen dims after 20 s without input (any input wakes it), automatic power off after 10 min
  without a connected Bluetooth device; the WAKE button turns it back on. Both delays are adjustable.
- Swipe left / right: next / previous track (LED: colour, Stopwatch: reset)
- Screen (iPhone): title, artist, elapsed / total time, circular progress arc, time of day
- iPhone: **shuffle** and **repeat** icons at the bottom (accent colour = on, "1" = repeat one): tap them to change.
  **Double tap** elsewhere = "Like" (a heart pops up). A single tap (play / pause) is sent after 0.3 s, to leave
  room for a double tap; the wheel press stays instant.

## Calls and notifications (iPhone)

Through Apple's ANCS service (no app to install, pairing is enough):

- **Incoming call**: full screen on any page, caller name in large type. **Press** the wheel = answer,
  **hold** (0.8 s) = decline. The screen then goes back to the previous page.
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
- **Clock**: iPhone time and date (Current Time Service), seconds ring, and the **temperature** from the IMU Unit
  sensor on Port A (close to cabin temperature; not shown without the IMU). The M5Dial RTC keeps the time while
  disconnected and off; it is set again on every connection (and when daylight saving time changes).
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
- **Always upright**: with an **M5Stack IMU Unit on Port A**, the UI rotates to stay level. The M5Dial has no
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

[`hardware/support_volant.stl`](hardware/support_volant.stl): a single printed part, held by one M5 screw per
spoke, for the area between the bottom spoke and the right spoke of an **OMP Corsica** 350 mm steering wheel
(95 mm dish).

- **Thin plate** (6 mm), one smooth outline between the two pads and the tube, open under the M5Dial.
- **M5Dial**, raised 33 mm (`DIAL_RISE`) to sit closer to the rim and moved towards the hub (centre 58.5 mm right,
  57.5 mm below the hub centre, ~3 mm from the pads), fixed as on a panel: its body goes through a **flat 3.5 mm
  ring** (Ø 45.6, 6 crush ribs) and its clamp ring (Ø 49.8 × 12.6 mm, from the M5Dial STL) is **put on from
  underneath** through the Ø 52.8 opening and screwed up. The ring sits on a solid tube with **2 arched windows**
  between 3 wide pillars (in line with the two arms and towards the rim): fingers reach the clamp, and **USB-C**
  (on the back of the M5Dial) and the battery cable go through them.
- **Battery**: no holder on the mount; tape the LiPo where it suits you.
- **Attachment**: each pad sits on the back of its spoke between two guide walls (angled like the spoke edges,
  no hooks, no peg: the part goes onto both spokes in one move). Per spoke: **M5 × 16 screw** (longer sticks out at the back) through the spoke
  hole nearest the hub (Ø 16) from the driver side, with a **wide washer Ø 20 or more** (fender washer
  5 × 20 or 5 × 25; a plain M5 washer would drop into the hole), and an **M5 nut** in the hex pocket at the back of the pad.
- Wheel: OMP Corsica (OD0-1954), 350 mm, 95 mm dish. Spoke slope (~43°) from photos of the wheel; widths
  (bottom 40 mm, right 38 mm at the hole), 5 mm thickness and Ø 16 holes measured with a caliper. Right spoke
  from OMP's front product photo (6 × 70 mm bolt circle as scale): its holes are as far from the hub as the
  bottom ones, their axis rises 12.7° and the spoke narrows towards the rim by ~5° per side (edges at ~7.5° and
  ~17.4°); the bottom spoke tapers ~4° per side.

- Source model: Fusion 360 script [`hardware/fusion/SupportVolant`](hardware/fusion/SupportVolant). Utilities ›
  Add-Ins › Scripts › "+" › pick this folder › Run. Dimensions are user parameters (Modify › Change Parameters).
  Spoke widths, thickness and hole size are measured; hole positions, slope and taper come from the photos
  (±2 mm / ±2°). Check the M5Dial body Ø (`BODY_D`) before printing. Run the script again, then export
  the STL.
- Printing: flat as is (one face on the bed, the back), **supports from the build plate under the raised ring**
  only (nothing else overhangs).
  PETG or ASA (PLA warps in the sun), 4 perimeters, 30-40 % infill.
- With the IMU on Port A, the screen stays level when the wheel turns.
- Check that nothing touches the stalks when turning the wheel lock to lock before driving.

## Build

```bash
pio run -t upload
```

## License

[MIT](LICENSE).

## Sensors (planned)

Ports: **A** = I2C (G13 SDA / G15 SCL), **B** = G2 / G1 (can be set up as a second I2C bus). Suggested M5Stack units:
**IMU Pro Mini** (BMI270 + BMM150 + BMP280) on Port A for "always upright", **ENV III** (SHT30 + QMP6988:
temperature, humidity, pressure) on Port B; their I2C addresses do not clash (avoid ENV IV with the IMU Pro Mini:
both have a BMP280 at 0x76). Also worth a look: **DLight** (BH1750, ambient light → automatic screen brightness).
Only the IMU is supported by the firmware today (its chip temperature is shown on the Clock page).

## Notes

- The ESP32-S3 has no Bluetooth Classic: A2DP / AVRCP are impossible, hence AMS (BLE).
- AMS does not send album art: title + artist are shown instead.
- Android and Windows do not expose track info over BLE: that would need a companion app.
- Encoder pulses per detent: 2 on this M5Dial; adjust in Settings › Wheel if one detent does not give one step.
- Code: `src/main.cpp` (loop, gestures, pages), `ble_phone` (AMS + HID + CTS), `ancs` (calls, notifications),
  `govee`, `display` (DMA rendering), `bootanim` (boot image over USB), `imu`, `clock`, `power`, `settings` (NVS), `pages/` (one file per page).
  Settings defaults: `src/config.h`. Code comments are in French.
