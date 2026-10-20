# MiVoz - plugin de voz (VST3) para Windows

Cadena: Low Cut -> Reduccion de ruido -> Gate -> Limpiar -> Compresor -> De-esser -> Presencia -> Aire -> Saturacion -> Reverb/Delay (con sidechain) -> Salida -> Limitador -> Mezcla.

- Grafica en vivo: espectro de tu voz, curva del EQ y ruido detectado.
- Boton Asistente: cantas 6 segundos y ajusta ruido, gate, compresor y de-esser.
- Presets de partida (menu de arriba), Bypass y Mezcla (dry/wet).
- Sidechain: baja reverb y delay mientras cantas. Si en FL Studio le envias otra pista al plugin (Sidechain to), usa esa pista como senal de control.
- No incluye autotune: usa el modulo Pitch de Nectar 4 antes de MiVoz.
- El plugin reporta 1024 muestras de latencia (el host las compensa solo).

## Compilar en la nube (sin instalar nada)

1. En tu repositorio de GitHub sube todo el contenido de esta carpeta (la carpeta `Source` y `CMakeLists.txt`; si ya existen, reemplazalos).
2. El workflow `.github/workflows/build.yml` ya compila solo al hacer commit. Ve a la pestana **Actions** y espera el check verde (10 a 15 min).
3. Abre la ejecucion, baja **MiVoz-Windows** (Artifacts), descomprime.
4. Copia `MiVoz.vst3` a `C:\Program Files\Common Files\VST3\`.
5. En FL Studio: Options -> Manage plugins -> **Find installed plugins**. Ponlo en el slot del mixer de tu voz.

Tambien se genera `MiVoz.exe` (version independiente) para probarlo sin DAW.

## Compilar en tu PC

Instala Visual Studio 2022 (carga de trabajo "Desarrollo para el escritorio con C++"), CMake y Git. En esta carpeta:

```
cmake -B build -A x64
cmake --build build --config Release
```

El plugin queda en `build\MiVoz_artefacts\Release\VST3\MiVoz.vst3`.

## Orden recomendado en el mixer

Nectar 4 (solo modulo Pitch / autotune) -> MiVoz.
