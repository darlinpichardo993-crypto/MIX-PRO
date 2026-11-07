# Instalación en Windows y FL Studio 25
## Opción A — Instalador
Ejecuta `MIX_PRO_Setup.exe` (instala VST3 en `C:\Program Files\Common Files\VST3\`, Standalone, presets en `C:\ProgramData\ORYMY\MIX PRO\Presets`; no sobrescribe presets del usuario; desinstalable desde Windows).
## Opción B — Manual
Copia la carpeta `MIX PRO.vst3` a `C:\Program Files\Common Files\VST3\` (requiere permisos de administrador).
## FL Studio 25
1. *Options → Manage plugins → Find installed plugins* (activa *Verify plugins* si hace falta).
2. Busca **MIX PRO** y añádelo a un slot FX del Mixer (canal individual o bus).
3. Elige **Mode** y **Role**. Para ERA MATCH: pestaña *ERA MATCH* → *Load reference audio* (WAV/AIFF/FLAC legalmente obtenido) → mode *ERA MATCH* o *REFERENCE MATCH*, sube *Match Amount*.
4. Una instancia por canal (guitarra, bajo, bongó, güira, voz...). El estado, la referencia y las automatizaciones se guardan dentro del proyecto de FL Studio.
## Datos del usuario
`%APPDATA%\ORYMY\MIX PRO\` → `Presets\*.mixpro`, `GenreProfiles.json`. Las huellas de referencia (JSON) se pueden exportar/importar entre instancias y sesiones.
