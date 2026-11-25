#include "../physics/doppler.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static bool closeTo(double actual, double expected, double tolerance)
{
    return(fabs(actual - expected) <= tolerance);
}

static bool constantVelocity(double time, void *context, DopplerState *result)
{
    DopplerState initial = *(DopplerState *)context;
    *result = initial;
    result->position.x += initial.velocity.x * time;
    result->position.y += initial.velocity.y * time;
    result->position.z += initial.velocity.z * time;
    return(true);
}

static bool unavailable(double time, void *context, DopplerState *result)
{
    (void)time;
    (void)context;
    (void)result;
    return(false);
}

int main(void)
{
    const double c = DOPPLER_LIGHT_SPEED;
    DopplerState source = {0};
    DopplerState observer = {0};
    DopplerVec3 n = {1.0, 0.0, 0.0};
    double frequency = 0.0;
    double wavelength = 0.0;
    assert(dopplerShift(DOPPLER_LIGHT, c, source, observer, n, &frequency, &wavelength) == DOPPLER_OK);
    assert(frequency == 1.0 && wavelength == 1.0);
    source.velocity.x = c * 0.5;
    assert(dopplerShift(DOPPLER_LIGHT, c, source, observer, n, &frequency, &wavelength) == DOPPLER_OK);
    assert(closeTo(frequency, sqrt(3.0), 1e-14));
    source.velocity.x = -c * 0.5;
    assert(dopplerShift(DOPPLER_LIGHT, c, source, observer, n, &frequency, &wavelength) == DOPPLER_OK);
    assert(closeTo(wavelength, sqrt(3.0), 1e-14));
    source.velocity = (DopplerVec3){0.0, c * 0.6, 0.0};
    assert(dopplerShift(DOPPLER_LIGHT, c, source, observer, n, &frequency, &wavelength) == DOPPLER_OK);
    assert(closeTo(wavelength, 1.25, 1e-14));
    observer.velocity = (DopplerVec3){0.0, c * 0.6, 0.0};
    assert(dopplerShift(DOPPLER_LIGHT, c, source, observer, n, &frequency, &wavelength) == DOPPLER_OK);
    assert(closeTo(frequency, 1.0, 1e-14));
    source.velocity = (DopplerVec3){30.0, 0.0, 0.0};
    observer.velocity = (DopplerVec3){-20.0, 0.0, 0.0};
    assert(dopplerShift(DOPPLER_SOUND, 340.0, source, observer, n, &frequency, &wavelength) == DOPPLER_OK);
    assert(closeTo(frequency, 360.0 / 310.0, 1e-14));
    assert(closeTo(wavelength, 310.0 / 340.0, 1e-14));
    source.velocity.x = 340.0;
    assert(dopplerShift(DOPPLER_SOUND, 340.0, source, observer, n, &frequency, &wavelength) == DOPPLER_SPEED_OUT_OF_RANGE);
    assert(dopplerShift(DOPPLER_LIGHT, 340.0, source, observer, n, &frequency, &wavelength) == DOPPLER_INVALID_ARGUMENT);
    assert(dopplerShift(DOPPLER_SOUND, 340.0, source, observer, (DopplerVec3){0}, &frequency, &wavelength) == DOPPLER_INVALID_STATE);

    source = (DopplerState){{0, 0, 0}, {10, 0, 0}};
    observer = (DopplerState){{1000, 0, 0}, {0, 0, 0}};
    DopplerSolver solver = {0.0, 10.0, 1e-11, 1e-11, 100};
    DopplerObservation observation = {0};
    assert(dopplerObserve(DOPPLER_SOUND, 340.0, constantVelocity, &source, observer, 10.0, solver, &observation) == DOPPLER_OK);
    assert(closeTo(observation.delay, 900.0 / 330.0, 1e-10));
    assert(closeTo(observation.source.position.x, 10.0 * (10.0 - observation.delay), 1e-9));
    assert(fabs(observation.residual) <= solver.residualTolerance);
    assert(closeTo(observation.frequencyFactor, 340.0 / 330.0, 1e-14));
    assert(isnan(observation.redshift));
    source = (DopplerState){{0, 0, 0}, {0, 0, 0}};
    observer = (DopplerState){{c * 2.0, 0, 0}, {0, 0, 0}};
    DopplerSolver lightSolver = {0.0, 5.0, 1e-11, 1e-11, 100};
    assert(dopplerObserve(DOPPLER_LIGHT, c, constantVelocity, &source, observer, 10.0, lightSolver, &observation) == DOPPLER_OK);
    assert(closeTo(observation.delay, 2.0, 1e-10) && closeTo(observation.emissionTime, 8.0, 1e-10));
    assert(observation.redshift == 0.0 && observation.frequencyFactor == 1.0);
    source = (DopplerState){{0, 0, 0}, {10, 0, 0}};
    observer = (DopplerState){{1000, 0, 0}, {0, 0, 0}};
    solver.maxDelay = 1.0;
    assert(dopplerObserve(DOPPLER_SOUND, 340.0, constantVelocity, &source, observer, 10.0, solver, &observation) == DOPPLER_NO_BRACKET);
    solver.maxDelay = 10.0;
    solver.maxIterations = 1;
    assert(dopplerObserve(DOPPLER_SOUND, 340.0, constantVelocity, &source, observer, 10.0, solver, &observation) == DOPPLER_NO_CONVERGENCE);
    solver.maxIterations = 100;
    assert(dopplerObserve(DOPPLER_SOUND, 340.0, unavailable, NULL, observer, 10.0, solver, &observation) == DOPPLER_TRAJECTORY_ERROR);
    puts("Doppler physics checks passed");
    return(0);
}
