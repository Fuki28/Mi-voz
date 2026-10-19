# MiVoz - plugin de voz (VST3) para Windows

Cadena: Low Cut -> EQ (limpiar medios) -> Compresor -> De-esser -> Presencia -> Aire -> Saturacion -> Delay -> Reverb -> Salida.
Boton "Asistente": cantas 6 segundos y ajusta solo el umbral del compresor y del de-esser.
No incluye autotune (usa el modulo Pitch de Nectar 4 antes o despues de MiVoz).

## Opcion A: compilar en la nube (sin instalar nada)

1. Crea una cuenta gratis en https://github.com y un repositorio PRIVADO nuevo.
2. Sube todo el contenido de esta carpeta (incluida la carpeta oculta `.github`).
3. Ve a la pestana **Actions** -> "Compilar MiVoz (Windows)" -> **Run workflow**.
4. Cuando termine (unos 10 a 15 min), abre la ejecucion y baja el artefacto **MiVoz-Windows**.
5. Descomprime y copia `MiVoz.vst3` a `C:\Program Files\Common Files\VST3\`.

## Opcion B: compilar en tu PC

1. Instala **Visual Studio 2022 Community** con la carga de trabajo "Desarrollo para el escritorio con C++".
2. Instala **CMake** (https://cmake.org/download/) y **Git**.
3. En esta carpeta, abre PowerShell y ejecuta:

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

La primera vez descarga JUCE solo (tarda unos minutos).
El plugin queda en `build\MiVoz_artefacts\Release\VST3\MiVoz.vst3` (carpeta).
Tambien se genera `Standalone\MiVoz.exe` para probarlo sin DAW.

## Usarlo en FL Studio

1. Copia `MiVoz.vst3` a `C:\Program Files\Common Files\VST3\` (necesita permisos de administrador).
2. En FL Studio: Options -> Manage plugins -> **Find installed plugins** (con "Verify plugins" activado).
3. Ponlo en un slot del mixer de tu voz.

## Orden recomendado en el mixer

Nectar 4 (solo modulo Pitch / autotune) -> MiVoz -> (opcional) Nectar para Voices/Backer.
