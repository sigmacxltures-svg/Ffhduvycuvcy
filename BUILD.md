# NullClient — Build & Inject
## Steal a Monke (LarpLogan) | arm64-v8a | Meta Quest

---

## 1. Dump offsets (do after every game update)

```bash
adb shell pm path com.LarpLogan.StealAMonke
adb pull <path>/base.apk SAM.apk
apktool d SAM.apk -o SAM_out/

# run il2cppdumper
Il2CppDumper SAM_out/lib/arm64-v8a/libil2cpp.so \
             SAM_out/assets/bin/Data/Managed/Metadata/global-metadata.dat \
             ./dump/

# grep your offsets
grep -n "GetMoveSpeed\|AddCoins\|CheckCollision\|PlayerUpdate\|RenderFog\|FixedUpdate\|OVRInput" dump/dump.cs
```

Paste updated values into `include/il2cpp_helpers.h` OFFSET_ constants.

---

## 2. Build

```bash
export NDK=/path/to/android-ndk-r25c

mkdir build && cd build
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-29 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build . --target NullClient
# output: build/libNullClient.so
```

---

## 3. Inject — no smali edit, no const-string needed

The `__attribute__((constructor))` in main.cpp fires automatically
when the linker loads the .so. Just drop it in the APK lib folder:

```bash
# copy into decompiled APK
cp build/libNullClient.so SAM_out/lib/arm64-v8a/libNullClient.so

# make sure AndroidManifest.xml has:
#   android:extractNativeLibs="true"
# (apktool preserves this; most Unity APKs already have it)

# repack + sign
apktool b SAM_out/ -o SAM_mod.apk
java -jar uber-apk-signer.jar --apks SAM_mod.apk

# install
adb install -r SAM_mod_aligned_signed.apk
```

---

## 4. Verify

```bash
adb logcat -s NullClient
# NullClient loaded — constructor fired
# il2cpp base = 0x7XXXXXXXXX
# hook 0xB4A210 OK
# ... (7 hooks)
```

---

## 5. In-game

| Action | Result |
|--------|--------|
| Press **B** (left controller) | Limit Menu opens/closes |
| Point right controller + trigger | Navigate categories / toggle features |

| Category | Feature | Effect |
|----------|---------|--------|
| Movement | Speed Boost | ×4.5 velocity |
| Movement | No Clip | geometry collision off |
| Movement | Fly Mode | gravity zeroed |
| Economy | Infinite Coins | balance → INT_MAX |
| Economy | Auto Steal | auto-steals within 2 m |
| Economy | ×10 Coins | coin delta ×10 |
| Visual | ESP | blue boxes through walls |
| Visual | No Fog | fog draw call skipped |
