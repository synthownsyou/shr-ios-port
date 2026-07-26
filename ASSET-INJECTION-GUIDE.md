# Asset injection guide — adding your game data to `SRR2-unsigned.ipa`

The `.ipa` in this release is **unsigned and has no game assets**. It will build
and install, but it won't play anything until you inject the game data from a
copy of *The Simpsons: Hit & Run* that **you legitimately own**. This guide
walks you through that.

> ⚠️ **Legal:** This is for personal use with your own legitimately owned copy
> of the game. Do not redistribute an `.ipa` with assets baked in. Do not sell
> it. See the [Legal](README.md#legal) section of the README. If a rights holder
> asks for this to come down, it comes down.

---

## What you need

1. **A legitimately owned copy of *The Simpsons: Hit & Run*** (PC CD/DVD or a
   digital install you bought). You need the game's data files from its install
   folder — you do **not** need the game to be installed/runnable on this
   machine, just the files.
2. The **`SRR2-unsigned.ipa`** from this release.
3. A **sideload tool**: [AltStore](https://altstore.io/) or
   [Sideloadly](https://sideloadly.io/) (Windows/macOS), and a free or paid
   **Apple ID**. You sideload to **your own device** with your own Apple ID —
   no $99 developer account required for personal use.
4. A computer to do the asset injection (Windows, macOS, or Linux — any OS
   that can zip/unzip).

---

## Step 1 — Locate your game's data files

In your SH&R install folder (typically
`C:\Program Files (x86)\Vivendi Universal Games\The Simpsons Hit & Run\` on the
PC version), you'll find a set of `.rcf` files and a few folders. The game
engine loads everything from these. The filenames the iOS build expects are:

```
ambience.rcf
carsound.rcf
dialog.rcf
music00.rcf      (if present in your copy)
music01.rcf
music02.rcf
music03.rcf
nis.rcf
scripts.rcf
soundfx.rcf
```

plus the loose folders (if your install has them):

```
art/
scripts/
movies/
sound/
```

If your copy doesn't have one of these (e.g. `music00.rcf`), skip it — the
engine tolerates a missing file better than a 0-byte placeholder. **Do not
copy a 0-byte file over a real one.** The `.ipa` ships 0-byte placeholders for
all of these; you're going to overwrite them with the real ones.

Copy the real `.rcf` files and the `art/`, `scripts/`, `movies/`, `sound/`
folders (with their contents) into a working folder on your computer, e.g.
`my-assets/`.

---

## Step 2 — Unpack the `.ipa`

An `.ipa` is just a zip. Unzip it:

```bash
mkdir work && cd work
unzip ../SRR2-unsigned.ipa
```

You now have a `Payload/` folder containing `SRR2.app/`. The asset root inside
the app is:

```
Payload/SRR2.app/Assets/TheSimpsons/
```

You'll see the 0-byte placeholder `.rcf` files and empty `art/`, `scripts/`,
`movies/`, `sound/` folders there. The `touch_controls/` folder next to them is
**already populated** with the on-screen button icons — leave it alone.

---

## Step 3 — Inject your assets

Copy your real game data into the asset root, overwriting the 0-byte
placeholders:

```bash
# from inside the work/ folder you just unzipped into
cd Payload/SRR2.app/Assets/TheSimpsons/

# overwrite the placeholder .rcf files with your real ones
cp /path/to/my-assets/*.rcf .

# copy the loose folders (with contents), merging over the empty ones
cp -R /path/to/my-assets/art/* art/      2>/dev/null || true
cp -R /path/to/my-assets/scripts/* scripts/  2>/dev/null || true
cp -R /path/to/my-assets/movies/* movies/   2>/dev/null || true
cp -R /path/to/my-assets/sound/* sound/    2>/dev/null || true
```

Verify the placeholders got replaced — the `.rcf` files should now be
non-zero size:

```bash
ls -lh *.rcf
```

If any are still `0`, your source copy doesn't have that file (see Step 1 —
that's usually fine).

---

## Step 4 — Repack the `.ipa`

Re-zip `Payload/` (not the outer folder — the zip's top entry must be
`Payload/`):

```bash
cd ..   # back to work/, so you're next to Payload/
zip -qry SRR2-with-assets.ipa Payload
```

> **Windows note:** use a real zip tool (7-Zip, or `tar -a -c -f out.zip
> Payload` on Windows 10+). Do **not** use the built-in "Send to → Compressed
> folder" for a 1+ GB archive — AltStore/SideStore have been observed to reject
> zips it produces. `zip` (Git Bash / WSL), 7-Zip, or `tar` all work.

You now have `SRR2-with-assets.ipa` with your game data inside.

---

## Step 5 — Sideload to your iPhone/iPad

1. Plug your iPhone into your computer.
2. Open **Sideloadly** (or AltStore).
3. Drag in `SRR2-with-assets.ipa`.
4. Enter your **Apple ID** and (app-specific) password. Sideloadly signs the
   app with a free personal certificate tied to your Apple ID and installs it
   on your device. (Your Apple ID is used only to request the signing
   certificate from Apple — Sideloadly does not store it.)
5. Wait for the install to finish.

> **Free Apple ID limitation:** a free personal signing certificate lasts **7
> days**, after which the app stops opening until you re-sign it (just plug in
> and re-sideload — same `.ipa`, takes a minute). A paid Apple Developer
> account ($99/yr) gives a 1-year certificate. AltStore's "AltJIT"/background
> refresh can auto-renew the 7-day cert over Wi-Fi.

---

## Step 6 — Trust the developer profile (first launch)

iOS won't open a sideloaded app until you trust its signing certificate:

1. On the iPhone: **Settings → General → VPN & Device Management**.
2. Tap your Apple ID under "Developer App".
3. **Trust** it.
4. Launch **The Simpsons: Hit & Run** from the home screen.

---

## Known launch caveat (read this before you panic)

There is **no `LaunchScreen.storyboard` in this build** (see roadmap item #1
in the README). On some iOS versions / devices this can cause the app to close
immediately on launch. If that happens, it is **not** an asset problem and
**not** a signing problem — it's the missing launch screen, and the fix is to
build from source in Xcode with a launch screen added (see the README's
"Local build" section). This is the single highest-priority open issue for
anyone continuing the project.

If it launches: you should see the game's loading sequence. The on-screen
touch controls (virtual d-pad, action buttons) appear over the gameplay —
their icons ship in the `.ipa` already. A connected Bluetooth controller also
works and auto-hides the touch HUD.

---

## Quick reference — asset layout inside the bundle

```
Payload/SRR2.app/
├── SRR2                          (the executable)
├── AppIcon*.png                  (home-screen icons — already present)
├── Info.plist
└── Assets/TheSimpsons/           ← your game data goes here
    ├── art/                      ← populate from your install
    ├── scripts/                  ← populate from your install
    ├── movies/                   ← populate from your install
    ├── sound/                    ← populate from your install
    ├── ambience.rcf              ← overwrite 0-byte placeholder
    ├── carsound.rcf              ← overwrite 0-byte placeholder
    ├── dialog.rcf                ← overwrite 0-byte placeholder
    ├── music00.rcf … music03.rcf ← overwrite 0-byte placeholder
    ├── nis.rcf                   ← overwrite 0-byte placeholder
    ├── scripts.rcf               ← overwrite 0-byte placeholder
    ├── soundfx.rcf               ← overwrite 0-byte placeholder
    └── touch_controls/           ← ALREADY populated — do not touch
```

Bundle ID: `com.simpsonsios.srr2` · Display name: "The Simpsons: Hit & Run" ·
Requires iOS 15.0+ · iPhone and iPad · landscape only.