#!/bin/sh
# Compila o DesignThumbs a partir do Linux com MinGW (pacote g++-mingw-w64-x86-64-posix).
# Usa os headers mais novos do mingw-w64 em third_party/mingw-w64-headers.
set -e
cd "$(dirname "$0")"
CXX=x86_64-w64-mingw32-g++
RC=x86_64-w64-mingw32-windres
FLAGS="-O2 -DUNICODE -D_UNICODE -static -s -ffunction-sections -fdata-sections -Wl,--gc-sections -Ithird_party/mingw-w64-headers"
$RC -c 65001 DesignThumbs.rc -O coff -o DesignThumbs.res.o
$CXX $FLAGS -shared -o DesignThumbs.dll DesignThumbs.cpp DesignThumbs.res.o DesignThumbs.def -lshlwapi -lwindowscodecs -ld2d1 -ldwrite -lole32 -luuid -lgdi32 -lruntimeobject -lshcore
# libwebp (só o codificador): compila uma vez em build/webp
mkdir -p build/webp
if [ ! -f build/webp/libwebp.a ]; then
  for f in third_party/libwebp/src/enc/*.c third_party/libwebp/src/dsp/*.c third_party/libwebp/src/utils/*.c third_party/libwebp/sharpyuv/*.c; do
    x86_64-w64-mingw32-gcc -O2 -Ithird_party/libwebp -DWEBP_USE_THREAD -c "$f" -o "build/webp/$(echo "$f" | tr / _).o"
  done
  x86_64-w64-mingw32-ar rcs build/webp/libwebp.a build/webp/*.o
fi
# Motor de PDF para PDF -> PPT: pdfium.dll (BSD-3/Apache-2.0) do wheel oficial pypdfium2 para Windows x64,
# baixado com SHA-256 fixo e embutido compactado no EXE (extraído só no primeiro uso).
PDFIUM_WHEEL=pypdfium2-5.15.2-py3-none-win_amd64.whl
PDFIUM_URL=https://files.pythonhosted.org/packages/fe/7f/8aa446529fa14523e5c18f89e823afc95526689f76a75efe68a82a10d850/$PDFIUM_WHEEL
PDFIUM_SHA256=74d1bace34fb01c660d0b64344d5a4b7d276942b7c6623c88ab59ea21c5acfbd
if [ ! -f build/pdfium.dll.gz ]; then
  mkdir -p build/pdfium
  [ -f "build/pdfium/$PDFIUM_WHEEL" ] || curl -fsSL --retry 4 -o "build/pdfium/$PDFIUM_WHEEL" "$PDFIUM_URL"
  echo "$PDFIUM_SHA256  build/pdfium/$PDFIUM_WHEEL" | sha256sum -c -
  unzip -oq "build/pdfium/$PDFIUM_WHEEL" pypdfium2_raw/pdfium.dll -d build/pdfium
  gzip -9n -c build/pdfium/pypdfium2_raw/pdfium.dll > build/pdfium.dll.gz
fi
$RC -c 65001 Vizu.rc -O coff -o Vizu.res.o # embute a DLL no EXE
$CXX $FLAGS -municode -mwindows -D_WIN32_WINNT=0x0A00 -Wl,-Map=build/Vizu.map -o Vizu.exe Vizu.cpp Vizu.res.o build/webp/libwebp.a -lshlwapi -lole32 -loleaut32 -luuid -lgdi32 -ldwmapi -lshell32 -ld2d1 -ldwrite -lwindowscodecs -lruntimeobject -lshcore -lwinhttp -lurlmon -lcomctl32 -lcomdlg32 -lxmllite -lbcrypt -lmscms -lpdh
# O próprio Vizu é o instalador. Ele leva a versão no nome: dist/Vizu_Setup_9.8.11_x64.exe
VERSION=$(sed -n 's/.*VALUE "FileVersion", "\([0-9.]*\)".*/\1/p' Vizu.rc)
mkdir -p dist
cp Vizu.exe "dist/Vizu_Setup_${VERSION}_x64.exe"
