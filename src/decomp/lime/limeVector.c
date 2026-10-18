/*
 * limeVector.c — src/lime/common/limeVector.cpp
 *
 * C limpio, escrito a mano a partir de:
 *   - el desensamblado de la slice armv7 (0x00061078, 0x000610a4)
 *   - la salida cruda de Ghidra en _raw/limeVector.c
 *   - y VERIFICADO contra el oraculo: tests/test_limevector.c, 10/10.
 *
 * ATENCION — no confiar en _raw/limeVector.c para estas dos funciones.
 * El compilador de EA emitio matematica escalar usando instrucciones NEON de
 * 2 carriles sobre registros D (`vmul.f32 d6, d6, d6`). Ghidra las modela como
 * `FloatVectorMult(...)` opaco y pierde el solapamiento entre d6 y el par
 * s12/s13: su version de _Len ni siquiera hace la raiz cuadrada y devuelve una
 * variable sin inicializar. Lo que hay aqui es lo que el hardware hace de
 * verdad, comprobado con el oraculo.
 */

#include "lime.h"

#include <math.h>

/*
 * Longitud euclidea de un vector de 3 componentes.
 *
 * Original: vmul/vadd sobre carriles NEON y un vsqrt.f32 final. El resultado
 * vuelve en r0 como patron de bits porque el binario usa AAPCS soft-float.
 */
float Len(const limeVECTOR3 *v)
{
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

/*
 * Normaliza el vector in situ.
 *
 * El original resuelve el caso degenerado con un bloque IT:
 *
 *     vcmp.f32  s14, #0
 *     vmrs      apsr_nzcv, fpscr
 *     itt ne
 *     vmovne.f32 s12, #1.0
 *     vdivne.f32 s12, s12, s14
 *     it eq
 *     vmoveq.f32 s12, #1.0
 *
 * es decir: si la longitud es 0 el factor de escala es 1.0 y el vector se
 * queda como esta, en lugar de dividir por cero. Verificado: Normalise(0,0,0)
 * devuelve (0,0,0) sin producir NaN.
 */
void Normalise(limeVECTOR3 *v)
{
    float len = Len(v);
    float scale = (len != 0.0f) ? (1.0f / len) : 1.0f;

    v->x *= scale;
    v->y *= scale;
    v->z *= scale;
}
