/**
 * Based on the original WELL512a by Francois Panneton and Pierre L'Ecuyer,
 * University of Montreal, Canada, and Makoto Matsumoto, Hiroshima University,
 * Japan.
 *
 * Modified (with kind permission of the authors) from the original by
 * Jessica Jones and James Davenport, University of Bath, UK. (2017)
 */

/* Original copyright notice follows: */
/* ***************************************************************************** */
/* Copyright:      Francois Panneton and Pierre L'Ecuyer, University of Montreal */
/*                 Makoto Matsumoto, Hiroshima University                        */
/* Notice:         This code can be used freely for personal, academic,          */
/*                 or non-commercial purposes. For commercial purposes,          */
/*                 please contact P. L'Ecuyer at: lecuyer@iro.UMontreal.ca       */
/* ***************************************************************************** */

#ifndef H_WELLRNG512_
#define H_WELLRNG512_

#include <stdlib.h>
#include <stdint.h>

#define W 32
#define R 16
#define P 0
#define M1 13
#define M2 9
#define M3 5

#define FACT 2.32830643653869628906e-10

uint32_t * InitWELLRNG512a (uint32_t * init);
double WELLRNG512a(void);
double WELLRNG512a_double(void);
uint32_t WELLRNG512a_int(void);
uint32_t * WELLRNG512a_get_state(void);
unsigned long long WELL512a_get_iteration(void);

#endif  /* include guard H_WELLRNG512_ */
