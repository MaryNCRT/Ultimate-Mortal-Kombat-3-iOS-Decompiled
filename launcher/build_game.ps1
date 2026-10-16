# Builds umk3-game.exe from the player's own UMK3 .ipa.
#
#   powershell -ExecutionPolicy Bypass -File launcher\build_game.ps1 -Ipa <file.ipa> [-Arch x86|x64]
#
# -Arch x86 (the default) builds umk3-game.exe, 32 bits; -Arch x64 builds
# umk3-game64.exe, 64 bits. Both can sit side by side; the launcher's
# arch= in umk3.ini picks which one it builds and plays.
#
# UMK3-Launcher.exe runs this when the player picks an .ipa and presses
# "Compilar". Everything lands in the folder above launcher\ (the release or
# repository root), and the game reads nothing outside it:
#
#   umk3-game.exe   compiled here, with the data tables read out of the .ipa
#   res\            the .ipa's Payload/UMK3.app/res, plus its Info.plist
#   umk3.ini        the launcher's settings (left alone if it exists)
#   toolchain\      the compiler and Python, downloaded once (can be deleted)
#   build-game\     intermediate files (can be deleted)
#
# No game data comes with the port: the tables the engine runs on are
# extracted from the player's binary by tools/logic_tables.py, level_info.py
# and seq_data.py at this step, exactly as the CMake build does.
param(
    [Parameter(Mandatory = $true)][string]$Ipa,
    [ValidateSet('x86', 'x64')][string]$Arch = 'x86'
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'   # Invoke-WebRequest is 10x slower with it
Add-Type -AssemblyName System.IO.Compression.FileSystem

$Root  = Split-Path -Parent $PSScriptRoot
$Tools = Join-Path $Root 'toolchain'
$Work  = Join-Path $Root 'build-game'

# Pinned downloads: official releases, checked before use.
$LlvmName = 'llvm-mingw-20260616-ucrt-x86_64'
$LlvmUrl  = "https://github.com/mstorsjo/llvm-mingw/releases/download/20260616/$LlvmName.zip"
$LlvmSha  = 'b9b68a4d276e16fa25802aaba458e4638f64b3884c290aaccdc2d87083b6ca35'
$PyName   = 'python-3.12.10-embed-amd64'
$PyUrl    = "https://www.python.org/ftp/python/3.12.10/$PyName.zip"
$PyMd5    = 'fe8ef205f2e9c3ba44d0cf9954e1abd3'     # python.org lists MD5 only

# The binary every address in decomp/ was read from (Payload/UMK3.app/UMK3,
# armv7 slice, version 1.2.59).
$WantUuid = '90d6f56a18e2303f8f2053b02a42742e'

function Step($msg) { Write-Host ""; Write-Host "== $msg" -ForegroundColor Cyan }
function Fail($msg) { Write-Host ""; Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }

function Fetch($url, $out, $algo, $hash) {
    if (Test-Path $out) {
        if ((Get-FileHash $out -Algorithm $algo).Hash -eq $hash.ToUpper()) { return }
        Remove-Item $out -Force
    }
    Write-Host "Descargando $url"
    $tmp = "$out.part"
    & "$env:SystemRoot\System32\curl.exe" --progress-bar -L --fail --retry 3 -o $tmp $url
    if ($LASTEXITCODE -ne 0) { Fail "no se pudo descargar $url" }
    if ((Get-FileHash $tmp -Algorithm $algo).Hash -ne $hash.ToUpper()) {
        Remove-Item $tmp -Force
        Fail "el archivo descargado no coincide con el $algo esperado: $url"
    }
    Move-Item $tmp $out -Force
}

function Unzip($zip, $dest) {
    New-Item -ItemType Directory -Force -Path $dest | Out-Null
    & "$env:SystemRoot\System32\tar.exe" -xf $zip -C $dest
    if ($LASTEXITCODE -ne 0) { Fail "no se pudo descomprimir $zip" }
}

$Ipa = (Resolve-Path $Ipa).Path
New-Item -ItemType Directory -Force -Path $Tools, $Work | Out-Null

# --- 1. The toolchain -------------------------------------------------------
Step "1/5 Compilador (llvm-mingw) y Python"
$llvm = Join-Path $Tools $LlvmName
# The 64-bit build (Port, not in the binary): the game's structures keep
# their 32-bit pointer words, so it is built with UMK3_LOW32, every pointer
# the game stores is a __ptr32 (P32 in runtime/savestate.h), and the exe
# and its heaps live below 2 GB.
$triple = if ($Arch -eq 'x64') { 'x86_64-w64-mingw32' } else { 'i686-w64-mingw32' }
$cc   = Join-Path $llvm "bin\$triple-clang.exe"
if (-not (Test-Path $cc)) {
    $z = Join-Path $Tools "$LlvmName.zip"
    Fetch $LlvmUrl $z 'SHA256' $LlvmSha
    Unzip $z $Tools
    Remove-Item $z -Force
}
$pydir = Join-Path $Tools $PyName
$py    = Join-Path $pydir 'python.exe'
if (-not (Test-Path $py)) {
    $z = Join-Path $Tools "$PyName.zip"
    Fetch $PyUrl $z 'MD5' $PyMd5
    Unzip $z $pydir
    Remove-Item $z -Force
}
Write-Host "listo"

# --- 2. The binary out of the .ipa ------------------------------------------
Step "2/5 Leyendo el binario de tu .ipa"
$zip = [IO.Compression.ZipFile]::OpenRead($Ipa)
try {
    $entry = $zip.Entries | Where-Object { $_.FullName -match '^Payload/[^/]+\.app/UMK3$' } |
             Select-Object -First 1
    if (-not $entry) { Fail "$Ipa no contiene Payload/UMK3.app/UMK3: no es el .ipa de UMK3" }
    $bin = Join-Path $Work 'UMK3'
    [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $bin, $true)
} finally {
    $zip.Dispose()
}
$check = Join-Path $Root 'launcher\check_binary.py'
& $py -I $check $bin $WantUuid
if ($LASTEXITCODE -ne 0) { Fail "este .ipa no es la version compatible (UMK3 1.2.59 para iPhone)" }

# --- 3. The data tables -----------------------------------------------------
Step "3/5 Extrayendo las tablas del juego de tu binario"
$t = Join-Path $Root 'tools'
& $py -I (Join-Path $t 'logic_tables.py') $bin (Join-Path $Work 'logic_tables.c') --report (Join-Path $Work 'logic_tables_report.txt')
if ($LASTEXITCODE -ne 0) { Fail "logic_tables.py" }
& $py -I (Join-Path $t 'level_info.py') $bin (Join-Path $Work 'level_info.c')
if ($LASTEXITCODE -ne 0) { Fail "level_info.py" }
& $py -I (Join-Path $t 'seq_data.py') $bin (Join-Path $Work 'seq_data.c')
if ($LASTEXITCODE -ne 0) { Fail "seq_data.py" }
# x64: runtime\gamecode_globals.c with its __ptr32 initialisers moved into
# umk3_globals_fixup() (clang cannot initialise them statically).
$globals64 = Join-Path $Work 'gamecode_globals64.c'
if ($Arch -eq 'x64') {
    & $py -I (Join-Path $t 'low32_static.py') (Join-Path $Root 'runtime\gamecode_globals.h') -- `
        (Join-Path $Root 'runtime\gamecode_globals.c') $globals64 umk3_globals_fixup | Out-Null
    if ($LASTEXITCODE -ne 0) { Fail "low32_static.py" }
}

# --- 4. Compile -------------------------------------------------------------
# The same sources, defines and libraries as the umk3-game target in
# CMakeLists.txt; keep the two in step.
$exeName = if ($Arch -eq 'x64') { 'umk3-game64.exe' } else { 'umk3-game.exe' }
Step "4/5 Compilando $exeName (tarda unos minutos)"
$src = @(
    'runtime\game_main.c', 'runtime\draw_gl.c', 'runtime\gamecode_globals.c',
    'runtime\gamecode_stubs.c', 'runtime\lime_menu.c', 'runtime\lime_platform.c',
    'runtime\lime_app.c', 'runtime\wav.c', 'runtime\fight_runtime.c',
    'runtime\debug_menu.c', 'runtime\savestate.c',
    'runtime\settings_menu.c', 'runtime\profiles.c', 'runtime\pad_nav.c',
    'runtime\platform\win32_gl.c', 'runtime\platform\win32_gl_cdecl.c',
    'runtime\platform\win32_audio.c'
) | ForEach-Object { Join-Path $Root $_ }
foreach ($d in 'decomp\gamecode', 'decomp\lime', 'runtime\lime', 'decomp\gamecode\logic') {
    $src += Get-ChildItem -Path (Join-Path $Root $d) -Filter *.c | ForEach-Object { $_.FullName }
}
$src += 'logic_tables.c', 'level_info.c', 'seq_data.c' | ForEach-Object { Join-Path $Work $_ }
if ($Arch -eq 'x64') {
    $g = Join-Path $Root 'runtime\gamecode_globals.c'
    $src = $src | ForEach-Object { if ($_ -eq $g) { $globals64 } else { $_ } }
}

$objdir = Join-Path $Work $(if ($Arch -eq 'x64') { 'obj64' } else { 'obj' })
New-Item -ItemType Directory -Force -Path $objdir | Out-Null
$flags = @('-std=gnu11', '-w', '-DUMK3_HAVE_MK3', '-DUMK3_REAL_GL', '-DUMK3_SHELL',
           '-I', (Join-Path $Root 'runtime'), '-I', (Join-Path $Root 'decomp\lime'),
           '-I', (Join-Path $Root 'decomp\gamecode\logic'))
if ($Arch -eq 'x64') {
    # -fno-jump-tables: clang cannot build its lookup tables of __ptr32
    # function pointers
    $flags += '-DUMK3_LOW32', '-fms-extensions', '-O2', '-fno-strict-aliasing', '-fno-jump-tables'
}
function Quote($s) { '"' + ($s -replace '\\', '/') + '"' }
# Save states (runtime/savestate.h): every file sees the header; the game's
# own files -- decomp\, runtime\lime\, the generated tables and the runtime
# files that hold game state -- also get UMK3_STATE_FILE. CMakeLists.txt
# does the same.
$flags += '-include', (Join-Path $Root 'runtime\savestate.h')
$stateRuntime = 'gamecode_globals.c', 'gamecode_stubs.c', 'lime_menu.c',
                'lime_platform.c', 'lime_app.c', 'wav.c', 'fight_runtime.c'
function IsStateFile($s) {
    $n = [IO.Path]::GetFileName($s)
    return ($s -like '*\decomp\*') -or ($s -like '*\runtime\lime\*') -or
           ($s -like (Join-Path $Work '*')) -or
           (($s -like '*\runtime\*') -and ($stateRuntime -contains $n))
}

# One clang per source, several at a time.
$jobs = [Math]::Max(1, [Environment]::ProcessorCount)
$objs = @()
$running = @()
$i = 0
foreach ($s in $src) {
    $i++
    $o = Join-Path $objdir ('{0:D3}_{1}.o' -f $i, [IO.Path]::GetFileNameWithoutExtension($s))
    $objs += $o
    $rsp = "$o.rsp"
    $f = $flags
    if (IsStateFile $s) { $f += '-DUMK3_STATE_FILE' }
    (($f | ForEach-Object { if ($_ -like '-*') { $_ } else { Quote $_ } }) +
        @('-c', (Quote $s), '-o', (Quote $o))) -join "`n" | Set-Content -Encoding ascii $rsp
    $p = Start-Process -FilePath $cc -ArgumentList "@`"$rsp`"" -NoNewWindow -PassThru
    $null = $p.Handle                     # keeps ExitCode readable after exit
    $running += $p
    while (@($running | Where-Object { -not $_.HasExited }).Count -ge $jobs) { Start-Sleep -Milliseconds 100 }
    Write-Host ("  [{0}/{1}] {2}" -f $i, $src.Count, [IO.Path]::GetFileName($s))
}
$running | ForEach-Object { $_.WaitForExit() }
$bad = $running | Where-Object { $_.ExitCode -ne 0 }
if ($bad) { Fail "fallo la compilacion de $(@($bad).Count) archivo(s); mira los mensajes de arriba" }

# The game's icon (runtime\umk3_game.rc), linked in as one more object.
$res = Join-Path $objdir 'umk3_game.res.o'
& (Join-Path $llvm "bin\$triple-windres.exe") (Join-Path $Root 'runtime\umk3_game.rc') -O coff -o $res
if ($LASTEXITCODE -ne 0) { Fail "umk3_game.rc" }
$objs += $res

$exe = Join-Path $Root $exeName
$rsp = Join-Path $Work 'link.rsp'
# --large-address-aware: a 32-bit exe may use 4 GB instead of 2 on 64-bit
# Windows. The heap fills from the bottom, so addresses above 2 GB only come
# when the lower 2 GB are full -- the case that would otherwise run out.
# x64: the exe at 0x400000, no ASLR, so the code and its data sit below
# 2 GB where a 32-bit pointer word reaches them.
$ldArch = if ($Arch -eq 'x64') {
    @('-Wl,--image-base,0x400000', '-Wl,--disable-dynamicbase', '-Wl,--disable-high-entropy-va')
} else { @('-Wl,--large-address-aware') }
(($objs | ForEach-Object { Quote $_ }) + @('-o', (Quote $exe), '-mwindows') + $ldArch +
    @('-lopengl32', '-lgdi32', '-lwinmm', '-lmsacm32', '-lm')) -join "`n" |
    Set-Content -Encoding ascii $rsp
if (Test-Path $exe) { Remove-Item $exe -Force }
& $cc "@$rsp"
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) { Fail "fallo el enlazado" }

# --- 5. res -----------------------------------------------------------------
Step "5/5 Copiando res desde tu .ipa"
& (Join-Path $PSScriptRoot 'setup_res.ps1') -Ipa $Ipa -Dest $Root
if ($LASTEXITCODE -ne 0) { Fail "setup_res.ps1" }

# Mary's own pictures for the wide screens (the continue screens on 16:9
# and 21:9): from launcher\aditional textures, which updates bring, to the
# folder beside res the game reads them from.
$addSrc = Join-Path $PSScriptRoot 'aditional textures'
if (Test-Path $addSrc) {
    $addDst = Join-Path $Root 'aditional textures'
    New-Item -ItemType Directory -Force -Path $addDst | Out-Null
    Copy-Item -Path (Join-Path $addSrc '*') -Destination $addDst -Recurse -Force
}

$ini = Join-Path $Root 'umk3.ini'
if (-not (Test-Path $ini)) {
    "width=960`r`nheight=640`r`nfullscreen=0`r`nlanguage=`r`n" | Set-Content -Encoding ascii $ini
}

Write-Host ""
Write-Host "Listo: $exe" -ForegroundColor Green
exit 0
