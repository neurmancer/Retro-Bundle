# Retro-Bundle


> A basic retro game bundle for NeurOS base programs 
> Idea is simple...distracting youself with some all time classic games when you're bored 

# Table Of Contents 

- [Games](#games)
- [Upcoming Games](#up-coming-games)
- [Game Details](#game-details)



## Games 

- Pong
- Breakout
- Tetris
- Partially Physically Accurate Galaga
- 2048
- Blackjack

## Up-coming Games

- Empty for now...

## Game Details

- [Galaga](#galaga)
- [Tetris](#tetris)
- [Blackjack](#blackjack)

### Building and running

Install a C compiler, `make`, `pkg-config`, and raylib, then build all games:

```sh
make
./bins/pong
```

Executables are placed in `bins/`; use `make bins/blackjack` (or another game name) to build one game and `make clean` to remove the built executables. The games locate their resources relative to the repository and the executable, so keep `bins/`, `games/`, and `shaders/` together.

### Galaga

> Well...it's partially phsyically accurate lol

### Tetris

Stack blocks and clear lines.

### Blackjack

> Well...yet again I've overengineered the rng with CSPRNG and Fisher-Yates (yeah yeah there is no modulo bias you nerd!)


## LICENSE shit

it's MIT go feral about it
here's the license -> [LICENSE](LICENSE)