# Overdrive

A raylib highway driving game. Install a C compiler, make, pkg-config, and raylib, then run:

```sh
make
./bin/overdrive
```

Nearby traffic has a synthesized engine sound whose pitch uses the Doppler physics engine; M mutes it along with the other sounds. Run `make test` for the handling and Doppler integration tests. W/Up accelerates, S/Down brakes, A/D or the arrows steer, P pauses, F toggles screen effects, M toggles sound, F11 toggles fullscreen, and R restarts after a crash.

The `assets/models` directory contains the supplied source archives and game-ready GLB exports. The player Senna, truck, and Mercedes use the supplied models; the Clio remains a low-poly placeholder. The unnamed coupe is excluded from traffic because its exported wheels are misaligned. Some source textures were absent from the archives, so the game exports use simplified materials. GLB files face -Z and use +Y as up; their origin is at the center of the car on the road. Keep the `assets` and `shaders` directories beside the standalone project or executable's parent directory.

Overdrive links the shared Doppler engine from the repository’s `physics` directory.
