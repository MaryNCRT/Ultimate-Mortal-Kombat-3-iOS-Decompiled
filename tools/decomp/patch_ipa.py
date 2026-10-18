"""
Parchea el binario de UMK3 dentro de una copia del IPA y lo reempaqueta.

El objetivo es neutralizar la inicializacion del SDK de EA lo justo para que la
app arranque en touchHLE, sin tocar nada de gamecode ni de lime.

NUNCA escribe en EXTRACTED\\ ni en IPA\\: trabaja siempre sobre una copia en
WORK\\.

Direcciones: se dan en VMADDR de la slice armv7. El offset dentro del fat es
    0x289000 + (vmaddr - 0x1000)
porque el segmento __TEXT de la slice empieza en vmaddr 0x1000 / offset 0.

Uso:
  python patch_ipa.py --list
  python patch_ipa.py --apply getProperty_asserts --out UMK3_1259_patched.ipa
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
# El IPA original vive en IPA\ y es SOLO LECTURA: se extrae a WORK\stage y se
# parchea la copia. Sobrescribible con UMK3_IPA para quien lo tenga en otro sitio.
SRC_IPA = os.environ.get(
    "UMK3_IPA",
    os.path.join(ROOT, "IPA", "UltimateMortalKombat3v1259.ipa"))
WORK = os.path.join(ROOT, "WORK")
# Where the patched IPA is written. touchHLE usually lives outside the repo.
APPS = os.environ.get("UMK3_APPS_DIR",
                      os.path.join(ROOT, "TouchHLE", "touchHLE_apps"))

ARMV7_FAT_OFFSET = 0x289000
TEXT_VMBASE = 0x1000

NOP16 = b"\x00\xbf"          # nop  (Thumb, 2 bytes)


# ---------------------------------------------------------------- builds
#
# Two retail builds exist and they are NOT interchangeable -- see
# docs/IPAD-BUILD.md. Their assets are byte-identical, but the iPhone binary is
# fat (armv6 + armv7) while the iPad one is thin armv7, so the armv7 slice sits
# at a different file offset in each.
#
# The iPhone build stays the default: it is newer (1.2.59 against 1.2.56) and it
# is the only one carrying the armv6 slice the project depends on for anything
# NEON-heavy.
BUILDS = {
    "iphone": {
        "ipa": "UltimateMortalKombat3v1259.ipa",
        "fat": True,
        "slice_offset": ARMV7_FAT_OFFSET,
        "out": "UMK3_1259_patched.ipa",
    },
    "ipad": {
        "ipa": "Ultimate_Mortal_Kombat__3_for_iPad_1.2.56_ios_3.2.ipa",
        "fat": False,
        "slice_offset": 0,
        "out": "UMK3_1256_ipad_patched.ipa",
    },
}


def vm_to_file(vmaddr, slice_offset=ARMV7_FAT_OFFSET):
    return slice_offset + (vmaddr - TEXT_VMBASE)


# --------------------------------------------------- symbol-relative patches
#
# The hardcoded addresses further down are iPhone 1.2.59. Applying the same
# fixes to another build means finding the same INSTRUCTION, not the same
# address -- every function in the iPad binary is relocated.
#
# So the patches that need to travel are expressed as (symbol, byte offset into
# the function, replacement, comment) and resolved against whichever binary is
# being patched. The offsets hold because both builds compile the same source:
# FE_Task_About_Eula has its `beq` at +0x0e in each.
SYMBOL_PATCHES = {
    "setlocale_nop": [
        ("__ZN13LocaleManager9setLocaleEPN4midp6StringE", 0,
         bytes.fromhex("7047"), "LocaleManager::setLocale -> bx lr"),
    ],
    "mp_disable": [
        ("_startMP", 0, bytes.fromhex("7047"), "startMP -> bx lr"),
    ],
    "eula_exit": [
        # cmp r0,#1 / beq -- forcing the branch skips the EULA gate. The high
        # byte of a 16-bit Thumb B<cond> carries the condition (0xD0 | cond);
        # 0xE0 makes it unconditional.
        ("_FE_Task_About_Eula", 0x0f, bytes.fromhex("e0"),
         "beq -> b (skip the EULA gate)"),
    ],
}


def resolve_symbol_patches(binpath, names, slice_offset, fat):
    """Turn SYMBOL_PATCHES entries into concrete (vmaddr, bytes, comment)."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from macho import MachO, read_file, N_STAB

    m = MachO(read_file(binpath), slice_offset if fat else 0)
    addr = {}
    for nm, t, _sec, _desc, val in m.symbols():
        if nm and not (t & N_STAB) and val:
            addr.setdefault(nm, val & ~1)

    out = []
    for name in names:
        for sym, delta, newbytes, comment in SYMBOL_PATCHES[name]:
            if sym not in addr:
                raise SystemExit("symbol not present in this build: %s" % sym)
            out.append((addr[sym] + delta, newbytes,
                        "%s  [%s+0x%x]" % (comment, sym, delta)))
    return out


# ---------------------------------------------------------------- parches
#
# Cada parche es una lista de (vmaddr, bytes_nuevos, comentario).

PATCHES = {}

# --- 1: neutralizar los asserts de midp::System::getProperty -------------
#
# getProperty hace [NSBundle mainBundle], recibe nil y dispara
#   SystemIPhone.mm:139  "mainBundle != null"
#
# En vez de tocar la llamada al assert (caer por ahi arrastra al codigo de
# desenrollado de excepciones), se anulan los SALTOS CONDICIONALES que llevan
# a cada assert. Asi la funcion sigue por su camino normal: mensajear a un
# objeto nil en Objective-C devuelve nil, que es justo lo que la funcion ya
# sabe tratar (la rama de 0x9d718 devuelve el String vacio).
# --- 0: neutralizar TODOS los asserts de un solo golpe -------------------
#
# El stub de ___assert_rtn en __symbol_stub4 es codigo ARM de 12 bytes:
#     e59fc000  ldr r12, [pc, #0]     ; r12 = slot del puntero perezoso
#     e59cf000  ldr pc,  [r12]        ; salta a la direccion enlazada
#     000f3acc  <direccion del slot>
#
# Sustituyendo la primera instruccion por "bx lr" (0xe12fff1e), cada
# "blx ___assert_rtn" vuelve de inmediato al llamante. Un solo parche de 4
# bytes neutraliza los 59 puntos de llamada, y ademas devuelve el control de
# forma limpia: no hay que preocuparse de caer dentro del codigo del assert
# ni del desenrollado de excepciones.
#
# Los 59 asserts estan TODOS en EA_SDK / eamtx (EASDK_Handler.mm 22,
# Mayhem.mm 21, LocaleManager.mm 7, SystemIPhone.mm 3, ReferenceCounted.cpp 3,
# JString.cpp 3). Ninguno en gamecode ni en lime: el juego no se toca.
PATCHES["assert_stub"] = [
    (0x000dd5cc, bytes.fromhex("1eff2fe1"), "stub ___assert_rtn -> bx lr (ARM)"),
]

# --- 2: forzar que el locale siempre se resuelva ------------------------
#
# Causa raiz del fallo de arranque en touchHLE:
#
#   LocaleManager::setLocale(String *locale) {
#       int i = getLocaleIndex(locale);
#       if (i != -1) { ...ok... }
#       assert(false);            // LocaleManager.mm:172
#   }
#
# touchHLE reporta los idiomas preferidos como codigos cortos ("es", "en"),
# y la lista de locales soportados de EA no los reconoce: getLocaleIndex
# devuelve -1 y salta el assert.
#
# Parche: getLocaleIndex devuelve siempre 0, es decir el primer idioma
# soportado. Cuatro bytes en el prologo de la funcion:
#     movs r0, #0    (0x2000)
#     bx   lr        (0x4770)
# OJO con el orden de los bytes: "bx lr" es el halfword 0x4770, que en
# little-endian son los bytes 70 47, o sea bytes.fromhex("7047"). Escribir
# "4770" mete los bytes 47 70 = halfword 0x7047 = "strb r7,[r0,#1]", que no
# retorna: la ejecucion sigue de largo hacia el cuerpo original de la funcion.
# Este parche lo tuvo mal hasta que se comprobo desensamblando el binario ya
# parcheado. Verifica siempre el resultado, no la intencion.
PATCHES["locale_index"] = [
    (0x0009e794, bytes.fromhex("00207047"),
     "getLocaleIndex -> return 0 (movs r0,#0; bx lr)"),
]

# --- 3: setLocale como no-op -------------------------------------------
#
# Variante mas conservadora que 'locale_index'. En vez de inventar un indice
# de locale (peligroso si la lista de soportados esta vacia), se anula
# setLocale entera: el LocaleManager se queda con el locale por defecto que
# le puso su constructor.
#   bx lr  (0x4770, Thumb)
PATCHES["setlocale_nop"] = [
    (0x0009e8c0, bytes.fromhex("7047"), "LocaleManager::setLocale -> bx lr"),
]

# --- 4: los dialogos modales no bloquean ------------------------------------
#
# Cualquier cuadro de confirmacion cierra touchHLE. La causa no esta en
# confirm: ni en ask:, sino dos niveles mas abajo: modalAlert.m tiene once
# metodos, y TODOS los que muestran un cuadro desembocan en uno de estos dos:
#
#   +[modalAlert infoWith:button1:]            0x000b5528   (un boton)
#   +[modalAlert queryWith:button1:button2:]   0x000b55f0   (dos botones)
#
# Los dos hacen lo mismo: crean un UIAlertView, llaman a CFRunLoopGetCurrent
# (stub 0x000dd164) y luego a CFRunLoopRun (stub 0x000dd170) para BLOQUEAR
# hasta que el usuario pulse. El delegado guarda el indice del boton y llama a
# CFRunLoopStop para desbloquear.
#
# touchHLE no implementa _CFRunLoopRun, asi que ahi se muere. Parchear los dos
# embudos cubre de golpe las seis entradas publicas (confirm:, ask:,
# askFull:textOK:textCANCEL:, inform:confirmation:, infoFull:textOK: y
# queryWith:).
#
# QUE DEVOLVER. Ambas funciones devuelven el INDICE DEL BOTON pulsado. Los
# wrappers booleanos comparten esta cola:
#     rsbs.w r0, r0, #1
#     it     lo
#     movlo  r0, #0
# o sea, indice 0 -> true, indice >=1 -> false. Y el orden lo fija la firma de
# askFull:textOK:textCANCEL:, que pasa textOK como button1: el boton 0 es OK y
# el 1 es CANCEL.
#
# Por eso NO se devuelve lo mismo en las dos:
#
#   queryWith (dos botones) -> 1  = CANCEL. Los wrappers dan false, o sea
#       "el usuario dijo que no". Asi un "¿Borrar el progreso de arcade?" se
#       declina solo en vez de ejecutarse. Devolver 0 aqui seria pulsar que SI
#       a cada dialogo destructivo.
#   infoWith (un boton)     -> 0  = el unico indice valido; equivale a
#       aceptar un aviso informativo, que es lo correcto.
#
# Efecto secundario deseable: como no se llega a crear el UIAlertView, el
# puntero global de "alerta actual" se queda a 0 y +[modalAlert isModalActive]
# sigue devolviendo falso, que es coherente con que no haya ningun modal.
#
# No se toca gamecode ni lime: modalAlert.m es capa iOS, de las 229 funciones
# que el port reescribe nativas de todas formas.
PATCHES["modal_nonblocking"] = [
    (0x000b5528, bytes.fromhex("00207047"),
     "+[modalAlert infoWith:button1:] -> return 0 (boton unico)"),
    (0x000b55f0, bytes.fromhex("01207047"),
     "+[modalAlert queryWith:button1:button2:] -> return 1 (CANCEL)"),
]

# --- 5: el multijugador no arranca ------------------------------------------
#
# El multijugador es GameKit peer-to-peer: la unica clase de GameKit que usa el
# binario es GKSession (Bluetooth / WiFi local; no hay Game Center ni
# matchmaking). touchHLE NO implementa GKSession -- ni una sola aparicion de esa
# cadena en su ejecutable -- asi que en cuanto el juego intenta crear la sesion
# se acaba la partida.
#
# El embudo es _startMP (0x000afe20), que mantiene el singleton de sesion:
#
#     ldr r4, [r3]        ; r4 = &g_mpSession
#     ldr r3, [r4]
#     cbz r3, crear       ; si es NULL, [[limeMPSession alloc] init]
#     pop {r4,r7,pc}      ; si ya existe, no hacer nada
#
# Anulando _startMP el singleton se queda a NULL para siempre. Eso NO revienta:
# en Objective-C mensajear a nil devuelve nil/0 sin fallar, asi que todas las
# consultas posteriores (mpIsWorking, mpGetConnectionState, isMPConnected...)
# contestan 0 y el juego toma su propia ruta de "no conectado" -- que ya existe,
# porque la clase tiene un noWifiAlertView para justamente ese caso.
#
# Se parchea la entrada y no la creacion porque la entrada es de 2 bytes y no
# hay nada apilado todavia: "bx lr" es un retorno limpio.
PATCHES["mp_disable"] = [
    (0x000afe20, bytes.fromhex("7047"),
     "_startMP -> bx lr (la sesion de GameKit nunca se crea)"),
]

# --- 6: la pantalla de licencia se puede cerrar -----------------------------
#
# _FE_Task_About_Eula (0x0000ecc8) son 28 bytes y NO crea ningun web view:
#
#     bl   BasicMenu
#     cmp  r0, #1
#     beq  salir          <- solo sale si BasicMenu devuelve exactamente 1
#     pop  {r7, pc}
#   salir:
#     bl   PopFETaskDeferred
#     b    pop
#
# Si BasicMenu devuelve cualquier otra cosa la tarea no se saca de la pila y la
# interfaz se queda encallada -- que es justo lo que se observa al entrar en
# "licencia de usuario" bajo touchHLE.
#
# Un solo byte: 0xD0 (beq) -> 0xE0 (b). La salida pasa a ser incondicional, asi
# que la pantalla siempre se puede cerrar. Se conserva el cmp para no mover
# nada de sitio; queda muerto y no molesta.
PATCHES["eula_exit"] = [
    (0x0000ecd7, bytes.fromhex("e0"),
     "_FE_Task_About_Eula: beq -> b (la salida deja de ser condicional)"),
]

PATCHES["getProperty_asserts"] = [
    (0x0009d5c8, NOP16 * 2, "beq.w -> assert 'key != null' (linea 95)"),
    (0x0009d606, NOP16 * 2, "beq.w -> assert 'mainBundle != null' (linea 139)"),
    (0x0009d610, NOP16 * 2, "beq.w -> assert 'keyRef != null' (linea 141)"),
]


def read_orig(data, vmaddr, n, slice_offset=ARMV7_FAT_OFFSET):
    off = vm_to_file(vmaddr, slice_offset)
    return data[off:off + n]


def apply_patches(data, patches, slice_offset=ARMV7_FAT_OFFSET):
    buf = bytearray(data)
    for vmaddr, newbytes, comment in patches:
        off = vm_to_file(vmaddr, slice_offset)
        old = bytes(buf[off:off + len(newbytes)])
        buf[off:off + len(newbytes)] = newbytes
        print("  0x%08x (fat 0x%08x): %s -> %s   %s"
              % (vmaddr, off, old.hex(), newbytes.hex(), comment))
    return bytes(buf)


def extract(src, dest):
    if os.path.isdir(dest):
        shutil.rmtree(dest)
    os.makedirs(dest)
    with zipfile.ZipFile(src) as z:
        z.extractall(dest)


def repack(srcdir, out):
    if os.path.exists(out):
        os.remove(out)
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=1) as z:
        for base, _dirs, files in os.walk(srcdir):
            for f in files:
                full = os.path.join(base, f)
                rel = os.path.relpath(full, srcdir).replace("\\", "/")
                z.write(full, rel)


def find_binary(appdir):
    for base, _d, files in os.walk(appdir):
        if base.endswith(".app"):
            for f in files:
                if f == "UMK3":
                    return os.path.join(base, f)
    raise SystemExit("no se encontro Payload/*.app/UMK3")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--apply", action="append", default=[])
    ap.add_argument("--build", default="iphone", choices=sorted(BUILDS))
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    build = BUILDS[args.build]
    src_ipa = os.environ.get("UMK3_IPA") if args.build == "iphone" else None
    # The IPAs are read-only sources and usually live outside the repo.
    # UMK3_IPA names one file (iPhone only, historical); UMK3_IPA_DIR names
    # the directory both builds sit in.
    ipa_dir = os.environ.get("UMK3_IPA_DIR", os.path.join(ROOT, "IPA"))
    src_ipa = src_ipa or os.path.join(ipa_dir, build["ipa"])
    out_name = args.out or build["out"]
    stage_dir = "stage" if args.build == "iphone" else "stage_" + args.build

    if args.list:
        for name, plist in PATCHES.items():
            print("%s  (%d parches)" % (name, len(plist)))
            for vmaddr, nb, c in plist:
                print("   0x%08x  %d bytes  %s" % (vmaddr, len(nb), c))
        return 0

    if not args.apply:
        raise SystemExit("indica --apply <nombre> o --list")

    os.makedirs(WORK, exist_ok=True)
    stage = os.path.join(WORK, stage_dir)
    print("build: %s   (%s)" % (args.build, build["ipa"]))
    print("extrayendo %s ..." % os.path.basename(src_ipa))
    extract(src_ipa, stage)

    binpath = find_binary(stage)
    with open(binpath, "rb") as fh:
        data = fh.read()
    print("binario: %s (%d bytes)" % (os.path.relpath(binpath, stage), len(data)))

    # comprobacion de cordura: el fat header debe estar donde esperamos
    magic = struct.unpack_from(">I", data, 0)[0]
    if build["fat"] and magic != 0xCAFEBABE:
        raise SystemExit("expected a fat binary, magic=0x%08x" % magic)
    if not build["fat"] and magic == 0xCAFEBABE:
        raise SystemExit("expected a thin binary but found a fat one")
    off = build["slice_offset"]

    todo = []
    for name in args.apply:
        if name in SYMBOL_PATCHES:
            # symbol-relative: resolves against whichever build is loaded
            plist = resolve_symbol_patches(binpath, [name], off, build["fat"])
        elif name in PATCHES:
            if args.build != "iphone":
                raise SystemExit(
                    "%s is defined by hardcoded iPhone addresses and has no "
                    "symbol-relative form; it would patch the wrong bytes "
                    "in the %s build." % (name, args.build))
            plist = PATCHES[name]
        else:
            raise SystemExit("parche desconocido: %s" % name)
        print("\naplicando '%s':" % name)
        data = apply_patches(data, plist, off)

    with open(binpath, "wb") as fh:
        fh.write(data)

    os.makedirs(APPS, exist_ok=True)
    out = os.path.join(APPS, out_name)
    print("\nreempaquetando -> %s" % out)
    repack(stage, out)
    print("listo: %.1f MB" % (os.path.getsize(out) / 1048576.0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
