#ifndef RETRO_DOPPLER_H
#define RETRO_DOPPLER_H

#include <stdbool.h>

#define DOPPLER_LIGHT_SPEED 299792458.0

typedef struct {
    double x;
    double y;
    double z;
} DopplerVec3;

typedef struct {
    DopplerVec3 position;
    DopplerVec3 velocity;
} DopplerState;

typedef enum {
    DOPPLER_OK,
    DOPPLER_INVALID_ARGUMENT,
    DOPPLER_INVALID_STATE,
    DOPPLER_SPEED_OUT_OF_RANGE,
    DOPPLER_NO_BRACKET,
    DOPPLER_NO_CONVERGENCE,
    DOPPLER_TRAJECTORY_ERROR,
    DOPPLER_PRECISION_LIMIT
} DopplerStatus;

typedef enum {
    DOPPLER_SOUND,
    DOPPLER_LIGHT
} DopplerModel;

typedef bool (*DopplerTrajectory)(double time, void *context, DopplerState *state);

typedef struct {
    double minDelay;
    double maxDelay;
    double timeTolerance;
    double residualTolerance;
    unsigned int maxIterations;
} DopplerSolver;

typedef struct {
    double emissionTime;
    double delay;
    double residual;
    unsigned int iterations;
    DopplerState source;
    DopplerVec3 direction;
    double frequencyFactor;
    double wavelengthFactor;
    double redshift;
} DopplerObservation;

DopplerStatus dopplerShift(DopplerModel model, double waveSpeed, DopplerState source,
                           DopplerState observer, DopplerVec3 direction,
                           double *frequencyFactor, double *wavelengthFactor);
DopplerStatus dopplerObserve(DopplerModel model, double waveSpeed, DopplerTrajectory trajectory,
                             void *context, DopplerState observer, double receptionTime,
                             DopplerSolver solver, DopplerObservation *observation);
const char *dopplerStatusString(DopplerStatus status);

#endif
