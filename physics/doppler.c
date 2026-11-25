#include "doppler.h"
#include <math.h>
#include <stddef.h>

static bool finiteVec(DopplerVec3 v)
{
    return(isfinite(v.x) && isfinite(v.y) && isfinite(v.z));
}

static bool finiteState(DopplerState state)
{
    return(finiteVec(state.position) && finiteVec(state.velocity));
}

static double magnitude(DopplerVec3 v)
{
    return(hypot(hypot(v.x, v.y), v.z));
}

static double dot(DopplerVec3 a, DopplerVec3 b)
{
    return(a.x * b.x + a.y * b.y + a.z * b.z);
}

static DopplerStatus validateModel(DopplerModel model, double speed)
{
    if ((model != DOPPLER_SOUND && model != DOPPLER_LIGHT) || !isfinite(speed) || speed <= 0.0) { return(DOPPLER_INVALID_ARGUMENT); }
    if (model == DOPPLER_LIGHT && speed != DOPPLER_LIGHT_SPEED) { return(DOPPLER_INVALID_ARGUMENT); }
    return(DOPPLER_OK);
}

DopplerStatus dopplerShift(DopplerModel model, double waveSpeed, DopplerState source,
                           DopplerState observer, DopplerVec3 direction,
                           double *frequencyFactor, double *wavelengthFactor)
{
    DopplerStatus status = validateModel(model, waveSpeed);
    if (status != DOPPLER_OK) { return(status); }
    if (frequencyFactor == NULL || wavelengthFactor == NULL) { return(DOPPLER_INVALID_ARGUMENT); }
    if (!finiteState(source) || !finiteState(observer) || !finiteVec(direction)) { return(DOPPLER_INVALID_STATE); }
    double length = magnitude(direction);
    if (!isfinite(length) || length <= 0.0) { return(DOPPLER_INVALID_STATE); }
    direction = (DopplerVec3){direction.x / length, direction.y / length, direction.z / length};
    double sourceBeta = magnitude(source.velocity) / waveSpeed;
    double observerBeta = magnitude(observer.velocity) / waveSpeed;
    if (!isfinite(sourceBeta) || !isfinite(observerBeta) || sourceBeta >= 1.0 || observerBeta >= 1.0) { return(DOPPLER_SPEED_OUT_OF_RANGE); }
    double sourceTerm = 1.0 - dot(source.velocity, direction) / waveSpeed;
    double observerTerm = 1.0 - dot(observer.velocity, direction) / waveSpeed;
    double frequency = observerTerm / sourceTerm;
    if (model == DOPPLER_LIGHT) {
        double sourceGammaInverse = sqrt((1.0 - sourceBeta) * (1.0 + sourceBeta));
        double observerGammaInverse = sqrt((1.0 - observerBeta) * (1.0 + observerBeta));
        frequency *= sourceGammaInverse / observerGammaInverse;
    }
    if (!isfinite(frequency) || frequency <= 0.0) { return(DOPPLER_INVALID_STATE); }
    double wavelength = model == DOPPLER_SOUND ? sourceTerm : 1.0 / frequency;
    if (!isfinite(wavelength)) { return(DOPPLER_INVALID_STATE); }
    *frequencyFactor = frequency;
    *wavelengthFactor = wavelength;
    return(DOPPLER_OK);
}

static DopplerStatus residualAt(double delay, double receptionTime, double speed,
                                DopplerTrajectory trajectory, void *context,
                                DopplerState observer, double *residual, DopplerState *source)
{
    double emission = receptionTime - delay;
    if (!isfinite(emission) || (delay > 0.0 && emission == receptionTime)) { return(DOPPLER_PRECISION_LIMIT); }
    if (!trajectory(emission, context, source)) { return(DOPPLER_TRAJECTORY_ERROR); }
    if (!finiteState(*source)) { return(DOPPLER_INVALID_STATE); }
    if (magnitude(source->velocity) >= speed) { return(DOPPLER_SPEED_OUT_OF_RANGE); }
    DopplerVec3 displacement = {observer.position.x - source->position.x, observer.position.y - source->position.y, observer.position.z - source->position.z};
    double distance = magnitude(displacement);
    if (!isfinite(distance)) { return(DOPPLER_INVALID_STATE); }
    *residual = delay - distance / speed;
    if (!isfinite(*residual)) { return(DOPPLER_INVALID_STATE); }
    return(DOPPLER_OK);
}

DopplerStatus dopplerObserve(DopplerModel model, double waveSpeed, DopplerTrajectory trajectory,
                             void *context, DopplerState observer, double receptionTime,
                             DopplerSolver solver, DopplerObservation *observation)
{
    DopplerStatus status = validateModel(model, waveSpeed);
    if (status != DOPPLER_OK) { return(status); }
    if (trajectory == NULL || observation == NULL || !isfinite(receptionTime) ||
        !isfinite(solver.minDelay) || !isfinite(solver.maxDelay) || solver.minDelay < 0.0 ||
        solver.maxDelay < solver.minDelay || !isfinite(solver.timeTolerance) || solver.timeTolerance <= 0.0 ||
        !isfinite(solver.residualTolerance) || solver.residualTolerance <= 0.0 || solver.maxIterations == 0) { return(DOPPLER_INVALID_ARGUMENT); }
    if (!finiteState(observer)) { return(DOPPLER_INVALID_STATE); }
    if (magnitude(observer.velocity) >= waveSpeed) { return(DOPPLER_SPEED_OUT_OF_RANGE); }

    double low = solver.minDelay;
    double high = solver.maxDelay;
    double lowResidual = 0.0;
    double highResidual = 0.0;
    DopplerState source = {0};
    status = residualAt(low, receptionTime, waveSpeed, trajectory, context, observer, &lowResidual, &source);
    if (status != DOPPLER_OK) { return(status); }
    status = residualAt(high, receptionTime, waveSpeed, trajectory, context, observer, &highResidual, &source);
    if (status != DOPPLER_OK) { return(status); }
    if (lowResidual > 0.0 || highResidual < 0.0) { return(DOPPLER_NO_BRACKET); }

    double delay = low;
    double residual = lowResidual;
    unsigned int iterations = 0;
    bool converged = false;
    for (unsigned int i = 0; i < solver.maxIterations; i++) {
        delay = low + (high - low) * 0.5;
        status = residualAt(delay, receptionTime, waveSpeed, trajectory, context, observer, &residual, &source);
        if (status != DOPPLER_OK) { return(status); }
        iterations = i + 1;
        if (residual == 0.0 || (fabs(residual) <= solver.residualTolerance && (high - low) * 0.5 <= solver.timeTolerance)) { converged = true; break; }
        if (delay == low || delay == high) { break; }
        if (residual < 0.0) { low = delay; }
        else { high = delay; }
    }
    if (!converged) { return(DOPPLER_NO_CONVERGENCE); }
    if (fabs((receptionTime - (receptionTime - delay)) - delay) > solver.timeTolerance) { return(DOPPLER_PRECISION_LIMIT); }
    DopplerVec3 direction = {observer.position.x - source.position.x, observer.position.y - source.position.y, observer.position.z - source.position.z};
    double distance = magnitude(direction);
    if (!isfinite(distance) || distance <= 0.0) { return(DOPPLER_INVALID_STATE); }
    direction = (DopplerVec3){direction.x / distance, direction.y / distance, direction.z / distance};
    double frequencyFactor = 0.0;
    double wavelengthFactor = 0.0;
    status = dopplerShift(model, waveSpeed, source, observer, direction, &frequencyFactor, &wavelengthFactor);
    if (status != DOPPLER_OK) { return(status); }
    *observation = (DopplerObservation){receptionTime - delay, delay, residual, iterations, source, direction, frequencyFactor, wavelengthFactor, model == DOPPLER_LIGHT ? wavelengthFactor - 1.0 : NAN};
    return(DOPPLER_OK);
}

const char *dopplerStatusString(DopplerStatus status)
{
    switch (status) {
        case DOPPLER_OK: return("ok");
        case DOPPLER_INVALID_ARGUMENT: return("invalid argument");
        case DOPPLER_INVALID_STATE: return("invalid state");
        case DOPPLER_SPEED_OUT_OF_RANGE: return("speed outside model range");
        case DOPPLER_NO_BRACKET: return("emission time not bracketed");
        case DOPPLER_NO_CONVERGENCE: return("solver did not converge");
        case DOPPLER_TRAJECTORY_ERROR: return("trajectory unavailable");
        case DOPPLER_PRECISION_LIMIT: return("time precision insufficient");
        default: return("unknown status");
    }
}
