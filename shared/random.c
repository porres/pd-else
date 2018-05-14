// random number generator from supercollider
// coded by matt barber

#include <m_pd.h>
#include <stdint.h>
#include "random.h"

#define PINK_ARRAY_SIZE 32768

int32_t random_hash(int32_t inKey)
{
    // Thomas Wang's integer hash.
    // http://www.concentric.net/~Ttwang/tech/inthash.htm
    // a faster hash for integers. also very good.
    uint32_t hash = (uint32_t)inKey;
    hash += ~(hash << 15);
    hash ^=   hash >> 10;
    hash +=   hash << 3;
    hash ^=   hash >> 6;
    hash += ~(hash << 11);
    hash ^=   hash >> 16;
    return (int32_t)hash;
}


void random_init(t_random_state* rstate, float f)
{
	// humans tend to use small seeds - mess up the bits
	uint32_t seed = (uint32_t)random_hash((int)f);
	uint32_t *s1 = &rstate->s1;
	uint32_t *s2 = &rstate->s2;
	uint32_t *s3 = &rstate->s3;
	// initialize seeds using the given seed value taking care of
	// the requirements. The constants below are arbitrary otherwise
	*s1 = 1243598713U ^ seed; if (*s1 <  2) *s1 = 1243598713U;
	*s2 = 3093459404U ^ seed; if (*s2 <  8) *s2 = 3093459404U;
	*s3 = 1821928721U ^ seed; if (*s3 < 16) *s3 = 1821928721U;
	
}




