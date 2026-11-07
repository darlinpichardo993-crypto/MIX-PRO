# Arquitectura
```
Source/
  DSP/        Biquad (RBJ), Compressor, Limiter (look-ahead), DeEsser, MultibandComp (LR4 3 bandas),
              Saturator, TransientShaper, StereoDelay, Reverb (Freeverb), StereoProcessor
  Analysis/   FFT, SpectrumBands (16 bandas log), LoudnessMeter (BS.1770-4 + gating), TruePeak (4x),
              Fingerprint (JSON), ReferenceAnalyzer, MiniJson
  Engines/    Roles (25), Genres (34, editables), ParamTable (X-macro, ~55 parámetros), Effective (resolución de modo/rol/estilo/macros),
              MatchEngine (16 filtros peaking adaptativos), MixEngine (cadena completa + medidores), FactoryPresets (34)
  Plugin/     PluginProcessor (APVTS, estado, presets, referencia, render WAV), PluginEditor (10 pantallas)
Tests/        test_core.cpp (sin JUCE)
```
## Cadena de señal (por instancia)
IN gain → HPF → EQ 4 bandas (rol + estilo + género + macros + manual) → MATCH/AUTO-EQ (16 bandas) → compresor de rol → multibanda → glue → transitorios → saturación → de-esser → estéreo (ancho/balance/mono-low) → delay/reverb (desactivados por defecto; Ambience los habilita) → loudness match → OUT gain → limiter (solo Master) → dry/wet/bypass.
## Modos
- **AUTO MIX / VOCAL / INSTRUMENT**: cadena del rol + detección de acumulaciones/resonancias en tiempo real (recorte máx. 4.5 dB).
- **REFERENCE MATCH**: el espectro (16 bandas, normalizado) se acerca a la huella cargada; sin referencia cae a AUTO-EQ.
- **ERA MATCH**: forma de época (ERA 01–03, mezcla continua) como punto de partida; la referencia cargada tiene prioridad (*Reference Weight*).
- **GENRE MIX**: aplica el perfil de género (editable, exportable a JSON) sobre el rol.
- **MASTER MATCH**: EQ + match, multibanda, glue, saturación, estéreo, objetivo de loudness definido por el usuario (BS.1770 corto plazo, ajuste lento) y limiter con ceiling.
- **MANUAL**: base neutra; compresor manual absoluto; EQ como offsets.
## Garantías de audio
Sin asignaciones ni E/S en el hilo de audio (todo se reserva en `prepare`; la referencia se sincroniza con try-lock); procesamiento en trozos de 256 muestras con suavizado de parámetros; sanitización NaN/Inf en la salida; compatible con cualquier tamaño de buffer y sample rate.
## Lo que NO hace (limitaciones reales)
- No inspecciona ni modifica otros canales de FL Studio (no existe esa API para VST3). Cada instancia procesa su propio canal.
- No hay comunicación entre instancias: se comparten huellas/presets por archivos JSON.
- No hay clasificación de instrumentos por IA/ONNX (rol manual). No se afirma reproducir equipos ni estudios concretos.
- Estimaciones de reverb/eco/reflexiones son aproximaciones de envolvente (con nivel de confianza); las reflexiones tempranas no se estiman (requeriría respuesta al impulso).
- Delay y reverb permanecen en 0 % por defecto; los presets de fábrica son secos y Ambience es el control explícito de espacio. Referencias: WAV, AIFF, FLAC, OGG y MP3 (decodificación MP3 habilitada en JUCE).
- Saturación sin sobremuestreo; true peak medido con interpolación 4x de 12 taps.
- Los valores de los roles, géneros y épocas son puntos de partida heurísticos, no mediciones de grabaciones comerciales.
