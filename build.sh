#!/bin/sh
# Compila o DesignThumbs a partir do Linux com MinGW (pacote g++-mingw-w64-x86-64-posix).
# Usa os headers mais novos do mingw-w64 em third_party/mingw-w64-headers.
# v19.5: adiciona engines/ (VizuVector, VizuPeek, VizuIDML)
set -e
cd "$(dirname "$0")"
CXX=x86_64-w64-mingw32-g++
RC=x86_64-w64-mingw32-windres
FLAGS="-O2 -flto -march=x86-64-v2 -DUNICODE -D_UNICODE -static -s -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,-O1 -Ithird_party/mingw-w64-headers -std=c++20 -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wunused-function -Wunused-variable -Wunused-but-set-variable"
$RC -c 65001 DesignThumbs.rc -O coff -o DesignThumbs.res.o
$CXX $FLAGS -shared -o DesignThumbs.dll DesignThumbs.cpp DesignThumbs.res.o DesignThumbs.def -lshlwapi -lwindowscodecs -ld2d1 -ldwrite -lole32 -luuid -lgdi32 -lruntimeobject -lshcore
# libwebp (só o codificador): compila uma vez em build/webp
mkdir -p build/webp
if [ ! -f build/webp/libwebp.a ]; then
  for f in third_party/libwebp/src/enc/*.c third_party/libwebp/src/dsp/*.c third_party/libwebp/src/utils/*.c third_party/libwebp/sharpyuv/*.c; do
    x86_64-w64-mingw32-gcc -O2 -flto -march=x86-64-v2 -Ithird_party/libwebp -DWEBP_USE_THREAD -c "$f" -o "build/webp/$(echo "$f" | tr / _).o"
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
# Leitor de CorelDRAW (CDR/CMX) sem Inkscape: libcdr e librevenge (MPL 2.0, sem modificação), Little CMS (MIT),
# zlib e Boost (só cabeçalhos). Fontes oficiais com SHA-256 fixo, compiladas uma vez em build/cdr/libvizucdr.a.
# O ICU vem do Windows (icu.dll), por meio de cdr/icu-shim.
CDR_SRC=build/cdr-src
mkdir -p $CDR_SRC
fetch() { # url sha256
  f="$CDR_SRC/$(basename "$1")"
  [ -f "$f" ] || curl -fsSL --retry 4 -o "$f" "$1"
  echo "$2  $f" | sha256sum -c - >/dev/null
}
fetch https://dev-www.libreoffice.org/src/libcdr/libcdr-0.1.9.tar.xz f7bb6abdd7f226820f288a93dd8d07759833c0250d9e202af90f9b312c4665a3
fetch https://dev-www.libreoffice.org/src/librevenge-0.0.6.tar.bz2 52a65e904d255dbdd97a8b7bb28d6574e14f999eb01416aff004502406d0904d
fetch https://github.com/mm2/Little-CMS/releases/download/lcms2.16/lcms2-2.16.tar.gz d873d34ad8b9b4cea010631f1a6228d2087475e4dc5e763eb81acc23d9d45a51
fetch https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz 9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
for t in libcdr-0.1.9.tar.xz librevenge-0.0.6.tar.bz2 lcms2-2.16.tar.gz zlib-1.3.1.tar.gz; do # cabeçalhos também para o Vizu.cpp
  d="$CDR_SRC/${t%.tar.*}"; [ -d "$d" ] || tar -xf "$CDR_SRC/$t" -C $CDR_SRC
done
if [ ! -f build/cdr/libvizucdr.a ]; then
  mkdir -p build/cdr/obj
  fetch https://archives.boost.io/release/1.86.0/source/boost_1_86_0.tar.bz2 1bed88e40401b2cb7a1f76d4bab499e352fa4d0c5f31c0dbae64e24d34d7513b
  [ -d $CDR_SRC/boost_1_86_0/boost ] || tar -xjf $CDR_SRC/boost_1_86_0.tar.bz2 -C $CDR_SRC boost_1_86_0/boost boost_1_86_0/LICENSE_1_0.txt
  CDR_INC="-Icdr/icu-shim -I$CDR_SRC/libcdr-0.1.9/inc -I$CDR_SRC/libcdr-0.1.9/src/lib -I$CDR_SRC/librevenge-0.0.6/inc -I$CDR_SRC/librevenge-0.0.6/src/lib -I$CDR_SRC/lcms2-2.16/include -I$CDR_SRC/zlib-1.3.1 -I$CDR_SRC/boost_1_86_0"
  CDR_DEF="-DNDEBUG -DBOOST_ALL_NO_LIB -D_USE_MATH_DEFINES"
  for f in $CDR_SRC/lcms2-2.16/src/*.c $CDR_SRC/zlib-1.3.1/*.c; do echo "$f"; done | xargs -P "$(nproc)" -I{} sh -c 'x86_64-w64-mingw32-gcc -O2 -flto -march=x86-64-v2 -w '"$CDR_INC $CDR_DEF"' -c "{}" -o "build/cdr/obj/$(basename {} .c)_c.o"'
  ls $CDR_SRC/libcdr-0.1.9/src/lib/*.cpp $CDR_SRC/librevenge-0.0.6/src/lib/*.cpp cdr/icu-shim/icu_shim.cpp | grep -v RVNGDirectoryStream | \
    xargs -P "$(nproc)" -I{} sh -c 'x86_64-w64-mingw32-g++ -std=c++17 -O2 -flto -march=x86-64-v2 -w '"$CDR_INC $CDR_DEF"' -c "{}" -o "build/cdr/obj/$(basename {} .cpp).o"'
  x86_64-w64-mingw32-ar rcs build/cdr/libvizucdr.a build/cdr/obj/*.o
  rm -rf build/cdr/obj
fi
CDR_FLAGS="-I$CDR_SRC/libcdr-0.1.9/inc -I$CDR_SRC/librevenge-0.0.6/inc -I$CDR_SRC/zlib-1.3.1"
# Compilar engines (VizuVector: SvgWriter, EpsWriter, PdfWriter; VizuPeek; VizuIDML)
ENGINE_SOURCES="engines/SvgWriter.h engines/EpsWriter.h engines/PdfWriter.h engines/Peek.h engines/Peek.cpp engines/IdmlWriter.h engines/VectorSink.cpp"
$RC -c 65001 Vizu.rc -O coff -o Vizu.res.o # embute a DLL no EXE
$CXX $FLAGS -municode -mwindows -D_WIN32_WINNT=0x0A00 -Wl,-Map=build/Vizu.map $CDR_FLAGS -o Vizu.exe Vizu.cpp Vizu.res.o build/webp/libwebp.a build/cdr/libvizucdr.a -lshlwapi -lole32 -loleaut32 -luuid -lgdi32 -ldwmapi -lshell32 -ld2d1 -ldwrite -lwindowscodecs -lruntimeobject -lshcore -lwinhttp -lcrypt32 -lurlmon -lcomctl32 -lcomdlg32 -lxmllite -lbcrypt -lmscms -lpdh -lsetupapi -lwinmm
# O próprio Vizu é o instalador. Ele leva a versão no nome: dist/Vizu_Setup_19.6.0_x64.exe
VERSION=$(sed -n 's/.*VALUE "FileVersion", "\([0-9.]*\)".*/\1/p' Vizu.rc)
mkdir -p dist
cp Vizu.exe "dist/Vizu_Setup_${VERSION}_x64.exe"
