# Running UMK3 1.2.59 in touchHLE — a 2-byte patch

**Result: Ultimate Mortal Kombat 3 version 1.2.59 boots and plays in [touchHLE](https://touchhle.org/) after changing two bytes.**

As far as we know, nobody had this version running before. The [touchHLE compatibility database](https://appdb.touchhle.org/apps/1348) lists only 1.0.4 and 1.0.49.

This document covers the root cause, the patch, what was tried and failed, and what it all implies for the port. The failed attempts are included deliberately — each one narrowed the problem, and one of them produced a constraint worth knowing about.

---

## 1. The patch

| Field | Value |
|---|---|
| Function | `LocaleManager::setLocale(midp::String*)` |
| Symbol | `__ZN13LocaleManager9setLocaleEPN4midp6StringE` |
| VMADDR (armv7 slice) | `0x0009e8c0` |
| Offset in the fat binary | `0x003268c0` |
| Original bytes | `f0 b5` (`push {r4-r7, lr}`) |
| Patched bytes | `70 47` (`bx lr`) |

Two bytes. It turns `setLocale` into a function that returns immediately.

```bash
python tools/patch_ipa.py --apply setlocale_nop --out UMK3_patched.ipa
```

The fat-binary offset comes from `0x289000 + (vmaddr - 0x1000)`, because the armv7 slice starts at file offset `0x289000` and its `__TEXT` begins at vmaddr `0x1000`.

**You apply this to your own copy.** No patched binary is distributed here.

---

## 2. Root cause

The initial symptom was actively misleading:

```
thread 'main' panicked at src\dyld.rs:811:9:
Call to unimplemented function ___assert_rtn
```

`___assert_rtn` is the routine a **failed** `assert()` calls. touchHLE does not implement it, so *any* failed assertion kills the emulator instantly — and the panic never says which assertion fired.

An earlier hypothesis blamed the unresolved `NSLocale` and `NSHTTPCookie` symbols visible in the log. That was wrong; those were only link-time warnings and nothing dereferenced them fatally.

The arguments to `__assert_rtn(func, file, line, expr)` travel in `r0`–`r3`, and in Thumb-2 they are loaded from the literal pool immediately before the call — which means they can be **recovered statically**. `tools/xref.py` does exactly that; all 59 assertion messages in the binary were dumped with their file and line.

The real chain, later confirmed against the decompilation:

```
LocaleManager::LocaleManager()                        static constructor
  └─ midp::System::getProperty("microedition.locale")     SystemIPhone.mm
       └─ [NSLocale preferredLanguages]                ← last line in the log
  └─ LocaleManager::setLocale(locale)
       └─ getLocaleIndex(locale)  →  -1                ← language not in table
       └─ assert(false)                                ← LocaleManager.mm:172
```

In code:

```c
void LocaleManager::setLocale(LocaleManager *this, String *locale)
{
    int i = getLocaleIndex(this, locale);
    if (i != -1) {
        this->localeIndex = i;
        /* ...store the String... */
        return;
    }
    __assert_rtn("setLocale", ".../LocaleManager.mm", 172, "false");
}
```

**touchHLE reports preferred languages as short codes — `["es", "en"]` — and EA's locale table does not recognise them.** `getLocaleIndex` returns −1 and the assertion fires.

It is a format mismatch, not a bug in the game. On a real iPhone the app receives something like `es_ES`, finds it in the table, and carries on.

---

## 3. What was tried and did not work

### 3.1 Patching the `___assert_rtn` stub ❌

The idea: the stub in `__symbol_stub4` is 12 bytes of ARM code

```
e59fc000   ldr r12, [pc, #0]
e59cf000   ldr pc,  [r12]
000f3acc   <pointer slot>
```

Replacing the first instruction with `bx lr` would neutralise **all 59 assertions at once** in 4 bytes, returning cleanly to the caller.

Result: **touchHLE validates stub table contents.**

```
panicked at src\dyld.rs:442:17:
assertion failed: mem.read(ptr + j.try_into().unwrap()) == instr
```

It checks each stub entry against the expected template before binding it.

**Reusable conclusion: in touchHLE you cannot patch the stub table — you have to patch the call sites.**

### 3.2 NOP-ing the branches to `getProperty`'s assertions ❌

Three `beq.w` instructions leading to assertions in `midp::System::getProperty` (`key != null`, `mainBundle != null`, `keyRef != null`) were replaced with `nop`. Still failed with `___assert_rtn` — those were not the ones firing. Useful anyway: it ruled `SystemIPhone.mm` out.

### 3.3 Forcing `getLocaleIndex` to return 0 ⚠ partial

`movs r0,#0; bx lr` in the prologue. This **got past the assertion** — the `___assert_rtn` panic disappeared — but failed later:

```
panicked at src\mem.rs:357:9:
Attempted null-page access at 0x1
```

Inventing a locale index is dangerous if the supported list is empty or entry 0 is uninitialised. Abandoned in favour of 3.4.

### 3.4 `setLocale` as a no-op ✅

Let `LocaleManager` keep whatever locale its constructor set, instead of inventing an index. The game boots and runs.

---

## 4. How far it gets

The full log runs to **4,281 lines** of normal game output, ending in a clean exit (code 0):

```
Settings[2] = 3
...
#############################################
## saving achievement tracker
#############################################
UMK3[0] app will terminate: disconnected!
```

So it clears startup, initialises the engine, loads assets, runs the game loop, and saves state on exit.

### Remaining issues (none of them blocking)

| Issue | Detail |
|---|---|
| Missing `*_LOW.PNG` textures | Hundreds of `returning nil` warnings — `NOTEXTPNG`, `NOTEXTURE_LOW.PNG`, `FIRE_PARTICLE_LOW.PNG`… The bundle does not contain the low-resolution variants the game asks for. Either the game selects a resolution tier by device and touchHLE presents as an older one, or those assets were CDN-delivered and never shipped in the IPA. **Unresolved, and relevant to the port's asset pipeline.** |
| OpenAL source exhaustion | 47 × `Failed to create OpenAL source, error code a005` |
| CoreAudio unimplemented | touchHLE warns at load; affects music (`GBMusicTrack.m` uses AudioToolbox) |

---

## 5. What this means for the port

This experiment was the EA SDK stubbing phase, run in miniature, and it produced concrete data.

### 5.1 The EA SDK does not block startup

This is the most useful finding, and it contradicts the starting assumption. With the game running, `EASDK_Handler.mm`, `Mayhem.mm` and the achievement layer all initialised without complaint — the log even shows `saving achievement tracker` working.

**Exactly one EA SDK function blocked startup. The other ~1,411 did not get in the way.** For the port, that means the EA SDK stubs can be simple: returning neutral values is enough, and nothing needs reimplementing.

### 5.2 `LocaleManager` is the exception

It cannot be stubbed to nothing — the game reads it to load its text. The port needs to:

- Implement `LocaleManager` properly, with its own locale table
- Have `System::getProperty("microedition.locale")` return a code the table accepts, or make the table tolerate short codes (`"es"`) as well as long ones (`"es_ES"`)
- **Never use `assert()` on that path** — which is precisely the failure that cost us this detour

The bundle's language folders are `de`, `en`, `es`, `fr`, `it`, `ko`, `zh`.

### 5.3 Revised stub priorities

| Module | Priority | Reason |
|---|---|---|
| `LocaleManager.mm` (26 fn) | **HIGH — implement** | blocks startup; text depends on it |
| `SystemIPhone.mm` | **HIGH — implement** | `getProperty` feeds LocaleManager |
| `EASDK_Handler.mm` (54 fn) | LOW — neutral stub | did not interfere |
| `Mayhem.mm` (239 fn) | LOW — neutral stub | did not interfere |
| `eamtx_iphone/` (~700 fn) | LOW — delete | store/social, did not interfere |
| `FBConnect` (~180 fn) | LOW — delete | touchHLE already substitutes fake classes |

### 5.4 The general rule

> A failed `assert()` in the EA SDK kills the entire startup. When writing the port's stubs, **none of them may use `assert()`** — they should return neutral values and continue. EA's code assumes a real iOS environment and checks invariants a port cannot satisfy.

### Distribution of the 59 assertions

All of them are in the EA SDK. **None in `gamecode`, none in `lime`** — the game proper does not use assertions, so neutralising them cannot affect it.

| File | Assertions |
|---|---|
| `EASDK_Handler.mm` | 22 |
| `Mayhem.mm` | 21 |
| `LocaleManager.mm` | 7 |
| `SystemIPhone.mm` | 3 |
| `ReferenceCounted.cpp` | 3 |
| `JString.cpp` | 3 |

---

## 6. Worth reporting upstream

Two things here would help other people:

1. **The compatibility report for 1.2.59** — the app database only lists 1.0.4 and 1.0.49.
2. **touchHLE does not implement `___assert_rtn`.** Any failed assertion in any app surfaces as a generic panic naming nothing. Implementing it — even to print `file:line: expr` before aborting — would save a lot of diagnostic time across the board.

---

## 7. Why this matters to the decompilation

Beyond convenience: a running copy of the game is a **behavioural reference**.

The static recompiler that verifies our decompiled code cannot follow indirect jumps (`blx reg`, `bx reg`, `tbb`/`tbh`). The fight logic — `mkdrone.c`, `moves.c`, `other.c`, roughly 1,300 functions — is a state machine built on function-pointer tables. **The oracle will not cover it.**

For those functions, a running game is the only reference available. touchHLE also ships a GDB stub, which makes it possible to set breakpoints and inspect guest memory — turning "the game runs" into "we can compare state against our reimplementation."

---

## Multiplayer: `mp_disable`

Multiplayer is **GameKit peer-to-peer**. `GKSession` is the only GameKit class
the binary references — no Game Center, no matchmaking — and **touchHLE does
not implement it**: the string does not appear once in its executable. Entering
multiplayer therefore ends the session.

The funnel is `_startMP` (`0x000afe20`), which owns the session singleton:

```
ldr r4, [r3]        ; r4 = &g_mpSession
ldr r3, [r4]
cbz r3, create      ; if NULL -> [[limeMPSession alloc] init]
pop {r4, r7, pc}    ; otherwise nothing to do
```

Neutralising it leaves the singleton NULL forever. **That is safe rather than
fatal**: messaging `nil` in Objective-C returns `nil`/0 without faulting, so
`mpIsWorking`, `mpGetConnectionState` and `isMPConnected` all answer 0 and the
game takes its own "not connected" path — which exists, since the class carries
a `noWifiAlertView` for exactly that case.

```python
PATCHES["mp_disable"] = [
    (0x000afe20, bytes.fromhex("7047"), "_startMP -> bx lr"),
]
```

Two bytes, at the entry rather than at the allocation, because nothing is
pushed yet and `bx lr` is a clean return.

### What did not work, and why

An earlier build added `modal_nonblocking`, neutralising
`+[modalAlert infoWith:button1:]` and `+[modalAlert queryWith:button1:button2:]`
so confirmation dialogs would stop blocking on `CFRunLoopRun`. That build was
**worse**, and the reason is instructive: those dialogs were acting as a
barrier. With them returning immediately the game walked further into the
GameKit path and failed harder. The patch is kept in `patch_ipa.py` and
deliberately **not applied** — the modal path is only worth neutralising once
whatever sits behind it is safe.

Also worth recording: `_CFRunLoopRun` has exactly **two callers in the whole
binary**, both patched by `modal_nonblocking`. So the long-standing theory that
"every confirmation dialog kills the emulator because of the nested run loop"
does not survive contact with the evidence. Something else closes it.

---

## A texture fallback that builds a malformed name

Not a patch, an observation from the same log, and it looks like a genuine
defect in the shipped game.

The game asks for low-resolution texture variants that were never shipped —
`FIRE_PARTICLE_LOW.PNG`, `MYBLOOD1_LOW.PNG` and so on, 30 distinct names over
527 attempts. Its own fallback then loads the full-resolution file, so those
warnings are benign by design.

One is not. `NOTEXTURE_LOW.PNG` fails **70 times**, and the log contains
`NOTEXTPNG` **exactly 70 times** — the same count. The fallback that strips
`_LOW.PNG` eats three characters too many *and* the dot, producing a filename
that cannot exist.

`NOTEXTURE` is the placeholder a texture load falls back to. If the placeholder
itself cannot load, a failed texture draws as nothing rather than as an obvious
"missing texture" marker — which is consistent with the babality model
sometimes not appearing at all. **Unconfirmed**: the exact string arithmetic has
not been traced, and the connection to the babality case is inference, not
evidence.

---

## The EULA screen: `eula_exit`

Selecting "User Licence" left the interface stuck. `_FE_Task_About_Eula`
(`0x0000ecc8`) is 28 bytes and — despite the class list making it look like a
`UIWebView` screen — **creates no web view at all**:

```
bl   BasicMenu
cmp  r0, #1
beq  exit          <- only leaves if BasicMenu returns exactly 1
pop  {r7, pc}
exit:
bl   PopFETaskDeferred
b    pop
```

If `BasicMenu` returns anything else the task is never popped and the front-end
sits on a screen it will not leave. **One byte**: `0xD0` (`beq`) becomes `0xE0`
(`b`), so the exit is unconditional. The `cmp` is left in place so nothing
moves; it is dead and harmless.

---

## Result

**`setlocale_nop` + `mp_disable` + `eula_exit`** — 5 bytes across three sites.
Confirmed by play: the game boots, the multiplayer menu opens without ending
the session, the licence screen closes, and **no crash has been found
anywhere** in testing since.

```bash
python tools/patch_ipa.py --apply setlocale_nop --apply mp_disable --apply eula_exit --out UMK3_patched.ipa
```

Multiplayer opens but cannot connect, and never will under touchHLE: `GKSession`
is peer-to-peer over Bluetooth or local WiFi, and touchHLE does not implement it
at all. Two emulator instances were tried side by side and, as expected, neither
sees the other. This is a graceful stop, not a fix — the game reaches its own
"not connected" state instead of dying. Networking belongs to the native port,
where those 126 multiplayer functions are marked for stubbing rather than
decompilation precisely because the port will not reimplement GKSession.
