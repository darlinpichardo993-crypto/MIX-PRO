# Local build on Windows 10/11 with Visual Studio 2022 + CMake
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
Write-Host "VST3: build\MixPro_artefacts\Release\VST3\MIX PRO.vst3"
