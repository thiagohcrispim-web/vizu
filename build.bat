@echo off
REM Build script para Vizu no Windows (Visual Studio 2022)
REM Uso: build.bat [clean|release|debug]

cd /d "%~dp0"

set CONFIG=Release
set CLEAN=0

for %%a in (%*) do (
    if /i "%%a"=="clean" set CLEAN=1
    if /i "%%a"=="debug" set CONFIG=Debug
    if /i "%%a"=="release" set CONFIG=Release
)

if %CLEAN%==1 (
    echo Limpando build anterior...
    rmdir /s /q build 2>nul
    rmdir /s /q dist 2>nul
)

echo Configurando CMake (%CONFIG%)...
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=%CONFIG% -DCMAKE_INSTALL_PREFIX=install ^
    -DVIZU_USE_VCPKG=ON ^
    -DCMAKE_TOOLCHAIN_FILE=%CD%\vcpkg\scripts\buildsystems\vcpkg.cmake ^
    -DVCPKG_TARGET_TRIPLET=x64-windows-static-md

if errorlevel 1 (
    echo ERRO: CMake falhou. Verifique se vcpkg esta instalado e as dependencias estao disponiveis.
    exit /b 1
)

echo Compilando (%CONFIG%)...
cmake --build build --config %CONFIG% --parallel

if errorlevel 1 (
    echo ERRO: Build falhou.
    exit /b 1
)

echo.
echo Build concluido com sucesso!
echo.
echo Executavel: build\%CONFIG%\Vizu.exe
echo.

if exist "build\%CONFIG%\Vizu.exe" (
    echo Para testar: build\%CONFIG%\Vizu.exe
)

REM Gerar instalador (opcional)
echo.
echo Para gerar o instalador NSIS:
echo   cmake --build build --config %CONFIG% --target package
echo.
echo Instalador sera gerado em: dist\Vizu_Setup_9.14.5_x64.exe