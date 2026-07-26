# The Simpsons: Hit & Run — iOS (iPhone / iPad) port

A community port of *The Simpsons: Hit & Run* (2003, Radical Entertainment) to
**iOS** — iPhone and iPad, touch controls.

This is **not** my project to finish. I'm moving to Android and leaving this
here for anyone who wants to take it forward. The hard part — getting a
20-year-old leaked engine to compile, link, and *run* on iOS with on-screen
touch controls — is done. What's left is real-device iteration and polish. If
you have a Mac, an iPhone, and a legitimate copy of the game, you can pick this
up today. **Read [PORTING-NOTES.md](PORTING-NOTES.md)** for the full
architecture story; this README is the map.

---

## Lineage / where this came from

| Source | Role |
| --- | --- |
| [`dxcool222/The-Simpsons-Hit-and-Run`](https://github.com/dxcool222/The-Simpsons-Hit-and-Run) | **Base.** The tvOS (Apple TV) port. This tree is that tree retargeted from Apple TV to iPhone/iPad. |
| [`Carlox33/The-Simpsons-Hit-and-Run-Android`](https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android) | **Touch controls.** The on-screen HUD + SDL2-finger-event input subsystem, ported across. |
| `ZenoArrows` lineage | The leaked source tree both forks build on. |

**No game assets are in this repo.** You must extract your own from a copy you
legitimately own (see [Legal](#legal)).

---

## Where the project is right now (status)

Done and pushed:

- **Source-level port tvOS → iOS.** Platform layer (`iosplatform`, `iosmain`),
  native `GameController.framework` controller backend (`ios_controller`),
  `premake5.lua` retargeted to `iphoneos` / `TARGETED_DEVICE_FAMILY=1,2` /
  deployment target 15.0, `Info.plist` with landscape-only + full-screen.
  See [PORTING-NOTES.md](PORTING-NOTES.md) for *why `RAD_TVOS` is still
  defined on an iOS build* (short answer: it's the engine's shared
  "modern Apple/SDL platform" flag across 100+ files; `RAD_IOS` is the new
  narrow flag for what's actually iPhone-specific).
- **It compiles and links for iOS.** GitHub Actions CI (`.github/workflows/build-ios.yml`)
  rebuilds SDL2 / libpng / OpenAL Soft from source against the `iphoneos` SDK
  (the vendored `.a` files are tvOS-tagged and the linker refuses them),
  fetches a prebuilt iOS FFmpeg 6.1.2 from a release asset, runs `xcodebuild`
  for device + simulator, and packages an **unsigned `SRR2.ipa`**.
- **FFmpeg 6.1.2 for iOS** built once via `.github/workflows/build-ffmpeg-ios.yml`
  and published as the release asset `ffmpeg-ios-6.1.2-arm64` (tag of the same
  name). `--disable-asm` plain-C cross-compile, ABI-matched to the vendored
  headers (avcodec 60 / avformat 60 / avutil 58).
- **Touch HUD works on iOS.** Control icons load from the app bundle
  (`assets_static/touch_controls/` → copied in by premake postbuild), and four
  runtime blockers that left the HUD compiled-green-but-dead-on-device were
  fixed (commit `8a99927`): the renderer is now hooked into
  `FrontEndRenderLayer::Render()`, the `BeginTouchHud2D`/`GetRenderDimensions`
  Android-only stubs were widened to iOS, the pad-0 controller object is now
  always created (so `mButtonNames` exists and touch input resolves to real
  actions even with no physical gamepad), and `InputManager::Update` dispatches
  virtual/touch button changes to mappables via `IsInputAvailable()`.
- **App icon + home-screen name** ("The Simpsons: Hit & Run") wired via
  `CFBundleIcons` / `CFBundleDisplayName`.
- **Sideloadly signing bug fixed.** FFmpeg "frameworks" here are static
  archives, not dylibs; embedding them in `Frameworks/` broke code signing.
  Removed (commit `4764d93`).

Built green, shipped an `.ipa`, signed and sideloaded to a real iPhone with
real game assets injected. **The app builds.** Whether it *launches cleanly*
on a fresh device is the open question — see roadmap item #1.

---

## Roadmap — what's left (in priority order)

1. **`LaunchScreen.storyboard` is missing.** `Info.plist` references
   `LaunchScreen` by name, but no storyboard exists in the tree (the tvOS repo
   doesn't ship one either — it's an Xcode-native IB asset, not practical to
   hand-author). **This is the most likely cause of the app closing on
   launch.** First concrete task for anyone picking this up: open the generated
   project in Xcode, add a launch screen, rebuild, confirm it opens on device.
2. **On-device verification of the touch fixes.** The four runtime blockers in
   `8a99927` were fixed *blind* — no Mac/iPhone was in the loop for the fix.
   Someone needs to confirm the HUD actually renders and touch actually drives
   the game, then file concrete, reproducible bugs against what they see.
3. **Audio bugs inherited from the tvOS port.** NPC speech speed and general
   audio glitches (carried over from the upstream tracker) — still open.
4. **Performance on real hardware.** GLES2 fixed-function pipeline; framerate
   on an actual iPhone is unmeasured. Likely needs tuning for modern devices.
5. **Simulator build.** Expected to fail at link today — `third_party/` libs
   are device-SDK-only single-platform `.a`/`.framework`, no `.xcframework`
   simulator slices. Low priority (the device build is the one that matters),
   but an `.xcframework` per lib would fix it cleanly.
6. **`RAD_CONSOLE` runtime behavior.** Kept defined to match Carlox33's Android
   build (which also defines it alongside touch), but not verified that the
   touch-vs-controller menu/UI behavior is correct on iOS specifically.
7. **Document a clean sideload walkthrough** (AltStore / Sideloadly with a
   personal Apple ID) end-to-end, for newcomers who've never sideloaded.

If you finish #1 and #2, you have a playable game. Everything after that is
polish.

---

## How to build

> **Just want to play?** Grab `SRR2-unsigned.ipa` from the
> [latest release](https://github.com/zmodelerlover/shr-ios-port/releases)
> and follow the **[Asset Injection Guide](ASSET-INJECTION-GUIDE.md)** to add
> your own game data and sideload. No Mac required.

### CI (no Mac needed — this is how the `.ipa` is produced)

Push to `main` (or run the workflow manually) and `.github/workflows/build-ios.yml`
runs on a `macos-14` runner: rebuilds the three libs, fetches FFmpeg, generates
the Xcode project with `premake5`, builds for device, and uploads an unsigned
`SRR2.ipa` as an artifact.

**First-time setup, one step:** trigger `.github/workflows/build-ffmpeg-ios.yml`
once manually (Actions tab → "Build FFmpeg 6.1 for iOS" → Run workflow). It
cross-compiles FFmpeg and publishes it as the release asset
`ffmpeg-ios-6.1.2-arm64`. The main build downloads that asset instead of
rebuilding FFmpeg every push (FFmpeg cross-compiles are slow). If you skip
this, the main build still validates C++/link for everything *except* FFmpeg
and the `.ipa` won't be complete.

### Local (needs a Mac + Xcode 16.x)

```bash
brew install premake5
# drop your legitimately-owned game assets into assets/ (expected layout: see
# the tvOS repo's README — art/, scripts/, movies/, sound/, *.rcf)
premake5 xcode4
open build/SRR2.xcodeproj
# in Xcode: add a LaunchScreen.storyboard, sign with your developer account,
# build & run on your iPhone (GLES2/GameController don't behave identically in
# the Simulator — use a real device)
```

The touch control icons already ship in `assets_static/touch_controls/` and are
copied into the bundle by premake's postbuild — you don't need to fetch those.

---

## Legal

- **No game assets are stored in this repository.** You must own a legitimate
  copy of *The Simpsons: Hit & Run* and extract your own data. The `assets/`
  folder is gitignored and empty by design.
- The source code is the leaked Radical Entertainment engine (2003) via the
  ZenoArrows lineage. *The Simpsons* and all related characters/IP are
  property of Fox / Disney. **This is not ours and this is not for sale.**
- **Personal / educational use only.** Don't distribute the built `.ipa` with
  assets baked in, don't put it on the App Store, don't charge for it.
  Sideload to your own device with your own Apple ID (AltStore / Sideloadly).
  The `.ipa` produced by CI is **unsigned** on purpose.
- If a rights holder asks for this to come down, it comes down. No argument.

---

## Credits

This is a **fork** — I stood on the shoulders of several people who did the
hard work before me. All credit where it's due:

- **dxcool222** — author of [the tvOS (Apple TV) port](https://github.com/dxcool222/The-Simpsons-Hit-and-Run),
  the direct base of this project. This whole tree is dxcool222's work
  retargeted from Apple TV to iPhone/iPad. The platform layer, the native
  GameController backend, the GLES2/EAGL rendering, the sandboxed file I/O,
  the premake project, the CI shape — all started here.
- **Jveda** — primary collaborator on the tvOS port per its original README:
  camera controls, UI/text scaling, and the long grind of fixing Lisa's
  School environment. Carried forward unchanged into this iOS tree.
- **Carlox33** — author of [the Android port](https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android).
  The entire touch-control subsystem (on-screen HUD, SDL2 finger-event input,
  camera-drag, context/interaction resolvers) and the **touch-control button
  icons** shipped under `assets_static/touch_controls/` come from this fork.
  Without Carlox33's work there are no touch controls on iOS, full stop.
- **ZenoArrows** and the wider community that preserved and circulated the
  leaked 2003 Radical Entertainment source tree. No source, no port.
- **Radical Entertainment** — the original engineers who wrote this engine and
  this game in 2003. Twenty-three years later it still has a community around
  it because the work was that good. All of this is built on their code.

If I missed anyone, it's an oversight, not a slight — open an issue and I'll
add you.

---

## A note from the original author

I started this to play *Hit & Run* on my iPhone. It got further than I
expected — a real, buildable, sideloadable iOS port with working touch
controls — but I'm moving to Android and won't be maintaining this. If you
pick it up, the highest-leverage thing you can do is **add the launch screen
and test on a real device**. Everything else flows from that. Good luck.

This project was made by a Brazilian. 🇧🇷

— *cLohan*