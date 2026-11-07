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
- iPhone: **shuffle** and **repeat** icons at the bottom (orange = on, "1" = repeat one): tap them to change.
  **Double tap** elsewhere = "Like" (a heart pops up). A single tap (play / pause) is sent after 0.3 s, to leave
  room for a double tap; the wheel press stays instant.

## Calls and notifications (iPhone)

Through Apple's ANCS service (no app to install, pairing is enough):

- **Incoming call**: full screen on any page, caller name in large type. **Press** the wheel = answer,
  **hold** (0.8 s) = decline. The screen then goes back to the previous page.
- **Notification**: for safety while driving, only the **app name** and the **notification count** are shown (4 s).
  Settings › Notif text › shown: title and text in large type (8 s). Press = close.
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
(10° per detent); another short hold goes back to brightness (the bottom line alternates between what the wheel
does and how to switch). The last colour (preset or hue) and brightness are
remembered and sent to the bar on every connection.
Close the Govee Home app: the bar only accepts one connection.

## Stopwatch, Clock, System and Settings

- **Stopwatch**: 7-segment display, hundredths, second-hand ring (one turn per minute). Press = start / pause,
  swipe left or right = reset. It keeps running when you change page.
- **Clock**: iPhone time and date (Current Time Service), seconds ring. The M5Dial RTC keeps the time while
  disconnected and off; it is set again on every connection (and when daylight saving time changes).
- **System**: last boot reason (normal, crash, watchdog, brownout), connected phone and its signal, Govee LEDs,
  CPU frequency and temperature, frames per second and render time, free RAM, uptime. Battery level cannot be
  measured: the M5Dial has no battery voltage or charge signal wired to the ESP32 (it would need a voltage divider
  soldered to an ADC GPIO).
- **Settings** (stored in the M5Dial memory, kept across power off and updates): turn = choose a row,
  press = edit (turn = value, applied immediately), press = done.
  - IMU direction (normal / reversed), encoder pulses per detent (1 to 4), dim delay (never to 120 s),
    power-off delay without a device (never to 60 min), day and night brightness, notification text
    (hidden by default).
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
arc and "Waiting for phone". Controls are locked, except the hold to power off. On connection, a fade leads to the
UI.

To use your own logo (PNG or BMP, ideally on a transparent or black background):

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

[`hardware/support_volant.stl`](hardware/support_volant.stl): a single **fully printed** part (no screws; only the
battery is taped) for the area between the bottom spoke and the right spoke of a GT2i Race 350 mm steering wheel
(90 mm dish).

- **Thin filled plate** (6 mm) between the two spokes: M5Dial ring, arms and the area between them, in one piece.
- **M5Dial**: its body goes into the ring (full circle Ø 45 / Ø 51, **3.5 mm high**), gripped by 6 crush ribs;
  the collar rests on it and the body sticks out behind, so **USB-C** and Port A stay free. Around it, the plate
  is lowered to the ring level with 1 mm clearance: nothing rubs.
- **LiPo 103450 battery** (50 × 34 × 10 mm): taped to the **back** of the plate (double-sided tape), diagonally,
  long side parallel to the inner edge (hub side) and aligned on it, between the two arms. There it touches
  neither the spokes (which sit further back near the hub) nor the M5Dial body; 99 % of it rests on the plate.
- **Attachment**: each pad clips onto its spoke (push the spoke between the two hooked walls, which spread then
  close over its front face); a peg goes into the spoke hole so it cannot slide.

- Source model: Fusion 360 script [`hardware/fusion/SupportVolant`](hardware/fusion/SupportVolant). Utilities ›
  Add-Ins › Scripts › "+" › pick this folder › Run. Dimensions are user parameters (Modify › Change Parameters).
  **Before printing, measure the `MESURER` values**: spoke thickness (`SPOKE_T`, the clip depends on it), spoke
  hole Ø (`PEG_D`), M5Dial body Ø (`BODY_D`), hole positions and spoke slope. Run the script again, then export
  the STL.
- Printing: flat as is (one face on the bed, the back), **no supports**. PETG or ASA (PLA warps in the sun and
  breaks when clipping), 4 perimeters, 30-40 % infill.
- Test the clip on a quick print first: too stiff, lower `CLIP_LIP`; too loose, lower `SPOKE_T` by 0.2 mm.
- With the IMU on Port A, the screen stays level when the wheel turns.
- Check that nothing touches the stalks when turning the wheel lock to lock before driving.

## Build

```bash
pio run -t upload
```

## License

[MIT](LICENSE).

## Notes

- The ESP32-S3 has no Bluetooth Classic: A2DP / AVRCP are impossible, hence AMS (BLE).
- AMS does not send album art: title + artist are shown instead.
- Android and Windows do not expose track info over BLE: that would need a companion app.
- Encoder pulses per detent: 2 on this M5Dial; adjust in Settings › Wheel if one detent does not give one step.
- Code: `src/main.cpp` (loop, gestures, pages), `ble_phone` (AMS + HID + CTS), `ancs` (calls, notifications),
  `govee`, `display` (DMA rendering), `imu`, `clock`, `power`, `settings` (NVS), `pages/` (one file per page).
  Settings defaults: `src/config.h`. Code comments are in French.
