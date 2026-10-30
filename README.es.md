<div align="center">

<img src="docs/img/banner.jpg" alt="Ultimate Mortal Kombat 3 Recomp" width="880">

# Ultimate Mortal Kombat 3 — Decompilación de iOS y port nativo a PC

**Una decompilación en sala limpia de la versión de 2011 de *Ultimate Mortal Kombat 3* para iOS (v1.2.59): las 2.572 funciones del juego escritas en C y comprobadas contra el binario original, con un port nativo para Windows y Linux que se compila a partir de la copia del juego de cada jugador.**

[Primeros pasos](docs/GETTING-STARTED.md) · [Cómo funciona el juego](docs/HOW-THE-GAME-WORKS.md) · [Arquitectura](docs/ARCHITECTURE.md) · [Metodología](docs/METHODOLOGY.md) · [Progreso](docs/PROGRESS.md) · [Cambios](docs/CHANGELOG.md) · [English](README.md)

</div>

---

> [!IMPORTANT]
> **Este repositorio no distribuye binarios ni recursos con derechos de autor.**
> Para compilar y jugar necesitas tu propia copia legal de **Ultimate Mortal Kombat 3 para iOS (v1.2.59)** (`.ipa`, UUID `armv7` `90d6f56a18e2303f8f2053b02a42742e`). El launcher compila el juego a partir de ella en tu equipo.

## Qué es este proyecto

El juego de iOS es un port de 2011 de EA Mobile del arcade de Midway de 1995: el motor de combate de Midway (golpes, IA, combos, remates) dentro del motor 3D propio de EA, LIME. Este repositorio contiene:

1. **La decompilación** (`src/decomp/`): cada función del binario armv7 del juego escrita como C legible, cada una comprobada contra el binario (ver [Cómo se trabaja](#cómo-se-trabaja)).
2. **El port nativo** (`src/runtime/`): lo que el iPhone le daba al juego, escrito para PC — la ventana, el dibujo con OpenGL / Vulkan / Direct3D 12, el sonido, teclado y mandos, archivos, juego en línea — y funciones que el original no tenía (repeticiones, mods, suavizado, pantallas anchas).
3. **El launcher** (`launcher/`): un programa de escritorio que recibe tu `.ipa`, compila el juego, lo configura, lo actualiza y trae herramientas completas para mods (visores, reemplazo de texturas y modelos, editores al estilo Blender, exportación).

La única fuente de verdad es el binario. Nada entra al árbol si no se deriva de él y se comprueba contra él ([AGENTS.md](AGENTS.md) explica por qué, y [AI Disclosure](docs/AI-DISCLOSURE.md) quién hizo qué).

## Estado

| Componente | Progreso | Notas |
|---|---|---|
| **Decompilación** (`src/decomp/`) | **2.572 / 2.572 (100%)** | Motor 3D LIME (109), menús y escenas (291), motor de combate arcade (2.172). |
| **Verificación** (`tools/decomp/`) | **100%** | Tests diferenciales contra el oráculo ARM→C `armrecomp`; 229 tablas de datos comprobadas al compilar (`ctest`). |
| **Port a PC** (`src/runtime/`) | **Jugable — v0.1.15 beta** | OpenGL, Vulkan, Direct3D 12; Windows y Linux; 3:2 / 4:3 / 16:9 / 21:9; teclado y mandos; 2 jugadores local, LAN y en línea con rollback; repeticiones; mods. |
| **Launcher** (`launcher/`) | Windows y Linux | Compila el juego con un `zig cc` incluido (nada que instalar), se actualiza solo, herramientas de MODS. |

Cada versión la prueba la tester del proyecto, Mary, antes de que la siguiente se construya encima; el [registro de cambios](docs/CHANGELOG.md) dice en cada cambio si ya lo jugó.

## Cómo jugar

1. Descarga la última versión de la [página de Releases](https://github.com/MaryNCRT/Ultimate-Mortal-Kombat-3-iOS-Decompiled/releases): `UMK3-PC-<versión>.zip` (Windows) o `UMK3-PC-<versión>-linux-x64.tar.gz` (Linux).
2. Descomprímela donde quieras y abre **`UMK3-Launcher`**.
3. Dale tu `.ipa` (**COMPILAR TU .IPA**). Desempaqueta los datos del juego en la carpeta, extrae las tablas de datos del binario y compila el juego. Nada del juego sale de tu equipo.
4. **JUGAR.** Gráficos, controles, idioma y lo demás están en el launcher y en el menú de ajustes del propio juego (`Esc`).

Las versiones siguientes llegan por el actualizador del launcher. Tus mods, partidas, repeticiones y ajustes se quedan en la carpeta.

### Controles

| Acción | Jugador 1 (teclado) | Mando |
|---|---|---|
| Moverse | `W` `A` `S` `D` o flechas | Cruceta / stick izquierdo |
| Puñetazo alto / bajo | `U` / `I` (numérico `7` / `8`) | Oeste / Norte |
| Bloquear | `O` (numérico `9`) | Gatillo / bumper derecho |
| Patada alta / baja | `J` / `K` (numérico `4` / `5`) | Sur / Este |
| Correr | `L` (numérico `6`) | Gatillo / bumper izquierdo |
| Pausa / lista de golpes | `P` / `M` | Start / Back |
| Ajustes | `Esc` | configurable (`pad_settings`) |
| Menú de depuración (`debug_keys=1`) | `F2` (`F3` HUD, `F9`/`F10` KO, `F11`/`F12` partida) | `L3 + R3` |

Todos se pueden cambiar en el launcher (CONTROLES) o en el juego (`Esc`).

## Qué tiene el port

**El juego, como en el dispositivo.** Todos los modos del juego de iOS (torre arcade, VS, entrenamiento, supervivencia, karnage, tesoros, kodes), sus menús, HUD, remates, sonidos y música, desde el código decompilado. En 4:3 se usa la disposición de la versión de iPad.

**Añadido por el port:**
- **Renderizadores:** OpenGL, Vulkan y Direct3D 12 (`src/runtime/render/`), texturas HD, antialiasing, sombras en tiempo real, VSYNC, cachés de pipelines compiladas antes de jugar.
- **Pantallas:** 3:2, 4:3, 16:9 y 21:9; menús y paneles adaptados a cada una.
- **ANIMACIONES SUAVES** (cuadros entre los cuadros del juego) y **SUAVIZAR MODELOS** (el weld y la subdivisión Catmull-Clark de Blender, con control de bordes afilados).
- **ELEGIR ESCENARIO** antes de las peleas VS, de entrenamiento y en línea, con imagen de cada escenario; la música de la selección sigue sonando en él.
- **En línea** por LAN o internet (Epic Online Services), netcode con rollback y un retardo de entrada que sigue al ping; las peleas en línea usan siempre los datos del juego (de los mods solo modelos y texturas).
- **REPETICIONES:** cada pelea local se graba y se reproduce en el juego con un reproductor completo (pausa, cuadro a cuadro, salto, velocidad).
- **Mods:** una carpeta por mod en `mods\`; los archivos con el nombre de uno del juego lo reemplazan; datos de las tablas de combate (`data.ini`).
- **Menú de depuración** (`F2`): cualquier luchador incluidos los jefes, escenario, ronda, hitboxes, estados guardados.

**La sección MODS del launcher:**
- **GESTOR DE MODS:** orden, activar/desactivar, mod nuevo, abrir carpeta.
- **VISOR DE PERSONAJES:** cada luchador con sus trajes, modelos derivados, animaciones y un escenario detrás; reemplazo de texturas; **REEMPLAZAR MODELO** desde FBX, GLB u OBJ (rigs humanoides retargeteados, pesos pintados y suavizados, huesos con un gizmo al estilo Blender, shape keys con línea de tiempo); capas de animación correctivas; exportación para Blender.
- **VISOR DE ESCENARIOS:** un escenario como lo reproduce el motor, con sus efectos; elementos que se eligen, se aíslan y se editan como en Blender (mover / rotar / escalar, keyframes, una línea de tiempo tipo dope sheet, shape keys y esculpido), guardados por escenario y exportados como GLB, FBX o USD; sus texturas se ven y se reemplazan.
- **EDITOR DE PERSONAJES:** golpes, datos y textos de un luchador.
- **TEXTURAS:** todas las texturas del juego según a qué pertenecen, reemplazadas en un mod.
- **EXPORTAR:** luchadores, escenarios, modelos y texturas en FBX, GLB, USD (USDA/USDC/USDZ) u OBJ con texturas PNG.

## Problemas conocidos (0.1.15 beta)

- La intro de una pelea puede saltarse un cuadro cuando suavizar a los dos luchadores tarda más de un tick.
- Las amistades y animalities pueden repetir un grito de muerte (abierto: los logs muestran que el juego pide uno por remate).
- Todavía no se puede duplicar un luchador en un hueco nuevo.
- Perder contra Motaro desde el menú de depuración bloquea el juego (abierto, hace falta un log).
- ESCULPIR en el editor de escenarios dio una vez "500 Internal Error"; no se pudo reproducir y está protegido, la causa es desconocida.
- En un kombo, el golpeado retrocede un poco tras varios puñetazos (issue #114). Es una regla del propio juego original (`t_ccp3` → `t_separate_us`), comprobada instrucción por instrucción contra el binario.

## Compilar desde el código

Lo normal es usar el launcher. Sin él:

```bash
# desempaqueta y comprueba el .ipa, extrae las 229 tablas, compila umk3-game
# y prepara una carpeta lista para jugar en dist/<plataforma>-<arquitectura>/
python3 build.py ruta/a/UMK3_1.2.59.ipa

# el .exe de Windows compilado desde Linux, y ctest
python3 build.py ruta/a/UMK3_1.2.59.ipa --platform windows --target all

# la simulación de combate sin gráficos de 32 bits y las comprobaciones de tablas
python3 build.py ruta/a/UMK3_1.2.59.ipa --target headless
```

o con CMake directamente:

```bash
cmake -S . -B build -G Ninja -DUMK3_IPA=ruta/a/UMK3_1.2.59.ipa
cmake --build build --target umk3-game
```

La interfaz del launcher es una app SvelteKit que se compila con `npm run ui` en `launcher/app` (ver [Primeros pasos](docs/GETTING-STARTED.md)).

## Cómo se trabaja

- **El binario es la referencia.** Cada función se escribe desde su desensamblado (`tools/decomp/disasm_range.py`), nunca de memoria ni desde otras versiones del juego.
- **Cada función se comprueba.** `tools/decomp/armrecomp/recomp.py` traduce el código ARM a C de forma mecánica (el oráculo); el C escrito a mano se compara con él hecho a hecho (`factdiff.py`: escrituras, llamadas, manejadores) y ejecutando los dos con miles de entradas generadas (`tools/decomp/difftest/`). Las tablas de datos se comprueban al compilar.
- **Lo que las herramientas no alcanzan** (llamadas indirectas, tablas de saltos) se comprueba observando el juego en touchHLE ([parche de touchHLE](docs/engine/TOUCHHLE-PATCH.md)).
- **El código del port está marcado.** Todo lo que el binario no tiene lleva `Port, not in the binary` donde está.
- **La tester juega cada versión** antes de darla por buena; los problemas se rastrean hasta el mecanismo del propio binario antes de cambiar nada.

[Metodología](docs/METHODOLOGY.md) y [Verificación](docs/VERIFICATION.md) lo describen todo en detalle.

## Estructura del repositorio

```text
├── build.py              compilación en un paso desde un .ipa 1.2.59
├── CMakeLists.txt        umk3-game y umk3-fight-headless
├── src/
│   ├── decomp/           el C decompilado (2.572 funciones)
│   │   ├── lime/         motor 3D LIME (matrices, mallas, escenas, fuentes, cámara)
│   │   ├── gamecode/     menús, HUD, dibujo de escenarios y luchadores, sonido, multijugador
│   │   └── gamecode/logic/  el motor de combate arcade (golpes, IA, combos, remates)
│   ├── runtime/          el port a PC alrededor (ventana, renderizadores, audio, entrada,
│   │                     en línea, repeticiones, mods, menús añadidos por el port)
│   └── thirdparty/       el código de subdivisión de Blender (SUAVIZAR MODELOS)
├── tools/
│   ├── build/            extractores al compilar (.ipa, Mach-O, las 229 tablas)
│   └── decomp/           herramientas de ingeniería inversa (armrecomp, difftest, factdiff, desensamblador)
├── launcher/             el launcher (Electron + SvelteKit), actualizador, empaquetador de versiones
├── tests/                tests diferenciales de los módulos del motor LIME
└── docs/                 documentación (abajo)
```

## Documentación

- **Para empezar:** [Primeros pasos](docs/GETTING-STARTED.md) · [Cómo funciona el juego](docs/HOW-THE-GAME-WORKS.md) · [Arquitectura](docs/ARCHITECTURE.md)
- **Cómo se hace:** [Metodología](docs/METHODOLOGY.md) · [Verificación](docs/VERIFICATION.md) · [Contribuir](docs/CONTRIBUTING.md) · [AGENTS.md](AGENTS.md)
- **Dónde está:** [Progreso](docs/PROGRESS.md) (estado actual, problemas abiertos) · [Traspaso](docs/HANDOFF.md) (cómo retomar el trabajo) · [Cambios](docs/CHANGELOG.md) (cada versión) · [Objetivos](docs/GOALS.md)
- **Formatos y motor:** ver la lista completa en el [README en inglés](README.md#documentation) (`docs/formats/`, `docs/engine/`).

## Legal y créditos

*Ultimate Mortal Kombat 3* fue creado por **Midway Games** (1995) y llevado a iOS por **EA Mobile** (2011). Este proyecto es un trabajo independiente de ingeniería inversa en sala limpia con fines de interoperabilidad y preservación, sin relación con Warner Bros. Entertainment ni Electronic Arts. Aquí no se distribuye código ni recursos con derechos de autor.

El proyecto se publica bajo la [GNU GPL, versión 2 o posterior](LICENSE): contiene código de [Blender](https://www.blender.org) (`src/thirdparty/blender`, el weld y el Catmull-Clark de SUAVIZAR MODELOS). Las versiones hasta 0.1.9-beta eran MIT y lo siguen siendo. Renders del banner de [ermaccer](https://github.com/ermaccer) ([CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)); logo UHD de [u/JuananoLaGarza](https://www.reddit.com/r/MortalKombat/comments/mvm4uo/ultimate_mortal_kombat_3_logo_redone_in_uhd/).
