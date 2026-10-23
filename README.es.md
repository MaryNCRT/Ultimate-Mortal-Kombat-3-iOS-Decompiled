<div align="center">

<img src="docs/img/banner.jpg" alt="Ultimate Mortal Kombat 3 Recomp" width="880">

# Ultimate Mortal Kombat 3 — Decompilación de iOS y Port Nativo a PC

**Decompilación completa de sala limpia de la versión de iOS (2011) de *Ultimate Mortal Kombat 3* (v1.2.59) — las 2.572 funciones del juego transcritas a C y verificadas — con un port nativo a PC para Windows, Linux y macOS.**

[Primeros pasos](docs/GETTING-STARTED.md) · [Cómo funciona el juego](docs/HOW-THE-GAME-WORKS.md) · [Arquitectura](docs/ARCHITECTURE.md) · [Progreso](docs/PROGRESS.md) · [Historial de cambios](docs/CHANGELOG.md) · [English](README.md)

</div>

---

## Estado del Proyecto

| Componente | Progreso | Notas |
|---|---|---|
| **Decompilación (`src/decomp/`)** | **2.572 / 2.572 (100%)** | Motor 3D LIME (`109`), Front-End y Escenas (`291`), Motor de Combate Arcade (`2.172`). |
| **Verificación (`tools/decomp/`)** | **100%** | Tests diferenciales contra el oráculo ARM→C (`armrecomp`) + verificación de las 229 tablas de datos (`ctest`). |
| **Port Nativo (`src/runtime/`)** | **Jugable (v0.1.10b-beta)** | Renderizadores OpenGL, Vulkan y Direct3D 12, backends Win32 y SDL2, relaciones 3:2 / 4:3 / 16:9 / 21:9, texturas HD, teclado y mando, 2 jugadores local (`DUELO`), multijugador LAN/Online y menú de depuración (`F2`). |

> [!IMPORTANT]
> **Este repositorio no distribuye binarios ni recursos con derechos de autor.**
> Para compilar y jugar necesitas tu propia copia obtenida legalmente de **Ultimate Mortal Kombat 3 para iOS (v1.2.59)** (`.ipa`, slice `armv7` con UUID `90d6f56a18e2303f8f2053b02a42742e`).

---

## Compilación Rápida (desde tu `.ipa`)

### 1. Compilación en un solo comando (`build.py`)

Coloca tu archivo `Ultimate_Mortal_Kombat_3_1.2.59.ipa` en la raíz del repositorio (o pásale su ruta) y ejecuta:

```bash
# Valida y desempaqueta el .ipa 1.2.59, extrae las 229 tablas de datos del binario,
# compila umk3-game y genera la carpeta lista para jugar en dist/<plataforma>-<arch>/
python3 build.py ruta/a/UMK3_1.2.59.ipa

# Compilar el .exe de Windows desde Linux (funciona con Wine/Proton) y ejecutar ctest:
python3 build.py ruta/a/UMK3_1.2.59.ipa --platform windows --target all

# Compilar y ejecutar la simulación headless nativa de 32 bits y la suite de verificación:
python3 build.py ruta/a/UMK3_1.2.59.ipa --target headless
```

### 2. Compilación directa con CMake

```bash
cmake -S . -B build -G Ninja -DUMK3_IPA=ruta/a/UMK3_1.2.59.ipa
cmake --build build --target umk3-game
```

### 3. Launcher de Escritorio Multiplataforma (Windows y Linux)

En Windows y Linux puedes usar **`UMK3-Launcher`** (construido desde [`launcher/app/`](launcher/app/)), que provee una interfaz arcade autocontenida: recibe tu archivo `.ipa`, descarga automáticamente el compilador `zig cc` (sin requerir dependencias de compiladores en el sistema), desempaqueta los recursos, compila el juego nativo y permite configurar gráficos y mandos (`umk3.ini`). Su sección **MODS** administra mods (`mods/<mod>/`, los archivos con el nombre de uno del juego lo reemplazan; prioridad y activado en `mods/mods.ini`), muestra los luchadores, escenarios, modelos y texturas del juego, reemplaza cualquier textura de un luchador, un escenario, la interfaz o los efectos (TEXTURAS), y los exporta para Blender en FBX, USD (USDA/USDC/USDZ) u OBJ con texturas PNG, los escenarios con su animación; las exportaciones y los mods guardan cada luchador, escenario, la interfaz y los efectos en su propia carpeta.

Paquetes disponibles:
- **Linux:** `.tar.gz` portable y ejecutable `.AppImage`.
- **Windows:** `.zip` portable y ejecutable `.exe`.

---

## Controles

| Acción | Jugador 1 (Teclado) | Mando (Gamepad) |
|---|---|---|
| **Movimiento** | `W` `A` `S` `D` o Flechas | Cruceta (D-Pad) / Stick Izquierdo |
| **Puño Alto / Puño Bajo** | `U` / `I` (o Numpad `7` / `8`) | Oeste / Norte |
| **Bloqueo** | `O` (o Numpad `9`) | Botón / Gatillo Derecho (RB/RT) |
| **Patada Alta / Patada Baja** | `J` / `K` (o Numpad `4` / `5`) | Sur / Este |
| **Correr** | `L` (o Numpad `6`) | Botón / Gatillo Izquierdo (LB/LT) |
| **Pausa / Lista de Movimientos** | `P` / `M` | `Start` / `Back` (`Select`) |
| **Menú de Ajustes** | `Esc` | Configurable (`pad_settings`) |
| **Menú Debug** (`debug_keys=1`) | `F2` (`F3` HUD, `F9`/`F10` KO, `F11`/`F12` Pelea) | `L3 + R3` |

---

## Estructura del Repositorio

```text
├── build.py                 # Script unificado de compilación multiplataforma desde un .ipa 1.2.59
├── CMakeLists.txt           # Configuración CMake (umk3-game y umk3-fight-headless)
├── src/
│   ├── decomp/              # Código C decompilado 1:1 del binario armv7 de iOS (2.572 funciones)
│   │   ├── lime/            # Núcleo del motor 3D LIME (matrices, mallas, escenas, fuentes, cámara)
│   │   ├── gamecode/        # Menús front-end, HUD, renderizado 3D de escenarios/luchadores, audio, MP
│   │   └── gamecode/logic/  # Motor de combate arcade de Midway (golpes, IA, combos, finishers)
│   └── runtime/             # Capa del port nativo a PC alrededor del código decompilado
│       ├── platform/        # Ventanas, contexto OpenGL, audio e input del SO (Win32 y SDL2)
│       └── lime/            # Cargadores nativos de recursos (PVRTC, PNG, meshset, skin, scene, par)
├── tools/
│   ├── build/               # Extractores de compilación (desempaquetador .ipa, Mach-O, 229 tablas)
│   └── decomp/              # Herramientas de ingeniería inversa (armrecomp, difftest, desensamblador)
├── launcher/                # Launcher de escritorio en Electron, auto-actualizador y empaquetador
├── tests/                   # Arneses de verificación diferencial de módulos del motor LIME
└── docs/                    # Documentación técnica
    ├── formats/             # Especificaciones de formatos binarios (.meshset, .skin, .scene, .pvr)
    └── engine/              # Funcionamiento interno del motor, físicas, tablas de golpes, touchHLE
```

---

## Documentación

- **Guías Principales:** [Primeros pasos](docs/GETTING-STARTED.md) · [Cómo funciona el juego](docs/HOW-THE-GAME-WORKS.md) · [Arquitectura](docs/ARCHITECTURE.md) · [Metodología](docs/METHODOLOGY.md) · [Verificación](docs/VERIFICATION.md)
- **Formatos de Archivos (`docs/formats/`):** [MeshSet](docs/formats/MESHSET-FORMAT.md) · [Skin y Huesos](docs/formats/SKIN-FORMAT.md) · [Scene](docs/formats/SCENE-FORMAT.md) · [Events](docs/formats/EVENTS-FORMAT.md) · [PVR / PVRTC](docs/formats/PVR-FORMAT.md) · [Fuentes](docs/formats/FONT-FORMAT.md) · [Iluminación](docs/formats/LIGHTING.md) · [Streams de Animación](docs/formats/ANIMATION-STREAMS.md) · [Frame Lists](docs/formats/FRAMELISTS.md) · [Tablas X](docs/formats/X-TABLES.md)
- **Referencia del Motor (`docs/engine/`):** [Motor LIME](docs/engine/LIME-ENGINE.md) · [Físicas](docs/engine/PHYSICS.md) · [Tablas de Movimientos](docs/engine/MOVES-TABLES.md) · [Tablas de Fatalities](docs/engine/FATALITY-TABLES.md) · [Eventos del Juego](docs/engine/GAME-EVENTS.md) · [Roster](docs/engine/ROSTER.md) · [Escenarios](docs/engine/STAGES.md) · [Bugs del Original](docs/engine/GAME-BUGS.md) · [Contenido Oculto](docs/engine/HIDDEN-CONTENT.md) · [Versión iPad](docs/engine/IPAD-BUILD.md) · [Referencia MAME Arcade](docs/engine/MAME-ARCADE.md) · [Visor 3D de Mallas](docs/engine/MESH-VIEWER.md) · [Parche touchHLE](docs/engine/TOUCHHLE-PATCH.md)
- **Información del Proyecto:** [Progreso](docs/PROGRESS.md) · [Historial de cambios](docs/CHANGELOG.md) · [Contribuir](docs/CONTRIBUTING.md) · [Declaración de IA](docs/AI-DISCLOSURE.md)

---

## Legal y Créditos

*Ultimate Mortal Kombat 3* fue creado por **Midway Games** (1995) y portado a iOS por **EA Mobile** (2011). Este proyecto es un trabajo independiente de ingeniería inversa de sala limpia con fines de interoperabilidad y preservación de software, sin afiliación con Warner Bros. Entertainment ni Electronic Arts. No se distribuye código ni recursos con derechos de autor.

El proyecto se publica bajo la [GNU GPL, versión 2 o posterior](LICENSE): contiene código de [Blender](https://www.blender.org) (`src/thirdparty/blender`, el weld y el Catmull-Clark de SUAVIZAR MODELOS). Las versiones hasta la 0.1.9-beta eran MIT y lo siguen siendo. Renders del banner por [ermaccer](https://github.com/ermaccer) ([CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)); logotipo UHD por [u/JuananoLaGarza](https://www.reddit.com/r/MortalKombat/comments/mvm4uo/ultimate_mortal_kombat_3_logo_redone_in_uhd/).
