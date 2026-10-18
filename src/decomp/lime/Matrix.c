/*
 * Matrix.c — src/lime/common/Matrix.cpp
 *
 * C limpio, escrito a mano a partir del DESENSAMBLADO de la slice armv7
 * (0x0005da1c .. 0x0005e0a0), no del decompilado de Ghidra.
 *
 * Motivo: 5 de estas 11 funciones usan instrucciones NEON de 2 carriles sobre
 * registros D (`vmul.f32 d6, d6, d7`) para hacer matematica ESCALAR. Ghidra las
 * modela como `FloatVectorMult(...)` opaco y pierde el solapamiento entre d6 y
 * el par s12/s13, asi que su salida sirve para ver la estructura de control
 * pero no el calculo. Las afectadas son limeScaleMatrix, limeScaleMatrixXYZ,
 * limeMatrix3x4RotateSkin, CreatePerspectiveMatrix y limeMatrixMult.
 *
 * Todo lo de aqui esta verificado contra el oraculo (la version recompilada,
 * fiel por construccion): tests/test_matrix_diff.c.
 *
 * Convencion de la matriz: 4x4 de floats en orden FILA-MAYOR, m[fila*4 + col].
 * Verificado, no supuesto: los tests comprueban A*I == A e
 * RotZ(a)*RotZ(b) == RotZ(a+b).
 */

#include "lime.h"

#include <math.h>

/* ------------------------------------------------------------------ */
/* Basicas                                                              */
/* ------------------------------------------------------------------ */

void limeMatrixLoadIdentity(float *m)
{
    m[0]  = 1.0f; m[1]  = 0.0f; m[2]  = 0.0f; m[3]  = 0.0f;
    m[4]  = 0.0f; m[5]  = 1.0f; m[6]  = 0.0f; m[7]  = 0.0f;
    m[8]  = 0.0f; m[9]  = 0.0f; m[10] = 1.0f; m[11] = 0.0f;
    m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f; m[15] = 1.0f;
}

/* El original copia de 4 en 4 con ldm/stm; el orden de los argumentos es
 * (origen, destino), no al reves. */
void limeMatrixCopy(const float *src, float *dst)
{
    for (int i = 0; i < 16; i++) {
        dst[i] = src[i];
    }
}

/* out = a * b, fila por columna. El resultado va en el TERCER argumento (r2).
 * El original desenrolla los 16 elementos y vuelca los temporales a la pila;
 * aqui basta el bucle: se comprobo que da lo mismo. */
void limeMatrixMult(const float *a, const float *b, float *out)
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            out[i * 4 + j] = a[i * 4 + 0] * b[0 * 4 + j]
                           + a[i * 4 + 1] * b[1 * 4 + j]
                           + a[i * 4 + 2] * b[2 * 4 + j]
                           + a[i * 4 + 3] * b[3 * 4 + j];
        }
    }
}

/* ------------------------------------------------------------------ */
/* Rotaciones                                                           */
/* ------------------------------------------------------------------ */
/*
 * Las tres cargan la identidad y luego escriben cuatro elementos. Usan cosf y
 * sinf (las variantes de float): el binario llama a _cosf / _sinf, no a
 * _cos / _sin. Importa para reproducir el resultado bit a bit.
 */

void RotMatrixX(float *m, float angle)
{
    float c = cosf(angle);
    float s = sinf(angle);

    limeMatrixLoadIdentity(m);
    m[5]  =  c;   /* 0x14 */
    m[6]  =  s;   /* 0x18 */
    m[9]  = -s;   /* 0x24 */
    m[10] =  c;   /* 0x28 */
}

void RotMatrixY(float *m, float angle)
{
    float c = cosf(angle);
    float s = sinf(angle);

    limeMatrixLoadIdentity(m);
    m[0]  =  c;   /* 0x00 */
    m[2]  = -s;   /* 0x08 */
    m[8]  =  s;   /* 0x20 */
    m[10] =  c;   /* 0x28 */
}

void RotMatrixZ(float *m, float angle)
{
    float c = cosf(angle);
    float s = sinf(angle);

    limeMatrixLoadIdentity(m);
    m[0] =  c;    /* 0x00 */
    m[1] =  s;    /* 0x04 */
    m[4] = -s;    /* 0x10 */
    m[5] =  c;    /* 0x14 */
}

/* ------------------------------------------------------------------ */
/* Escalados                                                            */
/* ------------------------------------------------------------------ */

/* Escala la 3x3 superior izquierda por un mismo factor. La fila y la columna
 * de traslacion (indices 3, 7, 11, 12..15) quedan intactas. */
void limeScaleMatrix(float *m, float scale)
{
    m[0] *= scale; m[1] *= scale; m[2]  *= scale;
    m[4] *= scale; m[5] *= scale; m[6]  *= scale;
    m[8] *= scale; m[9] *= scale; m[10] *= scale;
}

/*
 * Escala por eje. Ojo con el reparto: cada factor se aplica a una COLUMNA de
 * la 3x3, no a una fila.
 *   sx -> offsets 0x00, 0x10, 0x20  =  m[0], m[4], m[8]   (columna 0)
 *   sy -> offsets 0x04, 0x14, 0x24  =  m[1], m[5], m[9]   (columna 1)
 *   sz -> offsets 0x08, 0x18, 0x28  =  m[2], m[6], m[10]  (columna 2)
 * Sobre la identidad el efecto es diag(sx, sy, sz, 1), que es lo esperable.
 */
void limeScaleMatrixXYZ(float *m, float sx, float sy, float sz)
{
    m[0] *= sx; m[4] *= sx; m[8]  *= sx;
    m[1] *= sy; m[5] *= sy; m[9]  *= sy;
    m[2] *= sz; m[6] *= sz; m[10] *= sz;
}

/* ------------------------------------------------------------------ */
/* Transformacion de vectores                                           */
/* ------------------------------------------------------------------ */

/*
 * Rota un vector por la 3x3 de la matriz, IGNORANDO la traslacion (de ahi el
 * "3x4" del nombre: solo la parte de rotacion de una matriz 3x4).
 *
 *   out[j] = suma_i  vin[i] * m[i*4 + j]     con j = 0,1,2
 *
 * es decir, vin tratado como vector FILA multiplicado por la matriz.
 */
void limeMatrix3x4RotateSkin(const float *m, const limeVECTOR3 *vin,
                             limeVECTOR3 *vout)
{
    float x = vin->x, y = vin->y, z = vin->z;

    vout->x = x * m[0] + y * m[4] + z * m[8];
    vout->y = x * m[1] + y * m[5] + z * m[9];
    vout->z = x * m[2] + y * m[6] + z * m[10];
}

/* Alias: el original es literalmente un salto a limeMatrix3x4RotateSkin. */
void RotVector(const float *m, const limeVECTOR3 *vin, limeVECTOR3 *vout)
{
    limeMatrix3x4RotateSkin(m, vin, vout);
}

/* ------------------------------------------------------------------ */
/* Proyeccion — el gancho del widescreen                                */
/* ------------------------------------------------------------------ */

/*
 * Construye una matriz de proyeccion perspectiva.
 *
 * El quinto argumento (zFar) llega por la PILA: AAPCS solo pasa los cuatro
 * primeros en r0-r3.
 *
 * Detalle que hay que respetar para reproducir el resultado: el factor
 * vertical se calcula en DOBLE precision y solo despues se convierte a float.
 * El original hace vcvt.f64.f32, llama a _cos y _sin (versiones de double), y
 * cierra con vcvt.f32.f64. Calcularlo todo en float da un resultado distinto.
 *
 * WIDESCREEN: `aspect` divide unicamente el termino X. Cambiarlo de 4:3 a 16:9
 * ensancha el campo de vision horizontal y deja el vertical intacto — que es
 * exactamente el comportamiento que se quiere. Es el unico punto que hay que
 * tocar en el port para tener widescreen correcto.
 */
void CreatePerspectiveMatrix(float *m, float fov, float aspect,
                             float zNear, float zFar)
{
    /* f = sin(fov) / (1 - cos(fov)) = cot(fov/2), el factor vertical clasico
     * de una proyeccion perspectiva. `fov` es el campo de vision VERTICAL
     * completo, en radianes.
     *
     * El orden de las llamadas importa: el binario invoca primero el stub
     * 0x000ddcd4 (_sin) y luego 0x000dd770 (_cos). */
    double f64 = sin((double)fov) / (1.0 - cos((double)fov));
    float  f   = (float)f64;

    for (int i = 0; i < 16; i++) {
        m[i] = 0.0f;
    }

    m[0]  = f / aspect;                              /* 0x00 */
    m[5]  = f;                                       /* 0x14 */
    m[10] = (zFar + zNear) / (zNear - zFar);         /* 0x28 */
    m[11] = 2.0f * zFar * zNear / (zNear - zFar);    /* 0x2c */
    m[14] = -1.0f;                                   /* 0x38 */
}
