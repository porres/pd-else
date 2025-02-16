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
#define t_complex _Complex t_float

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
    coeffs->complex_poles[0] = -9.999999999999968 + 17.320508075688757 * I;
    coeffs->complex_poles[1] = -5562.019693104996 + 7721.557745942449 * I;
    coeffs->complex_poles[2] = -3936.754373279431 + 13650.191094084097 * I;
    coeffs->complex_poles[3] = -2348.1627071584026 + 17360.269257396852 * I;
    coeffs->complex_poles[4] = -1177.6059328793112 + 19350.807275259638 * I;
    coeffs->complex_poles[5] = -351.8405852427604 + 20192.24393379015 * I;

    coeffs->real_poles[0] = -20.000000000000025;
    coeffs->real_poles[1] = -6298.035731484052;

    coeffs->complex_coeffs_direct[0] = -20.13756830149893 - 11.467013478535181 * I;
    coeffs->complex_coeffs_direct[1] = -16453.812748230637 - 7298.835752208561 * I;
    coeffs->complex_coeffs_direct[2] = 7771.069750908201 + 9555.31023870685 * I;
    coeffs->complex_coeffs_direct[3] = -825.3820172192254 - 6790.877301990311 * I;
    coeffs->complex_coeffs_direct[4] = -1529.6770476201002 + 2560.1909145592135 * I;
    coeffs->complex_coeffs_direct[5] = 755.260843981231 - 310.336256340709 * I;

    coeffs->real_coeffs_direct[0] = -20.138060433528526;
    coeffs->real_coeffs_direct[1] = 10325.52721970985;

    coeffs->complex_coeffs_blep[0] = -0.1375683014988951 + 0.0799919052573852 * I;
    coeffs->complex_coeffs_blep[1] = -16453.812748230637 - 7298.835752208561 * I;
    coeffs->complex_coeffs_blep[2] = 7771.069750908201 + 9555.31023870685 * I;
    coeffs->complex_coeffs_blep[3] = -825.3820172192254 - 6790.877301990311 * I;
    coeffs->complex_coeffs_blep[4] = -1529.6770476201002 + 2560.1909145592135 * I;
    coeffs->complex_coeffs_blep[5] = 755.260843981231 - 310.336256340709 * I;

    coeffs->real_coeffs_blep[0] = -0.13806043352856534;
    coeffs->real_coeffs_blep[1] = 10325.52721970985;
}

static void elliptic_blep_add_pole(t_elliptic_blep *blep, size_t index, t_complex pole, t_complex coeff, t_float angular_frequency)
{
    blep->coeffs[index] = coeff*angular_frequency;

    // Set up partial powers of the pole (so we can move forward/back by fractional samples)
    for (size_t s = 0; s <= partial_step_count; ++s) {
        t_float partial = ((t_float)s)/partial_step_count;
        blep->partial_step_poles[s][index] = cexp(partial*pole*angular_frequency);
    }

    // Set up
    t_complex blepCoeff = 1.0;
    for (size_t o = 0; o <= max_blep_order; ++o) {
        blep->blep_coeffs[o][index] = blepCoeff;
        blepCoeff /= pole*angular_frequency; // factor from integrating
    }
}

static void elliptic_blep_create(t_elliptic_blep *blep, int direct, t_float srate) {
    t_elliptic_blep_coeffs s_plane_coeffs; // S-plane (continuous time) filter
    elliptic_blep_coeffs_init(&s_plane_coeffs);

    t_float angular_frequency = (2*M_PI)/srate;

    // For now, just cast real poles to complex ones
    const t_float *realCoeffs = (direct ? s_plane_coeffs.real_coeffs_direct : s_plane_coeffs.real_coeffs_blep);
    for (size_t i = 0; i < real_count; ++i) {
        elliptic_blep_add_pole(blep, i, s_plane_coeffs.real_poles[i], realCoeffs[i], angular_frequency);
    }
    const t_complex *complexCoeffs = (direct ? s_plane_coeffs.complex_coeffs_direct : s_plane_coeffs.complex_coeffs_blep);
    for (size_t i = 0; i < complex_count; ++i) {
        elliptic_blep_add_pole(blep, i + real_count, s_plane_coeffs.complex_poles[i], complexCoeffs[i], angular_frequency);
    }
}

static t_float elliptic_blep_get(t_elliptic_blep* blep) {
    t_float sum = 0;
    for (size_t i = 0; i < count; ++i) {
        sum += creal(blep->state[i]*blep->coeffs[i]);
    }
    return sum;
}

static void elliptic_blep_add(t_elliptic_blep *blep, t_float amount, size_t blep_order) {
    if (blep_order > max_blep_order) return;
    t_complex* bc = blep->blep_coeffs[blep_order];

    for (size_t i = 0; i < count; ++i) {
        blep->state[i] += amount*bc[i];
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
        t_complex lerp_pole = low_poles[i] + (high_poles[i] - low_poles[i])*frac_index;
        blep->state[i] += bc[i]*lerp_pole*amount;
    }
}

static void elliptic_blep_step(t_elliptic_blep *blep) {
    t_float sum = 0;
    const t_complex *poles = blep->partial_step_poles[partial_step_count-1];
    for (size_t i = 0; i < count; ++i) {
        sum += creal(blep->state[i]*blep->coeffs[i]);
        blep->state[i] *= poles[i];
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
        t_complex lerp_pole = low_poles[i] + (high_poles[i] - low_poles[i])*frac_index;
        blep->state[i] *= lerp_pole;
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
