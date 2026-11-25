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

## Up-coming Games

- Empty for now...

## Game Details

- [Galaga](#galaga)
- [Tetris](#tetris)
- [Blackjack](#blackjack)

### Galaga

> Well...it's partially phsyically accurate lol

build that bad boy with:  
```sh
cc -std=c11 -Wall -Wextra -Wpedantic games/galaga.c physics/doppler.c \                                 ❮❮
                  -o galaga $(pkg-config --cflags --libs raylib) -lm
```

### Tetris

Build with a C compiler, raylib, and pkg-config installed:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic games/tetris.c -o tetris $(pkg-config --cflags --libs raylib) -lm
./tetris
```

### Blackjack

> Well...yet again I've overengineered the rng with CSPRNG and Fisher-Yates (yeah yeah there is no modulo bias you nerd!)


## LICENSE shit

it's MIT go feral about it
here's the license -> [LICENSE](LICENSE)