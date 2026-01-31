# Bootanimation

Tool used to convert bootanimation.zip (and optional bootaudio.mp3) into a single video (bootanimation.mp4).

# Usage examples

## 1. Just animation
```
bootanimation -anim <PATH TO bootanimation.zip> -out <OUTPUT FILE PATH>
```

## 2. Animation + audio
```
bootanimation -anim <PATH TO bootanimation.zip> -audio <PATH TO bootaudio.mp3> -out <OUTPUT FILE PATH>
```

## 3. Animation + dynamic coloring (end colors)
NOTE: Color input is signed 32-bit integer (in hex the values would be (A<<24)|(R<<16)|(G<<8)|B))
```
bootanimation -anim <PATH TO bootanimation.zip> -dynamic <R color> <G color> <B color> <A color> -out <OUTPUT FILE PATH>
```

# Parameters description:

```
-anim: Path to animation zip file
-audio: Path to audio file played in animation.zip
-dynamic: R G B and A mask colors to apply during and end of dynamic color change
-out: Output file path (by default it would be <current cmd path>\bootanimation.mp4)
```

# Dependencies

The project depends on a copy of ImageMagick for building. The variant used is 8-bit quantum.

# What it can't do at the moment

1. Fade effect in animation (don't have an example to implement)

2. Clock and progress in bootanimation (and probably will never be done)

3. Play till completion

4. Samsung/Quram QMG decode support (will implement later under Dynarmic emulation)

5. Pantech SKY LZ frames format (will implement later)

# License(s)

At this moment the project does not have a license. I planned to license it under Apache-2.0 but x264 and FDK-AAC licenses have incompatibilities with Apache-2.0.

Its dependencies are under the following licenses:

1. ImageMagick - [ImageMagick license](LICENSE.IMAGEMAGICK)

2. VideoLAN X264 (modified for Visual Studio building) - [GPL 2.0 or later](LICENSE.X264)

3. FDK-AAC - [Fraunhofer license](LICENSE.FDKAAC)

4. libzip (modified for Visual Studio building) - [BSD 3-clause](LICENSE.LIBZIP)

5. miniaudio - [MIT No Attribution](LICENSE.MINIAUDIO)

6. OGG and Vorbis - [BSD 3-Clause](LICENSE.OGGVORBIS)

7. zlib (modified for Visual Studio building) - [MIT License](LICENSE.ZLIBWIN), [ZLIB license](LICENSE.ZLIB)

8. L-SMASH (modified to build under Visual Studio 2022) - [ISC](LICENSE.LSMASH)

9. AVIR - [MIT License](LICENSE.AVIR)