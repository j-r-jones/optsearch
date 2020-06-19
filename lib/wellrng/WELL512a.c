/**
 * Based on the original WELL512a by Francois Panneton and Pierre L'Ecuyer,
 * University of Montreal, Canada, and Makoto Matsumoto, Hiroshima University,
 * Japan.
 *
 * Modified (with kind permission of the authors) from the original by
 * Jessica Jones and James Davenport, University of Bath, UK. (2017)
 */
#include "WELL512a.h"

#define MAT0POS(t,v) (v^(v>>t))
#define MAT0NEG(t,v) (v^(v<<(-(t))))
#define MAT3NEG(t,v) (v<<(-(t)))
#define MAT4NEG(t,b,v) (v ^ ((v<<(-(t))) & b))

#define V0            STATE[state_i                   ]
#define VM1           STATE[(state_i+M1) & 0x0000000fU]
#define VM2           STATE[(state_i+M2) & 0x0000000fU]
#define VM3           STATE[(state_i+M3) & 0x0000000fU]
#define VRm1          STATE[(state_i+15) & 0x0000000fU]
#define VRm2          STATE[(state_i+14) & 0x0000000fU]
#define newV0         STATE[(state_i+15) & 0x0000000fU]
#define newV1         STATE[state_i                 ]
#define newVRm1       STATE[(state_i+14) & 0x0000000fU]

static uint32_t state_i;
static uint32_t STATE[R];
static uint32_t z0, z1, z2;

static unsigned long long iteration;


unsigned long long WELL512a_get_iteration(void)
{
    /* The -1 is because our iterator is counting from 1, not from 0.  This
     * makes reasoning elsewhere difficult because that's not what we expect
     * in C */
    return (iteration - 1 );
}

uint32_t * InitWELLRNG512a(uint32_t * init)
{
    int j = 0;
    iteration = 0ULL;
    state_i = 0;
    for (j = 0; j < R; j++) {
        STATE[j] = init[j];
    }

    return STATE;
}

uint32_t WELLRNG512a_int(void)
{
    iteration++;
    z0    = VRm1;
    z1    = MAT0NEG (-16,V0)^ MAT0NEG (-15, VM1);
    z2    = MAT0POS (11, VM2);
    newV1 = z1 ^ z2; 
    newV0 = MAT0NEG(-2,z0) ^ MAT0NEG(-18,z1) ^ MAT3NEG(-28,z2) ^ MAT4NEG(-5,0xda442d24U,newV1);
    state_i = (state_i + 15) & 0x0000000fU;
    return STATE[state_i];
}

uint32_t * WELLRNG512a_get_state(void)
{
    return STATE;
}

double WELLRNG512a_double(void)
{
    /*
     * Casting is not ideal, as you can't get the full range of values of
     * double from a uint32_t and you certainly won't get any negative
     * numbers.  This function is potentially confusing to those using it as a
     * result.  The cast remains as that is what the original authors did, but
     * it should be fixed.
     *
     * TODO FIXME
     */
    return ((double) WELLRNG512a_int());
}

double WELLRNG512a(void)
{
    /*
     * This function is here to keep the API of the original implementation.
     *
     * The "* FACT" converts the resulting double to a number in the real
     * interval [0,1), but JRJ is not convinced this is the best way to do it.
     *
     * Assuming they've tailored their constant to yield [0,1) (so it's
     * something like [0,0.9999999999] in practice), the approach used here
     * seems risky; It guarantees that entropy is lost (a fraction of a bit,
     * probably). Worse, one might end up compiling on a system which silently
     * truncates their magic constant in ways the original authors haven't
     * accounted for.  This would be extremely bad.
     *
     * The only way to avoid this is to normalise at point of use, rather than
     * generation, where the user hopefully knows what they are doing ..
     *
     * (DJS agrees.)
     */
    return WELLRNG512a_double() * FACT;

}
