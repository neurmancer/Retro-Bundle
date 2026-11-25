# Doppler engine

A headless C library for computing retarded wave observations. It has no Raylib dependency and is not yet connected to the games. Include `doppler.h` and link `doppler.c` with the math library.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror physics/doppler.c tests/doppler_test.c -lm -o /tmp/doppler-test
/tmp/doppler-test
```

## Coordinates and units

Use a single inertial frame and consistent units: meters, seconds, and meters per second are recommended. For sound, the frame must be the rest frame of a homogeneous, stationary medium; wave speed is the speed in that medium. An observer's state is evaluated at reception. The source trajectory callback evaluates its state at the requested emission time. Velocities must be derivatives of positions in the same frame. The direction points from the emitted source position to the observer at reception.

`dopplerShift` computes a shift from supplied states and direction without calculating propagation delay. `dopplerObserve` finds the emission time with a bracketed bisection in delay seconds and then computes the shift. Callers provide a nonnegative delay bracket, absolute time and residual tolerances in seconds, and an iteration limit. Results are written only on success. Inspect the status on every call. A failed callback must return false; a callback should be deterministic and valid throughout the bracket.

`frequencyFactor` is observed frequency divided by emitted frequency. For light, `wavelengthFactor` is observed vacuum wavelength divided by emitted vacuum wavelength, and `redshift` is that factor minus one. For sound, `wavelengthFactor` is the wavelength in the medium divided by wave speed / source frequency; observer movement changes heard frequency, not wavelength in the medium. Sound `redshift` is NaN. Multiply a source frequency by `frequencyFactor` to obtain the received frequency.

## Models and limitations

- Sound uses a classical, non-dispersive wave in a homogeneous stationary medium. Both source and observer must have speeds below the wave speed throughout the relevant interval. Wind, supersonic motion, shock waves, reflections, attenuation, and finite-size sources are outside this model. For a moving medium, transform the entire trajectory and observer state into the medium frame before calling it.
- Light uses special relativity in flat spacetime with exactly `DOPPLER_LIGHT_SPEED`. It includes both source and observer Lorentz factors using their full speeds. Gravity, cosmological redshift, acceleration-induced changes during emission, and refraction are not included.
- The callback must describe a continuous trajectory whose speed remains below the propagation speed over the entire bracket. The engine checks sampled states, not every point in between; violating the requirement can yield multiple or missed roots.
- Use local elapsed times close to zero. The solver detects some timestamp precision failures, but very large epochs or distances can still lose precision in positions and time differences. It does not yet implement the differential-path/reference-arrival technique needed for high-precision astronomical distances. No scientific accuracy budget has been established.
- Acoustic frequency shifting alone is not audio resampling or spatialization. A racing game still needs an audio playback pipeline that applies the computed factor smoothly, with appropriate attenuation and pitch limits.

The equations follow the sound model described by [OpenStax](https://openstax.org/books/university-physics-volume-1/pages/17-7-the-doppler-effect) and the flat-space light model in the [design document](../GALAGA_DESIGN.md). The need for full three-dimensional relativistic motion in precision work is discussed by [Wright](https://arxiv.org/abs/1812.06748).

## Arbitrary-precision direction

When a higher-precision Doppler path is needed, use [Neuro Software Distribution's big-nums](https://github.com/Neuro-Software-Distribution/big-nums) as the intended C arithmetic library. It is not a dependency of the current double-precision engine. The API reviewed at commit `c51b3465a3d6773a1c2199dfdd323a1ae507037b` provides `BigFloat` addition, subtraction, multiplication, division, and square root, with configurable precision in 32-bit limbs. Its own test suite passed at that revision.

A future high-precision interface needs high-precision inputs, trajectory evaluation, root solving, and outputs throughout; converting existing double results after calculation cannot restore lost digits. Respect `BigFloat` initialization, destruction, status returns, destination precision, and truncation-toward-zero behavior. The reviewed library does not provide a decimal float parser, selectable rounding modes, or trigonometric functions, so input conversion and any Kepler-orbit functions need separate design. Verify numerical convergence against independent analytic cases and, where useful, a separately implemented reference before making accuracy claims.
