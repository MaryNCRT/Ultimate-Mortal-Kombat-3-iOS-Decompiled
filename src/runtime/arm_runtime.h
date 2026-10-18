/*
 * arm_runtime.h — contexto de CPU y primitivas para el codigo recompilado.
 *
 * Modelo (igual que N64Recomp): cada funcion ARM original se convierte en una
 * funcion C `void func_XXXXXXXX(arm_ctx *ctx)`. El estado de la CPU es
 * explicito y vive en `arm_ctx`; la memoria del invitado es un bloque plano
 * direccionado con enteros de 32 bits.
 *
 * El binario original es 100% Thumb y usa el ABI AAPCS "soft-float": los
 * argumentos y retornos en coma flotante viajan en registros ENTEROS como
 * patron de bits (float en r0, double en r0:r1). Por eso los shims de las
 * funciones importadas leen y escriben en ctx->r[].
 */

#ifndef ARM_RUNTIME_H
#define ARM_RUNTIME_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Estado de la CPU                                                     */
/* ------------------------------------------------------------------ */

/* s[2n] y s[2n+1] son las dos mitades de d[n] (little endian), igual que en
 * VFPv2/v3 para d0..d15. La union reproduce ese solapamiento sin trucos. */
typedef union {
    uint32_t s[32];
    uint64_t d[16];
} vfp_regs;

typedef struct {
    uint32_t r[16];   /* r0..r12, sp=13, lr=14, pc=15 */
    vfp_regs v;
    uint32_t nf, zf, cf, vf;             /* flags NZCV del APSR */
    uint32_t fp_n, fp_z, fp_c, fp_v;     /* flags de comparacion del FPSCR */
} arm_ctx;

#define SP 13
#define LR 14
#define PC 15

/* ------------------------------------------------------------------ */
/* Memoria del invitado                                                 */
/* ------------------------------------------------------------------ */

extern uint8_t *g_ram;        /* base del espacio de direcciones */
extern uint32_t g_ram_size;

void arm_mem_init(uint32_t size);
void arm_mem_free(void);

/*
 * Carga la imagen del binario en la memoria del invitado.
 *
 * Es imprescindible en cuanto una funcion recompilada toca datos estaticos:
 * las cadenas literales, las tablas y los punteros viven en __cstring y
 * __data, y el codigo los referencia por su direccion ORIGINAL. Sin la imagen
 * mapeada, esas lecturas devuelven ceros y el resultado es basura silenciosa.
 *
 * Se aprovecha que en esta slice vmaddr == fileoff + 0x1000 tanto para __TEXT
 * como para __DATA, asi que basta con volcar la slice entera en 0x1000.
 * Devuelve 0 si todo fue bien.
 */
int arm_load_image(const char *thin_slice_path);

/* Accesos sin alinear a proposito: el codigo original lee floats en offsets
 * impares (p.ej. los UV del .meshset en offset 6). memcpy es la forma
 * portable de hacerlo sin invocar comportamiento indefinido. */
static inline uint32_t MEM_LD32(uint32_t a) { uint32_t v; memcpy(&v, g_ram + a, 4); return v; }
static inline uint16_t MEM_LD16(uint32_t a) { uint16_t v; memcpy(&v, g_ram + a, 2); return v; }
static inline uint8_t  MEM_LD8 (uint32_t a) { return g_ram[a]; }
static inline void MEM_ST32(uint32_t a, uint32_t v) { memcpy(g_ram + a, &v, 4); }
static inline void MEM_ST16(uint32_t a, uint16_t v) { memcpy(g_ram + a, &v, 2); }
static inline void MEM_ST8 (uint32_t a, uint8_t  v) { g_ram[a] = v; }

static inline uint64_t MEM_LD64(uint32_t a) { uint64_t v; memcpy(&v, g_ram + a, 8); return v; }
static inline void MEM_ST64(uint32_t a, uint64_t v) { memcpy(g_ram + a, &v, 8); }

/* ------------------------------------------------------------------ */
/* Conversion entre patrones de bits y tipos de coma flotante           */
/* ------------------------------------------------------------------ */

static inline float    U32_F32(uint32_t b) { float f;    memcpy(&f, &b, 4); return f; }
static inline uint32_t F32_U32(float f)    { uint32_t b; memcpy(&b, &f, 4); return b; }
static inline double   U64_F64(uint64_t b) { double d;   memcpy(&d, &b, 8); return d; }
static inline uint64_t F64_U64(double d)   { uint64_t b; memcpy(&b, &d, 8); return b; }

/* Azucar para leer/escribir registros VFP como valores, no como bits. */
#define VS(i)      U32_F32(ctx->v.s[i])
#define VS_SET(i, x)  (ctx->v.s[i] = F32_U32(x))

/* Float arithmetic on RAW BIT PATTERNS.
 *
 * recomp.py emits these for the fused multiply-accumulate forms -- vmla, vmls,
 * vnmla -- because those read and write the same register, so the value has to
 * be decoded and re-encoded around one operation rather than through VS/VS_SET.
 *
 * They were referenced by the emitter and never defined here. Nothing noticed
 * because neither calibration module contains a vmls: the gap only appears when
 * the gate is run on a module that does. That is the fourth defect of this shape
 * found by extending the differential tests, after the ASR #32 shift, the
 * missing tbb/tbh labels and the uULL literal suffix. */
static inline uint32_t F32ADD(uint32_t a, uint32_t b)
{ return F32_U32(U32_F32(a) + U32_F32(b)); }

static inline uint32_t F32SUB(uint32_t a, uint32_t b)
{ return F32_U32(U32_F32(a) - U32_F32(b)); }

static inline uint32_t F32MUL(uint32_t a, uint32_t b)
{ return F32_U32(U32_F32(a) * U32_F32(b)); }

static inline uint32_t F32DIV(uint32_t a, uint32_t b)
{ return F32_U32(U32_F32(a) / U32_F32(b)); }

static inline uint32_t F32NEG(uint32_t a)
{ return F32_U32(-U32_F32(a)); }
#define VD(i)      U64_F64(ctx->v.d[i])
#define VD_SET(i, x)  (ctx->v.d[i] = F64_U64(x))

/* ------------------------------------------------------------------ */
/* Flags                                                                */
/* ------------------------------------------------------------------ */

#define SET_NZ(res) do {                       \
        uint32_t _r = (uint32_t)(res);         \
        ctx->nf = (_r >> 31) & 1;              \
        ctx->zf = (_r == 0);                   \
    } while (0)

static inline void set_flags_add(arm_ctx *ctx, uint32_t a, uint32_t b, uint32_t res)
{
    ctx->nf = (res >> 31) & 1;
    ctx->zf = (res == 0);
    ctx->cf = (res < a) || (res == a && b != 0);
    ctx->vf = (((a ^ ~b) & (a ^ res)) >> 31) & 1;
}

static inline void set_flags_sub(arm_ctx *ctx, uint32_t a, uint32_t b, uint32_t res)
{
    ctx->nf = (res >> 31) & 1;
    ctx->zf = (res == 0);
    ctx->cf = (a >= b);
    ctx->vf = (((a ^ b) & (a ^ res)) >> 31) & 1;
}

/* ------------------------------------------------------------------ */
/* Comparaciones VFP                                                    */
/* ------------------------------------------------------------------ */

/* vcmp deja el resultado en los flags del FPSCR, y hace falta un vmrs
 * explicito para pasarlos al APSR antes de que los use un bloque IT.
 * Se modelan por separado para reproducir esa separacion. */
#define VFP_CMP(a, b) do {                                     \
        double _x = (a), _y = (b);                             \
        if (_x != _x || _y != _y) {         /* NaN: no ordenado */ \
            ctx->fp_n = 0; ctx->fp_z = 0; ctx->fp_c = 1; ctx->fp_v = 1; \
        } else if (_x == _y) {                                 \
            ctx->fp_n = 0; ctx->fp_z = 1; ctx->fp_c = 1; ctx->fp_v = 0; \
        } else if (_x < _y) {                                  \
            ctx->fp_n = 1; ctx->fp_z = 0; ctx->fp_c = 0; ctx->fp_v = 0; \
        } else {                                               \
            ctx->fp_n = 0; ctx->fp_z = 0; ctx->fp_c = 1; ctx->fp_v = 0; \
        }                                                      \
    } while (0)

/* vmrs APSR_nzcv, FPSCR */
#define VMRS_APSR() do {                                       \
        ctx->nf = ctx->fp_n; ctx->zf = ctx->fp_z;              \
        ctx->cf = ctx->fp_c; ctx->vf = ctx->fp_v;              \
    } while (0)

/* ------------------------------------------------------------------ */
/* Pila                                                                 */
/* ------------------------------------------------------------------ */

#define PUSH_REG(reg) do { ctx->r[SP] -= 4; MEM_ST32(ctx->r[SP], ctx->r[reg]); } while (0)
#define POP_REG(reg)  do { ctx->r[reg] = MEM_LD32(ctx->r[SP]); ctx->r[SP] += 4; } while (0)

/* ------------------------------------------------------------------ */
/* Monton (heap) del invitado                                           */
/* ------------------------------------------------------------------ */
/*
 * Las funciones del motor reservan memoria con limeMalloc y devuelven punteros
 * del INVITADO (direcciones de 32 bits dentro de g_ram), no punteros del
 * anfitrion. Para poder verificar RenderMesh.cpp hace falta un asignador que
 * viva dentro de ese espacio de direcciones.
 *
 * Es deliberadamente simple — primer hueco que sirva, con fusion de bloques
 * libres adyacentes. No busca rendimiento: busca ser obviamente correcto, que
 * es lo que necesita un oraculo de verificacion.
 */

/* El monton va POR ENCIMA de la imagen del binario: __TEXT + __DATA ocupan
 * de 0x1000 a ~0x6bd000, asi que empezar antes lo pisaria. */
#define GUEST_HEAP_BASE  0x00800000u
#define GUEST_HEAP_SIZE  0x01800000u   /* 24 MB */

void     guest_heap_init(void);
uint32_t guest_malloc(uint32_t size);
void     guest_free(uint32_t ptr);
uint32_t guest_heap_used(void);
uint32_t guest_heap_blocks(void);

/* Copia un bloque del anfitrion al invitado y devuelve el puntero invitado. */
uint32_t guest_alloc_copy(const void *src, uint32_t size);
/* Copia una cadena C al invitado (incluye el terminador). */
uint32_t guest_strdup(const char *s);
/* Lee una cadena del invitado a un buffer del anfitrion. */
void     guest_read_cstr(uint32_t ptr, char *dst, size_t cap);

/* Raiz del sistema de archivos del invitado: donde se buscan los assets que
 * pide limeLoadFile. Por defecto EXTRACTED/Payload/UMK3.app/res. */
void arm_set_asset_root(const char *path);

/* ------------------------------------------------------------------ */
/* Shims de funciones importadas (libc / libm del sistema iOS)          */
/* ------------------------------------------------------------------ */

void stub_cosf(arm_ctx *ctx);
void stub_sinf(arm_ctx *ctx);
void stub_cos(arm_ctx *ctx);
void stub_sin(arm_ctx *ctx);
void stub_tanf(arm_ctx *ctx);
void stub_tan(arm_ctx *ctx);
void stub_sqrtf(arm_ctx *ctx);
void stub_memcpy(arm_ctx *ctx);
void stub_memset(arm_ctx *ctx);

/* libc de cadenas */
void stub_strlen(arm_ctx *ctx);
void stub_strcpy(arm_ctx *ctx);
void stub_strcmp(arm_ctx *ctx);
void stub_strstr(arm_ctx *ctx);
void stub_sprintf(arm_ctx *ctx);
void stub_printf(arm_ctx *ctx);

/* Capa de asignacion y de archivos del motor LIME */
void stub_limeMalloc(arm_ctx *ctx);    /* limeMalloc(const char *tag, size_t n) */
void stub_limeFree(arm_ctx *ctx);      /* limeFree(void *p)                     */
void stub_limeLoadFile(arm_ctx *ctx);  /* limeLoadFile(const char *path)        */
void stub_limeFileSize(arm_ctx *ctx);  /* limeFileSize(const char *path)        */

/* Se llama cuando el recompilador encontro una instruccion que no sabe
 * traducir. Aborta ruidosamente: preferimos fallar a producir un resultado
 * silenciosamente incorrecto. */
void arm_unimplemented(const char *func, uint32_t addr, const char *text);

#ifdef __cplusplus
}
#endif

#endif /* ARM_RUNTIME_H */
