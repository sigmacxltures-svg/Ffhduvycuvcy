name: Build
on: [push]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: nttld/setup-ndk@v1
        with:
          ndk-version: r25c
      - run: |
          mkdir build && cd build
          cmake .. \
            -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
            -DANDROID_ABI=arm64-v8a \
            -DANDROID_PLATFORM=android-29 \
            -DCMAKE_BUILD_TYPE=Release
          cmake --build . --target NullClient
      - uses: actions/upload-artifact@v3
        with:
          name: libNullClient
          path: build/libNullClient.so
