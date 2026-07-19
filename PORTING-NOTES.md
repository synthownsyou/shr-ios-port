# iOS Port — What Changed and Why

This tree is `dxcool222/The-Simpsons-Hit-and-Run` (tvOS) retargeted to iPhone/iPad,
with the touch control subsystem ported from `Carlox33/The-Simpsons-Hit-and-Run-Android`.
Personal/educational use only — see the top-level plan (`SH&RPlan.md` on the Desktop)
for the legal position. **No game assets are included here**; you still need to extract
your own from a legitimately owned copy.

## Why `RAD_TVOS` is still defined on an "iOS" build

Don't be alarmed seeing `RAD_TVOS` in `premake5.lua`'s `IOS_DEFINES`. It's not a bug.

The engine's own `src/code/main/platformdef.h` documents `RAD_TVOS` as the shared
"modern Apple/SDL platform" flag (`RAD_SDL_PLATFORM`, `RAD_MODERN_PLATFORM`,
`RAD_OPENGL_PLATFORM` all key off it), explicitly noting it covers "Win32, tvOS
(and future: Switch, Vita, etc.)". A full-tree audit found **100+ files** across
`radcore`, `pure3d`/`pddi`/GLES, `radsound`, and the GUI layer gated on `RAD_TVOS`
with zero tvOS-*specific* behavior inside — file I/O sandboxing, GLES2 rendering,
Win32-HAL-reuse for sound, debug-console suppression, etc. All of it applies to
iOS unchanged (same GLES2, same UIKit/SDL2 windowing model, same sandboxed
filesystem). Renaming all of that without a compiler to verify each change would
have been reckless.

So: **`RAD_TVOS`** = "generic modern Apple/SDL platform" (kept, unmodified).
**`RAD_IOS`** = new, narrow flag for what's *actually* different on iPhone:
touch controls, and not blocking boot on a physical controller.

This also matches precedent: Android's own `CMakeLists.txt` defines `RAD_WIN32`
(not a new `RAD_ANDROID`-only path) as its base SDL-platform flag, plus
`RAD_CONSOLE` and `RAD_ANDROID` on top for the bits that are genuinely
Android-specific. Same pattern, different existing macro reused.

## What actually changed vs. the tvOS tree

- **`src/code/main/iosplatform.{h,cpp}`, `iosmain.mm`** — renamed from
  `TvosPlatform`/`tvosmain.mm`. The one real behavioral change: tvOS's
  `WaitForController()` blocks boot and shows a "Controller Required" splash
  screen until a physical controller connects, because Apple TV has no
  touchscreen. iOS never blocks boot on a controller — touch is the primary
  input, a controller is optional. `InitializePlatform()` just logs whether one
  was found at boot.
- **`src/code/input/ios/ios_controller.{h,mm}`,
  `src/libs/radcore/src/radcontroller/ioscontroller.cpp`** — mechanical rename
  of the tvOS native `GameController.framework` backend (`TvOSInput_*` →
  `IosInput_*`, `@available(tvOS X,*)` → `@available(iOS X,*)`). Logic is
  unchanged; GameController.framework behaves the same on both OSes. The old
  `radcore/src/radcontroller/tvoscontroller.cpp` and
  `code/input/tvos/` were deleted (superseded, not kept in parallel).
- **`src/code/input/touch/*`** — copied verbatim from Carlox33's Android port.
  It's engine-native C++ (SDL2 finger events → this module), not Android UI, so
  it needed no rewrite, only wider preprocessor gates:
  - `touchhudsystem.cpp`, `touchhudrenderer.cpp`, `touchcameracontroller.cpp`,
    `touchcontextresolver.cpp`, `touchinteractionresolver.cpp`: every
    `#if defined(RAD_ANDROID)` widened to `#if defined(RAD_ANDROID) || defined(RAD_IOS)`.
    These gates are real feature code (HUD rendering, camera drag, context
    switching), not Android-only shims — verified by reading each site.
  - `touchassetextractor.cpp`, `touchassetmanager.cpp`,
    `touchcontrolsconfigurationmanager.cpp`: **left untouched on purpose**.
    Their `RAD_ANDROID` branches are JNI/APK-extraction-specific; the existing
    `#else` branch is already the generic cross-platform fallback and iOS
    correctly falls into it unmodified.
- **`src/code/main/game.cpp`, `src/code/input/inputmanager.cpp`** — merged
  Carlox33's touch-integration diff (init/update/shutdown calls, SDL
  finger-event dispatch) onto the tvOS base, gated on
  `defined(RAD_ANDROID) || defined(RAD_IOS)`, preserving the tvOS tree's own
  `RAD_SDL_PLATFORM` event-loop structure. The controller-connect/disconnect →
  "hide touch HUD" bridge was **re-architected**, not copied: Android drives it
  through custom `radControllerSDLSetAndroid*` callbacks tied to SDL's
  controller subsystem, which iOS/tvOS deliberately bypass in favor of native
  GameController.framework (see `ios_controller.mm`). iOS instead notifies
  `TouchInputModeManager` directly from `InputManager::Update()`'s existing
  connect-state-change block — same effect, correct architecture for this
  platform's actual controller backend.
- **`premake5.lua`** — `iOS` platform instead of `tvOS`: `SDKROOT=iphoneos`,
  `TARGETED_DEVICE_FAMILY=1,2` (iPhone+iPad), `IPHONEOS_DEPLOYMENT_TARGET=15.0`,
  new bundle ID `com.simpsonsios.srr2`, and file list swapped to the `ios*`
  sources above.
- **`Info.plist`** — bundle ID updated; added `UIStatusBarHidden`,
  `UIRequiresFullScreen`, and landscape-only `UISupportedInterfaceOrientations`
  (game renders as a fixed-aspect letterboxed 3D view, same as the tvOS build).
- **Untouched, reused as-is:** `radfile/tvos/tvosdrive.cpp` (SDL
  `GetBasePath`/`GetPrefPath`-based sandboxed file I/O — works identically on
  iOS), `pure3d/pddi/gles/display_tvos/gldisplay.cpp` (GLES2/EAGL surface
  code — its own comments already say "iOS/tvOS"), all game/engine logic.

## Known gaps — cannot verify without a Mac + Xcode

This was built without access to Xcode/macOS, so **none of this has compiled**.
Before assuming it works:

1. **No `LaunchScreen.storyboard` or `.xcassets` exist in this tree** (the tvOS
   repo doesn't ship them either — they're Xcode-native binary/IB assets, not
   practical to hand-author). `Info.plist` references `LaunchScreen` by name;
   you'll need to add a launch storyboard and app icon set in Xcode after
   `premake5 xcode4` generates the project, or the app may fail to launch/submit.
2. **First actual compile will surface header/link errors** the file-level
   audit above can't catch (missing includes, symbol mismatches, premake glob
   edge cases). Treat this as a strong first draft, not a finished port.
3. **Touch HUD asset paths**: `touchassetmanager`/`touchassetextractor`'s
   generic (non-Android) fallback path was written for a "just read files
   normally" case — confirm it resolves correctly against the iOS app bundle
   layout once you can actually run it; the touch PNGs from Carlox33's
   `android-project/app/src/main/assets/touch_controls/` still need to be
   copied into this project's own `assets/` folder (not done here — no
   copyrighted/bundled art was moved).
4. **`RAD_CONSOLE` decision**: kept defined (matches Carlox33's own Android
   build, which also defines it alongside touch controls) — but this hasn't
   been runtime-verified to produce the right touch-vs-controller UI/menu
   behavior on iOS specifically.
5. Distribution is sideload/personal-device only — same reasoning as the tvOS
   plan (Simpsons/Fox IP, unlicensed leaked-source lineage). Don't plan around
   App Store or wide TestFlight distribution.

## Pre-flight checks already done (Windows, no Xcode/macOS available)

No Apple toolchain exists on Windows/Linux without the real iOS SDK (Apple IP,
not something to fetch informally) — code signing, `actool`/`ibtool`, and
linking against `UIKit`/`GameController`/`OpenGLES` all require an actual Mac.
**Nothing here has compiled.** What *was* checked without a compiler:

- Every file path and glob pattern in `premake5.lua` resolves to real files —
  no dead references (the #1 cause of a first `premake5 xcode4` failure).
- `Info.plist` is well-formed XML.
- No unguarded tvOS-only APIs leaked into the iOS path (`@available(tvOS...)`
  is gone everywhere it matters). One incidental `GCMicroGamepad` reference in
  `ios_controller.mm` is harmless dead code — that class exists in
  GameController.framework on iOS too, it just never matches any iOS
  controller's profile.
- Brace and `#if`/`#endif` balance verified on every hand-edited file
  (`game.cpp`, `inputmanager.cpp`, `iosplatform.cpp`, `ioscontroller.cpp`,
  `ios_controller.mm`, `srrmemory.cpp`, and all 11 `touch/*.cpp` files after
  the `RAD_ANDROID` → `RAD_IOS` gate widening) — no dangling directives.
- Caught and fixed one real dangling reference: `code/memory/srrmemory.cpp`
  still `#include`d the deleted `tvosplatform.h` and called `TvosPlatform::*`;
  now uses `iosplatform.h`/`IosPlatform` under `RAD_IOS`.

This clears the cheap, mechanical class of first-build errors. It does **not**
prove the code links — only an actual Xcode compile can do that.

## Next steps (suggested order)

1. On a Mac: `brew install premake5`, then `premake5 xcode4` in this directory.
2. Extract your own game assets into `assets/` (see the tvOS README for the
   expected layout) and copy Carlox33's `touch_controls/` PNGs in alongside.
3. Open the generated Xcode project, add a LaunchScreen storyboard + app icon,
   fix whatever the first compile turns up.
4. Run on a real iPhone (GLES2/GameController don't behave identically in the
   Simulator) and iterate on the touch HUD from there.
