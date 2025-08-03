set src_path=%1
set proj_build_path=%2
set proj_arch=%3
set proj_type=%4
set imagemagick_dir=%5

if "%proj_arch%" equ "x86" (set libzip_proj_arch=Win32) else (set libzip_proj_arch=%proj_arch%)

set libzip_proj_arch

rem Copy FDK-AAC
copy /b %src_path%deps\fdk-aac\build\out\bin\*.dll %proj_build_path%

rem Copy LIBZIP and ZLIB
copy /b %src_path%deps\libzip\build-VS2022\%libzip_proj_arch%\%proj_type%\libzip.dll %proj_build_path%
copy /b %src_path%deps\zlib-win-build\build-VS2022\%libzip_proj_arch%\%proj_type%\libz.dll %proj_build_path%

rem Copy ImageMagick
copy /b "%imagemagick_dir%\*.dll" %proj_build_path%

rem Copy X264 and L-SMASH
copy /b %src_path%deps\x264\build\%proj_type%\libx264.dll %proj_build_path%
copy /b %src_path%deps\l-smash\%proj_arch%\%proj_type%\*.dll %proj_build_path%

rem Copy OGG libs (LibAudio)
copy /b %src_path%deps\miniaudio\build\external\ogg\%proj_type%\ogg.dll %proj_build_path%
copy /b %src_path%deps\miniaudio\build\external\vorbis\lib\%proj_type%\*.dll %proj_build_path%
