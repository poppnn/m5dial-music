# M5Dial Music

Bluetooth (BLE) media remote on the M5Stack M5Dial (ESP32-S3), for **iPhone, Android and Windows**.

- **iPhone**: native **Apple Media Service (AMS)** → controls + title, artist, progress, shuffle / repeat,
  plus time (CTS), calls and notifications (ANCS).
- **Android**: **BLE HID** remote (standard media keys) for the controls; title, artist, progress, time, calls and
  notifications through [Gadgetbridge](https://gadgetbridge.org/) (free app), which takes the M5Dial for a Bangle.js watch.
- **Windows**: **BLE HID** remote for the controls; title, artist, progress, time and notifications through a small
  Python app on the PC ([`tools/windows`](tools/windows/m5dial_pc.py)).

**One firmware per platform**, picked in the [web flasher](https://poppnn.github.io/m5dial-music/) (iPhone by
default): each one only runs what its platform needs. When updating, leave **"Erase device"** unchecked in the
web flasher: settings, paired phones and the boot image are kept (ticked, the whole M5Dial is wiped). After that,
updates can also come **over the air** through the phone's hotspot (Settings › Update). The first install with the
OTA layout moves the boot image partition: send the boot image again once.

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
- **Notification**: full screen, sliding down from the top. For safety while driving, the **app name**, the
  **sender** (contact name in messaging apps) and the notification count (5 s). Settings › Notifs: **off** (none shown), **sender** (default), **full** (plus
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
the page never freezes). The last colour (preset or hue) and brightness are remembered and sent to the bar
on every connection.
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
    active icons, notification border), **Click**: a short buzzer tick on every wheel detent and press (the
    M5Dial has no vibration motor), on by default.
  - **Update** (over the air): shows the phone hotspot saved from the web flasher (section 7). Hotspot on
    (iPhone: Maximize Compatibility, 2.4 GHz only), first press = "confirm?", second press: Bluetooth stops, the
    M5Dial joins the hotspot, reads the published version and, if it differs, downloads this platform's firmware
    over HTTPS (certificate checked against pinned roots), writes it to the second app slot, checks it and
    restarts on it. Up to date, no Wi-Fi or any error: it says so and restarts as before. Press during
    "Connecting" to cancel.
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
in its own flash partition (`bootanim`, 1.9 MB, `partitions.csv`: the rest holds two 3 MB app slots for the
over-the-air update); it is then shown full screen while waiting for the
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
| [`support_volant_lipo.stl`](hardware/support_volant_lipo.stl) | **with the LiPo shell** | instead of `support_volant.stl` (bayonet at the tube foot) | same as the mount |
| [`couvercle_batterie.stl`](hardware/couvercle_batterie.stl) | 1 | **lay it on its flat plate** | none |
| **Or, removable 16350 battery:** | | | |
| [`support_cartouche_16350.stl`](hardware/support_cartouche_16350.stl) | 1 | floor down (as exported), **instead of the shell and lid**, with `support_volant.stl` | none (the top bridges 27 mm) |
| [`cartouche_16350.stl`](hardware/cartouche_16350.stl) | 1 per battery | bottom down (as exported) | none |
| [`bouchon_cartouche_16350.stl`](hardware/bouchon_cartouche_16350.stl) | 1 per battery | end plate on the bed (plug and cover up) | none |
| [`chargeur_cartouche_16350.stl`](hardware/chargeur_cartouche_16350.stl) | optional | charging dock, base plate down (as exported) | none |
| [`couvercle_chargeur_16350.stl`](hardware/couvercle_chargeur_16350.stl) | with the dock | **upside down** (plate on the bed, lip up) | none |
| [`cache_ecrou.stl`](hardware/cache_ecrou.stl) | 2 | flat base | none |
| [`cache_factice.stl`](hardware/cache_factice.stl) | 1 | peg on the bed | under the dome rim |
| [`support_volant_test.stl`](hardware/support_volant_test.stl) | optional | flat underside | none |

PETG or ASA (PLA softens in a car in the sun), 4 perimeters, 30–40 % infill for the mount, 3 perimeters for the
other parts. The test part (two pads joined by a bar, ~20 cm³) checks the fit on the wheel before the full print.

### Hardware

- M5Stack **M5Dial** (v1.1). Optional: **IMU Pro Mini** unit on Port A (screen stays level, pressure) and **ENV III**
  unit on Port B (temperature, humidity, pressure), with short Grove cables.
- LiPo **103450** (50 × 34 × 10 mm) with its protection board and a lead long enough to reach the M5Dial.
  **Or** the removable cartridge: per battery one **16350** Li-ion cell, **850 mAh** (Ø 16 × 35 mm, the cell of a
  **JNR** disposable puff, with solder tabs: + on one end, - on the other) and **2 female Dupont jumper ends** (2.54 mm, 14 mm housings, wire attached); for the holder
  **2 male Dupont jumper ends** (6 mm pins), their wires going to the M5Dial battery plug. Vape cells have **no protection circuit**:
  check they read above 3.0 V before use, and a small protection board (DW01) on the cell is safer.
  Charging dock: the **charge board of a JNR puff**, marked **SIC C023-M22 V1.5** on the back (28 × 22 mm plus an
  11 mm tab with the USB-C socket underneath,
  "188 %" display on top, red + / black - battery wires), 2 more male Dupont jumper ends and 3 M1.4 × 4-5 mm
  self-tapping screws. It charges the same
  cell it came with. **Make sure it can't heat**: unsolder the puff sensor (or its heater wires) before use.
- Per spoke (bottom and right): **M5 × 14 hex screw**, **fender washer M5 × 20** (a plain M5 washer drops into the Ø 16 hole), **M5 nut**.

### Assembly

1. Install the firmware for your phone or PC from the [web flasher](https://poppnn.github.io/m5dial-music/).
2. Put the M5Dial into the flat ring from the top, then screw its **clamp ring on from underneath** through the tube
   (the 3 large windows leave room for the fingers).
3. Plug the battery into the M5Dial (check the polarity), slide it into the shell through the open end, its lead
   going up through the slot in the top, then **clip the lid** on (to open it: press the 2 bumps through the side holes).
   **Removable 16350 cartridge instead** (polarity rule: **the jumper on the "+" side is +**: engraved "+" next to the
   holder pocket and on the cartridge cap, on the same side as the cartridge's wire slot). The connection sits on top
   of the cell, so the cartridge is short:
   - Holder: drop the 2 male jumper ends side by side into the pocket at the back of the holder, from above (inside
     the collar), pins towards the open end through the front lip; their wires go straight up to the M5Dial battery
     plug. Tight fit; a drop of glue if they move. Put a small **compression spring** (ballpoint pen spring, Ø 5 mm
     max, 14 mm or longer) into the round hole at the back of the holder.
   - Cartridge: solder one female jumper wire to each end of the cell, slide the cell in through the open end; both
     wires come up through the **long open slot** beside the cradle (no hole to thread), the far-end one runs back
     along the top channel; drop the 2 female ends into the cradle (sockets towards the inner end, + on the "+"
     side, wires over the back step). Then **slide the cap on**: its cover goes into the channel's side grooves and
     hides the wires, its plug enters the cell bore, and it clicks into place.
   - Swap: slide the cartridge in, cradle up and connector end first, until both side arms click (the spring pushes
     it against the hooks: no play); to remove, **pinch the two arm ends** that stick out and the spring pushes it
     out. Upside down or the wrong way round it stops against the pin tips long before any contact: don't force it.
   - Charging dock: drop 2 male jumper ends into the dock's pocket (in the bay, + on the "+" side), put a spring in
     its back hole, join the board's **red wire to the + end and black to the -** behind the board, then lay the
     board display up on its rails, tab forward, USB-C socket in the front notch and its back edge against the two
     stops, and fix it with **3 M1.4 × 4 (or 5) self-tapping screws** through its holes into the posts; press the lid
     on (the display shows through its window). Slide the cartridge in like in the car.
4. Push the shell's (or holder's) round collar into the tube from underneath (3 lugs into 3 notches) and **turn it
   35° clockwise as seen from below** until it clicks.
5. Fit the mount behind the bottom and right spokes, one screw per spoke through the Ø 16 hole with the Ø 20
   washer, nut and screw head on either side (the hex pocket at the back of the pad holds a nut or a screw head),
   then press a **nut cap** onto each nut on the driver side, and clip the **dummy cap** into the left spoke's
   Ø 16 hole (nothing is fixed there; the three spokes look alike).
6. Optional sensors: plug the **IMU Pro Mini** into Port A and the **ENV III** into Port B and fix them where you
   like (double-sided tape, jumper wires); the printed holders are off by default. They are found at power on.
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
- **16350 cartridge** (`BAT_TYPE` = 1; 0 = LiPo shell): same bayonet as the shell, but the holder rises **inside
  the tube**, up to `BAT_GAP` (5 mm) under the M5Dial's lowest point, its clamp ring (bottom 14.6 mm above the
  mount's underside, from the official [Dial.stl](https://github.com/m5stack/M5_Hardware/tree/master/Products/K130_Dial/Structures)
  model and `CLAMP_H` 12.6). It hangs only **18 mm** below the mount (the LiPo shell: 14.2, the first cartridge:
  27). For that the holder is centred with rounded corners (it turns inside the Ø 52.8 bore for the quarter turn),
  the bayonet crown is level with its top, the lug entry notches start at the tube foot, and a notch in the tube
  foot lets the end of the cartridge out on the rim side (pinch the arms below the mount). So this mount
  (`support_volant.stl`) only takes the cartridge holder; the LiPo shell goes with `support_volant_lipo.stl`.
  The connection rides **on top of the cell**: 2 Dupont jumper ends side by side (2.54 mm pitch, 14 mm housings),
  male ones in a block of the holder right under the M5Dial (wires straight up), female ones in a cradle on the
  cartridge (sockets recessed behind a 0.6 mm lip: no exposed live contact in a pocket). Holder 42.8 × 30.3 ×
  27.0 mm; cartridge 47.2 + 1.6 mm cap, 26.5 mm wide with its arms, cell in a Ø 16.9 bore, its first 17.5 mm
  lowered under the holder block, 4 mm rounds underneath and 1 mm on top (cap to match). Two flexible side arms
  (1.5 mm, 26 mm long) click 0.9 mm hooks into **blind pockets** of the holder walls (the wall stays closed outside,
  the hook bears on a solid 2.5 mm edge); the pocket edge sits exactly on the hook's slightly undercut face, so the
  arm's spring pulls the cartridge against its stop (no end play). Pinching the arm ends over their 2.4 mm gap moves
  the hooks back about 1.15 mm, enough to release them (0.9 % bending on insertion: PETG or ASA rather than PLA). Vents:
  4 slots centred under the cell and 2 on each front flank, matched by slots in the holder floor and walls. Wires: a 4 mm
  slot opens the cell bore into the top channel over the whole length, 6.5 mm wide over the last 7 mm, and the
  cap plug is notched on that side, so the far-end wire comes up past the plug. Cap: end plate, plug in the bore (0.05 mm
  clearance), and an 11 mm cover sliding in 0.6 mm grooves of the channel walls with two 0.2 mm click bumps.
  Ejection: Ø 5.4 × 9 mm spring pocket behind the holder's back wall, centred on the cartridge's inner end (on the
  car mount it stays below the mount). No keying rib: the cradle-up, connector-first shape is the only way the
  sockets can meet the pins.
- **Charging dock**: the same pocket, clip, spring and male jumper block as the car holder on a wider base plate,
  symmetric, with a 32.4 mm wide bay on top (the base widened to match) for the **JNR puff charge board**, display
  up and readable from the front (the cartridge side) through a window in the lid. The board rests on two rails and
  **3 screw posts** (Ø 3.4, 1.1 mm pilot holes for M1.4 self-tapping screws, at its 3 holes: `HOLES`) 4.2 mm up,
  1.9 mm off-centre in the bay so the top post clears the jumper pocket (its USB-C socket is under the tab, on a
  1 mm pad, so the plug clears the cartridge end). The tab ends at the front face, so the **socket sits right at
  the edge**, through a 9.2 mm notch and a tab slot (`TAB_IN`, `TAB_OFF`, `DISP` if your board differs); two stops
  behind the board take the plug's push, the wires join behind it; the push-on lid has a tongue that fills the slot
  above the tab and holds it down.
- **Nut caps**: low dome Ø 24 × 7.5 mm pushed onto the nut (hex socket 8.1 mm across flats), with or without the
  washer underneath; an M5 × 14 screw ends inside it. **Dummy cap**: same dome with a peg split into 4 fingers that
  clips into the Ø 16 hole (hook 0.3 mm under the 5 mm spoke). Source
  [`hardware/fusion/CacheEcrou`](hardware/fusion/CacheEcrou): `NUT_AF` 8.0 if too loose, 8.2 if too tight.
- **Sensor holders** (off by default; `ENV_HOLDER`, `IMU_HOLDER` = 1 to bring them back): the IMU Pro Mini (24 × 24 × 8) lies flat in a holder on top
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
