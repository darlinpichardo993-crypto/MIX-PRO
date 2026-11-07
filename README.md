# MIX PRO — Powered by ERA MATCH
**BY ORYMY · YMY RECORDS** — *RECREATE THE ERA. MATCH THE SOUND.*

Un solo producto (VST3 + Standalone, Windows 10/11 x64, FL Studio 25) con motores reales de DSP y análisis:
ERA MATCH, Auto Mix, Genre, Vocal, Instrument, Master Match, Reference Analyzer, Effects, Presets.

## Estado honesto del proyecto (v1.0.0)
| Parte | Estado |
|---|---|
| DSP, análisis, motores, presets, parámetros (`Source/DSP`, `Analysis`, `Engines`) | Implementado y **probado**: 74 comprobaciones pasan (`Tests/test_core.cpp`, ver `docs/TEST_REPORT.md`) |
| Plugin JUCE (procesador, estado, automatización, UI, presets, render WAV; importación de referencias MP3) | Implementado; **no se pudo compilar en el entorno donde fue escrito** (sin JUCE ni Windows). El primer run de GitHub Actions puede mostrar errores de compilación a corregir |
| VST3/Standalone/ZIPs/instalador | **No hay binarios en este repositorio.** Los genera el workflow `build-windows.yml` al compilar |
| Pruebas dentro de FL Studio 25 | No realizadas. Debes validarlas tú |

## Compilar (automático)
1. Sube este repo a GitHub (rama `main`).
2. *Actions → Build MIX PRO (Windows) → Run workflow*.
3. Descarga los artefactos: `MIX_PRO_VST3.zip`, `MIX_PRO_WINDOWS.zip`, `MIX_PRO_SOURCE.zip`, `MIX_PRO_Setup.exe`.
El workflow compila Release x64 (JUCE 8.0.4 fijado), ejecuta la suite de pruebas del núcleo, comprueba que el bundle VST3 y el ejecutable Standalone existen, genera el instalador Inno Setup y publica los artefactos. La validación dentro de FL Studio y con pluginval todavía debe realizarse por separado.

## Compilar en local
`Scripts/build-windows.ps1` (VS 2022 + CMake ≥ 3.22). Solo pruebas del núcleo, sin JUCE: `sh Scripts/build-core-tests.sh`.

## Uso en FL Studio 25
Ver `docs/INSTALL.md`. Resumen: instala en `C:\Program Files\Common Files\VST3\`, *Options → Manage plugins → Find plugins*, inserta MIX PRO en un canal/bus, elige **Mode** (arriba) y **Role** (instrumento).

## Arquitectura
Ver `docs/ARCHITECTURE.md`. Núcleo sin JUCE (`mixpro_core`, solo cabeceras C++17) + capa JUCE fina en `Source/Plugin`.

## Licencias
JUCE se descarga en tiempo de compilación (8.0.4). Distribuir el plugin compilado exige cumplir su licencia (GPLv3 o licencia comercial de JUCE). Ver `THIRD_PARTY_LICENSES.md`.
