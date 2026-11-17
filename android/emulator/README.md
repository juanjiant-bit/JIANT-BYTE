# JIANT Offline — Android 0.2 beta

Installable Android app for Jiant FM and JIANT /B1T3. Android 10 or newer, with a current Android System WebView. Includes every engine on first install; no first-launch download, login, microphone, storage or USB permission.

The selector/download/storage layer is native Android. Audio and firmware execute as WebAssembly in the system WebView's AudioWorklet, **not native JNI/NDK DSP**. This allows engine updates independently of APK updates. The fixed HTML/JS panel is included in the APK; downloaded engines cannot replace that shell. Portrait and landscape reuse the browser emulator's touchscreen layouts. Rotation retains the running engine. Drag encoders, tap/hold keys and buttons together. Back or background stops audio and releases held input. Use the firmware's SAVE controls before leaving; saved flash and normal firmware autosave persist, but an unsaved live editing session is not a checkpoint.

## Versions and updates

Four independent profiles: FM published/beta and BYTE published/beta. Their saved projects never cross profiles. Initial exact commits and SHA-256 are in `app/src/main/assets/engines.json`; their WASM files are included in assets. FM published V10.7.46, FM beta V10.7.47 preview, BYTE v0.7 (both channels initially).

When foregrounded, the app checks official HTTPS endpoints automatically (also every 15 minutes while open) or through **Buscar actualizaciones**. Public FM resolves `JIANT/main/web/EMULATOR_BETA.json`, Pages `emu/SOURCE_COMMIT` and `site-manifest.json`; BYTE resolves Pages `emu/build.json`, `firmware/versions.json` and the matching immutable catalogue WASM (or canonical `wasm_size` from newer build metadata). Transport HEAD/Content-Length is never the engine size authority; requests explicitly use identity encoding. Beta engines come from BYTE Pages `android/engines.json`; BYTE beta follows published when both catalogue channels match. A firmware-only FM beta without a corresponding WASM is reported as pending compatible emulator: `.fwsc` hardware images cannot run in this app. The catalogue publisher must compile and publish that WASM; the app does not compile a repository on the phone.

Downloads are HTTPS publisher-allowlisted, bounded, SHA-256 checked, and activated atomically the next time the profile opens. Running music is never hot-swapped. The previous motor and flash snapshot remain available for rollback; a boot error, timeout or interrupted trial restores them. Rejected versions are not retried repeatedly. SHA verification protects integrity against partial/inconsistent publications; publisher HTTPS is the trust anchor, not an independent signature scheme. WebView external network loads and navigations are disabled.

The Android shell itself updates by installing a newer APK with the same application ID and signing key, with Android's normal confirmation. This development APK uses the repository's public debug key, so it is suitable for testing, not a production trusted distribution. Uninstalling removes local projects. Switching engines, channels, or updating the APK normally preserves app data.

## Reproducible build

JDK 17, Android SDK 35, Gradle 8.9:

```
gradle -p android/emulator testDebugUnitTest assembleDebug
```

APK: `app/build/outputs/apk/debug/app-debug.apk`. `.github/workflows/android-emulator.yml` builds it and runs an Android 11 emulator with Wi-Fi/mobile data disabled. Instrumentation boots all bundled profiles, verifies audio and held/released keys, and rotates each. The upgrade fixture first installs the actual published 0.1 APK, saves firmware flash offline, reproduces its BYTE rejection against public HTTPS endpoints, installs 0.2 with `adb install -r`, verifies both BYTE channels download without changing active playback/flash, then opens the updated engines offline. After success, the workflow publishes the APK, checksums and screenshots under `/android/` on the existing Pages branch, preserving newer published engine catalogues and leaving firmware/installer/emulator paths unchanged. JVM tests cover corrupt files, deferred activation, persistent updates, separate profile storage, and crash rollback including flash restoration. Real S23 Ultra performance/audio latency still require testing on the phone.

Source/attribution: GPL-3.0, JIANT/Felucca, Juan Ignacio Antonucci, Leo Kuroshita / Hügelton Instruments and Charles Vestal. DotGothic16 is SIL OFL; licenses included in assets.

## Updating from 0.1

Install 0.2 over the existing app; **do not uninstall**. Same package and signing key preserve stored profiles and saved flash. Tap Buscar actualizaciones, then open each BYTE profile to apply its pending version. 0.1 compared gzip transport size with decompressed WASM and rejected valid BYTE downloads. 0.2 keeps size, WASM-format and SHA-256 checks, with specific failure reasons; errors never report all profiles up to date.
