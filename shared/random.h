// random number generator from supercollider, coded by Matt Barber

#include <stdint.h>

typedef struct _random_state
{
	uint32_t s1;
	uint32_t s2;
	uint32_t s3;
} t_random_state;


int32_t random_hash(int32_t inKey);

void random_init(t_random_state* rstate, float f);

inline uint32_t random_trand(uint32_t* s1, uint32_t* s2, uint32_t* s3 )
{
	// This function is provided for speed in inner loops where the
	// state variables are loaded into registers.
	// Thus updating the instance variables can
	// be postponed until the end of the loop.
	*s1 = ((*s1 &  (uint32_t)- 2) << 12) ^ (((*s1 << 13) ^  *s1) >> 19);
	*s2 = ((*s2 &  (uint32_t)- 8) <<  4) ^ (((*s2 <<  2) ^  *s2) >> 25);
	*s3 = ((*s3 &  (uint32_t)-16) << 17) ^ (((*s3 <<  3) ^  *s3) >> 11);
	return *s1 ^ *s2 ^ *s3;
}

inline float random_frand(uint32_t* s1, uint32_t* s2, uint32_t* s3)
{
	// return a float from -1.0 to +0.999...
	union { uint32_t i; float f; } u;		// union for floating point conversion of result
	u.i = 0x40000000 | (random_trand(s1, s2, s3) >> 9);
	return u.f - 3.f;
}
