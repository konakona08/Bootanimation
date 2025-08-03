# Prerequiresite setup

## 1. X264(VideoLAN)

1. Launch "x86_x64 Cross Tools Command Prompt for VS 2022"

2. c:\msys64\msys2_shell -> cd <PATH TO PROJECT>/deps/x264

3. ./configure --disable-avs --disable-swscale --disable-lavf --disable-ffms --disable-gpac --disable-asm --disable-opencl --disable-interlaced --enable-shared --enable-pic --disable-cli --disable-thread --bit-depth=8

4. cd <PATH TO PROJECT>\deps\x264 -> md build -> cd build

5. cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_INSTALL_PREFIX=./build -DBUILD_SHARED_LIBS=ON ..

6. cmake --build . --config Debug/Release

## 2. FDK-AAC

1. cd <PATH TO PROJECT>\deps\fdk-aac -> <DRIVE TO PROJECT> -> md build -> cd build

2. cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_INSTALL_PREFIX=./build -DBUILD_SHARED_LIBS=ON ..

3. cmake --build . --config Debug/Release --target install

## 3. L-Smash

1. Open L-SMASH.sln and build

## 4. libzip, zlib

1. Open solutions under build-VS2022 and build

## 5. ImageMagick

1. Install IM and change path of the install path in <PATH TO PROJECT>\bootanimation\bootanimation.props (variable IM_DIR)

## 6. MiniAudio

1. Clone ogg and vorbis into <PATH TO PROJECT>\deps\miniaudio\external

2. cd <PATH TO PROJECT>\deps\miniaudio -> <DRIVE TO PROJECT> -> md build -> cd build

3. cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_INSTALL_PREFIX=./build -DBUILD_SHARED_LIBS=ON ..

4. cmake --build . --config Debug/Release
