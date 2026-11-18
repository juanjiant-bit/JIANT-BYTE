#!/usr/bin/env bash
set -euo pipefail
app=android/emulator/app/build/outputs/apk/debug/app-debug.apk
test=android/emulator/app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk
mkdir -p android/emulator/upgrade-reports
curl --fail --retry 3 -L https://raw.githubusercontent.com/juanjiant-bit/JIANT-BYTE/0da2926fd085555c8f07d0c9f4ab1c2d8302c5b8/android/jiant-offline.apk -o /tmp/jiant-0.1.apk
printf '%s\n' 'e4da397d08bed27df3bb773f0893b80e6ae20577b37f91b3a34369487016b8a4  /tmp/jiant-0.1.apk' | sha256sum -c -
run_test() {
  adb shell am instrument -w -r -e class "fm.jiant.emulator.UpdateNetworkTest#$1" fm.jiant.emulator.test/androidx.test.runner.AndroidJUnitRunner | tee "android/emulator/upgrade-reports/$1.txt"
  grep -q 'OK (1 test)' "android/emulator/upgrade-reports/$1.txt"
}
adb shell svc wifi disable
adb shell svc data disable
adb install -r /tmp/jiant-0.1.apk
adb install -r "$test"
run_test seedLegacyOffline
adb shell svc wifi enable
sleep 10
run_test reproduceLegacyNetworkFailure
adb install -r "$app"
run_test updateOverNetworkAfterApkUpgrade
adb shell svc wifi disable
adb shell svc data disable
run_test updatedEngineOpensOfflineWithSavedFlash
adb logcat -d -s JiantUpgrade:I '*:S' > android/emulator/upgrade-reports/network.log
# Clean first-install offline run independently from the upgrade fixture.
adb shell pm clear fm.jiant.emulator
gradle -p android/emulator connectedDebugAndroidTest -Pandroid.testInstrumentationRunnerArguments.class=fm.jiant.emulator.OfflineTest --no-daemon
adb pull /sdcard/Pictures/JiantTest android/emulator/screenshots
