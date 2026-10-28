//
//  main.cpp
//  elliptic-blep
//
//  Created by Timothy Schoen on 13/11/2024.
//

#include <complex.h>
#include <stdint.h>
#include <stdlib.h>

#define _USE_MATH_DEFINES
#include <math.h>

#define t_float float
#if _MSC_VER
#define t_complex _Fcomplex
#else
#define t_complex _Complex t_float
#endif

#ifndef CMPLXF
#define CMPLXF(re,im) (t_complex){re,im}
#endif

#if defined(_MSC_VER)
    #include <complex.h>
    #define COMPLEX_ADD(a, b) _FCbuild(crealf(a) + crealf(b), cimagf(a) + cimagf(b))
    #define COMPLEX_SUB(a, b) _FCbuild(crealf(a) - crealf(b), cimagf(a) - cimagf(b))
    #define COMPLEX_MUL(a, b) _FCmulcc(a, b)
    #define COMPLEX_SCALE(a, b) _FCmulcr(a, b)
    static t_complex COMPLEX_DIV(t_complex in1, t_complex in2)
    {
        double real = (crealf(in1) * crealf(in2) + cimagf(in1) * cimagf(in2)) / (crealf(in2) * crealf(in2) + cimagf(in2) * cimagf(in2));
        double imag = (cimagf(in1) * crealf(in2) - crealf(in1) * cimagf(in2)) / (crealf(in2) * crealf(in2) + cimagf(in2) * cimagf(in2));
        return CMPLXF(real,imag);
    }
#else
    #include <complex.h>
    #define COMPLEX_ADD(a, b) ((a) + (b))
    #define COMPLEX_SUB(a, b) ((a) - (b))
    #define COMPLEX_MUL(a, b) ((a) * (b))
    #define COMPLEX_SCALE(a, b) ((a) * (b))
    #define COMPLEX_DIV(a, b) ((a) / (b))
#endif

#define partial_step_count 127
#define max_integrals 3
#define max_blep_order max_integrals
#define complex_count 6
#define real_count 2
#define count (complex_count + real_count)

typedef struct {
    t_complex complex_poles[complex_count];
    t_complex real_poles[complex_count];
    // Coeffs for direct bandlimited synthesis of a polynomial-segment waveform
    t_complex complex_coeffs_direct[complex_count];
    t_float real_coeffs_direct[real_count];
    // Coeffs for cancelling the aliasing from discontinuities in an existing waveform
    t_complex complex_coeffs_blep[complex_count];
    t_float real_coeffs_blep[real_count];
} t_elliptic_blep_coeffs;

typedef struct
{
    t_complex coeffs[count];
    t_complex state[count];
    t_complex blep_coeffs[max_blep_order+1][count];

    // Lookup table for powf(pole, fractional)
    t_complex partial_step_poles[partial_step_count+1][count];
} t_elliptic_blep;

static void elliptic_blep_coeffs_init (t_elliptic_blep_coeffs *coeffs)
{
    coeffs->complex_poles[0] = CMPLXF(-9.999999999999968, 17.320508075688757);
    coeffs->complex_poles[1] = CMPLXF(-5562.019693104996, 7721.557745942449);
    coeffs->complex_poles[2] = CMPLXF(-3936.754373279431, 13650.191094084097);
    coeffs->complex_poles[3] = CMPLXF(-2348.1627071584026, 17360.269257396852);
    coeffs->complex_poles[4] = CMPLXF(-1177.6059328793112, 19350.807275259638);
    coeffs->complex_poles[5] = CMPLXF(-351.8405852427604, 20192.24393379015);

    coeffs->real_poles[0] = CMPLXF(-20.000000000000025, 0);
    coeffs->real_poles[1] = CMPLXF(-6298.035731484052, 0);

    coeffs->complex_coeffs_direct[0] = CMPLXF(-20.13756830149893,  -11.467013478535181);
    coeffs->complex_coeffs_direct[1] = CMPLXF(-16453.812748230637, -7298.835752208561);
    coeffs->complex_coeffs_direct[2] = CMPLXF(7771.069750908201, 9555.31023870685);
    coeffs->complex_coeffs_direct[3] = CMPLXF(-825.3820172192254, -6790.877301990311);
    coeffs->complex_coeffs_direct[4] = CMPLXF(-1529.6770476201002, 2560.1909145592135);
    coeffs->complex_coeffs_direct[5] = CMPLXF(755.260843981231, -310.336256340709);

    coeffs->real_coeffs_direct[0] = -20.138060433528526;
    coeffs->real_coeffs_direct[1] = 10325.52721970985;

    coeffs->complex_coeffs_blep[0] = CMPLXF(-0.1375683014988951, 0.0799919052573852);
    coeffs->complex_coeffs_blep[1] = CMPLXF(-16453.812748230637, -7298.835752208561);
    coeffs->complex_coeffs_blep[2] = CMPLXF(7771.069750908201, 9555.31023870685);
    coeffs->complex_coeffs_blep[3] = CMPLXF(-825.3820172192254, -6790.877301990311);
    coeffs->complex_coeffs_blep[4] = CMPLXF(-1529.6770476201002, 2560.1909145592135);
    coeffs->complex_coeffs_blep[5] = CMPLXF(755.260843981231, -310.336256340709);

    coeffs->real_coeffs_blep[0] = -0.13806043352856534;
    coeffs->real_coeffs_blep[1] = 10325.52721970985;
}

static void elliptic_blep_add_pole(t_elliptic_blep *blep, size_t index, t_complex pole, t_complex coeff, t_float angular_frequency)
{
    blep->coeffs[index] = COMPLEX_SCALE(coeff, angular_frequency);

    // Set up partial powers of the pole (so we can move forward/back by fractional samples)
    for (size_t s = 0; s <= partial_step_count; ++s) {
        t_float partial = ((t_float)s)/partial_step_count;
        blep->partial_step_poles[s][index] = cexpf(COMPLEX_SCALE(pole, partial*angular_frequency));
    }

    // Set up
    t_complex blepCoeff = CMPLXF(1.0, 0.0f);
    for (size_t o = 0; o <= max_blep_order; ++o) {
        blep->blep_coeffs[o][index] = blepCoeff;
        blepCoeff = COMPLEX_DIV(blepCoeff, COMPLEX_SCALE(pole, angular_frequency)); // factor from integrating
    }
}

static void elliptic_blep_create(t_elliptic_blep *blep, int direct, t_float srate) {
    t_elliptic_blep_coeffs s_plane_coeffs; // S-plane (continuous time) filter
    elliptic_blep_coeffs_init(&s_plane_coeffs);

    t_float angular_frequency = (2*M_PI)/srate;

    // For now, just cast real poles to complex ones
    const t_float *realCoeffs = (direct ? s_plane_coeffs.real_coeffs_direct : s_plane_coeffs.real_coeffs_blep);
    for (size_t i = 0; i < real_count; ++i) {
        t_complex real = CMPLXF(realCoeffs[i], 0);
        elliptic_blep_add_pole(blep, i, s_plane_coeffs.real_poles[i], real, angular_frequency);
    }
    const t_complex *complexCoeffs = (direct ? s_plane_coeffs.complex_coeffs_direct : s_plane_coeffs.complex_coeffs_blep);
    for (size_t i = 0; i < complex_count; ++i) {
        elliptic_blep_add_pole(blep, i + real_count, s_plane_coeffs.complex_poles[i], complexCoeffs[i], angular_frequency);
    }
}

static t_float elliptic_blep_get(t_elliptic_blep* blep) {
    t_float sum = 0;
    for (size_t i = 0; i < count; ++i) {
        sum += crealf(COMPLEX_MUL(blep->state[i], blep->coeffs[i]));
    }
    return sum;
}

static void elliptic_blep_add(t_elliptic_blep *blep, t_float amount, size_t blep_order) {
    if (blep_order > max_blep_order) return;
    t_complex* bc = blep->blep_coeffs[blep_order];
    
    for (size_t i = 0; i < count; ++i) {
        blep->state[i] = COMPLEX_ADD(blep->state[i], COMPLEX_SCALE(bc[i], amount));
    }
}

static void elliptic_blep_add_in_past(t_elliptic_blep *blep, t_float amount, size_t blep_order, t_float samplesInPast) {
    if (blep_order > max_blep_order) return;

    samplesInPast = samplesInPast < 1.0 ? samplesInPast : 0.999999;

    t_complex *bc = blep->blep_coeffs[blep_order];

    t_float table_index = samplesInPast*partial_step_count;

    size_t int_index = floor(table_index);
    t_float frac_index = table_index - floor(table_index);

    // move the pulse along in time, the same way as state progresses in .step()
    t_complex *low_poles = blep->partial_step_poles[int_index];
    t_complex *high_poles = blep->partial_step_poles[int_index + 1];
    for (size_t i = 0; i < count; ++i) {
        t_complex lerp_pole = COMPLEX_ADD(low_poles[i], COMPLEX_SCALE(COMPLEX_SUB(high_poles[i], low_poles[i]), frac_index));
        blep->state[i] = COMPLEX_ADD(blep->state[i], COMPLEX_MUL(bc[i], COMPLEX_SCALE(lerp_pole, amount)));
    }
}

static void elliptic_blep_step(t_elliptic_blep *blep) {
    t_float sum = 0;
    const t_complex *poles = blep->partial_step_poles[partial_step_count-1];
    for (size_t i = 0; i < count; ++i) {
        sum += crealf(COMPLEX_MUL(blep->state[i], blep->coeffs[i]));
        blep->state[i] = COMPLEX_MUL(blep->state[i], poles[i]);
    }
}

static void elliptic_blep_step_samples(t_elliptic_blep *blep, t_float samples) {
    t_float table_index = samples*partial_step_count;
    size_t int_index = floor(table_index);
    t_float frac_index = table_index - floor(table_index);
    while (int_index >= partial_step_count) {
        elliptic_blep_step(blep);
        int_index -= partial_step_count;
    }

    t_complex *low_poles = blep->partial_step_poles[int_index];
    t_complex *high_poles = blep->partial_step_poles[int_index + 1];

    for (size_t i = 0; i < count; ++i) {
        t_complex lerp_pole = COMPLEX_ADD(low_poles[i], COMPLEX_SCALE(COMPLEX_SUB(high_poles[i], low_poles[i]), frac_index));
        blep->state[i] = COMPLEX_MUL(blep->state[i], lerp_pole);
    }
}

static t_float phasewrap(t_float phase){
    while(phase < 0.0)
        phase += 1.0;
    while(phase >= 1.0)
        phase -= 1.0;
    return(phase);
}

static t_float saw(t_float phase, t_float increment)
{
    return 1.0 - 2.0 * phase;
}
static t_float square(t_float phase, t_float increment)
{
    return (phase < 0.5) ? 1.0 : -1.0;
}
static t_float triangle(t_float phase, t_float increment)
{
    return 2.0 * fabs(2.0 * (phase - floor(phase + 0.5))) - 1.0;
}

static t_float sine(t_float phase, t_float increment)
{
    return sin(phase * M_PI * 2.0);
}

static t_float imp(t_float phase, t_float increment)
{
    return (phase < increment);
}
