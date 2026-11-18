#!/usr/bin/env bash
set -euo pipefail
mkdir -p android/engine-check/reports
trap 'adb logcat -d > android/engine-check/reports/logcat.txt; adb shell screencap -p /sdcard/fm-update-last.png; adb pull /sdcard/fm-update-last.png android/engine-check/reports/last-screen.png' EXIT
curl --fail --retry 3 -L https://juanjiant-bit.github.io/JIANT-BYTE/android/jiant-offline.apk -o /tmp/jiant-offline.apk
printf '%s\n' 'f5e780a52aca72df2bfadaab7f8cd94f4e152efe97222ce8d7e78c3e4c2084a7  /tmp/jiant-offline.apk' | sha256sum -c -
adb install -r /tmp/jiant-offline.apk
adb install -r android/emulator/app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk
run_test() {
  adb shell am instrument -w -r -e class "fm.jiant.emulator.FMUpdateTest#$1" fm.jiant.emulator.test/androidx.test.runner.AndroidJUnitRunner | tee "android/engine-check/reports/$1.txt"
  grep -q 'OK (1 test)' "android/engine-check/reports/$1.txt"
}
adb shell svc wifi disable
adb shell svc data disable
run_test seedInstalledProfilesOffline
adb shell svc wifi enable
sleep 10
run_test publishedClientDownloadsBothFmUpdates
adb shell svc wifi disable
run_test updatedFmEnginesPlayOffline
adb logcat -d -s JiantFMUpdate:I '*:S' > android/engine-check/reports/network.log
adb pull /sdcard/Pictures/JiantTest android/engine-check/reports/screenshots
