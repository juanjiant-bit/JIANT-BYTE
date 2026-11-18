# Published FM engine updates

The Android APK0.2 is unchanged. Its bundled FM46/47 profiles update to FM48/49 using the catalogue's immutable WASM downloads. `fm-catalog.json` records the exact source, byte size and SHA-256 of this publication; the live catalogue preserves BYTE channels independently.

- Public FM48: unmodified official `JIANT/emu/felucca.wasm`, checked against `JIANT/site-manifest.json` and `emu/SOURCE_COMMIT` (`a6d52c3`).
- Beta FM49: built from the official `JIANT/main/web/SPLIT_BETA.json` source `24d76c0`, not a renamed FM47 binary. Run `tools/build.py`'s `generate()` function, then `EMU_PREVIEW=1 sh web/emu/build.sh` with Emscripten. No firmware source modifications. ABI and Android shell audio/multitouch/rotation checked.

`android-engine-check.yml` installs the **published** APK0.2 (pinned SHA), saves actual firmware settings offline in the old FM46/47 profiles, calls that APK's real UpdateClient over public HTTPS, checks pending48/49 without changing saved flash, then opens and plays both updated profiles offline. It builds instrumentation only and never replaces the APK or publishes FM firmware.

On the phone: Buscar actualizaciones, then open each FM profile. Downloaded versions apply on open, never during playback. A newer firmware beta still needs a compatible WASM published in this catalogue; the app does not execute `.fwsc` hardware images or relabel an older emulator.
