/*
 * arm_runtime.c — memoria del invitado y shims de las funciones importadas.
 */

#include "arm_runtime.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

uint8_t *g_ram = NULL;
uint32_t g_ram_size = 0;

void arm_mem_init(uint32_t size)
{
    g_ram = (uint8_t *)calloc(1, size);
    if (!g_ram) {
        fprintf(stderr, "arm_mem_init: no se pudo reservar %u bytes\n", size);
        exit(1);
    }
    g_ram_size = size;
    if (size >= GUEST_HEAP_BASE + GUEST_HEAP_SIZE) {
        guest_heap_init();
    }
}

#define IMAGE_BASE 0x1000u

int arm_load_image(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "arm_load_image: no se pudo abrir %s\n", path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || (uint32_t)n + IMAGE_BASE > g_ram_size) {
        fprintf(stderr, "arm_load_image: tamano invalido (%ld)\n", n);
        fclose(f);
        return 1;
    }
    size_t got = fread(g_ram + IMAGE_BASE, 1, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) {
        fprintf(stderr, "arm_load_image: lectura incompleta\n");
        return 1;
    }
    return 0;
}

void arm_mem_free(void)
{
    free(g_ram);
    g_ram = NULL;
    g_ram_size = 0;
}

void arm_unimplemented(const char *func, uint32_t addr, const char *text)
{
    fprintf(stderr, "\n*** instruccion no implementada en %s @ 0x%08x: %s\n",
            func, addr, text);
    abort();
}

/* ------------------------------------------------------------------ */
/* Shims — ABI AAPCS soft-float                                         */
/*   float:  argumento y retorno en r0 (patron de bits)                 */
/*   double: argumento y retorno en el par r0:r1 (r0 = mitad baja)      */
/* ------------------------------------------------------------------ */

static double get_double_arg(arm_ctx *ctx)
{
    uint64_t bits = ((uint64_t)ctx->r[1] << 32) | ctx->r[0];
    return U64_F64(bits);
}

static void set_double_ret(arm_ctx *ctx, double d)
{
    uint64_t bits = F64_U64(d);
    ctx->r[0] = (uint32_t)(bits & 0xFFFFFFFFu);
    ctx->r[1] = (uint32_t)(bits >> 32);
}

void stub_cosf(arm_ctx *ctx)  { ctx->r[0] = F32_U32(cosf(U32_F32(ctx->r[0]))); }
void stub_sinf(arm_ctx *ctx)  { ctx->r[0] = F32_U32(sinf(U32_F32(ctx->r[0]))); }
void stub_tanf(arm_ctx *ctx)  { ctx->r[0] = F32_U32(tanf(U32_F32(ctx->r[0]))); }
void stub_sqrtf(arm_ctx *ctx) { ctx->r[0] = F32_U32(sqrtf(U32_F32(ctx->r[0]))); }

void stub_cos(arm_ctx *ctx) { set_double_ret(ctx, cos(get_double_arg(ctx))); }
void stub_sin(arm_ctx *ctx) { set_double_ret(ctx, sin(get_double_arg(ctx))); }
void stub_tan(arm_ctx *ctx) { set_double_ret(ctx, tan(get_double_arg(ctx))); }

void stub_memcpy(arm_ctx *ctx)
{
    memmove(g_ram + ctx->r[0], g_ram + ctx->r[1], ctx->r[2]);
    /* memcpy devuelve el destino */
}

void stub_memset(arm_ctx *ctx)
{
    memset(g_ram + ctx->r[0], (int)(ctx->r[1] & 0xFF), ctx->r[2]);
}

/* ================================================================== */
/* Monton del invitado                                                  */
/* ================================================================== */
/*
 * Cada bloque lleva delante una cabecera de 8 bytes:
 *     +0  uint32 size   tamano util, sin contar la cabecera
 *     +4  uint32 free   1 = libre, 0 = ocupado
 * Los bloques van consecutivos; el ultimo tiene size = 0 y marca el final.
 */

#define HDR 8u

static uint32_t hdr_size(uint32_t b)          { return MEM_LD32(b); }
static uint32_t hdr_free(uint32_t b)          { return MEM_LD32(b + 4); }
static void set_hdr(uint32_t b, uint32_t s, uint32_t f)
{
    MEM_ST32(b, s);
    MEM_ST32(b + 4, f);
}

void guest_heap_init(void)
{
    set_hdr(GUEST_HEAP_BASE, GUEST_HEAP_SIZE - 2 * HDR, 1);
    /* centinela final */
    set_hdr(GUEST_HEAP_BASE + GUEST_HEAP_SIZE - HDR, 0, 0);
}

uint32_t guest_malloc(uint32_t size)
{
    if (size == 0) {
        size = 1;
    }
    size = (size + 3u) & ~3u;           /* alineado a 4 */

    for (uint32_t b = GUEST_HEAP_BASE; hdr_size(b) != 0; b += HDR + hdr_size(b)) {
        uint32_t bs = hdr_size(b);
        if (!hdr_free(b) || bs < size) {
            continue;
        }
        /* partir si sobra sitio para otro bloque util */
        if (bs >= size + HDR + 16u) {
            uint32_t rest = b + HDR + size;
            set_hdr(rest, bs - size - HDR, 1);
            set_hdr(b, size, 0);
        } else {
            set_hdr(b, bs, 0);
        }
        return b + HDR;
    }
    fprintf(stderr, "guest_malloc: sin memoria para %u bytes\n", size);
    return 0;
}

void guest_free(uint32_t ptr)
{
    if (ptr == 0) {
        return;
    }
    uint32_t b = ptr - HDR;
    set_hdr(b, hdr_size(b), 1);

    /* fusionar los libres adyacentes, de principio a fin */
    uint32_t cur = GUEST_HEAP_BASE;
    while (hdr_size(cur) != 0) {
        uint32_t nxt = cur + HDR + hdr_size(cur);
        if (hdr_free(cur) && hdr_size(nxt) != 0 && hdr_free(nxt)) {
            set_hdr(cur, hdr_size(cur) + HDR + hdr_size(nxt), 1);
            continue;               /* reintentar con el bloque ya crecido */
        }
        cur = nxt;
    }
}

uint32_t guest_heap_used(void)
{
    uint32_t used = 0;
    for (uint32_t b = GUEST_HEAP_BASE; hdr_size(b) != 0; b += HDR + hdr_size(b)) {
        if (!hdr_free(b)) {
            used += hdr_size(b);
        }
    }
    return used;
}

uint32_t guest_heap_blocks(void)
{
    uint32_t n = 0;
    for (uint32_t b = GUEST_HEAP_BASE; hdr_size(b) != 0; b += HDR + hdr_size(b)) {
        if (!hdr_free(b)) {
            n++;
        }
    }
    return n;
}

uint32_t guest_alloc_copy(const void *src, uint32_t size)
{
    uint32_t p = guest_malloc(size);
    if (p) {
        memcpy(g_ram + p, src, size);
    }
    return p;
}

uint32_t guest_strdup(const char *s)
{
    uint32_t n = (uint32_t)strlen(s) + 1;
    return guest_alloc_copy(s, n);
}

void guest_read_cstr(uint32_t ptr, char *dst, size_t cap)
{
    size_t i = 0;
    if (cap == 0) {
        return;
    }
    while (i + 1 < cap && ptr + i < g_ram_size) {
        char c = (char)g_ram[ptr + i];
        dst[i] = c;
        if (c == '\0') {
            return;
        }
        i++;
    }
    dst[i] = '\0';
}

/* ================================================================== */
/* Shims de cadenas                                                     */
/* ================================================================== */

void stub_strlen(arm_ctx *ctx)
{
    uint32_t p = ctx->r[0], n = 0;
    while (p + n < g_ram_size && g_ram[p + n]) {
        n++;
    }
    ctx->r[0] = n;
}

void stub_strcpy(arm_ctx *ctx)
{
    uint32_t d = ctx->r[0], s = ctx->r[1], i = 0;
    do {
        g_ram[d + i] = g_ram[s + i];
    } while (g_ram[s + i++]);
    /* strcpy devuelve el destino */
}

void stub_strcmp(arm_ctx *ctx)
{
    uint32_t a = ctx->r[0], b = ctx->r[1];
    while (g_ram[a] && g_ram[a] == g_ram[b]) {
        a++; b++;
    }
    ctx->r[0] = (uint32_t)((int)g_ram[a] - (int)g_ram[b]);
}

/* strstr: devuelve el puntero INVITADO a la primera aparicion, o 0. */
void stub_strstr(arm_ctx *ctx)
{
    char hay[1024], needle[256];
    guest_read_cstr(ctx->r[0], hay, sizeof(hay));
    guest_read_cstr(ctx->r[1], needle, sizeof(needle));
    const char *hit = strstr(hay, needle);
    ctx->r[0] = hit ? (ctx->r[0] + (uint32_t)(hit - hay)) : 0u;
}

/*
 * sprintf minimo. Solo cubre lo que usa el codigo que verificamos: %s, %d,
 * %u, %x, %c, %f y %%. Con AAPCS los tres primeros variadicos van en r2, r3 y
 * luego la pila.
 *
 * Si aparece un especificador no soportado se aborta en vez de escribir algo
 * incorrecto: mas vale fallar ruidosamente en un oraculo.
 */
static uint32_t va_next(arm_ctx *ctx, int *idx)
{
    int i = (*idx)++;
    if (i == 0) return ctx->r[2];
    if (i == 1) return ctx->r[3];
    return MEM_LD32(ctx->r[SP] + 4u * (uint32_t)(i - 2));
}

void stub_sprintf(arm_ctx *ctx)
{
    uint32_t dst = ctx->r[0];
    char fmt[512];
    guest_read_cstr(ctx->r[1], fmt, sizeof(fmt));

    char out[2048];
    size_t o = 0;
    int vi = 0;

    for (size_t i = 0; fmt[i] && o + 1 < sizeof(out); i++) {
        if (fmt[i] != '%') {
            out[o++] = fmt[i];
            continue;
        }
        i++;
        char spec = fmt[i];
        char tmp[128];
        switch (spec) {
        case '%':
            out[o++] = '%';
            break;
        case 's': {
            char s[512];
            guest_read_cstr(va_next(ctx, &vi), s, sizeof(s));
            for (size_t k = 0; s[k] && o + 1 < sizeof(out); k++) out[o++] = s[k];
            break;
        }
        case 'd': case 'i':
            snprintf(tmp, sizeof(tmp), "%d", (int32_t)va_next(ctx, &vi));
            for (size_t k = 0; tmp[k] && o + 1 < sizeof(out); k++) out[o++] = tmp[k];
            break;
        case 'u':
            snprintf(tmp, sizeof(tmp), "%u", va_next(ctx, &vi));
            for (size_t k = 0; tmp[k] && o + 1 < sizeof(out); k++) out[o++] = tmp[k];
            break;
        case 'x':
            snprintf(tmp, sizeof(tmp), "%x", va_next(ctx, &vi));
            for (size_t k = 0; tmp[k] && o + 1 < sizeof(out); k++) out[o++] = tmp[k];
            break;
        case 'c':
            out[o++] = (char)va_next(ctx, &vi);
            break;
        case 'f': {
            /* los double variadicos van alineados a 8 y ocupan dos ranuras */
            if (vi & 1) vi++;
            uint32_t lo = va_next(ctx, &vi), hi = va_next(ctx, &vi);
            snprintf(tmp, sizeof(tmp), "%f",
                     U64_F64(((uint64_t)hi << 32) | lo));
            for (size_t k = 0; tmp[k] && o + 1 < sizeof(out); k++) out[o++] = tmp[k];
            break;
        }
        default:
            arm_unimplemented("stub_sprintf", 0, "especificador no soportado");
        }
    }
    out[o] = '\0';
    memcpy(g_ram + dst, out, o + 1);
    ctx->r[0] = (uint32_t)o;    /* sprintf devuelve el numero de caracteres */
}

void stub_printf(arm_ctx *ctx)
{
    char fmt[512];
    guest_read_cstr(ctx->r[0], fmt, sizeof(fmt));
    /* En el oraculo la salida por consola no interesa: basta con no romper el
     * flujo y devolver algo razonable. */
    ctx->r[0] = (uint32_t)strlen(fmt);
}

/* ================================================================== */
/* Capa de asignacion y archivos de LIME                                */
/* ================================================================== */

/* limeMalloc(const char *tag, size_t size) — el primer argumento es una
 * etiqueta de depuracion ("meshsethandle", "meshset_meshes"...), no un tamano. */
void stub_limeMalloc(arm_ctx *ctx)
{
    ctx->r[0] = guest_malloc(ctx->r[1]);
}

void stub_limeFree(arm_ctx *ctx)
{
    guest_free(ctx->r[0]);
    ctx->r[0] = 0;
}

static char g_asset_root[512] = "";

void arm_set_asset_root(const char *path)
{
    snprintf(g_asset_root, sizeof(g_asset_root), "%s", path);
}

/*
 * limeLoadFile(const char *path) — carga el archivo entero en memoria del
 * invitado y devuelve el puntero, o 0 si no existe. Las rutas del juego son
 * relativas al bundle ("FLOOR.meshset", "STATICLIGHTING/FLOOR.lighting").
 */
void stub_limeLoadFile(arm_ctx *ctx)
{
    char rel[512];
    guest_read_cstr(ctx->r[0], rel, sizeof(rel));

    char full[1100];
    snprintf(full, sizeof(full), "%s/%s", g_asset_root, rel);

    FILE *f = fopen(full, "rb");
    if (!f) {
        ctx->r[0] = 0;
        return;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) {
        fclose(f);
        ctx->r[0] = 0;
        return;
    }
    uint32_t p = guest_malloc((uint32_t)n);
    if (p && fread(g_ram + p, 1, (size_t)n, f) != (size_t)n) {
        guest_free(p);
        p = 0;
    }
    fclose(f);
    ctx->r[0] = p;
}

/* limeFileSize(const char *path) — el original lo resuelve con NSFileManager,
 * o sea capa de plataforma iOS; aqui se consulta el sistema de archivos del
 * anfitrion. Devuelve 0 si el archivo no existe. */
void stub_limeFileSize(arm_ctx *ctx)
{
    char rel[512];
    guest_read_cstr(ctx->r[0], rel, sizeof(rel));

    char full[1100];
    snprintf(full, sizeof(full), "%s/%s", g_asset_root, rel);

    FILE *f = fopen(full, "rb");
    if (!f) {
        ctx->r[0] = 0;
        return;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    ctx->r[0] = (n > 0) ? (uint32_t)n : 0u;
}
